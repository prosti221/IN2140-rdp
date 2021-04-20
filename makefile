params=-Wall -Wextra

all: server client rdp

server: server.c server.h rdp.h rdp.c
	gcc $(params) -g $^ -o $@

client: client.c client.h rdp.h rdp.c
	gcc $(params) -g $^ -o $@

rdp: rdp.c rdp.h
	gcc $(params) -g $^ -o $@

clean: 
	rm server client
