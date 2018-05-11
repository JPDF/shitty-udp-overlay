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

int main(int argc, char **argv) {
	int state = CLOSED, isTimeout, on = 1;
	struct client client = { 0 };
	struct packet packet;
	struct sockaddr_in destination;
	srand(time(NULL));
	struct sockaddr_in source;
	int timer = 0, resendCount = 0;
	
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
				packet = createPacket(SYN, 0, 0, 0, 0, NULL);
				if (sendPacket(mySocket, &packet, &destination) == -1)
					fatalerror("Failed to send syn");
				printf("SYN sent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
				state = SYN_SENT;
				break;
			case SYN_SENT:
				if (isTimeout) { // TIMEOUT!
					resendCount++;
					packet = createPacket(SYN, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("SYN resent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port)); 
				}
				else if (packet.flags == SYNACK) {
					printf("SYNACK received from %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("ACK sent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
					state = ACK_SENT;
					resendCount = 0;
				}
			
				break;
			case ACK_SENT: //TODO: LÄGG TILL TIMEOUT EFTER X ANTAL SKICKADE PAKET
				timer++;
				if (!isTimeout && packet.flags == SYNACK) {
					resendCount++;
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("ACK resent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
				}
				else if(resendCount == MAX_RESENDS){
				resendCount = 0;
				printf("MAX resends reached going to CLOSED");
				state = CLOSED;
				}
				else if (isTimeout && timer == MAX_RESENDS) { // TIMEOUT!
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
					packet = createPacket(FIN, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("FIN sent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port)); 
				state = FIN_WAIT_1;
				}
				break;
			case FIN_WAIT_1:
				if (resendCount == MAX_RESENDS){
				resendCount = 0;
				printf("Timeout max sent FIN going to CLOSED");
				state = CLOSED;
				}
				else if (isTimeout){
					resendCount++;
					packet = createPacket(FIN, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("FIN resent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
				}
				else if (packet.flags == ACK){
					resendCount = 0;
					printf("ACK received going to FIN_WAIT_2\n");
					state = FIN_WAIT_2;
				}
				else if (packet.flags == FIN){
					resendCount = 0;
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("ACK sent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
					state = CLOSING;
				}
				else if (resendCount == MAX_RESENDS){
					resendCount = 0;
					state = CLOSED;
				}
				break;
			case FIN_WAIT_2:
				resendCount++;
				if (isTimeout && resendCount==MAX_RESENDS) { // TIMEOUT!
					printf("Timeout in FIN_WAIT_2 going to CLOSED\n");
	 				state = CLOSED;
				}
				else if(!isTimeout && packet.flags == FIN){
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("ACK sent to %s:%d\nGoing to TIME_WAIT\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
				
					state = TIME_WAIT;
				}			
				break;
			case CLOSING:
				if(resendCount==MAX_RESENDS){
					resendCount = 0;
					state = CLOSED;
				}
				else if(isTimeout){
					resendCount++;
					packet = createPacket(ACK, 0, 0, 0, 0, NULL);
					sendPacket(mySocket, &packet, &destination);
					printf("ACK resent to %s:%d\n", (char*)inet_ntoa(destination.sin_addr), ntohs(destination.sin_port));
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
		isTimeout = waitAndReceivePacket(mySocket, &packet, &source, TIMEOUT);
	}
	return 0;
}
