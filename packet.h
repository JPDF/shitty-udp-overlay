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

#define RECEIVE_OK 1
#define RECEIVE_TIMEOUT 0
#define RECEIVE_BROKEN -1

#define CRC_POLYNOM 0x04c11db7

struct packet{
	int flags;
	int id;
	int seq;
	int windowsize;
	uint32_t crc;
	char *data;
};

struct packetTimer {
	struct packet packet;
	struct sockaddr_in address;
	time_t start;
	time_t stop;
	struct packetTimer *next;
};
typedef struct packetTimer *TimerList;

void addPacketTimer(TimerList *list, struct packet *packet, struct sockaddr_in *address, time_t startTime, time_t timeout);

void removePacketTimerBySeq(TimerList *list, int seq);

void updateTimers(int socket, TimerList *list, time_t time, int *resendCount);

// Creating a packet and returning it...
void createPacket(struct packet *packet, int flags, int id, int seq, int windowsize, char *data);

/* Waits for a package to be received untill timeout
 * Parameters:
 *	mySocket: socket file descriptor
 *	packet: The received packet
 *	source: The source from the received packet
 *	timeout: time before timeout in milliseconds
 * Returns -1 if packet is broken, 0 if timeout else 1 */
int receivePacketOrTimeout(const int mySocket, struct packet *packet, struct sockaddr_in *source, const int timeout);
int receivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source); // Blocks until packet received

/* Simply sends a packet to destination
	* Parameters:
 *	mySocket: socket file descriptor
 *	packet: The packet to send
 *	destination: The destination to send the packet
 * Returns -1 on error, 0 on timeout or amounts of bytes read */
void sendPacket(const int mySocket, const struct packet *packet, const struct sockaddr_in *destination, int isResend);

/* Checks if a packet is broken using CRC (Cyclic Redundancy Check)
 * Also resets the crc-field of the packet 
 * Parameters:
 *	packet: The packet to check if broken
 * Returns: 1 if broken else 0 */
int isPacketBroken(struct packet *packet);

#endif
