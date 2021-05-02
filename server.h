#ifndef SERVER_H
#define SERVER_H

#include "rdp.h"

void send_to_client(Connection *c); //Send packet to a connected client

Packet **create_packets(char *filename, int *data_len, int max_bytes, int *total_packets); //Takes a data buffer and splits it into data packets with flags 0x04 and correct pktseq.

#endif
