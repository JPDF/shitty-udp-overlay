#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include "packet.h"
#include "misc.h"

// SETTINGS
#define HEARTBEAT_TIMEOUT 1
#define TIMEOUT 10 // The time before timeout on each packet in milliseconds
#define MAX_RESENDS 10
#define MAX_SEQUENCE 2
#define MAX_WINDOWSIZE MAX_SEQUENCE / 2

// STATES
#define CLOSED 0
#define SYN_SENT 1
#define ACK_SENT 2
#define DATA_TRANSMISSION 3
#define FIN_WAIT_1 4
#define FIN_WAIT_2 5
#define CLOSING 6
#define TIME_WAIT 7

int main(int argc, char **argv) {
	int state = CLOSED, receiveStatus, on = 1;
	struct client client = { 0 };
	struct packet packet;
	struct sockaddr_in destination;
	srand(time(NULL));
	struct sockaddr_in source;
	int timer = 0, resendCount = 0, seq = 0, windowsize = MAX_WINDOWSIZE;
	TimerList timerList = NULL;
	struct timespec start, stop;
	time_t deltaTime = 0, tTimeout = 0;
	
	if (argc < 3)
		fatalerror("Too few arguments");
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(argv[1]);
	destination.sin_port = htons(atoi(argv[2]));
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
				createPacket(&packet, SYN, 0, seq, windowsize, NULL);
				sendPacket(mySocket, &packet, &destination, 0);
				addPacketTimer(&timerList, &packet, &destination, deltaTime, TIMEOUT);
				state = SYN_SENT;
				break;
			case SYN_SENT:
				if (packet.flags == SYNACK) {
					removePacketTimerBySeq(&timerList, packet.seq);
					windowsize = packet.windowsize;
					createPacket(&packet, ACK, 0, seq, windowsize, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					state = ACK_SENT;
					tTimeout = deltaTime;
					resendCount = 0;
				}
			
				break;
			case ACK_SENT: //TODO: LÄGG TILL TIMEOUT EFTER X ANTAL SKICKADE PAKET
				if (receiveStatus == RECEIVE_OK && packet.flags == SYNACK) {
					resendCount++;
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
				}
				else if(resendCount >= MAX_RESENDS){
					resendCount = 0;
					printf("MAX resends reached going to CLOSED");
					state = CLOSED;
				}
				else if (deltaTime - tTimeout > 50) { // TIMEOUT!
					resendCount = 0;
					printf("TIMEOUT in ACK_SENT going to DATA_TRANSMISSION\n");
	 				state = DATA_TRANSMISSION;
				}
			
				break;
			case DATA_TRANSMISSION:
				printf("vi skickar data [%d]\n", resendCount+1);
				resendCount++;
				//were done send fin and close
				if(resendCount >= MAX_RESENDS){
					resendCount = 0;
					createPacket(&packet, FIN, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					addPacketTimer(&timerList, &packet, &destination, deltaTime, TIMEOUT);
					printf(ANSI_WHITE"DATA_TRANSMISSION GOING TO FIN_WAIT_1\n"ANSI_RESET);
					state = FIN_WAIT_1;
				}
				break;
			case FIN_WAIT_1:
				if (resendCount >= MAX_RESENDS){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO CLOSED - MAX FIN SENT\n"ANSI_RESET);
					state = CLOSED;
				}
				else if (packet.flags == ACK)	{
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO FIN_WAIT_2\n"ANSI_RESET);
					state = FIN_WAIT_2;
					tTimeout = deltaTime;
				}
				else if (packet.flags == FIN){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					addPacketTimer(&timerList, &packet, &destination, deltaTime, TIMEOUT);
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO CLOSING\n"ANSI_RESET);
					state = CLOSING;
				}
				break;
			case FIN_WAIT_2:
				if (deltaTime - tTimeout > 50) { // TIMEOUT!
					printf(ANSI_WHITE"FIN_WAIT_2 GOING TO CLOSED\n"ANSI_RESET);
	 				state = CLOSED;
				}
				else if(receiveStatus == RECEIVE_OK && packet.flags == FIN){
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					printf(ANSI_WHITE"FIN_WAIT_2 GOING TO TIME_WAIT\n"ANSI_RESET);
					state = TIME_WAIT;
					tTimeout = deltaTime;
				}			
				break;
			case CLOSING:
				if(resendCount >= MAX_RESENDS){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"CLOSING GOING TO CLOSED\n"ANSI_RESET);
					state = CLOSED;
				}
				else if(packet.flags == ACK){
					removePacketTimerBySeq(&timerList, packet.seq);
					resendCount = 0;
					printf(ANSI_WHITE"CLOSING GOING TO TIME_WAIT\n"ANSI_RESET);
					state = TIME_WAIT;
					tTimeout = deltaTime;
				}
				break;
			case TIME_WAIT:
				if(deltaTime - tTimeout > 50){
					printf("TIME_WAIT GOING TO CLOSED\n");
					resendCount = 0;
					state = CLOSED;
				}
				break;
			default:
				break;
		}
		fflush(stdout);
		receiveStatus = receivePacketOrTimeout(mySocket, &packet, &source, HEARTBEAT_TIMEOUT);
		
		clock_gettime(CLOCK_MONOTONIC, &stop);
		deltaTime = (stop.tv_sec * 1000 + stop.tv_nsec / 1000000) - (start.tv_sec * 1000 + start.tv_nsec / 1000000);
		updateTimers(mySocket, &timerList, deltaTime, &resendCount);
	
	}
	return 0;
}
