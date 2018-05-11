#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include "packet.h"
#include "color.h"
#include "error.h"
#include "slidingwindow.h"

// SETTINGS
#define TIMEOUT 1000 // The time before timeout in milliseconds
#define ESTABLISH_TIMEOUT 2000
#define FIN_WAIT_2_TIMEOUT 2000
#define MAX_RESENDS 10
#define BUFFER_SIZE 5

#define MAX_SEQUENCE 10
#define WINDOW_SIZE MAX_SEQUENCE/2

// CONNECTION STATES
#define CLOSED 0
#define SYN_SENT 1
#define ACK_SENT 2
#define ESTABLISHED 3
// SLIDING WINDOW STATES
#define WAIT 10
#define ACK_RECEIVED 11
#define ACK_IN_WINDOW 12
#define BUFFER 13
#define MOVE_WINDOW 14
#define CLOSE 15
// TEARDOWN STATES
#define FIN_WAIT_1 20
#define FIN_WAIT_2 21
#define CLOSING 22
#define TIME_WAIT 23
#define DISCONNECTED 24

struct client {
	int id;
	int windowsize;
};

void connectTo(int socket, const struct sockaddr_in *destination, struct client *client) {
	int state = CLOSED;
	struct packet packet;
	struct sockaddr_in source;
	int timeout = TIMEOUT, isTimeout, resendCount = 0;
	
	printf("CONNECTING...\n");
	
	while (state != ESTABLISHED) {
		fflush(stdout);
		switch (state) {
			case CLOSED:
				packet = createPacket(SYN, 0, 0, WINDOW_SIZE, 0, NULL);
				sendPacket(socket, &packet, destination);
				printf(ANSI_GREEN"SYN sent to"ANSI_CYAN" %s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
				printf(ANSI_WHITE"CLOSED GOING TO SYN_SENT\n"ANSI_RESET);
				state = SYN_SENT;
				break;
			
			case SYN_SENT:
				if (isTimeout) { // TIMEOUT!
					resendCount++;
					packet = createPacket(SYN, 0, 0, WINDOW_SIZE, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_RED"SYN resent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
				}
				else if (packet.flags == SYNACK) {
					// Saving settings received from server
					client->windowsize = packet.windowsize;
					client->id = packet.id;
					sleep(4);
					printf(ANSI_GREEN"SYNACK received from "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					packet = createPacket(ACK, 0, 0, packet.windowsize, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"ACK sent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
					printf(ANSI_WHITE"SYN_SENT GOING TO ACK_SENT\n"ANSI_RESET);
					state = ACK_SENT;
					timeout = ESTABLISH_TIMEOUT;
					resendCount = 0;
				}
				break;
			
			case ACK_SENT:
				if (isTimeout) {
					resendCount = 0;
					printf(ANSI_WHITE"CONNECTION ESTABLISHED\n"ANSI_RESET);
	 				state = ESTABLISHED;
				}
				else if (packet.flags == SYNACK) {
					printf(ANSI_RED"SYNACK received from "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					resendCount++;
					packet = createPacket(ACK, 0, 0, client->windowsize, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"ACK resent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
				}
				else if(resendCount >= MAX_RESENDS){
					resendCount = 0;
					printf(ANSI_WHITE"MAX RESENDS REACHED, ACK_SENT GOING TO CLOSED\n"ANSI_RESET);
					state = CLOSED;
				}
				break;
		}
		isTimeout = receivePacketWithTimeout(socket, &packet, &source, timeout);
	}
}

void insertToBuffer(int buffer[], int *bufferCount, int seq) {
	int i;
	for (i = *bufferCount - 1; i >= 0; i++) {
		if (seq > buffer[i]) {
			buffer[i + 1] = buffer[i];
			buffer[i] = seq;
		}
	}
	(*bufferCount)++;
}

void slidingWindow(int socket, const struct sockaddr_in *destination, const char **data, int dataCount, const struct client *client) {
	int state = WAIT;
	struct packet packet;
	struct sockaddr_in source;
	int isTimeout = 0, i = 0, k = client->windowsize;
	int *buffer = malloc(sizeof(int) * client->windowsize);
	int bufferCount = 0, firstSeq = 0, lastSeq = client->windowsize - 1;
	printf("%d - %d\n", firstSeq, lastSeq);
	
	while (state != CLOSE) {
		fflush(stdout);
		switch (state) {
			case WAIT:
				for (i; i < k; i++) {
					packet = createPacket(FRAME, client->id, i & MAX_SEQUENCE, client->windowsize, 0, data[i]);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"FRAME ["ANSI_BLUE"%s"ANSI_GREEN"]:"ANSI_BLUE"%d"ANSI_GREEN" sent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, data[i], i, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
					dataCount--;
				}
				isTimeout = receivePacketWithTimeout(socket, &packet, &source, TIMEOUT);
				if (isTimeout) {
					// TODO: resend lost frame
					packet = createPacket(FRAME, client->id, firstSeq, client->windowsize, 0, data[firstSeq]);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"FRAME ["ANSI_BLUE"%s"ANSI_GREEN"]:"ANSI_BLUE"%d"ANSI_GREEN" sent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, data[firstSeq], firstSeq, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
				}
				else if (packet.flags == ACK) {
					// TODO: handle ack
					printf(ANSI_GREEN"ACK received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					state = ACK_RECEIVED;
				}
				else if (dataCount == 0) {
					packet = createPacket(FIN, 0, 0, client->windowsize, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"FIN sent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
					state = CLOSE;
				}
				break;
		
			case ACK_RECEIVED:
				if (isInsideWindow(packet.seq, firstSeq, lastSeq)) {
					printf(ANSI_WHITE"ACK_RECEIVED GOING TO ACK_IN_WINDOW\n"ANSI_RESET);
					state = ACK_IN_WINDOW;
				}
				else {
					printf(ANSI_WHITE"ACK_RECEIVED GOING TO WAIT - FRAME OUTSIDE WINDOW\n"ANSI_RESET);
					state = WAIT;
				}
				break;
		
			case ACK_IN_WINDOW:
				if (packet.seq == firstSeq) { // Check if packet is first frame
					printf(ANSI_WHITE"ACK_IN_WINDOW GOING TO MOVE_WINDOW - ACK IS FIRST\n"ANSI_RESET);
					state = MOVE_WINDOW;
				}
				else {
					printf("packet not first ACK, insert to buffer\n");
					insertToBuffer(buffer, &bufferCount, packet.seq);
					state = WAIT;
				}
				break;
		
			case BUFFER:
				if (bufferCount > 0 && buffer[bufferCount - 1] == firstSeq) {
					printf("Buffered ack in window\n");
					bufferCount--;
					state = MOVE_WINDOW;
				}
				else {
					printf("No buffered in window\n");
					state = WAIT;
				}
				break;
		
			case MOVE_WINDOW:
				printf("Moving window from "ANSI_BLUE"%d"ANSI_RESET"-"ANSI_BLUE"%d "ANSI_RESET, firstSeq, lastSeq);
				firstSeq = (firstSeq + 1) % MAX_SEQUENCE;
				lastSeq = (lastSeq + 1) % MAX_SEQUENCE;
				k++;
				printf("to "ANSI_BLUE"%d"ANSI_RESET"-"ANSI_BLUE"%d\n"ANSI_RESET, firstSeq, lastSeq);
				state = BUFFER;
				break;
		}
	}
}

void teardown(int socket, struct sockaddr_in *destination) {
	int state = FIN_WAIT_1;
	struct packet packet;
	struct sockaddr_in source;
	int timeout = TIMEOUT, isTimeout = 0, resendCount = 0;
	
	printf("Begin teardown...\n");
	
	while (state != DISCONNECTED) {
		fflush(stdout);
		switch (state) {
			case FIN_WAIT_1:
				if (resendCount >= MAX_RESENDS) {
					resendCount = 0;
					printf(ANSI_WHITE"TIMEOUT MAX SENT\n"ANSI_RESET);
					state = DISCONNECTED;
				}
				else if (isTimeout) {
					resendCount++;
					packet = createPacket(FIN, 0, 0, 0, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_RED"FIN resent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
				}
				else if (packet.flags == ACK){
					resendCount = 0;
					printf(ANSI_GREEN"ACK received from "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					timeout = FIN_WAIT_2_TIMEOUT;
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO FIN_WAIT_2\n"ANSI_RESET);
					state = FIN_WAIT_2;
				}
				else if (packet.flags == FIN){
					printf(ANSI_GREEN"FIN received from "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					resendCount = 0;
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"FIN sent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO CLOSING\n"ANSI_RESET);
					state = CLOSING;
				}
				break;
				
			case FIN_WAIT_2:
				if (isTimeout) { // TIMEOUT!
					printf(ANSI_WHITE"Timeout IN FIN_WAIT_2 GOING TO CLOSED\n"ANSI_RESET);
	 				state = DISCONNECTED;
				}
				else if (packet.flags == FIN){
					printf(ANSI_GREEN"FIN received from "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_GREEN"ACK sent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
					printf(ANSI_WHITE"FIN_WAIT_2 GOING TO TIME_WAIT\n"ANSI_RESET);
					state = TIME_WAIT;
				}			
				break;
			
			case CLOSING:
				if (resendCount >= MAX_RESENDS){
					resendCount = 0;
					state = DISCONNECTED;
				}
				else if (isTimeout) {
					resendCount++;
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(socket, &packet, destination);
					printf(ANSI_RED"ACK resent to "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(destination->sin_addr), ntohs(destination->sin_port));
				}
				else if(packet.flags == ACK){
					printf(ANSI_GREEN"ACK received from "ANSI_CYAN"%s"ANSI_GREEN":"ANSI_CYAN"%d\n"ANSI_RESET, (char*)inet_ntoa(source.sin_addr), ntohs(source.sin_port));
					resendCount = 0;
					printf(ANSI_WHITE"CLOSING GOING TO TIME_WAIT\n"ANSI_RESET);
					state = TIME_WAIT;
				}
				break;
			
			case TIME_WAIT:
				if(isTimeout){
					printf(ANSI_WHITE"TIME_WAIT DONE\n"ANSI_RESET);
					resendCount = 0;
					state = DISCONNECTED;
				}
				break;
			
		}
		if (state != DISCONNECTED)
			isTimeout = receivePacketWithTimeout(socket, &packet, &source, timeout);
	}
	printf(ANSI_WHITE"DISCONNECTED\n"ANSI_RESET);
}

int main(int argc, char **argv) {
	int mySocket, on = 1;
	struct client client;
	struct sockaddr_in destination;
	
	if (argc < 3)
		fatal_error("Too few arguments");
		
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(argv[1]);
	destination.sin_port = htons(atoi(argv[2]));
	// Socket creation...
	mySocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (mySocket == -1)
		fatal_error("Failed to create a socket");
		
	setsockopt(mySocket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
	
	int dataCount = 10;
	const char *data[10] = {
		"a",
		"b",
		"c",
		"d",
		"e",
		"f",
		"g",
		"h",
		"i",
		"j"
	};
	
	while (1) {
		connectTo(mySocket, &destination, &client);
		
		slidingWindow(mySocket, &destination, data, dataCount, &client);
		
		teardown(mySocket, &destination);
	}
	return 0;
}
