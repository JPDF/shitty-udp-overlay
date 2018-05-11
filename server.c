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


//colors
#define ANSI_WHITE   "\x1b[1;37m"
#define ANSI_RED     "\x1b[1;31m"
#define ANSI_GREEN   "\x1b[1;32m"
#define ANSI_YELLOW  "\x1b[1;33m"
#define ANSI_BLUE    "\x1b[1;34m"
#define ANSI_MAGENTA "\x1b[1;35m"
#define ANSI_CYAN    "\x1b[1;36m"
#define ANSI_RESET   "\x1b[0m"
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
<<<<<<< HEAD
#define FRAME_RECEIVED 101
#define FRAME_IN_WINDOW 102
#define MOVE_WINDOW 103
#define BUFFER 104
//TEARDOWN STATES
#define CLOSE_WAIT 20
#define LAST_ACK 21
//SLIDING WINDOW SIZE
#define BUFFER_SIZE 2
#define WINDOW_SIZE BUFFER_SIZE/2

/* Handles received packets and set the state accordingly.
   Packet is NULL if timeout occured */
int handlePacket(int state, int mySocket, struct packet *packet, struct sockaddr_in *source, struct client *client) {
	static int resendCount = 0;
	static int slidingWindowSize = WINDOW_SIZE;
	static int slidingWindowIndexFirst = 0, slidingWindowIndexLast = 0;
	static struct packet *windowBuffer[BUFFER_SIZE];
	
	struct packet myPacket;
	switch (state) {
		case INIT:
			printf(ANSI_WHITE"WAITING FOR CONNECTION..."ANSI_RESET "\n");
			state = LISTEN;
			break;
		case LISTEN:
			if (packet != NULL && packet->flags == SYN) {
				printf(ANSI_GREEN"SYN received from"ANSI_BLUE" %s" ANSI_GREEN":" ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				myPacket = createPacket(SYNACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf(ANSI_GREEN"SYNACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				printf(ANSI_WHITE"LISTEN GOING TO SYN_RECEIVED\n"ANSI_RESET);
				state = SYN_RECEIVED;
			}
			break;
			
		case SYN_RECEIVED:
			if (resendCount >= MAX_RESENDS){
				resendCount = 0;
				state = INIT;
				printf(ANSI_WHITE"CONNECTION TIMEOUT GOING TO CLOSED\n"ANSI_RESET);
			}
			else if (packet == NULL){//packet == NULL -> timeout
				myPacket = createPacket(SYNACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf(ANSI_RED"SYNACK resent to "ANSI_BLUE"%s"ANSI_RED":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				resendCount++;
			}
			else if (packet->flags == ACK){
				resendCount = 0;
				state = WAIT;
				printf(ANSI_GREEN"ACK received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				printf(ANSI_WHITE"CONNECTION SUCCESS GOING TO DATA TRANSMISSION\n"ANSI_RESET);
			}
			break;
			
		//sliding window
			
		case WAIT:
			if(packet != NULL && packet->flags == FRAME){
				printf(ANSI_WHITE"WAIT GOING TO FRAME_RECEIVED\n"ANSI_RESET);
				state = FRAME_RECEIVED;
				slidingWindowIndexLast = packet->windowsize-1;
				while(state != WAIT){
					switch (state) {
						case FRAME_RECEIVED:
							if (slidingWindowIndexFirst < slidingWindowIndexLast){
								if(packet->seq >= slidingWindowIndexFirst && packet->seq <= slidingWindowIndexLast){
									printf(ANSI_WHITE"FRAME_RECEIVED GOING TO FRAME_IN_WINDOW\n"ANSI_RESET);
									state = FRAME_IN_WINDOW;
								}
								else{
									myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
									sendPacket(mySocket, &myPacket, source);
									printf(ANSI_WHITE"FRAME_RECEIVED GOING TO WAIT - FRAME OUTSIDE WINDOW\n"ANSI_RESET);
									printf(ANSI_GREEN"OUT D ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
									state = WAIT;
								}
							}
							else if (slidingWindowIndexFirst > slidingWindowIndexLast){
								if((packet->seq >= slidingWindowIndexFirst && packet->seq <= BUFFER_SIZE-1) || (packet->seq <= slidingWindowIndexLast && packet->seq >= 0)){
									printf(ANSI_WHITE"FRAME_RECEIVED GOING TO FRAME_IN_WINDOW\n"ANSI_RESET);
									state = FRAME_IN_WINDOW;
								}
								else{
									myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
									sendPacket(mySocket, &myPacket, source);
									printf(ANSI_WHITE"FRAME_RECEIVED GOING TO WAIT - FRAME OUTSIDE WINDOW\n"ANSI_RESET);
									printf(ANSI_GREEN"OUT D ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
									state = WAIT;
								}
							}
							//TODO: ELSE IF FOR BROKEN FRAME THAT SENDS A NAK
							break;
						case FRAME_IN_WINDOW:
							if(packet->seq == slidingWindowIndexFirst){
								myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
								sendPacket(mySocket, &myPacket, source);
								printf(ANSI_WHITE"FRAME_IN_WINDOW GOING TO MOVE_WINDOW - FRAME IS FIRST\n"ANSI_RESET);
								printf(ANSI_GREEN"D ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
								state = MOVE_WINDOW;
							}
							break;
						case MOVE_WINDOW:
							if(slidingWindowIndexFirst != BUFFER_SIZE-1 && slidingWindowIndexLast != BUFFER_SIZE-1){
							slidingWindowIndexFirst++;
							slidingWindowIndexLast++;
							}
							else if(slidingWindowIndexFirst == BUFFER_SIZE-1){
							slidingWindowIndexFirst = 0;
							slidingWindowIndexLast++;
							}
							else if(slidingWindowIndexLast == BUFFER_SIZE-1){
							slidingWindowIndexLast = 0;
							slidingWindowIndexFirst++;
							}
							printf(ANSI_WHITE"MOVE_WINDOW GOING TO BUFFER\n"ANSI_RESET);
							state = BUFFER;
							
							break;
						case BUFFER:
							if(windowBuffer[slidingWindowIndexFirst]!=NULL){
								windowBuffer[slidingWindowIndexFirst] = NULL;
								printf(ANSI_WHITE"BUFFER GOING TO MOVE_WINDOW\n"ANSI_RESET);
								state = MOVE_WINDOW;
							}
							else{
							printf(ANSI_WHITE"BUFFER GOING TO WAIT - NO FRAME IN BUFFER IS FIRST IN WINDOW\n"ANSI_RESET);
							state = WAIT;
							}
							break;
						
					}
				}
			}
				
			else if (packet != NULL && packet->flags == FIN){
				printf(ANSI_GREEN"FIN received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf(ANSI_GREEN"ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				printf(ANSI_WHITE"DATA RECEIVE STATE GOING TO CLOSE_WAIT\n"ANSI_RESET);
				state = CLOSE_WAIT;
=======
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
>>>>>>> 848fd7a6bd8791fbef7e7b502e5097f622aa55d5
				}
				break;
			
<<<<<<< HEAD
		//TEARDOWN
		
		case CLOSE_WAIT:
			myPacket = createPacket(FIN, 0, 0, 0, 0, NULL);
			sendPacket(mySocket, &myPacket, source);
			printf(ANSI_GREEN"FIN sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
			printf(ANSI_WHITE"CLOSE_WAIT GOING TO LAST_ACK\n"ANSI_RESET);
			state = LAST_ACK;
			break;
			
		case LAST_ACK:
			if (resendCount >= MAX_RESENDS){
				resendCount = 0;
				state = INIT;
				printf(ANSI_WHITE "MAX RESENDS REACHED - LAST_ACK GOING TO CLOSED\n"ANSI_RESET);
			}
			else if (packet == NULL){
				myPacket = createPacket(FIN, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf(ANSI_RESET"FIN resent to "ANSI_BLUE"%s"ANSI_RED":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				resendCount++;
			}
			else if (packet->flags == ACK){
				printf(ANSI_GREEN"ACK received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				resendCount = 0;
				state = INIT;
				printf(ANSI_WHITE"CONNECTION SHUTDOWN SUCCESSFULLY - LAST_ACK GOING TO CLOSED\n"ANSI_RESET);
			}
			break;
=======
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
>>>>>>> 848fd7a6bd8791fbef7e7b502e5097f622aa55d5
			
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
