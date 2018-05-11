CC = gcc
CFLAGS = -Wall
PROGRAMS = client server

ALL: ${PROGRAMS}

client: client.c
	${CC} ${CFLAGS} -o client client.c packet.c error.c slidingwindow.c

server: server.c
	${CC} ${CFLAGS} -o server server.c packet.c error.c slidingwindow.c

clean:
	rm -f ${PROGRAMS}
