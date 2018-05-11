#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include "packet.h"
#include "error.h"
#include "color.h"
#include "slidingwindow.h"

struct client {
	int id;
	int windowsize;
};

// SETTINGS
#define PORT 5555
#define TIMEOUT 1000 // The time before timeout in milliseconds
#define MAX_RESENDS 10

#define MAX_SEQUENCE 10
#define WINDOW_SIZE MAX_SEQUENCE/2

// CONNECTION STATE
#define LISTEN 1
#define SYN_RECEIVED 2
#define ESTABLISHED 3
// SLIDING WINDOW STATES
#define WAIT 10
#define FRAME_RECEIVED 11
#define FRAME_IN_WINDOW 12
#define MOVE_WINDOW 13
#define BUFFER 14
#define CLOSE 15
// TEARDOWN STATES
#define CLOSE_WAIT 20
#define LAST_ACK 21
#define DISCONNECTED 22

void acceptConnection(int socket, struct client *client) {
	int state = LISTEN, resendCount = 0, isTimeout;
	struct packet packet;
	struct sockaddr_in source;
	
	printf("WAITING FOR CONNECTION...\n");
	
	while (state != ESTABLISHED) {
		fflush(stdout);
		isTimeout = receivePacketWithTimeout(socket, &packet, &source, TIMEOUT);
		
		switch (state) {
			case LISTEN:
				if (!isTimeout && packet.flags == SYN) {
					printf(ANSI_GREEN"SYN received from"ANSI_BLUE" %s" ANSI_GREEN":" ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					packet = createPacket(SYNACK, packet.id, 0, packet.windowsize, 0, NULL);
					sendPacket(socket, &packet, &source);
					printf(ANSI_GREEN"SYNACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					printf(ANSI_WHITE"LISTEN GOING TO SYN_RECEIVED\n"ANSI_RESET);
					state = SYN_RECEIVED;
				}
				break;
			
			case SYN_RECEIVED:
				if (resendCount >= MAX_RESENDS){
					resendCount = 0;
					state = LISTEN;
					printf(ANSI_WHITE"CONNECTION TIMEOUT GOING TO LISTEN\n"ANSI_RESET);
				}
				else if (isTimeout){
					packet = createPacket(SYNACK, 0, 0, 0, 0, NULL);
					sendPacket(socket, &packet, &source);
					printf(ANSI_RED"SYNACK resent to "ANSI_BLUE"%s"ANSI_RED":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					resendCount++;
				}
				else if (packet.flags == ACK){
					state = ESTABLISHED;
					client->id = packet.id;
					client->windowsize = packet.windowsize;
					printf(ANSI_GREEN"ACK received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					printf(ANSI_WHITE"CONNECTION ESTABLISHED\n"ANSI_RESET);
				}
				break;
		}
	}
}

void insertToBuffer(struct packet buffer[], int *bufferCount, const struct packet *packet) {
	int i;
	for (i = *bufferCount - 1; i >= 0; i++) {
		if (packet->seq > buffer[i].seq) {
			buffer[i + 1] = buffer[i];
			buffer[i] = *packet;
		}
	}
	(*bufferCount)++;
}

void slidingWindow(int socket, const struct client *client) {
	int state = WAIT, fin = 0;
	struct packet packet;
	struct sockaddr_in source;
	struct packet *buffer = malloc(sizeof(struct packet) * (client->windowsize - 1));
	int bufferCount = 0;
	int firstSeq = 0, lastSeq = client->windowsize - 1;
	
	printf("windowsize: %d\n", client->windowsize);
	
	while (state != CLOSE) {
		fflush(stdout);
		switch (state) {
			case WAIT:
				if (!fin)
					receivePacket(socket, &packet, &source);
				else if (receivePacketWithTimeout(socket, &packet, &source, TIMEOUT)) { // if timeout occured, close
					state = CLOSE;
				}
				if (packet.flags == FRAME) {
					printf(ANSI_GREEN"FRAME received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					state = FRAME_RECEIVED;
				}
				else if (packet.flags == FIN) {
					printf(ANSI_GREEN"FIN received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					fin = 1;
					packet = createPacket(ACK, packet.id, packet.seq, packet.windowsize, 0, NULL);
					sendPacket(socket, &packet, &source);
					printf(ANSI_GREEN"ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
				}
				break;
			
			case FRAME_RECEIVED:
				if (isInsideWindow(packet.seq, firstSeq, lastSeq)) {
					printf(ANSI_WHITE"FRAME_RECEIVED GOING TO FRAME_IN_WINDOW\n"ANSI_RESET);
					state = FRAME_IN_WINDOW;
				}
				else {
					packet = createPacket(ACK, client->id, packet.seq, client->windowsize, 0, NULL);
					sendPacket(socket, &packet, &source);
					printf(ANSI_WHITE"FRAME_RECEIVED GOING TO WAIT - FRAME OUTSIDE WINDOW\n"ANSI_RESET);
					printf(ANSI_GREEN"OUT D ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					state = WAIT;
				}
				break;
				
			case FRAME_IN_WINDOW:
				if (packet.seq == firstSeq) { // Check if packet is first frame
					packet = createPacket(ACK, client->id, packet.seq, client->windowsize, 0, NULL);
					sendPacket(socket, &packet, &source);
					printf(ANSI_WHITE"FRAME_IN_WINDOW GOING TO MOVE_WINDOW - FRAME IS FIRST\n"ANSI_RESET);
					printf(ANSI_GREEN"D ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					state = MOVE_WINDOW;
				}
				else {
					printf("packet not first frame, insert to buffer\n");
					insertToBuffer(buffer, &bufferCount, &packet);
					state = WAIT;
				}
				break;
			
			case MOVE_WINDOW:
				printf("Moving window from "ANSI_BLUE"%d"ANSI_RESET"-"ANSI_BLUE"%d "ANSI_RESET, firstSeq, lastSeq);
				firstSeq = (firstSeq + 1) % MAX_SEQUENCE;
				lastSeq = (lastSeq + 1) % MAX_SEQUENCE;
				printf("to "ANSI_BLUE"%d"ANSI_RESET"-"ANSI_BLUE"%d\n"ANSI_RESET, firstSeq, lastSeq);
				state = BUFFER;
				break;
			
			case BUFFER:
				if (bufferCount > 0 && buffer[bufferCount - 1].seq == firstSeq) {
					printf("Buffered frame in window\n");
					bufferCount--;
					state = MOVE_WINDOW;
				}
				else {
					printf("No buffered in window\n");
					state = WAIT;
				}
				break;
		}
	}
	free(buffer);
}

void teardown(int socket) {
	int state = CLOSE_WAIT;
	struct packet packet;
	struct sockaddr_in source;
	int resendCount = 0, isTimeout = 0;
	
	printf("Begin teardown...\n");
	
	while (state != DISCONNECTED) {
		fflush(stdout);
		switch (state) {
			case CLOSE_WAIT:
				packet = createPacket(FIN, packet.id, packet.seq, packet.windowsize, 0, NULL);
				sendPacket(socket, &packet, &source);
				printf(ANSI_GREEN"FIN sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
				state = LAST_ACK;
				break;
				
			case LAST_ACK:
				if (resendCount >= MAX_RESENDS)
					state = DISCONNECTED;
				isTimeout = receivePacketWithTimeout(socket, &packet, &source, TIMEOUT);
				if (isTimeout) {
					packet = createPacket(FIN, packet.id, packet.seq, packet.windowsize, 0, NULL);
					sendPacket(socket, &packet, &source);
					printf(ANSI_RED"FIN resent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					resendCount++;
				}
				else if (packet.flags == ACK) {
					printf(ANSI_RED"Last ACK received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					state = DISCONNECTED;
				}
				break;
		}
	}
	printf("Teardown complete\n");
}

int main() {
	struct sockaddr_in myAddress;
	int mySocket;
	struct client client;
	
	mySocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (mySocket == -1)
		fatal_error("Failed to create socket");
	
	myAddress.sin_family = AF_INET;
	myAddress.sin_port = htons(PORT);
	myAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (bind(mySocket, (struct sockaddr*)&myAddress, sizeof(myAddress)) == -1)
		fatal_error("Failed to bind socket");
	
	while (1) {
		acceptConnection(mySocket, &client); // Blocks untill connection is established
		
		slidingWindow(mySocket, &client);
		
		teardown(mySocket);
		
	}
	return 0;
}
