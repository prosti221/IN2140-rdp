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
#include "res/send_packet.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

#define MAX_CONNECTIONS 100
#define MAX_PACKET_BYTES 1000

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
        printf("\nInvalid arguments. Expected: port, filename, number of files N, loss probability: 0 < prob < 1\n");
        //exit(-1);
    }
    //unsigned PORT = atoi(argv[1]);
    unsigned short PORT = 8080;
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_length = sizeof(client_addr);
    //All connections
    Connection *connections = malloc(sizeof(Connection) * MAX_CONNECTIONS);
    //Storing file in databuffer and allocating memory to it
    int datalen = 0;
    //char *data_buffer = file_buffer(fd,argv[2], &datalen);
    char *data_buffer = file_buffer("file.dat", &datalen);
    printf("Lenght of data buffer: %d", datalen);
    //Number of packets needed is datalen/MAX_PACKET_BYTES
    int packet_num = datalen/MAX_PACKET_BYTES;
    
    //N is number of files to transfer before terminating
    int N = atoi(argv[2]);

    //Setting loss probability
    float prob = strtof(argv[3], NULL);                                           
    //set_loss_probability(prob);
    
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
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    Connection *c;
    if ( (c = rdp_accept(&sockfd)) == NULL ){
        perror("Connection FLAG is probably wrong");
        exit(-1);
    }
    struct sockaddr_in *test = &c->client_addr;
    printf("Stored port: %d\n", htons(test->sin_port)); 
    send_ACK(&sockfd, 0, c->client_id, 1, &c->client_addr);

    print_connection(c);
    close(sockfd);
    free(c);
    free(connections);
    free(data_buffer);
}
