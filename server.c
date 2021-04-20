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

char *file_buffer(FILE *fd, int *datalen){
    fseek(fd, 0, SEEK_END);
    *datalen = ftell(fd);
    rewind(fd);
    char *data_buffer = (char*)malloc(*datalen);
    fread(data_buffer, *datalen, 1, fd);
    return data_buffer;
}

int main(int argc, char **argv)
{
    if(argc < 5){
        printf("\nInvalid arguments. Expected: port, filename, number of files N, loss probability: 0 < prob < 1\n");
        //exit(-1);
    }
    unsigned PORT = atoi(argv[1]);
    FILE *fd;
    if ( ( fd = fopen(argv[2], "rb") ) == NULL ){
        perror("File does not exist.");
        //exit(-1);
    }
    int datalen = 0;
    char *data_buffer = file_buffer(fd, &datalen);
    int N = atoi(argv[2]);

    float prob = strtof(argv[3], NULL);                                           
    set_loss_probability(prob);  
   


    fclose(fd);
    free(data_buffer);
}
