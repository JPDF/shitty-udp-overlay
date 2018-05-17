// MADE BY: Patrik, Jakob, Simon
#include "packet.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <string.h>
#include "misc.h"

char *getFlagString(int flag) {
	switch (flag) {
		case SYN: return "SYN";
		case SYNACK: return "SYNACK";
		case ACK: return "ACK";
		case FIN: return "FIN";
		case FRAME: return "FRAME";
		case NACK: return "NACK";
		default: return "NOT DEFINED";
	}
}

uint32_t crc32(const void *data, int size) {
	unsigned int i, j;
	uint32_t remainder = 0;
	const uint8_t *bytes = (const uint8_t *)data;
	for (i = 0; i < size; i++) { 
		remainder ^= bytes[i]; // Shift in next byte into remainder
		for (j = 0; j < 8; j++) {
			remainder = (remainder & 1) != 0 ? (remainder >> 1) ^ CRC_POLYNOM : remainder >> 1;
		}
	}
	return remainder;
}

void createPacket(struct packet *packet, int flags, int id, int seq, int windowsize, char *data) {
	packet->flags = flags;
	packet->id = id;
	packet->seq = seq;
	packet->windowsize = windowsize;
	packet->crc = 0;
	packet->data[0] = '\0';
	if (data != NULL)
		strcpy(packet->data, data);
	packet->crc = crc32(packet, sizeof(*packet));
}



int receivePacketOrTimeout(const int mySocket, struct packet *packet, struct sockaddr_in *source, const int timeout) {
	struct timeval timevalue;
	timevalue.tv_sec = 0; // we don't use seconds...
	timevalue.tv_usec = timeout * 1000; // Converts milliseconds to microseconds
	fd_set fdSet;
	
	// Reset FD and add our socket to it
	FD_ZERO(&fdSet);
	FD_SET(mySocket, &fdSet);
	
	// Wait for packet to arrive or TIMEOUT
	int readyFDs = select(mySocket + 1, &fdSet, NULL, NULL, &timevalue);
	if (readyFDs == -1) { // Check if select returned an error
		fatalerror("Select failed");
	}
	else if (readyFDs == 0) // Checks if select did timeout
		return RECEIVE_TIMEOUT;
	return receivePacket(mySocket, packet, source); // Returns whatever receivePacket function returns
}

int receivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source) {
	socklen_t addressSize = sizeof(*source);
	// Blocks untill packet arrives and check if recvfrom returned an error
	if (recvfrom(mySocket, packet, sizeof(struct packet), 0, (struct sockaddr*)source, &addressSize) == -1)
		fatalerror("Failed to receive packet");
	// Simply prints received packet detail
	printf(ANSI_GREEN"RECEIVED: %s [id:"ANSI_BLUE"%d"ANSI_GREEN" seq:"ANSI_BLUE"%d"ANSI_GREEN" ws:"ANSI_BLUE"%d"ANSI_GREEN" data:"ANSI_BLUE"%s"ANSI_GREEN"] to"ANSI_BLUE" %s" ANSI_GREEN":" ANSI_BLUE"%d\n"ANSI_RESET,
				 getFlagString(packet->flags),
				 packet->id,
				 packet->seq,
				 packet->windowsize,
				 packet->data,
				 inet_ntoa(source->sin_addr),
				 ntohs(source->sin_port));
	// Checks if the packet is broken and return its state (BROKEN OR OK)
	if (isPacketBroken(packet))
		return RECEIVE_BROKEN;
	return RECEIVE_OK;
}



void sendPacket(const int mySocket, struct packet *packet, const struct sockaddr_in *destination, int isResend) {
	int chance = 1, throwAway = 0;
	int crc = packet->crc; // Saves the crc in a variable if we want to break the packet
	
	// Generate error if there is one
	if (error != 0) {
		chance = rand() % 3;
		if ((error == ERROR_CRC || error == ERROR_CHAOS) && chance == 2) {
			packet->crc = 1;
			printf(ANSI_RED"POSTNORD RUINED PACKET\n"ANSI_RESET);
		}
		else if ((error == ERROR_LOST_FRAME || error == ERROR_CHAOS) && chance == 1) {
			throwAway = 1;
		}
	}
	
	// Simply prints sent packet, and change color if it is a resend
	if (isResend)
		printf(ANSI_YELLOW"RESENT: ");
	else
		printf(ANSI_GREEN"SENT: ");
	printf(ANSI_GREEN "%s [id:"ANSI_BLUE"%d"ANSI_GREEN" seq:"ANSI_BLUE"%d"ANSI_GREEN" ws:"ANSI_BLUE"%d"ANSI_GREEN" data:"ANSI_BLUE"%s"ANSI_GREEN"] to"ANSI_BLUE" %s" ANSI_GREEN":" ANSI_BLUE"%d\n"ANSI_RESET,
				 getFlagString(packet->flags),
				 packet->id,
				 packet->seq,
				 packet->windowsize,
				 packet->data,
				 inet_ntoa(destination->sin_addr),
				 ntohs(destination->sin_port));
	
	// If the error generate a "throw away packet", then do so
	if (!throwAway) {
		// Send the packet and check if there was any errors
		if (sendto(mySocket, packet, sizeof(*packet), 0, (struct sockaddr*)destination, sizeof(*destination)) == -1)
			fatalerror("Failed to send");
	}
	else
		printf(ANSI_RED"SNEAKY NINJA THREW AWAY PACKAGE\n"ANSI_RESET);
	packet->crc = crc; // Gives back the correct CRC to packet
}

void createAndSendPacket(int mySocket, int flags, int id, int seq, int windowsize, char *data, const struct sockaddr_in *destination) {
	struct packet packet;
	// Creates packet
	createPacket(&packet, flags, id, seq, windowsize, data);
	// Sends packet
	sendPacket(mySocket, &packet, destination, 0);
}

void createAndSendPacketWithResendTimer(int mySocket, int flags, int id, int seq, int windowsize, char *data, const struct sockaddr_in *destination, TimerList *list, time_t time) {
	struct packet packet;
	// Creates packet
	createPacket(&packet, flags, id, seq, windowsize, data);
	// Send packet
	sendPacket(mySocket, &packet, destination, 0);
	// Add packet to timer list
	addPacketTimer(list, &packet, destination, time);
}

int isPacketBroken(struct packet *packet) {
	uint32_t checksum = packet->crc;
	packet->crc = 0;
	// Creates a variables to hold both packet and checksum
	unsigned char *data = malloc(sizeof(struct packet) + sizeof(checksum));
	
	// PUT PACKET AND CHECKSUM TOGETHER
	memcpy(data, packet, sizeof(*packet));
	memcpy(data + sizeof(*packet), (unsigned char *)&checksum, sizeof(checksum));
	
	// Check if crc32 returned 0 (Packet is not broken)
	if (crc32(data, sizeof(*packet) + sizeof(checksum)) == 0) {
		return 0;
	}
	// Packet is broken...
	printf(ANSI_RED"WARNING: Broken packet found!\n"ANSI_RESET);
	free(data);
	return 1;
}

void addPacketTimer(TimerList *list, const struct packet *packet, const struct sockaddr_in *address, time_t startTime) {
	struct packetTimer *timer;
	
	// If we are in the last spot
	if (*list == NULL) {
		// Initialize the packetTimer
		timer = malloc(sizeof(struct packetTimer));
		if (timer == NULL)
			fatalerror("Failed to malloc");
		timer->address = *address;
		timer->packet = *packet;
		timer->start = startTime;
		timer->stop = startTime + RESEND_TIMEOUT;
		timer->next = NULL;
	
		printf(ANSI_GREEN"ADDED TO TIMER LIST WITH START:%d STOP:%d\n"ANSI_RESET, (int)timer->start, (int)timer->stop);
		
		*list = timer;
		(*list)->next = NULL;
	}
	else
		addPacketTimer(&(*list)->next, packet, address, startTime); // Moves to next packetTimer
}

void removeFirstPacketTimer(TimerList *list) {
	struct packetTimer *temp;
	// Checks if list is empty
	if (*list == NULL)
		return;
	
	temp = *list;
	*list = temp->next;
	printf(ANSI_GREEN "DELETED FROM TIMER LIST %d\n"ANSI_RESET, temp->packet.seq);
	free(temp);
	temp = NULL;
}

void removePacketTimerBySeq(TimerList *list, int seq) {
	// if list is empty
	if (*list == NULL)
		return;
	// Checks if we found the currect packet to remove
	if ((*list)->packet.seq == seq) {
		removeFirstPacketTimer(list);
	}
	else
		removePacketTimerBySeq(&(*list)->next, seq); // Moves to next packet
}

void removeAllFromTimerList(TimerList *list) {
	if (*list != NULL) {
		removeAllFromTimerList(&(*list)->next);
		removeFirstPacketTimer(list);
	}
}

void resendPacketBySeq(int socket, TimerList *list, time_t time, int seq) {
	// Checks if the list is empty
	if (*list == NULL)
		return;
	// Found the correct packet
	else if ((*list)->packet.seq == seq) {
		sendPacket(socket, &(*list)->packet, &(*list)->address, 1);
		// Add packet first because we reuse the packet in the list, else it will be removed
		addPacketTimer(list, &(*list)->packet, &(*list)->address, time);
		removeFirstPacketTimer(list);
	}
	else
		resendPacketBySeq(socket, &(*list)->next, time, seq); // Moves to the next packet
}

void updateTimers(int socket, TimerList *list, time_t time, int *resendCount) {
	while (*list != NULL && time >= (*list)->stop) {
		sendPacket(socket, &(*list)->packet, &(*list)->address, 1);
		// Add packet first because we reuse the packet in the list, else it will be removed
		addPacketTimer(list, &(*list)->packet, &(*list)->address, time);
		removeFirstPacketTimer(list);
		(*resendCount)++; // Add 1 to resendCount at every resend
	}
}
