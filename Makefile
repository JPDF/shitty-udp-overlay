CC = gcc
CFLAGS = -Wall
PROGRAMS = client server

ALL: ${PROGRAMS}

client: client.c
	${CC} ${CFLAGS} -o client client.c packet.c misc.c

server: server.c
	${CC} ${CFLAGS} -o server server.c packet.c misc.c

clean:
	rm -f ${PROGRAMS}
