#include <stdio.h>  
#include <stdlib.h>
#include "client.h"
#include "rdp.h"
#include "res/send_packet.h"                                                                                
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


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
    //unsigned short port = htons(atoi(argv[2]));
    unsigned short port = 8080;
    struct sockaddr_in server, from;
    socklen_t server_len = sizeof(server);
    socklen_t from_len = sizeof(from_len);
    //set_loss_probability(prob); 
    
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
        perror("Socket error");
        exit(-1);
    }
   
    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    //server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    Packet *connect = malloc(sizeof(Packet));                            
    connect->metadata = 0;                                                   
    connect->ackseq = 'a';                                                         
    connect->unassigned = 0;                                                         
    connect->pktseq = 'a';                                                         
    connect->flags = 0x01;                                                          
    connect->recv_id = 0;                                                        
    connect->sender_id = 234;
    unsigned int size = 0;
    char *serial = serialize(connect, &size);
    if (sendto(sock, serial, sizeof(Packet), 0, (const struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("sendto()");
        exit(2);
    }
   
    //wait_ACK(&sock, serial, 0, 1, &server);
    if ( (wait_rdp_accept(&sock, serial, 234, &server) ) != 0){
        printf("#### Terminating client ####\n");
        exit(-1);
    }
    
    free(serial);
    free(connect);
    close(sock);

}
