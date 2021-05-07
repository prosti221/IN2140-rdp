PARAMS=-Wall -Wextra
COMMON=rdp/rdp.h rdp/rdp.c send-packet/send_packet.h send-packet/send_packet.c

ALL_DEPS=$(COMMON) server/server.h server/server.c client/client.c 

all: $(ALL_DEPS)
	gcc -g server/server.c rdp/rdp.c send-packet/send_packet.c -o run-server
	gcc -g client/client.c rdp/rdp.c send-packet/send_packet.c -o run-client 
run-server: $(COMMON) server/server.h server/server.c
	gcc $(PARAMS) -g server/server.c rdp/rdp.c send-packet/send_packet.c -o $@

run-client: $(COMMON) client/client.c
	gcc $(PARAMS) -g client/client.c rdp/rdp.c send-packet/send_packet.c -o $@

clean: 
	rm run-server run-client
	rm -rf out/kernel-file-*
