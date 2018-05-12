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
		default: return "NOT DEFINED";
	}
}

uint32_t crc32(const void *data, int size) {
	unsigned int i, j;
	uint32_t remainder = 0;
	const uint8_t *bytes = (const uint8_t *)data;
	for (i = 0; i < size; i++) { 
		remainder ^= bytes[i];
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
	packet->data = data;
	packet->crc = crc32(packet, sizeof(*packet));
}

int receivePacketOrTimeout(const int mySocket, struct packet *packet, struct sockaddr_in *source, const int timeout) {
	struct timeval timevalue;
	timevalue.tv_sec = 0;
	timevalue.tv_usec = timeout * 1000;
	fd_set fdSet;
	
	FD_ZERO(&fdSet);
	FD_SET(mySocket, &fdSet);
	
	int readyFDs = select(mySocket + 1, &fdSet, NULL, NULL, &timevalue);
	if (readyFDs == -1) {
		return -1;
	}
	else if (readyFDs == 0)
		return RECEIVE_TIMEOUT;
	return receivePacket(mySocket, packet, source);
}

int receivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source) {
	socklen_t addressSize = sizeof(*source);
	if (recvfrom(mySocket, packet, sizeof(struct packet), 0, (struct sockaddr*)source, &addressSize) == -1)
		fatalerror("Failed to receive packet");
	printf(ANSI_GREEN"RECEIVED: %s [id:"ANSI_BLUE"%d"ANSI_GREEN" seq:"ANSI_BLUE"%d"ANSI_GREEN" ws:"ANSI_BLUE"%d"ANSI_GREEN" data:"ANSI_BLUE"%s"ANSI_GREEN"] to"ANSI_BLUE" %s" ANSI_GREEN":" ANSI_BLUE"%d\n"ANSI_RESET,
				 getFlagString(packet->flags),
				 packet->id,
				 packet->seq,
				 packet->windowsize,
				 packet->data,
				 inet_ntoa(source->sin_addr),
				 ntohs(source->sin_port));
	if (isPacketBroken(packet))
		return RECEIVE_BROKEN;
	return RECEIVE_OK;
}

void sendPacket(const int mySocket, const struct packet *packet, const struct sockaddr_in *destination, int isResend) {
	int chance = rand()%2;
	
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
	
	if (chance == 1)
		if (sendto(mySocket, packet, sizeof(*packet), 0, (struct sockaddr*)destination, sizeof(*destination)) == -1)
			fatalerror("Failed to send");
	else
		printf(ANSI_RED"SNEAKY NINJA THREW AWAY PACKAGE\n"ANSI_RESET);
}

int isPacketBroken(struct packet *packet) {
	uint32_t checksum = packet->crc;
	packet->crc = 0;
	unsigned char *data = malloc(sizeof(struct packet) + sizeof(checksum));
	
	// PUT PACKET AND CHECKSUM TOGETHER
	memcpy(data, packet, sizeof(*packet));
	memcpy(data + sizeof(*packet), (unsigned char *)&checksum, sizeof(checksum));
	
	if (crc32(data, sizeof(*packet) + sizeof(checksum)) == 0) {
		return 0;
	}
	printf(ANSI_RED"WARNING: Broken packet found!\n"ANSI_RESET);
	free(data);
	return 1;
}
