params=-Wall -Wextra

all: server client

server: server.c server.h rdp.h rdp.c
	gcc $(params) -g server.c rdp.c -o $@

client: client.c client.h rdp.h rdp.c
	gcc $(params) -g client.c rdp.c -o $@

clean: 
	rm server client
