CC = gcc
CFLAGS = -Wall
LIBS = -lws2_32

all: server client

server: server.c
	$(CC) $(CFLAGS) server.c -o server $(LIBS)

client: client.c
	$(CC) $(CFLAGS) client.c -o client $(LIBS)

clean:
	del /f server.exe client.exe 2>nul || rm -f server client
