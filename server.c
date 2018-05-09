#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include "packet.h"
#include "misc.h"


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

// STATES
#define INIT 0
#define LISTEN 1
#define SYN_RECEIVED 2
//sliding window states
#define WAIT 10
#define FRAME_RECEIVED 11
//TEARDOWN STATES
#define CLOSE_WAIT 20
#define LAST_ACK 21

/* Handles received packets and set the state accordingly.
   Packet is NULL if timeout occured */
int handlePacket(int state, int mySocket, struct packet *packet, struct sockaddr_in *source, struct client *client) {
	static int resendCount = 0;
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
			printf(ANSI_MAGENTA"DATA RECEIVE STATE\n"ANSI_RESET);
			if (packet != NULL && packet->flags == FIN){
				printf(ANSI_GREEN"FIN received from "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf(ANSI_GREEN"ACK sent to "ANSI_BLUE"%s"ANSI_GREEN":"ANSI_BLUE"%d\n"ANSI_RESET, inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				printf(ANSI_WHITE"DATA RECEIVE STATE GOING TO CLOSE_WAIT\n"ANSI_RESET);
				state = CLOSE_WAIT;
				}
			break;
			
		case FRAME_RECEIVED:
			break;
			
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
			
		default:
			fatalerror("Server entered incorrect state");
			break;
	}
	return state;
}

int main() {
	srand(time(NULL));
	struct sockaddr_in address;
	
	// Socket creation...
	int mySocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (mySocket == -1)
		fatalerror("Failed to create a socket");
	
	address.sin_family = AF_INET;
	address.sin_port = htons(PORT);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	// Bind to a specific port
	if (bind(mySocket, (struct sockaddr*)&address, sizeof(address)) == -1)
		fatalerror("Failed to bind socket");
		
	int state = INIT;
	int bytesRead = 0;
	struct client client = { 0 };
	struct packet packet;
	struct sockaddr_in source;
	// Event handling loop
	while (1) {
		if (bytesRead == -1)
			fatalerror("Failed to receive packet");
		else if (bytesRead == 0) // checks if timeout
			state = handlePacket(state, mySocket, NULL, &source, &client);
		else 
			state = handlePacket(state, mySocket, &packet, &source, &client);
		fflush(stdout);
		bytesRead = waitAndReceivePacket(mySocket, &packet, &source, TIMEOUT);
	}
	return 0;
}
