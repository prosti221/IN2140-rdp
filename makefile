params=-Wall -Wextra
DEPS= server.c server.h rdp.h rdp.c send_packet.h send_packet.c
all: server client

server: $(DEPS)
	gcc $(params) -g server.c rdp.c send_packet.c -o $@

client: $(DEPS)
	gcc $(params) -g client.c rdp.c send_packet.c -o $@

clean: 
	rm server client
