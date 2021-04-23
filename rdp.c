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
int wait_rdp_accept(int *sockfd, char *serial, int recv_ID, struct sockaddr_in *resend){
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    struct sockaddr_in re = *resend;
    int ACK = 0;
    char buffer[sizeof(Packet)];
    while(ACK == 0){
        int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + 1000, 0, (struct sockaddr*)&from_addr, &from_length);
        if (bytes > 0){
            Packet *p = de_serialize(buffer, sizeof(Packet));
            if(p->flags == 0x10 && p->sender_id == 0 && p->recv_id == recv_ID){
                printf("\n[+]Connection accepted by server %d\n", p->sender_id);
                ACK = 1;
                free(p);
                return 0;
            }
            if(p->flags == 0x20 && p->sender_id == 0 && p->recv_id == recv_ID){
                printf("\n[-]Connection refused by server %d\n", p->sender_id);
                ACK = 1;
                free(p);
                return -1;
            }
            free(p);
        }
        if (sendto(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&re, sizeof(re)) < 0) {
            perror("sendto()");
            exit(-1);
        }        
    } 
    return 1; 
}
void send_ACK(int *sockfd, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *dest){
    Packet *p = malloc(sizeof(Packet));
    struct sockaddr_in d = *dest;
    p->flags = 0x08;
    p->metadata = 0;
    p->sender_id = sender_ID;
    p->recv_id = recv_ID;
    p->ackseq = packet_nb;
    p->pktseq = 0;
    unsigned int size = 0;
    char* serial = serialize(p, &size);
    if (sendto(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&d, sizeof(d)) < 0) {
        perror("sendto()");
        exit(-1);
    }      
    free(p);
    free(serial);

}

void wait_ACK(int *sockfd, char *serial, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *resend){
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    struct sockaddr_in re = *resend;
    int ACK = 0;
    char buffer[sizeof(Packet)];
    while(ACK == 0){
        int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + 1000, 0, (struct sockaddr*)&from_addr, &from_length);
        if (bytes > 0){
            Packet *p = de_serialize(buffer, sizeof(Packet));
            if(p->flags == 0x08 && p->sender_id == sender_ID && p->recv_id == recv_ID){
                printf("\n[+]ACK recieved");
                ACK = 1;
                free(p);
                return;
            }
            free(p);
        }
        if (sendto(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&re, sizeof(re)) < 0) {
            perror("sendto()");
            exit(-1);
        }        
    }
}

struct Connection *rdp_accept(int *sockfd, Connection **connections, int *connected){
    Packet *p;
    Packet *ret = malloc(sizeof(Packet));
    struct sockaddr_in client_addr;
    socklen_t client_length = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));

    char buffer[sizeof(Packet)];
    int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + 1000, 0, (struct sockaddr*)&client_addr, &client_length);
    if(bytes > 0){
        p = de_serialize(buffer, sizeof(Packet));
        if(p->flags == 0x01){
            for(int i = 0; i < *connected; i++){
                if(connections[i]->client_id == p->sender_id){
                    ret->ackseq = 0;
                    ret->flags = 0x20;
                    ret->sender_id = p->recv_id;
                    ret->recv_id = p->sender_id;
                    ret->metadata = 0;
                    ret->pktseq = 0;
                    ret->unassigned = 0;
                    unsigned int size = 0; 
                    char *serial = serialize(ret, &size);
                    if (sendto(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("sendto()");                                                                                        
                        exit(-1);                                                                                                  
                    }
                    printf("\n[-] Connection refused: client -> %d", p->sender_id);
                    free(p);
                    free(ret);
                    return NULL;                    
                }  
            }
            ret->ackseq = 0;
            ret->flags = 0x10;
            ret->sender_id = p->recv_id;
            ret->recv_id = p->sender_id;
            ret->metadata = 0;
            ret->pktseq = 0;
            ret->unassigned = 0;
            unsigned int size = 0; 
            char *serial = serialize(ret, &size);
            if (sendto(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                perror("sendto()");                                                                                        
                exit(-1);                                                                                                  
            }
            Connection *c = malloc(sizeof(Connection));
            c->client_id = p->sender_id;
            c->client_addr = client_addr;
            c->server_id = 0;
            c->packet_seq = 0;
            free(ret);
            free(p);
            return c; 
        }
        free(ret);
        free(p);
    }
    return NULL;
}

void print_packet(Packet *packet){
    printf("\nFlag: %x \nPKTseq: %x \nACKseq: %x\nSender ID: %d\nReciver ID: %d\nMetadata: %d\nPayload: %s\n", 
            packet->flags, packet->pktseq, packet->ackseq, packet->sender_id, packet->recv_id, packet->metadata, "NONE");
}

void print_connection(Connection *c){
    printf("\nCONNECTED TO: %d", c->client_id);
}


