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
        printf("\nInvalid arguments. Arguments expected are: IP/hostname, port, loss probability: 0 < prob < 1\n");
        exit(-1);
    }

    float prob = strtof(argv[3], NULL);
    int sock;
    unsigned short port = htons(atoi(argv[2]));
    struct sockaddr_in server;
    //set_loss_probability(prob); 
    
    if( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ){
        perror("Socket error");
        exit(-1);
    }
    server.sin_family = AF_INET;
    server.sin_port = port;
    server.sin_addr.s_addr = inet_addr(argv[1]);

    //Create packets 
    //send packet to server

}
