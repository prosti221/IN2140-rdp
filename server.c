//main loop
   //1. Check if a new RDP connect request has arrived
   //2. try to deliver next or final packet to each connected RDP client
   //3. checks if an RDP connection has been closed
   //4. if nothing has happened in any of these three cases(no new connection, no connection was closed, no packet could be sent to any connection),
   //   we wait for a period
   //   Here there are two implementation options:
   //      a. Waits for one second before trying again
   //      b. Use select() to wait until an event occurs 
#include <stdio.h>
#include "rdp.h"
#include "server.h"
#include "send_packet.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#define MAX_CONNECTIONS 100
#define MAX_PACKET_BYTES 10
#define TIMEOUT 2 
char *file_buffer(char *filename, int *datalen){
    FILE *fd;
    if ( ( fd = fopen(filename, "rb") ) == NULL ){
        perror("File does not exist.");
        //exit(-1);
    }
    fseek(fd, 0, SEEK_END);
    *datalen = ftell(fd);
    rewind(fd);
    char *data_buffer = (char*)malloc(*datalen); //REMEMBER TO CHECK RETURN
    fread(data_buffer, *datalen, 1, fd);
    fclose(fd);
    return data_buffer;
}

int main(int argc, char **argv)//PORT, FILENAME, N, PROB
{
    if(argc < 5){
        printf("\nUsage: %s <port> <filename> <number of files> <loss probability>\n", argv[0]);
        //exit(-1);
    }
    float prob = strtof(argv[3], NULL);
    if ( prob < 0 || prob > 1 ){
        printf("\nInvalid loss probability. Excpected: 0 < p < 1 ");
        //exit(-1);
    }
    //unsigned PORT = atoi(argv[1]);
    unsigned short PORT = 8080;
    int sockfd;
    Packet **packs = NULL;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_length = sizeof(client_addr);
    //All connections
    int connected = 0;
    Connection **connections = malloc(sizeof(Connection) * MAX_CONNECTIONS);
    //Storing file in databuffer and allocating memory to it
    int datalen = 0;
    //char *data_buffer = file_buffer(fd,argv[2], &datalen);
    char *data_buffer = file_buffer("README.txt", &datalen);
    //Number of packets needed is datalen/MAX_PACKET_BYTES
    int packet_num = datalen/MAX_PACKET_BYTES; 
    packs = create_packets(data_buffer, datalen, MAX_PACKET_BYTES, &packet_num);
    //N is number of files to transfer before terminating
    int N = atoi(argv[2]);

    //Setting loss probability
    set_loss_probability(0.5);
    
    //setting up the socket
    if( (sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ){
       perror("Socket error");
       exit(-1); 
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if( (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) ){
        perror("Socket binding error");
        close(sockfd);
        exit(1); 
    }

    int one = 1;
    struct timeval t;
    t.tv_sec = TIMEOUT;
    t.tv_usec = TIMEOUT/1000;
    if ( (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&t, sizeof(struct timeval))) < 0){
        perror("setsockopt()");
        exit(-1);
    }
    if ( (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) < 0){
        perror("setsockopt()");
        exit(-1);
    }
   
    Connection *c;
    while(1){
        if ( (c = rdp_accept(&sockfd, connections, &connected)) != NULL ){
            printf("\n[+] CONNECTED: %d --> %d\n", c->server_id, c->client_id);
            connections[connected] = c;
            connected++;
        }

        for(int i = 0; i < connected; i++){
            if(connections[i]->packet_seq < packet_num){
                Packet *data = packs[(int)connections[i]->packet_seq];
                data->sender_id = connections[i]->server_id;
                data->recv_id = connections[i]->client_id;
                print_packet(data);
                send_data(&sockfd,connections[i]->client_id, connections[i]->server_id, &connections[i]->client_addr, data);
                char *resend = serialize(data);
                wait_ACK(&sockfd, resend, data->recv_id, data->sender_id, data->pktseq, &connections[i]->client_addr);  
                connections[i]->packet_seq += 1; 
            }
        }
    }
    
    
    
    close(sockfd);
    for (int i = 0; i < connected; i++){
        free(connections[i]);
    }
    free(connections);
    for (int i = 0; i < packet_num; i++){
        free(packs[i]);
    }
    free(packs);
    free(data_buffer);
}
