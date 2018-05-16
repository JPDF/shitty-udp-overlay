#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "packet.h"
#include "misc.h"

// SETTINGS
#define HEARTBEAT_TIMEOUT 1
#define TIMEOUT 1000 // The time before timeout on each packet in milliseconds
#define MAX_RESENDS 10
#define MAX_SEQUENCE 8
#define MAX_WINDOWSIZE MAX_SEQUENCE / 2
#define ACK_SENT_TIMEOUT 5000
#define FIN_WAIT_2_TIMEOUT ACK_SENT_TIMEOUT
#define WAIT_TIMEOUT 10000

// STATES
#define CLOSED 0
#define SYN_SENT 1
#define ACK_SENT 2
// MENU STATES
#define MESSAGE_MENU 100
#define ERROR_MENU 101
#define LOST_FRAMES 102
#define WRONG_ORDER 103
#define BROKEN_CRC 104
// SLIDING WINDOW STATES
#define WAIT 10
#define ACK_RECEIVED 11
#define ACK_IN_WINDOW 12
#define MOVE_WINDOW 13
#define BUFFER 14
// TEARDOWN STATES
#define FIN_WAIT_1 20
#define FIN_WAIT_2 21
#define CLOSING 22
#define TIME_WAIT 23

#define MAX_MESSAGE 101


void hackAndSlashMessage(char *message, char **data, int dataLength){
	int chunklen;
	int i;
	for(i = 0; i < dataLength; i++){
		chunklen = strlen(&message[(DATA_LENGHT) * i]);
		if (chunklen > DATA_LENGHT)
			chunklen = DATA_LENGHT;
		
		data[i] = malloc(chunklen + 1);
		strncpy(data[i], &message[DATA_LENGHT*i], chunklen);
		data[i][chunklen] = '\0';
	}
}

int isAlreadyBuffered(int *buffer, int size, int seq) {
	int i;
	for (i = 0; i < size; i++) {
		if (buffer[i] == seq)
			return 1;
	}
	return 0;
}


int main(int argc, char **argv) {
	int maxSequence = MAX_SEQUENCE;
	int state = CLOSED, receiveStatus, on = 1, currentSeq = 0, buffersize = 0, i;
	int slidingWindowIndexFirst = 0, slidingWindowIndexLast = 0;
	struct packet packet;
	struct sockaddr_in otherAddress;
	srand(time(NULL));
	int resendCount = 0, seq = 0, windowsize = MAX_WINDOWSIZE, dataIndex = 0, acksReceived = 0;
	TimerList timerList = NULL;
	struct timespec start, stop;
	time_t deltaTime = 0, tTimeout = 0;
	int *windowBuffer;
	int errorChoice;
	char message[MAX_MESSAGE];
	
	char *data[(int)ceil((float)MAX_MESSAGE/(float)DATA_LENGHT)];
	int dataCount = 0;
	
	if (argc < 3)
		fatalerror("Too few arguments");
	otherAddress.sin_family = AF_INET;
	otherAddress.sin_addr.s_addr = inet_addr(argv[1]);
	otherAddress.sin_port = htons(atoi(argv[2]));
	// Socket creation...
	int mySocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (mySocket == -1)
		fatalerror("Failed to create a socket");
		
	setsockopt(mySocket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
	
	clock_gettime(CLOCK_MONOTONIC, &start);

	// Event handling loop
	while (1) {
	
		switch (state) {
			case CLOSED:
				maxSequence=MAX_SEQUENCE;
				windowsize = MAX_WINDOWSIZE;
				state = MESSAGE_MENU;
				for (i = 0; i < dataCount; i++) {
					if (data[i] != NULL)
						free(data[i]);
				}
				while (state != CLOSED){
					switch (state){
						case (MESSAGE_MENU):
							//clear();
							printf("\n\n\nType a message to send up to 100 characters long\n");
							if(fgets(message, 101, stdin)!=NULL) {
								//clear();
								state = ERROR_MENU;
								message[strlen(message) - 1] = '\0';
							}
							break;
							
						case (ERROR_MENU):
							printf("How do you want to mess up?\n0. Everything like heaven\n1. Lost frames\n2. Broken crc\n3. CHAOS!!1!!\n");
							scanf("%d%*c", &errorChoice);
							//clear();
							if(errorChoice >= 0 && errorChoice <= 3)
								state = CLOSED;
							else
								printf("User selected fucket up\n");
							break;
					}
				}
				error = errorChoice;
				dataCount = ceil((float)strlen(message)/(float)(DATA_LENGHT));
				hackAndSlashMessage(message, data, dataCount);
				clock_gettime(CLOCK_MONOTONIC, &stop);
				deltaTime = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);
				createAndSendPacketWithResendTimer(mySocket, SYN, 0, seq, windowsize, NULL, &otherAddress, &timerList, deltaTime);
				state = SYN_SENT;
				break;
			case SYN_SENT:
				if (packet.flags == SYNACK) {
					removePacketTimerBySeq(&timerList, packet.seq);
					
					// saving settings from server
					windowsize = packet.windowsize;
					slidingWindowIndexLast = packet.windowsize-1;
					buffersize = windowsize - 1;
					maxSequence = packet.windowsize*2;
					windowBuffer = malloc(sizeof(int) * (windowsize - 1));
					for (i = 0; i < buffersize; i++)
						windowBuffer[i] = -1;
					if (windowBuffer == NULL)
						fatalerror("Failed to malloc window buffer");
					createAndSendPacket(mySocket, ACK, 0, seq, windowsize, NULL, &otherAddress);
					state = ACK_SENT;
					tTimeout = deltaTime;
					resendCount = 0;
				}
				else if (resendCount >= MAX_RESENDS) {
					resendCount = 0;
					state = CLOSED;
					printf(ANSI_WHITE"SYN_SENT GOING TO CLOSED - MAX RESENDS REACHED (SYN)"ANSI_RESET "\n");
				}
				break;
			case ACK_SENT:
				if (receiveStatus == RECEIVE_OK && packet.flags == SYNACK) {
					resendCount++;
					createAndSendPacket(mySocket, ACK, 0, 0, windowsize, NULL, &otherAddress);
				}
				else if(resendCount >= MAX_RESENDS){
					resendCount = 0;
					printf("MAX resends reached going to CLOSED");
					state = CLOSED;
				}
				else if (deltaTime - tTimeout > ACK_SENT_TIMEOUT) { // TIMEOUT!
					resendCount = 0;
					printf("TIMEOUT in ACK_SENT going to WAIT\n");
					tTimeout=deltaTime;
	 				state = WAIT;
				}
			
				break;
			case WAIT:
				if (dataIndex == dataCount && acksReceived == dataCount) {
					createAndSendPacketWithResendTimer(mySocket, FIN, 0, currentSeq, windowsize, NULL, &otherAddress, &timerList, deltaTime);
					printf(ANSI_WHITE"WAIT GOING TO FIN_WAIT_1\n"ANSI_RESET);
					state = FIN_WAIT_1;
					
					resendCount = 0;
					acksReceived = 0;
					dataIndex = 0;
					currentSeq = 0;
					slidingWindowIndexFirst = 0;
				}
				else if (dataIndex != dataCount && currentSeq != (slidingWindowIndexLast + 1) % maxSequence) { // SEND DATA
					createAndSendPacketWithResendTimer(mySocket, FRAME, 0, currentSeq, windowsize, data[dataIndex], &otherAddress, &timerList, deltaTime);					
					
					currentSeq = (currentSeq + 1) % maxSequence;
					dataIndex++;
				}
				else if (deltaTime - tTimeout > WAIT_TIMEOUT) {
					createAndSendPacketWithResendTimer(mySocket, FIN, 0, currentSeq, windowsize, NULL, &otherAddress, &timerList, deltaTime);
					printf(ANSI_WHITE"WAIT GOING TO FIN_WAIT_1 - NO RESPONSE FROM SERVER"ANSI_RESET);
					state = FIN_WAIT_1;
				}
				
				if (receiveStatus == RECEIVE_OK && packet.flags == NACK) {
					tTimeout=deltaTime;
					resendPacketBySeq(mySocket, &timerList, deltaTime, packet.seq);
				}
				else if(receiveStatus == RECEIVE_OK && packet.flags == ACK) {
					tTimeout=deltaTime;
					printf(ANSI_WHITE"WAIT GOING TO FRAME_RECEIVED\n"ANSI_RESET);
					state = ACK_RECEIVED;
				
					while (state != WAIT) {
						switch (state) {
							case ACK_RECEIVED:
								resendCount = 0;
								if (((slidingWindowIndexFirst <= slidingWindowIndexLast) && (packet.seq >= slidingWindowIndexFirst) && (packet.seq <= slidingWindowIndexLast)) ||
										((slidingWindowIndexFirst > slidingWindowIndexLast && (packet.seq >= slidingWindowIndexFirst && packet.seq <= maxSequence-1)) || (packet.seq <= slidingWindowIndexLast && packet.seq >= 0))) {
									// INSIDE WINDOW
									removePacketTimerBySeq(&timerList, packet.seq);
									printf(ANSI_WHITE"ACK_RECEIVED GOING TO ACK_IN_WINDOW\n"ANSI_RESET);
									state = ACK_IN_WINDOW;
								}
								else {
									// OUTSIDE WINDOW
									printf(ANSI_WHITE"ACK_RECEIVED GOING TO WAIT - ACK OUTSIDE WINDOW\n"ANSI_RESET);
									state = WAIT;
								}
								break;
							case ACK_IN_WINDOW:
								removePacketTimerBySeq(&timerList, packet.seq);
								if (packet.seq == slidingWindowIndexFirst) {
									printf(ANSI_WHITE"ACK_IN_WINDOW GOING TO MOVE_WINDOW - ACK IS FIRST\n"ANSI_RESET);
									state = MOVE_WINDOW;
									acksReceived++;
								}
								else {
									if (!isAlreadyBuffered(windowBuffer, buffersize, packet.seq)) {
										for (i = 0; i < buffersize; i++) {
											if (windowBuffer[i] == -1 || windowBuffer[i] == packet.seq) {
												acksReceived++;
												windowBuffer[i] = packet.seq;
												printf(ANSI_WHITE"ACK NOT FIRST PUT IN WINDOW BUFFER AT %d\n" ANSI_RESET, i);
												break;
											}
										}
									}
									state = WAIT;
								}
								break;
							
							case MOVE_WINDOW:
								slidingWindowIndexFirst = (slidingWindowIndexFirst+1)%maxSequence;
								slidingWindowIndexLast = (slidingWindowIndexLast+1)%maxSequence;
								printf(ANSI_WHITE"MOVE_WINDOW GOING TO BUFFER\n"ANSI_RESET);
								state = BUFFER;
								break;
						
							case BUFFER:
								for (i = 0; i < buffersize; i++) {
									if (windowBuffer[i] == slidingWindowIndexFirst) {
										windowBuffer[i] = -1;
										printf(ANSI_WHITE"BUFFER GOING TO MOVE_WINDOW\n"ANSI_RESET);
										state = MOVE_WINDOW;
										break;
									}
									
								}
								if (state != MOVE_WINDOW) {
									state = WAIT;
									printf(ANSI_WHITE"BUFFER GOING TO WAIT - NO ACK IN BUFFER IS FIRST IN WINDOW\n"ANSI_RESET);
								}
								break;
						}
					}
				}
				break;
				
			case FIN_WAIT_1:
				if (resendCount >= MAX_RESENDS) {
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO CLOSED - MAX FIN SENT\n"ANSI_RESET);
					state = CLOSED;
				}
				else if (receiveStatus == RECEIVE_OK && packet.flags == ACK)	{
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO FIN_WAIT_2\n"ANSI_RESET);
					state = FIN_WAIT_2;
					tTimeout = deltaTime;
				}
				else if (receiveStatus == RECEIVE_OK && packet.flags == FIN) {
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					createAndSendPacketWithResendTimer(mySocket, ACK, 0, packet.seq, windowsize, NULL, &otherAddress, &timerList, deltaTime);

					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO CLOSING\n"ANSI_RESET);
					state = CLOSING;
				}
				break;
			case FIN_WAIT_2:
				if (deltaTime - tTimeout > FIN_WAIT_2_TIMEOUT) { // TIMEOUT!
					printf(ANSI_WHITE"FIN_WAIT_2 GOING TO CLOSED\n"ANSI_RESET);
	 				state = CLOSED;
				}
				else if(receiveStatus == RECEIVE_OK && packet.flags == FIN) {
					createAndSendPacket(mySocket, ACK, 0, packet.seq, windowsize, NULL, &otherAddress);
					printf(ANSI_WHITE"FIN_WAIT_2 GOING TO TIME_WAIT\n"ANSI_RESET);
					state = TIME_WAIT;
					tTimeout = deltaTime;
				}			
				break;
			case CLOSING:
				if(resendCount >= MAX_RESENDS) {
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"CLOSING GOING TO CLOSED\n"ANSI_RESET);
					state = CLOSED;
				}
				else if(packet.flags == ACK) {
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"CLOSING GOING TO TIME_WAIT\n"ANSI_RESET);
					state = TIME_WAIT;
					tTimeout = deltaTime;
				}
				break;
			case TIME_WAIT:
				if(deltaTime - tTimeout > FIN_WAIT_2_TIMEOUT) {
					printf(ANSI_WHITE"TIME_WAIT GOING TO CLOSED\n"ANSI_RESET);
					resendCount = 0;
					state = CLOSED;
				}
				break;
			default:
				break;
		}
		fflush(stdout);
		receiveStatus = receivePacketOrTimeout(mySocket, &packet, &otherAddress, HEARTBEAT_TIMEOUT);
		
		clock_gettime(CLOCK_MONOTONIC, &stop);
		deltaTime = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);

		updateTimers(mySocket, &timerList, deltaTime, &resendCount);
	
	}
	return 0;
}
