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
#define MAX_RESENDS 100

// STATES
#define INIT 0
#define ACK_SENT 1

/* Handles received packets and set the state accordingly.
 * Parameters:
 *  state: what state the program is in
 *  mySocket: Socket used by this application
 *  packet: The packet received OR NULL if timeout occured
 *  source: Source address from the sender of the packet
 *	client: Settings of the client eg. id and windowsize...
*/
int handlePacket(int state, int mySocket, struct packet *packet, struct sockaddr_in *source, struct client *client) {
	static int resendCount = 0; // Static variable only initialized once, and keeps its value between function calls
	struct packet myPacket;
	switch (state) {
		case INIT:
			if (packet == NULL) { // TIMEOUT!
				myPacket = createPacket(SYN, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("SYN resent...\n");
			}
			else if (packet->flags == SYNACK) {
				printf("SYNACK received\n");
				state = ACK_SENT;
			}
			
			break;
		default:
			break;
	}
	return state;
}

int main(int argc, char **argv) {
	struct sockaddr_in destination;
	
	if (argc < 3)
		fatalerror("Too few arguments");
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = inet_addr(argv[1]);
	destination.sin_port = htons(atoi(argv[2]));
	// Socket creation...
	int mySocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (mySocket == -1)
		fatalerror("Failed to create a socket");
	
	// Sending syn to server for connection request
	struct packet SYNpacket = createPacket(SYN, 0, 0, 0, 0, NULL);
	if (sendPacket(mySocket, &SYNpacket, &destination) == -1)
		fatalerror("Failed to send syn");
	printf("SYN sent to %s:%s\n", argv[1], argv[2]);
	
	int state = INIT;
	int bytesRead = 0;
	struct client client = { 0 };
	struct packet packet;
	struct sockaddr_in source;
	// Event handling loop
	while (1) {
		bytesRead = waitAndReceivePacket(mySocket, &packet, &source, TIMEOUT);
		if (bytesRead == -1)
			fatalerror("Failed to receive packet");
		else if (bytesRead == 0) // Check if timeout
			state = handlePacket(state, mySocket, NULL, &destination, &client);
		else
			state = handlePacket(state, mySocket, &packet, &destination, &client);
		fflush(stdout);
	}
	return 0;
}
