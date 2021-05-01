#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "rdp.h"

char *file_buffer(char *filename, int *datalen); //Store file data in memory

void send_to_client(Connection *c); //Send packet to a connected client

Packet **create_packets(char* data_bfr, int data_len, int max_bytes, int *total_packets); //Takes a data buffer and splits it into data packets with flags 0x04 and correct pktseq.
