#include <stdio.h>  
#include <stdlib.h>
#include "client.h"
#include "rdp.h"
#include "send_packet.h"                                                                                
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAX_PACKET_BYTES 10
#define TIMEOUT 100000

int main(int argc, char **argv)                                                  
{
    if(argc < 4){
        printf("\nUsage: %s <ip> <port> <loss probability>", argv[0]);
        //exit(-1);
    }
    float prob; 
    if ( (strtof(argv[3], NULL) ) < 0 || (strtof(argv[3], NULL) > 1 ) ){
        printf("\nInvalid loss probability. Excpected: 0 < p < 1 ");
        //exit(-1);
    }
    
    
    int sock;
    int current_packet = -1;
    //unsigned short port = htons(atoi(argv[2]));
    unsigned short port = 8080;
    struct sockaddr_in server, from;
    socklen_t server_len = sizeof(server);
    socklen_t from_len = sizeof(from_len);
    set_loss_probability(0.5); 
    
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
        perror("Socket error");
        exit(-1);
    }
    struct timeval t;
    t.tv_sec = 0;                                                          
    t.tv_usec = TIMEOUT;    
    if ( (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&t, sizeof(struct timeval))) < 0){
        perror("setsockopt()");
        exit(-1);
    }    
   
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    //server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    Packet *connect = malloc(sizeof(Packet));
    *connect = (Packet){.metadata=0, .ackseq=0, .unassigned=0, .pktseq=0, .flags=0x01, .recv_id=0, .sender_id=234}; 
    char *serial = serialize(connect);
    if (send_packet(sock, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&server, sizeof(server)) < 0) { //Sending connect request to server
        perror("sendto()");
        exit(2);
    }
   
    //wait_ACK(&sock, serial, 0, 1, &server);
    if ( (wait_rdp_accept(&sock, serial, 234, &server) ) != 0){ //Waits for connect request response from server, if no response it resends request. If return = -1, connection is refused.
        free(serial);
        free(connect);
        close(sock);
        printf("#### Terminating client ####\n");
        exit(-1);
    }
    while(1){
        char buffer[sizeof(Packet) + MAX_PACKET_BYTES];
        int bytes = recvfrom(sock, buffer, sizeof(Packet) + MAX_PACKET_BYTES + 1, 0, (struct sockaddr*)&from, &from_len);
        if (bytes > 0){                                                         
            Packet *p = de_serialize(buffer);                                   
            if(p->flags == 0x04 && p->sender_id == 0 && p->recv_id == 234 && p->pktseq > current_packet){
                printf("\n[+]Packet %d recieved from %d\n", p->pktseq, p->sender_id);
                current_packet = p->pktseq;
                print_packet(p);              
                send_ACK(&sock, p->recv_id, p->sender_id, current_packet, &server); 
            }else if(p->flags == 0x04 && p->sender_id == 0 && p->recv_id == 234 && p->pktseq <= current_packet){
                send_ACK(&sock, p->recv_id, p->sender_id, current_packet, &server); 
            }
            free(p);                                                            
        }               
    }
    
    free(serial);
    free(connect);
    close(sock);

}
