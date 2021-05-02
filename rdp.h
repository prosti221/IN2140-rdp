#ifndef RDP_H
#define RDP_H

#include <netinet/in.h> 

typedef struct __attribute__((packed)) Packet{
    unsigned char flags; //0x01-->connect request  0x02-->terminate connection  0x04-->data packet  0x08-->ACK  0x10-->accept connect   0x20-->refuse connect
    unsigned char pktseq; //Sequence number of packet
    unsigned char ackseq; //Sequence number ACK-ed by packet
    unsigned char unassigned;
    int sender_id;
    int recv_id;
    int metadata;
    char payload[0]; //Maximum of 1000 bytes
}Packet;

typedef struct Connection{
    int client_id, server_id; //server id is 0
    int  packet_nb; //Packet_nb is the amount of packets delivered. 
    unsigned char state; //State is whether connecion is active or not(1 or 0).
    struct sockaddr_in client_addr; //recv_addr -> client
}Connection;


char *serialize(Packet *packet); 

struct Packet *de_serialize(char *d);

struct Connection *rdp_accept(int *sockfd, Connection **connections, int *connected, int *N); //Listens for connection requests and sends accept/refuse connection packet back

void rdp_send_ACK(int *sockfd, int recv_ID, int sender_ID, int packet_nb, struct sockaddr_in *dest); //sends ACK

void rdp_send_terminate(int *sockfd, int recv_ID, int sender_ID, struct sockaddr_in *dest); //Send terminate notice when recived final packet

void rdp_send_data(int *sockfd, struct sockaddr_in *dest, char* serial); //sends data packets

Packet *rdp_recv_data(int *sockfd, int recv_ID, unsigned char *current_packet, struct sockaddr_in *resend); //recives packet sendt by sender

int rdp_wait_ACK(int *sockfd,char* serial,int size, int recv_ID, int sender_ID, int packet_nb, struct sockaddr_in *resend); //Waits for ACK

int rdp_wait_accept(int *sockfd, char *serial, int recv_ID, struct sockaddr_in *resend); //Client waits for rdp_accept response, returns 0 if accepted, returns -1 if refused.

void print_packet(Packet *packet);

void print_connection(Connection *c);

#endif
