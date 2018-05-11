#include "packet.h"
#include <netinet/in.h>
#include <time.h>

struct packet createPacket(int flags, int id, int seq, int windowsize, int crc, char *data) {
	struct packet packet;
	packet.flags = flags;
	packet.id = id;
	packet.seq = seq;
	packet.windowsize = windowsize;
	packet.crc = crc;
	packet.data = data;
	return packet;
}

int waitAndReceivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source, const int timeout) {
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
	else if (readyFDs > 0) {
		return receivePacket(mySocket, packet, source);
	}
	return 0;
}

int receivePacket(const int mySocket, struct packet *packet, struct sockaddr_in *source) {
	socklen_t addressSize = sizeof(*source);
	return recvfrom(mySocket, packet, sizeof(struct packet), 0, (struct sockaddr*)source, &addressSize);
}

int sendPacket(const int mySocket, const struct packet *packet, const struct sockaddr_in *destination) {
	//int chance = rand()%2;
	//if (chance == 0)
	//	return 0;
	return sendto(mySocket, packet, sizeof(*packet), 0, (struct sockaddr*)destination, sizeof(*destination));
}
