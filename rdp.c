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

void send_ACK(int *sockfd, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *dest){
    //struct sockaddr_in *test = &(c->client_addr);                               
    printf("Stored port: %d\n", htons(dest->sin_port));  
    Packet *p = malloc(sizeof(Packet));
    p->flags = 0x01;
    p->metadata = 0;
    p->sender_id = sender_ID;
    p->recv_id = recv_ID;
    p->ackseq = packet_nb;
    unsigned int size = 0;
    char* serial = serialize(p, &size);
    if (sendto(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&dest, (socklen_t)sizeof(*dest)) < 0) {
        perror("sendto()");
        exit(-1);
    }      
    free(p);
    free(serial);

}

struct Connection *rdp_accept(int *sockfd){
    Packet *p;
    struct sockaddr_in client_addr;
    socklen_t client_length = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));

    char buffer[sizeof(Packet)];
    int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + 1000, 0, (struct sockaddr*)&client_addr, &client_length);
    if (bytes < 0){
        perror("error in recvfrom()");
        exit(1);
    }
    p = de_serialize(buffer, sizeof(Packet)); 
    print_packet(p);
    if(p->flags == 0x01){
        Connection *c = malloc(sizeof(Connection));
        c->client_id = p->sender_id;
        c->client_addr = client_addr;
        c->server_id = 0;
        free(p);
        return c; 
    }
    free(p);
    return NULL;
}

void print_packet(Packet *packet){
    printf("\nFlag: %x \nPKTseq: %x \nACKseq: %x\nSender ID: %d\nReciver ID: %d\nMetadata: %d\nPayload: %s\n", 
            packet->flags, packet->pktseq, packet->ackseq, packet->sender_id, packet->recv_id, packet->metadata, "NONE");
}

void print_connection(Connection *c){
    printf("\nCONNECTED TO: %d", c->client_id);
}


