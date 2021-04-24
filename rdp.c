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

#define MAX_PACKET_BYES 10

char *serialize(Packet *packet){
    int packet_size = sizeof(Packet) + packet->metadata;
    Packet *cpy = malloc(packet_size);
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
    Packet *p = malloc(size);
    memcpy(p, d, size);
    p->sender_id = ntohl(p->sender_id);
    p->recv_id = ntohl(p->recv_id);
    p->metadata = ntohl(p->metadata);
    return p;
}

Packet **create_packets(char* data_bfr, int data_len, int max_bytes, int *total_packets){
    if(data_len < 1){
        printf("\nEmpty data buffer. Cannot create packets \n");
        exit(-1);
    }
    Packet **packets;
    if(data_len < max_bytes){
        *total_packets = 1;
        packets = malloc(sizeof(Packet *));
        Packet *p = malloc(sizeof(Packet) + data_len);
        *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=data_len};
        memcpy(p->payload, data_bfr , data_len);
        packets[0] = p;
        return packets; 
    }
    int r = 0; //Remainder 
    if (data_len % max_bytes == 0){
        *total_packets = data_len / max_bytes;
        packets = malloc(sizeof(Packet*) * (*total_packets));
    }else{
        r = data_len % max_bytes;
        *total_packets = (data_len - r)/max_bytes;
        packets = malloc(sizeof(Packet*) * (*total_packets + 1));
    }
    for(int i = 0; i < *total_packets; i++){
        Packet *p = malloc(sizeof(Packet) + max_bytes);
        *p = (Packet){.flags=0x04, .pktseq=i, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=max_bytes};
        memcpy(p->payload, data_bfr + (i * max_bytes) , max_bytes);
        packets[i] = p;
    }
    if(r != 0){
        *total_packets += 1;
        Packet *p = malloc(sizeof(Packet) + r);
        *p = (Packet){.flags=0x04, .pktseq=*total_packets-1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=r};
        memcpy(p->payload, data_bfr + (data_len - r), r);
        packets[*total_packets-1] = p;
    }
    return packets;
}

int wait_rdp_accept(int *sockfd, char *serial, int recv_ID, struct sockaddr_in *resend){
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    struct sockaddr_in re = *resend;
    int ACK = 0;
    char buffer[sizeof(Packet)];
    while(ACK == 0){
        int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYES, 0, (struct sockaddr*)&from_addr, &from_length);
        if (bytes > 0){
            Packet *p = de_serialize(buffer);
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
        if (send_packet(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&re, sizeof(re)) < 0) {
            perror("sendto()");
            exit(-1);
        }        
    } 
    return 1; 
}
void send_ACK(int *sockfd, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *dest){
    Packet *p = malloc(sizeof(Packet));
    struct sockaddr_in d = *dest;
    *p = (Packet){.flags=0x08, .metadata=0, .sender_id=sender_ID, .recv_id=recv_ID, .ackseq=packet_nb, .pktseq=0, .unassigned=0};
    char* serial = serialize(p);
    if (send_packet(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&d, sizeof(d)) < 0) {
        perror("sendto()");
        exit(-1);
    }
    printf("\n[+] Sending ACK to %d\n", p->recv_id);    
    free(p);
    free(serial);

}
void send_data(int *sockfd, int recv_ID, int sender_ID, struct sockaddr_in *dest, Packet *data){
    char *serial = serialize(data);
    struct sockaddr_in d = *dest;
    if (send_packet(*sockfd, serial, sizeof(Packet) + data->metadata, 0, (const struct sockaddr *)&d, sizeof(d)) < 0) {
        perror("sendto()");
        exit(-1);
    }
    printf("\n[+] Sending packet %d to client %d\n", data->pktseq, data->recv_id);
    free(serial);
}

void wait_ACK(int *sockfd, char *serial, int sender_ID, int recv_ID, int packet_nb, struct sockaddr_in *resend){ //sender ID is the one sending the ACK, while recv_ID is waiting
    struct sockaddr_in from_addr;
    socklen_t from_length = sizeof(from_addr);
    struct sockaddr_in re = *resend;
    int ACK = 0;
    char buffer[sizeof(Packet)];
    while(ACK == 0){
        int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYES, 0, (struct sockaddr*)&from_addr, &from_length);
        if (bytes > 0){
            Packet *p = de_serialize(buffer);
            if(p->flags == 0x08 && p->sender_id == sender_ID && p->recv_id == recv_ID && p->ackseq == packet_nb){
                printf("\n[+] ACK recieved by %d\n", p->sender_id);
                ACK = 1;
                free(p);
                return;
            }
            free(p);
        }
        if (send_packet(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&re, sizeof(re)) < 0) {
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
    int bytes = recvfrom(*sockfd, buffer, sizeof(Packet) + MAX_PACKET_BYES, 0, (struct sockaddr*)&client_addr, &client_length);
    if(bytes > 0){
        p = de_serialize(buffer);
        if(p->flags == 0x01){
            for(int i = 0; i < *connected; i++){
                if(connections[i]->client_id == p->sender_id){
                    *ret = (Packet){.flags=0x20, .metadata=0, .sender_id=p->recv_id, .recv_id=p->sender_id, .ackseq=0, .pktseq=0, .unassigned = 0};
                    char *serial = serialize(ret);
                    if (send_packet(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                        perror("sendto()");                                                                                        
                        exit(-1);                                                                                                  
                    }
                    printf("\n[-] NOT CONNECTED: %d --> %d\n", p->recv_id, p->sender_id);
                    free(serial);
                    free(p);
                    free(ret);
                    return NULL;                    
                }  
            }
            *ret = (Packet){.flags=0x10, .metadata=0, .sender_id=p->recv_id, .recv_id=p->sender_id, .ackseq=0, .pktseq=0, .unassigned=0};
            char *serial = serialize(ret);
            if (send_packet(*sockfd, serial, sizeof(Packet), 0, (const struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                perror("sendto()");                                                                                        
                exit(-1);                                                                                                  
            }
            Connection *c = malloc(sizeof(Connection));
            *c = (Connection){.client_id=p->sender_id, .client_addr=client_addr, .server_id=p->recv_id, .packet_seq=0};
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
    printf("\nCONNECTED TO: %d", c->client_id);
}


