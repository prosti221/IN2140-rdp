#include <stdio.h>                                                              
#include <stdlib.h>                                                             
#include "rdp.h"                                                                
#include "send_packet.h"                                                    
#include <unistd.h>                                                             
#include <string.h>                                                             
#include <sys/types.h>                                                          
#include <sys/socket.h>                                                         
#include <arpa/inet.h>                                                          
#include <netinet/in.h>  
#include <time.h>

#define MAX_PACKET_BYTES 1000
#define NORM "\x1B[0m"                                                          
#define RED "\x1B[31m"                                                          
#define GREEN "\x1B[32m"  
#define YEL  "\x1B[33m"

char *serialize(Packet *packet){
    int packet_size = sizeof(Packet) + packet->metadata;
    Packet *cpy;
    if ( (cpy = malloc(packet_size)) == NULL){
        perror("Malloc");
        exit(-1);
    }
    memcpy(cpy, packet, packet_size);
    cpy->recv_id = htonl(cpy->recv_id);
    cpy->metadata = htonl(cpy->metadata);
    cpy->sender_id = htonl(cpy->sender_id);
    return (char *)cpy;
}

struct Packet *de_serialize(char *d){
    int size;
    memcpy(&size, (d+12), sizeof(int));
    size = sizeof(Packet) + ntohl(size);
    Packet *p;
    if ( (p = malloc(size)) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    memcpy(p, d, size);
    p->sender_id = ntohl(p->sender_id);
    p->recv_id = ntohl(p->recv_id);
    p->metadata = ntohl(p->metadata);
    return p;
}
Packet *rdp_recv_data(int *sockfd, int recv_ID, unsigned char *current_packet, struct sockaddr_in *resend){ //return = 0 -> no packets, return = 1 -> last empty packet
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    char buffer[sizeof(Packet) + MAX_PACKET_BYTES];                         
    int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYTES, 0, (struct sockaddr*)&from_addr, &from_length);
    if (bytes > 0){
        Packet *p = de_serialize(buffer);
        if(p->flags == 0x04 && p->sender_id == 0 && p->recv_id == recv_ID && p->pktseq != *current_packet){     //Checking if we recived a
            if(p->metadata == 0){   //If the data packet is empty we send a terminate notice
                rdp_send_terminate(sockfd, p->recv_id, p->sender_id, resend);
                return p;
            }
            rdp_send_ACK(sockfd, p->recv_id, p->sender_id, p->pktseq, resend);     //Send ACK
            *current_packet = p->pktseq;
            return p;
        }else if(p->flags == 0x04 && p->sender_id == 0 && p->recv_id == recv_ID && p->pktseq == *current_packet){
            rdp_send_ACK(sockfd, p->recv_id, p->sender_id, p->pktseq, resend);     //if the same packet is recived, resend ACK. 
            free(p);
            return NULL;
        }  
    }
    return NULL;
} 
int rdp_wait_accept(int *sockfd, char *serial, int recv_ID, struct sockaddr_in *resend){
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    struct sockaddr_in re = *resend;
    char buffer[sizeof(Packet)];
    struct timeval start, end, res;
    gettimeofday(&start, NULL);
    gettimeofday(&end, NULL);
    res.tv_sec = 0; 
    res.tv_usec = 0; 
    while(res.tv_sec < 1){
        if (send_packet(*sockfd, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&re, sizeof(re)) < 0) {
            perror("sendto()");
            exit(-1);
        }        
        int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYTES, 0, (struct sockaddr*)&from_addr, &from_length);
        if (bytes > 0){
            Packet *p = de_serialize(buffer);
            if(p->flags == 0x10 && p->sender_id == 0 && p->recv_id == recv_ID){
                printf("\n%s[+]CONNECTED:%s %d --> %d\n",GREEN, NORM, p->recv_id ,p->sender_id);
                free(p);
                return 0;
            }
            if(p->flags == 0x20 && p->sender_id == 0 && p->recv_id == recv_ID){
                printf("\n%s[-]CONNECTION REFUSED:%s %d --> %d\n",YEL, NORM, p->recv_id, p->sender_id);
                free(p);
                return -1;
            }
            free(p);
        }
        gettimeofday(&end, NULL);
        timersub(&end, &start, &res);
    } 
    printf("\nNo response from server\n");
    return 1; 
}
void rdp_send_ACK(int *sockfd, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *dest){
    Packet *p;
    if ( (p = malloc(sizeof(Packet))) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    struct sockaddr_in d = *dest;
    *p = (Packet){.flags=0x08, .metadata=0, .sender_id=sender_ID, .recv_id=recv_ID, .ackseq=packet_nb, .pktseq=0, .unassigned=0};
    char* serial = serialize(p);
    if (send_packet(*sockfd, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&d, sizeof(d)) < 0) {
        perror("sendto()");
        exit(-1);
    }
    free(p);
    free(serial);
}

void rdp_send_terminate(int *sockfd, int sender_ID, int recv_ID, struct sockaddr_in *dest){
    Packet *p;
    if ( (p = malloc(sizeof(Packet))) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    struct sockaddr_in d = *dest;
    *p = (Packet){.flags=0x02, .metadata=0, .sender_id=sender_ID, .recv_id=recv_ID, .ackseq=0, .pktseq=0, .unassigned=0};
    char* serial = serialize(p);
    if (send_packet(*sockfd, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&d, sizeof(d)) < 0) {
        perror("sendto()");
        exit(-1);
    }
    free(p);
    free(serial);
}

void rdp_send_data(int *sockfd, struct sockaddr_in *dest, char* serial){
    struct sockaddr_in d = *dest;
    Packet *data = de_serialize(serial);
    if (send_packet(*sockfd, (const char*)serial, sizeof(Packet) + data->metadata, 0, (const struct sockaddr *)&d, sizeof(d)) <= 0) {
        perror("sendto()");
        exit(-1);
    }
    free(data);
}

int rdp_wait_ACK(int *sockfd,char* serial,int size, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *resend){ //sender ID is the one sending the ACK, while recv_ID is waiting
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    struct sockaddr_in re = *resend;
    char buffer[sizeof(Packet)];
    while(1){
        int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYTES, 0, (struct sockaddr*)&from_addr, &from_length);
        if (bytes > 0){
            Packet *p = de_serialize(buffer);
            if(p->flags == 0x08 && p->sender_id == sender_ID && p->recv_id == recv_ID && p->ackseq == packet_nb){
                free(p);
                return 0;
            }else if(p->flags == 0x02 && p->sender_id == sender_ID && p->recv_id == recv_ID){
                printf("\n[+] File delivered to %d\n", p->sender_id);
                free(p);
                return 1;
            }
            free(p);
        }
        if (send_packet(*sockfd, (const char*)serial, sizeof(Packet) + size, 0, (const struct sockaddr *)&re, sizeof(re)) < 0){
            perror("sendto()");
            exit(-1);
        }
    }
    return -1;
}

struct Connection *rdp_accept(int *sockfd, Connection **connections, int *connected, int *N){
    Packet *p;
    Packet *ret;
    if ( (ret = malloc(sizeof(Packet))) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    struct sockaddr_in client_addr;
    socklen_t client_length = sizeof(client_addr);
    memset(&client_addr, 0, sizeof(client_addr));

    char buffer[sizeof(Packet)];
    int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYTES, 0, (struct sockaddr*)&client_addr, &client_length);
    if(bytes > 0){
        p = de_serialize(buffer);
        if(p->flags == 0x01){
            for(int i = 0; i < *connected; i++){
                if(connections[i]->client_id == p->sender_id || *connected >= *N){ //If connection already exists, or if we are serving Nth file, refuse connection.
                    *ret = (Packet){.flags=0x20, .metadata=0, .sender_id=p->recv_id, .recv_id=p->sender_id, .ackseq=0, .pktseq=0, .unassigned = 0};
                    char *serial = serialize(ret);
                    if (send_packet(*sockfd, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("sendto()");                                                                                        
                        exit(-1);                                                                                                  
                    }
                    printf("\n%s[-] NOT CONNECTED:%s %d --> %d\n",YEL, NORM, p->recv_id, p->sender_id);
                    free(serial);
                    free(p);
                    free(ret);
                    return NULL;                    
                }  
            }
            *ret = (Packet){.flags=0x10, .metadata=0, .sender_id=p->recv_id, .recv_id=p->sender_id, .ackseq=0, .pktseq=0, .unassigned=0};
            char *serial = serialize(ret);
            if (send_packet(*sockfd, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                perror("sendto()");                                                                                        
                exit(-1);                                                                                                  
            }
            Connection *c;
            if ( (c = malloc(sizeof(Connection))) == NULL){
                perror("Malloc error: ");
                exit(-1);
            }
            *c = (Connection){.client_id=p->sender_id, .client_addr=client_addr, .server_id=p->recv_id, .packet_nb=0, .state=1};
            free(serial);
            free(ret);
            free(p);
            return c; 
        }
        free(p);
    }
    free(ret);
    return NULL;
}

void print_packet(Packet *packet){
    printf("\nFlag: %x \nPKTseq: %x \nACKseq: %x\nSender ID: %d\nReciver ID: %d\nMetadata: %d\nPayload: %s\n", 
            packet->flags, packet->pktseq, packet->ackseq, packet->sender_id, packet->recv_id, packet->metadata, packet->payload);
}

void print_connection(Connection *c){
    if(c == NULL){
        printf("\nNULL\n");
    }
    printf("\nCONNECTED TO: %d", c->client_id);
}


