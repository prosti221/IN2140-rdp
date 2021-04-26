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
#include <time.h>
#include <sys/time.h>

#define MAX_PACKET_BYTES 1000
#define NORM "\x1B[0m"                                                          
#define RED "\x1B[31m"                                                          
#define GREEN "\x1B[32m"  

int main(int argc, char **argv)                                                  
{
    if(argc < 4){
        printf("\nUsage: %s <ip> <port> <loss probability>", argv[0]);
        exit(-1);
    }
    float prob = strtof(argv[3], NULL);
    if ( prob < 0 || prob > 1 ){
        printf("\nInvalid loss probability. Excpected: 0 < p < 1 ");
        exit(-1);
    }
    //Generating random client ID 
    struct timeval tm;                                                      
    gettimeofday(&tm, NULL);                                                
    srandom(tm.tv_sec + tm.tv_usec * 1000000ul);                            
    const int CLIENT_ID = rand(); 
    //File to wrtie data
    char filename[26];
    sprintf(filename, "out/kernel-file-%d", CLIENT_ID);
    FILE *fd = fopen(filename, "wb"); //Check return on this!
    //Socket info
    int sock;
    int current_packet = -1;
    unsigned short port = atoi(argv[2]);
    //unsigned short port = 8080;
    struct sockaddr_in server, from;
    socklen_t from_len = sizeof(from_len);
    set_loss_probability(prob); 
    
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
        perror("Socket error");
        exit(-1);
    }
    //Setting timeout
    struct timeval t;
    t.tv_sec = 1;                                                          
    t.tv_usec = 0;    
    if ( (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&t, sizeof(struct timeval))) < 0){
        perror("setsockopt()");
        exit(-1);
    }    
   
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(argv[1]);

    //Sending a connect request
    Packet *connect = malloc(sizeof(Packet));
    *connect = (Packet){.metadata=0, .ackseq=0, .unassigned=0, .pktseq=0, .flags=0x01, .recv_id=0, .sender_id=CLIENT_ID}; 
    char *serial = serialize(connect);
    if (send_packet(sock, (const char*)serial, sizeof(Packet), 0, (const struct sockaddr *)&server, sizeof(server)) < 0) { //Sending connect request to server
        perror("sendto()");
        exit(2);
    }
    //Waiting for connect response from server(-1 = refused, 0 = accepted) 
    if ( (wait_rdp_accept(&sock, serial, CLIENT_ID, &server) ) != 0){
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
            if(p->flags == 0x04 && p->sender_id == 0 && p->recv_id == CLIENT_ID && p->pktseq > current_packet){
                if(p->metadata == 0){
                    send_terminate(&sock, p->recv_id, p->sender_id, &server); 
                    printf("\n%s[+] File recieved:%s \"%s\"\n",GREEN, NORM, filename);
                    printf("\n%s[-] Disconnected\n%s", RED, NORM);
                    free(p);
                    break;
                }
                current_packet = p->pktseq;
                fwrite(&(p->payload), 1, p->metadata, fd);
                send_ACK(&sock, p->recv_id, p->sender_id, current_packet, &server); 
            }else if(p->flags == 0x04 && p->sender_id == 0 && p->recv_id == CLIENT_ID && p->pktseq <= current_packet){
                send_ACK(&sock, p->recv_id, p->sender_id, current_packet, &server); //if the same packet is recived, resend ACK. 
            }
            free(p);                                                            
        }               
    }
    
    free(serial);
    free(connect);
    fclose(fd);
    close(sock);

}
