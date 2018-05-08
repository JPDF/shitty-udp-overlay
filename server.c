#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include "packet.h"
#include "misc.h"

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
			printf("Waiting for connection...\n");
			state = LISTEN;
			break;
		case LISTEN:
			if (packet != NULL && packet->flags == SYN) {
				printf("SYN received from %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				myPacket = createPacket(SYNACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("SYNACK sent to %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				state = SYN_RECEIVED;
			}
			break;
			
		case SYN_RECEIVED:
			if (resendCount >= MAX_RESENDS){
				resendCount = 0;
				state = INIT;
				printf("CONNECTION TIMEOUT GOING TO CLOSED\n");
			}
			else if (packet == NULL){//packet == NULL -> timeout
				myPacket = createPacket(SYNACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("SYNACK resent to %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				resendCount++;
			}
			else if (packet->flags == ACK){
				resendCount = 0;
				state = WAIT;
				printf("CONNECTION SUCCESS GOING TO DATA TRANSMISSION\n");
			}
			break;
			
		//sliding window
			
		case WAIT:
			printf("DATA RECEIVE STATE\n");
			if (packet != NULL && packet->flags == FIN){
				printf("FIN received from %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("ACK sent to %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				state = CLOSE_WAIT;
				}
			break;
			
		case FRAME_RECEIVED:
			break;
			
		//TEARDOWN
		
		case CLOSE_WAIT:
			myPacket = createPacket(FIN, 0, 0, 0, 0, NULL);
			sendPacket(mySocket, &myPacket, source);
			printf("FIN sent to %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
			state = LAST_ACK;
			break;
			
		case LAST_ACK:
			if (resendCount >= MAX_RESENDS){
				resendCount = 0;
				state = INIT;
				printf("MAX RESENDS REACHED GOING TO CLOSED\n");
			}
			else if (packet == NULL){
				myPacket = createPacket(FIN, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("FIN resent to %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				resendCount++;
			}
			else if (packet->flags == ACK){
				printf("ACK received from %s:%d\n", inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				resendCount = 0;
				state = INIT;
				printf("CONNECTION SHUTDOWN SUCCESSFULLY GOING TO CLOSED\n");
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
