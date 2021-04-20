#include <stdio.h>
#include <string.h>

typedef struct __attribute__((packed)) Packet{
    unsigned char flags;
    unsigned char pktseq; //Sequence number of packet
    unsigned char ackseq; //Sequence number ACK-ed by packet
    unsigned char unassigned;
    int sender_id;
    int recv_id;
    int metadata;
    char payload[0];
}Packet;

char *serialize(Packet *packet, unsigned int *size);

struct Packet *de_serialize(char *d, unsigned int size);

void print_packet(Packet *packet);


