CC = g++

all: server client 

client: 
	$(CC) -o client client.cpp

server: 
	$(CC) -o server server.cpp

clean:
	rm -f server client