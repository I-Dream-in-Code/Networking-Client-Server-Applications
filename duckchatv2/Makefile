CC=g++
#CFLAGS=-Wall -W -g -Werror 

CFLAGS= -g  -DGRAD=1

LOADLIBES= -lnsl 

all: server



server: server.c 
	$(CC) server.c  $(LOADLIBES) $(CFLAGS) -o server

clean:
	rm -f  server *.o

