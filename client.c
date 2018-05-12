#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include "packet.h"
#include "misc.h"

// SETTINGS
#define TIMEOUT 1000 // The time before timeout in milliseconds
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
		
	// Event handling loop
	while (1) {
		switch (state) {
			case CLOSED:
				createPacket(&packet, SYN, 0, seq, windowsize, NULL);
				sendPacket(mySocket, &packet, &destination, 0);
				state = SYN_SENT;
				break;
			case SYN_SENT:
				if (receiveStatus == RECEIVE_TIMEOUT) { // TIMEOUT!
					resendCount++;
					sendPacket(mySocket, &packet, &destination, 1);
				}
				else if (packet.flags == SYNACK) {
					windowsize = packet.windowsize;
					createPacket(&packet, ACK, 0, seq, windowsize, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					state = ACK_SENT;
					resendCount = 0;
				}
			
				break;
			case ACK_SENT: //TODO: LÄGG TILL TIMEOUT EFTER X ANTAL SKICKADE PAKET
				timer++;
				if (receiveStatus == RECEIVE_OK && packet.flags == SYNACK) {
					resendCount++;
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
				}
				else if(resendCount == MAX_RESENDS){
					resendCount = 0;
					printf("MAX resends reached going to CLOSED");
					state = CLOSED;
				}
				else if (receiveStatus == RECEIVE_TIMEOUT && timer == MAX_RESENDS) { // TIMEOUT!
					resendCount = 0;
					timer = 0;
					printf("TIMEOUT in ACK_SENT going to DATA_TRANSMISSION\n");
	 				state = DATA_TRANSMISSION;
				}
			
				break;
			case DATA_TRANSMISSION:
				printf("vi skickar data [%d]\n", resendCount+1);
				resendCount++;
				//were done send fin and close
				if(resendCount == MAX_RESENDS){
					resendCount = 0;
					createPacket(&packet, FIN, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					state = FIN_WAIT_1;
				}
				break;
			case FIN_WAIT_1:
				if (resendCount == MAX_RESENDS){
				resendCount = 0;
				printf("Timeout max sent FIN going to CLOSED");
				state = CLOSED;
				}
				else if (receiveStatus == RECEIVE_TIMEOUT){
					resendCount++;
					createPacket(&packet, FIN, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 1);
				}
				else if (packet.flags == ACK){
					resendCount = 0;
					printf(ANSI_WHITE"FIN_WAIT_1 GOING TO FIN_WAIT_2\n"ANSI_RESET);
					state = FIN_WAIT_2;
				}
				else if (packet.flags == FIN){
					resendCount = 0;
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					state = CLOSING;
				}
				else if (resendCount == MAX_RESENDS){
					resendCount = 0;
					state = CLOSED;
				}
				break;
			case FIN_WAIT_2:
				resendCount++;
				if (receiveStatus == RECEIVE_TIMEOUT && resendCount==MAX_RESENDS) { // TIMEOUT!
					printf("Timeout in FIN_WAIT_2 going to CLOSED\n");
	 				state = CLOSED;
				}
				else if(receiveStatus == RECEIVE_OK && packet.flags == FIN){
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 0);
					state = TIME_WAIT;
				}			
				break;
			case CLOSING:
				if(resendCount==MAX_RESENDS){
					resendCount = 0;
					state = CLOSED;
				}
				else if(receiveStatus == RECEIVE_TIMEOUT) { // TIMEOUT
					resendCount++;
					createPacket(&packet, ACK, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination, 1);
				}
				else if(packet.flags == ACK){
					resendCount = 0;
					state = TIME_WAIT;
				}
				break;
			case TIME_WAIT:
				resendCount++;
				if(resendCount==MAX_RESENDS){
					printf("TIME_WAIT done going to CLOSED\n");
					resendCount = 0;
					state = CLOSED;
				}
				break;
			default:
				break;
		}
		fflush(stdout);
		receiveStatus = receivePacketOrTimeout(mySocket, &packet, &source, TIMEOUT);
	}
	return 0;
}
