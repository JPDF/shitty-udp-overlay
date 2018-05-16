#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include "packet.h"
#include "misc.h"
#include <time.h>

// SETTINGS
#define PORT 5555
#define TIMEOUT 1000 // The time before timeout in milliseconds
#define MAX_RESENDS 10
#define NO_MESSAGE_TIMEOUT 10000 //time before the client is considered to have disconnected out of order

// STATES
#define INIT 0
#define LISTEN 1
#define SYN_RECEIVED 2
//sliding window states
#define WAIT 10
#define FRAME_RECEIVED 11
#define FRAME_IN_WINDOW 12
#define MOVE_WINDOW 13
#define BUFFER 14
//TEARDOWN STATES
#define CLOSE_WAIT 20
#define LAST_ACK 21
//SLIDING WINDOW SIZE
#define MAX_SEQUENCE 10
#define WINDOW_SIZE MAX_SEQUENCE/2

int makeSocket(int port) {
	int mySocket;
	struct sockaddr_in address;
	
	// Socket creation...
	mySocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (mySocket == -1)
		fatalerror("Failed to create a socket");
	
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	// Bind to a specific port
	if (bind(mySocket, (struct sockaddr*)&address, sizeof(address)) == -1)
		fatalerror("Failed to bind socket");
	return mySocket;
}

/* Handles received packets and set the state accordingly.
   Packet is NULL if timeout occured */


int main() {
	int mySocket = makeSocket(PORT);
	int state = INIT;
	int receiveStatus;
	int i = 0;
	struct packet packet;
	struct sockaddr_in otherAddress;
	int resendCount = 0;
	int windowsize = WINDOW_SIZE;
	int slidingWindowIndexFirst = 0, slidingWindowIndexLast = 0;
	struct packet *windowBuffer;
	TimerList timerList = NULL;
	struct timespec start, stop;
	time_t deltaTime = 0, tTimeout = 0;
	char *finalBuffer = NULL;
	
	createPacket(&packet, SYN, 0, 100, 200, "bla");
	if (!isPacketBroken(&packet))
		printf("Packet is not broken, hurray!\n");
	
	srand(time(NULL));
	clock_gettime(CLOCK_MONOTONIC, &start);
	// Event handling loop
	while (1) {
		switch (state) {
			case INIT:
				printf(ANSI_WHITE"WAITING FOR CONNECTION..."ANSI_RESET "\n");
				state = LISTEN;
				break;
			case LISTEN:
				if (receiveStatus == RECEIVE_OK && packet.flags == SYN) {
					if(windowsize > packet.windowsize){
						windowsize = packet.windowsize;
					}
					
					createAndSendPacketWithResendTimer(mySocket,SYNACK, 0, 0, windowsize, NULL, &otherAddress, &timerList, deltaTime);
					/*createPacket(&packet, SYNACK, 0, 0, windowsize, NULL);
					sendPacket(mySocket, &packet, &otherAddress, 0);
					addPacketTimer(&timerList, &packet, &otherAddress, deltaTime);*/
					printf(ANSI_WHITE"LISTEN GOING TO SYN_RECEIVED\n"ANSI_RESET);
					state = SYN_RECEIVED;
				}
				break;
			case SYN_RECEIVED:
				if (resendCount >= MAX_RESENDS){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					state = INIT;
					printf(ANSI_WHITE"CONNECTION TIMEOUT GOING TO CLOSED\n"ANSI_RESET);
				}
				else if (receiveStatus == RECEIVE_OK && packet.flags == ACK){
					removePacketTimerBySeq(&timerList, packet.seq);
					if((windowBuffer=malloc(sizeof(struct packet)*(windowsize-1))) == NULL)
						fatalerror("malloc of windowBuffer failed");
					for(i = 0; i < windowsize-1;i++){
						windowBuffer[i].seq = -1;
					}
					tTimeout = deltaTime;
					resendCount = 0;
					state = WAIT;
					printf(ANSI_WHITE"CONNECTION SUCCESS GOING TO DATA TRANSMISSION\n"ANSI_RESET);
				}
				slidingWindowIndexLast=windowsize-1;
				slidingWindowIndexFirst=0;
				break;
			
			//sliding window
			
			case WAIT:
				if (deltaTime - tTimeout > NO_MESSAGE_TIMEOUT) { // TIMEOUT!
					printf(ANSI_WHITE"NO MESSAGE RECEIVED. WAIT GOING TO INIT\n"ANSI_RESET);
	 				state = INIT;
					printf(" - - - - %s - - - -\n", finalBuffer);
					free(finalBuffer);
					resendCount = 0;
					finalBuffer = NULL;
				}
				else if(receiveStatus != RECEIVE_TIMEOUT && packet.flags == FRAME){
					tTimeout = deltaTime;
					printf(ANSI_WHITE"WAIT GOING TO FRAME_RECEIVED\n"ANSI_RESET);
					state = FRAME_RECEIVED;
					while(state != WAIT){
						switch (state) {
							case FRAME_RECEIVED:
								if (receiveStatus == RECEIVE_BROKEN){
									createAndSendPacket(mySocket, NACK, 0, packet.seq, windowsize, NULL, &otherAddress);
								/*		createPacket(&packet, NACK, 0, packet.seq, windowsize, NULL);
										sendPacket(mySocket, &packet, &otherAddress, 0);*/
									printf(ANSI_WHITE"FRAME_RECEIVED GOING TO WAIT - FRAME BROKEN SENDING NACK\n"ANSI_RESET);
									state = WAIT;
								}
								else if (slidingWindowIndexFirst <= slidingWindowIndexLast){
									if(packet.seq >= slidingWindowIndexFirst && packet.seq <= slidingWindowIndexLast){
										printf(ANSI_WHITE"FRAME_RECEIVED GOING TO FRAME_IN_WINDOW\n"ANSI_RESET);
										state = FRAME_IN_WINDOW;
									}
									else{
										createAndSendPacket(mySocket, ACK, 0, packet.seq, windowsize, NULL, &otherAddress);
										/*createPacket(&packet, ACK, 0, packet.seq, windowsize, NULL);
										sendPacket(mySocket, &packet, &otherAddress, 0);*/
										printf(ANSI_WHITE"FRAME_RECEIVED GOING TO WAIT - FRAME OUTSIDE WINDOW\n"ANSI_RESET);
										state = WAIT;
									}
								}
								else if (slidingWindowIndexFirst > slidingWindowIndexLast){
									if((packet.seq >= slidingWindowIndexFirst && packet.seq <= MAX_SEQUENCE-1) || (packet.seq <= slidingWindowIndexLast && packet.seq >= 0)){
										printf(ANSI_WHITE"FRAME_RECEIVED GOING TO FRAME_IN_WINDOW\n"ANSI_RESET);
										state = FRAME_IN_WINDOW;
									}
									else{
										createAndSendPacket(mySocket, ACK, 0, packet.seq, windowsize, NULL, &otherAddress);
										/*createPacket(&packet, ACK, 0, packet.seq, windowsize, NULL);
										sendPacket(mySocket, &packet, &otherAddress, 0);*/
										printf(ANSI_WHITE"FRAME_RECEIVED GOING TO WAIT - FRAME OUTSIDE WINDOW\n"ANSI_RESET);
										printf(ANSI_GREEN"OUT D ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(otherAddress.sin_addr), ntohs(otherAddress.sin_port));
										state = WAIT;
									}
								}
								break;
							case FRAME_IN_WINDOW:
								if(packet.seq == slidingWindowIndexFirst){
									printf(ANSI_WHITE"FRAME_IN_WINDOW GOING TO MOVE_WINDOW - FRAME IS FIRST\n"ANSI_RESET);
									state = MOVE_WINDOW;
								}
								else{
									for(int i = 0; i < windowsize-1;i++){
										if(windowBuffer[i].seq==-1){
											windowBuffer[i]=packet;
											printf(ANSI_WHITE"FRAME_IN_WINDOW GOING TO BUFFER - FRAME IS NOT FIRST");
											state = BUFFER;
											break;
										}
									}
								}
								createAndSendPacket(mySocket, ACK, 0, packet.seq, windowsize, NULL, &otherAddress);
								/*createPacket(&packet, ACK, 0, packet.seq, windowsize, NULL);
								sendPacket(mySocket, &packet, &otherAddress, 0);*/
								break;
							case MOVE_WINDOW:
								if (finalBuffer == NULL)
									finalBuffer = malloc(strlen(packet.data) + 1);
								else
									finalBuffer = (char*)realloc(finalBuffer, strlen(finalBuffer)+strlen(packet.data)+1);
								if (finalBuffer == NULL)
									fatalerror("Failed to malloc/realloc finalBuffer");
								if(finalBuffer == NULL){
									finalBuffer = malloc(strlen(packet.data)+1);
									strcpy(finalBuffer, "Med:");
								}
								else 
									finalBuffer = (char*)realloc(finalBuffer, strlen(finalBuffer) + strlen(packet.data) + 1);
								if (finalBuffer == NULL)
									fatalerror("failed to malloc/realloc finalBuffer\n");
								strcat(finalBuffer, packet.data);
								slidingWindowIndexFirst = (slidingWindowIndexFirst+1) % MAX_SEQUENCE;
								slidingWindowIndexLast = (slidingWindowIndexLast+1) % MAX_SEQUENCE;
								printf(ANSI_WHITE"MOVE_WINDOW GOING TO BUFFER\n"ANSI_RESET);
								state = BUFFER;
							
								break;
							case BUFFER:
								for(i = 0; i < windowsize-1;i++){
									if(windowBuffer[i].seq == slidingWindowIndexFirst){
										state = MOVE_WINDOW;
										packet = windowBuffer[i];
										windowBuffer[i].seq = -1;
										break;
									}
								}	
								if(state != MOVE_WINDOW){
								printf(ANSI_WHITE"BUFFER GOING TO WAIT - NO FRAME IN BUFFER IS FIRST IN WINDOW\n"ANSI_RESET);
								state = WAIT;
								}
								break;
						
						}
					}
				}
				else if (receiveStatus == RECEIVE_OK && packet.flags == FIN){
					createAndSendPacket(mySocket, ACK, 0, packet.seq, windowsize, NULL, &otherAddress);
					/*
					createPacket(&packet, ACK, 0, packet.seq, windowsize, NULL);
					sendPacket(mySocket, &packet, &otherAddress, 0);*/
					printf(ANSI_WHITE"DATA RECEIVE STATE GOING TO CLOSE_WAIT\n"ANSI_RESET);
					state = CLOSE_WAIT;
					printf(" - - - - %s - - - -\n", finalBuffer);
					free(finalBuffer);
					resendCount = 0;
					finalBuffer = NULL;
					}
				break;
			
			//TEARDOWN
		
			case CLOSE_WAIT:
				createAndSendPacketWithResendTimer(mySocket, FIN, 0, 0, windowsize, NULL, &otherAddress, &timerList, deltaTime);
				/*createPacket(&packet, FIN, 0, packet.seq, windowsize, NULL);
				sendPacket(mySocket, &packet, &otherAddress, 0);
				addPacketTimer(&timerList, &packet, &otherAddress, deltaTime);*/
				printf(ANSI_WHITE"CLOSE_WAIT GOING TO LAST_ACK\n"ANSI_RESET);
				state = LAST_ACK;
				break;
			
			case LAST_ACK:
				if (resendCount >= MAX_RESENDS){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					state = INIT;
					printf(ANSI_WHITE "MAX RESENDS REACHED - LAST_ACK GOING TO CLOSED\n"ANSI_RESET);
				}
				else if (receiveStatus == RECEIVE_OK && packet.flags == ACK){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					state = INIT;
					printf(ANSI_WHITE"CONNECTION SHUTDOWN SUCCESSFULLY - LAST_ACK GOING TO CLOSED\n"ANSI_RESET);
				}
				break;
			
			default:
				fatalerror("Server entered incorrect state");
				break;
		}
		fflush(stdout);
		receiveStatus = receivePacketOrTimeout(mySocket, &packet, &otherAddress, TIMEOUT);
		
		clock_gettime(CLOCK_MONOTONIC, &stop);
		deltaTime = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);

		updateTimers(mySocket, &timerList, deltaTime, &resendCount);
	}
	return 0;
}
