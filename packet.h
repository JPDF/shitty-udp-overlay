#ifndef PACKET_H
#define PACKET_H

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

// FLAGS
#define SYN 1
#define SYNACK 2 
#define ACK 3
#define FIN 4
#define FRAME 5

struct packet{
	int flags;
	int id;
	int seq;
	int windowsize;
	int crc;
	char *data;
};

// Creating a packet and returning it...
struct packet createPacket(int flags, int id, int seq, int windowsize, int crc, char *data);

/* Waits for a package to be received untill timeout
 * Parameters:
 *	mySocket: socket file descriptor
 *	packet: The received packet
 *	source: The source from the received packet
 *	timeout: time before timeout in milliseconds
 * Returns 1 on timeout, else 0 */
int receivePacketWithTimeout(const int mySocket, struct packet *packet, struct sockaddr_in *source, const int timeout);
void receivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source); // Blocks until packet received

/* Simply sends a packet to destination
	* Parameters:
 *	mySocket: socket file descriptor
 *	packet: The packet to send
 *	destination: The destination to send the packet */
void sendPacket(const int mySocket, const struct packet *packet, const struct sockaddr_in *destination);

#endif
