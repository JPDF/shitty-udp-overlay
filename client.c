#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include "packet.h"
#include "misc.h"


#define bla 123
#define blaa 1234
// SETTINGS
#define TIMEOUT 1000 // The time before timeout in milliseconds
#define MAX_RESENDS 10

// STATES
#define CLOSED 0
#define SYN_SENT 1
#define ACK_SENT 2
#define DATA_TRANSMISSION 3
#define FIN_WAIT_1 4
#define FIN_WAIT_2 5
#define CLOSING 6
#define TIME_WAIT 7

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
	static int timer = 0;
	struct packet myPacket;
	switch (state) {
		case CLOSED:
			myPacket = createPacket(SYN, 0, 0, 0, 0, NULL);
			if (sendPacket(mySocket, &myPacket, source) == -1)
				fatalerror("Failed to send syn");
			printf("SYN sent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
			state = SYN_SENT;
			break;
		case SYN_SENT:
			if (packet == NULL) { // TIMEOUT!
				resendCount++;
				myPacket = createPacket(SYN, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("SYN resent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port)); 
			}
			else if (packet->flags == SYNACK) {
				printf("SYNACK received from %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("ACK sent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				state = ACK_SENT;
				resendCount = 0;
			}
			
			break;
		case ACK_SENT: //TODO: LÄGG TILL TIMEOUT EFTER X ANTAL SKICKADE PAKET
			timer++;
			if(resendCount == MAX_RESENDS){
			resendCount = 0;
			printf("MAX resends reached going to CLOSED");
			state = CLOSED;
			}
			else if (packet == NULL && resendCount == timer) { // TIMEOUT!
				resendCount = 0;
				timer = 0;
				printf("TIMEOUT in ACK_SENT going to DATA_TRANSMISSION\n");
 				state = DATA_TRANSMISSION;
			}
			else if (packet != NULL && packet->flags == SYNACK) {
				resendCount++;
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("ACK resent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
			}
			
			break;
		case DATA_TRANSMISSION:
			printf("vi skickar data [%d]\n", resendCount+1);
			resendCount++;
			//were done send fin and close
			if(resendCount == MAX_RESENDS){
				resendCount = 0;
				myPacket = createPacket(FIN, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("FIN sent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port)); 
			state = FIN_WAIT_1;
			}
			break;
		case FIN_WAIT_1:
			if (resendCount == MAX_RESENDS){
			resendCount = 0;
			printf("Timeout max sent FIN going to CLOSED");
			state = CLOSED;
			}
			else if (packet == NULL){
				resendCount++;
				myPacket = createPacket(FIN, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("FIN resent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
			}
			else if (packet->flags == ACK){
				resendCount = 0;
				printf("ACK received going to FIN_WAIT_2\n");
				state = FIN_WAIT_2;
			}
			else if (packet->flags == FIN){
				resendCount = 0;
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("ACK sent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				state = CLOSING;
			}
			else if (resendCount == MAX_RESENDS){
				resendCount = 0;
				state = CLOSED;
			}
			break;
		case FIN_WAIT_2:
			resendCount++;
			if (packet == NULL && resendCount==MAX_RESENDS) { // TIMEOUT!
				printf("Timeout in FIN_WAIT_2 going to CLOSED\n");
 				state = CLOSED;
			}
			else if(packet != NULL && packet->flags == FIN){
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("ACK sent to %s:%d\nGoing to TIME_WAIT\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
				
				state = TIME_WAIT;
			}			
			break;
		case CLOSING:
			if(resendCount==MAX_RESENDS){
				resendCount = 0;
				state = CLOSED;
			}
			else if(packet == NULL){
				resendCount++;
				myPacket = createPacket(ACK, 0, 0, 0, 0, NULL);
				sendPacket(mySocket, &myPacket, source);
				printf("ACK resent to %s:%d\n", (char*)inet_ntoa(source->sin_addr), ntohs(source->sin_port));
			}
			else if(packet->flags == ACK){
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
	return state;
}

int main(int argc, char **argv) {
	srand(time(NULL));
	struct sockaddr_in destination;
		int on=1;
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
		
	// Sending syn to server for connection request

	int state = CLOSED;
	int bytesRead = 0;
	struct client client = { 0 };
	struct packet packet;
	struct sockaddr_in source;
	// Event handling loop
	while (1) {
		if (bytesRead == -1)
			fatalerror("Failed to receive packet");
		else if (bytesRead == 0) // Check if timeout
			state = handlePacket(state, mySocket, NULL, &destination, &client);
		else
			state = handlePacket(state, mySocket, &packet, &destination, &client);
		fflush(stdout);
		bytesRead = waitAndReceivePacket(mySocket, &packet, &source, TIMEOUT);
	}
	return 0;
}
