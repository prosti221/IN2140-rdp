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
    FILE *fd;
    //Socket info 
    int sock;
    unsigned char current_packet = 2; 
    unsigned short port = atoi(argv[2]);
    struct sockaddr_in server;
    set_loss_probability(prob); //Setting probability
    //Creating socket
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
        perror("Socket error");
        exit(-1);
    }
    //Setting up timeout
    struct timeval t;
    t.tv_sec = 0;                                                          
    t.tv_usec = 900000;    
    if ( (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&t, sizeof(struct timeval))) < 0){
        perror("setsockopt()");
        exit(-1);
    }    

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(argv[1]);

    //Setting up a connect request
    Packet *connect;
    if ( (connect  = malloc(sizeof(Packet))) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    *connect = (Packet){.metadata=0, .ackseq=0, .unassigned=0, .pktseq=0, .flags=0x01, .recv_id=0, .sender_id=CLIENT_ID}; 
    char *serial = serialize(connect);
    
    //Waiting for connect response from server(-1 = refused, 0 = accepted) 
    if ( (wait_rdp_accept(&sock, serial, CLIENT_ID, &server) ) != 0){
        free(serial);
        free(connect);
        close(sock);
        printf("#### Terminating client ####\n");
        exit(-1);
    }
    //Check if file already exists before opening
    if(access(filename, F_OK) == 0){ //if file already exists
        printf("\nFile \"%s\" already exists\n", filename);
        exit(-1);
    }else{ //if file does not exist
        fd = fopen(filename, "wb");
    }
    //Event loop
    while(1){
        Packet *p = recv_data(&sock, CLIENT_ID, &current_packet, &server);
        if (p != NULL){                                                         
            if(p->metadata == 0){
                printf("\n%s[+] File recieved:%s \"%s\"\n",GREEN, NORM, filename);
                printf("\n%s[-] Disconnected\n%s", RED, NORM);
                free(p);
                break;
            }
            fwrite(&(p->payload), 1, p->metadata, fd);      //Write new packet to file
            free(p);                                                            
        }               
    }
    free(serial);
    free(connect);
    fclose(fd);
    close(sock);
}
