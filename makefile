params=-Wall -Wextra
DEPS=server.c server.h rdp.h rdp.c send_packet.h send_packet.c

all: $(DEPS)
	gcc -g server.c rdp.c send_packet.c -o server
	gcc -g client.c rdp.c send_packet.c -o client 

server: server.h server.c rdp.h rdp.c
	gcc $(params) -g server.c rdp.c send_packet.c -o $@

client: client.c rdp.c rdp.h
	gcc $(params) -g client.c rdp.c send_packet.c -o $@

clean: 
	rm server client
	rm -rf out/kernel-file-*
