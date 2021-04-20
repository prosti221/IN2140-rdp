#include <stdio.h>                                                              
#include <stdlib.h>                                                             
#include "rdp.h"                                                                
#include "res/send_packet.h"                                                    
#include <unistd.h>                                                             
#include <string.h>                                                             
#include <sys/types.h>                                                          
#include <sys/socket.h>                                                         
#include <arpa/inet.h>                                                          
#include <netinet/in.h>  

char *serialize(Packet *packet, unsigned int *size){
    int packet_size = sizeof(Packet) + packet->metadata;
    printf("%d", packet_size);
    Packet *cpy = malloc(packet_size);
    memcpy(cpy, packet, packet_size);
    cpy->recv_id = htonl(cpy->recv_id);
    cpy->metadata = htonl(cpy->metadata);
    cpy->sender_id = htonl(cpy->sender_id);
    *size = packet_size;
    return (char *)cpy;
}

struct Packet *de_serialize(char *d, unsigned int size){
    Packet *p = malloc(size);
    memcpy(p, d, size);
    p->sender_id = ntohl(p->sender_id);
    p->recv_id = ntohl(p->recv_id);
    p->metadata = ntohl(p->metadata);
    return p;
}

void print_packet(Packet *packet){
    printf("\nFlag: %x \nPKTseq: %x \nACKseq: %x\nSender ID: %d\nReciver ID: %d\nMetadata: %d\nPayload: %s\n", 
            packet->flags, packet->pktseq, packet->ackseq, packet->sender_id, packet->recv_id, packet->metadata, packet->payload);
}
