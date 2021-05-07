#include <stdio.h>
#include "server.h"
#include "../send-packet/send_packet.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

#define MAX_PACKET_BYTES 1000
#define TIMEOUT 1000 

#define NORM "\x1B[0m"
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"

//Global variables
Packet **packs;
Connection **connections;
int connected = 0;
int served = 0;
int sockfd;

int main(int argc, char **argv)//PORT, FILENAME, N, PROB
{
    //Checking arguments
    if(argc < 5){
        printf("\nUsage: %s <port> <filename> <number of files> <loss probability>\n", argv[0]);
        exit(-1);
    }
    //Checking probability range
    float prob = strtof(argv[4], NULL);
    if ( prob < 0 || prob > 1 ){
        printf("\nInvalid loss probability. Excpected: 0 < p < 1 ");
        exit(-1);
    }
    set_loss_probability(prob);
    //Allocating memory for N connections
    int N = atoi(argv[3]);
    if ( (connections = malloc(sizeof(Connection*) * N)) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    //Creating data packets to send and storing in memory
    int datalen = 0;
    int packet_num = 0;
    packs = create_packets(argv[2], &datalen, MAX_PACKET_BYTES, &packet_num); 
    
    //setting up the socket
    unsigned PORT = atoi(argv[1]);
    struct sockaddr_in server_addr;
    if( (sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ){
       perror("Socket error");
       exit(-1); 
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if( (bind(sockfd, (const struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) ){
        perror("Socket binding error");
        close(sockfd);
        exit(1); 
    }

    //Socket options
    int one = 1;
    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = TIMEOUT;
    if ( (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval*)&t, sizeof(struct timeval))) < 0){
        perror("setsockopt()");
        exit(-1);
    }
    if ( (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) < 0){
        perror("setsockopt()");
        exit(-1);
    }

    //Event loop
    fd_set rfds;
    while(1){
    int wait = 1;
    Connection *c;
        //Listening to incoming connections
        if (served < N && (c = rdp_accept(&sockfd, connections, &connected, &N)) != NULL ){
            wait = 0;
            printf("\n%s[+] CONNECTED:%s %d --> %d\n",GREEN, NORM, c->server_id, c->client_id);
            connections[connected] = c;
            connected++;
        }
        //Send the next data packet for every connection
        for(int i = 0; i < connected; i++){
            if(connections[i]->state != 0 && connections[i]->packet_nb < packet_num){
                send_to_client(connections[i]); //Sends packet and checks for ACK or termination packet
                wait = 0;
            }
        }
        //If there is no activity we wait(no connection requests or connected clients) 
        if (wait == 1){
            if (served >= N) 
                break;
            printf("\nWaiting for requests...\n");
            FD_ZERO(&rfds);
            FD_SET(sockfd, &rfds);
            int ret = select(sockfd + 1, &rfds, NULL, NULL, NULL);
            if (ret == -1)
                perror("Select()");
        }
    }
    //Free memory
    close(sockfd);
    for (int i = 0; i < connected; i++){
        free(connections[i]);
    }
    free(connections);
    for (int i = 0; i < packet_num; i++){
        free(packs[i]);
    }
    free(packs);
}

Packet **create_packets(char *filename, int *data_len, int max_bytes, int *total_packets){
    //Open file and find length of file                                         
    FILE *fd;                                                                   
    if ( (fd = fopen(filename, "rb")) == NULL ){                                
        perror("\nFile error");                                             
        exit(-1);                                                               
    }                                                                           
    fseek(fd, 0, SEEK_END);                                
    *data_len = ftell(fd);                                                      
    rewind(fd);                                                                 
    if(*data_len < 1){                                                          
        printf("\nEmpty data buffer. Cannot create packets \n");                
        exit(-1);                                                               
    }
    //Allocate memory for the packets    
    Packet **packets;                                                           
    if(*data_len < max_bytes){ //If data buffer fits in a single packet          
        *total_packets = 2;                                                     
        if ( (packets = malloc(sizeof(Packet *) * *total_packets)) == NULL){    
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                       
        Packet *p;                                                              
        if ( (p = malloc(sizeof(Packet) + *data_len)) == NULL){                 
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                       
        *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=*data_len};
        fread(p->payload, 1 , *data_len, fd);                                   
        packets[0] = p;                                                         
        //Add final empty packet                                                
        Packet *final;                                                          
        if ( (final = malloc(sizeof(Packet))) == NULL){                         
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                       
        *final = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=0};
        packets[1] = final;                                                     
        fclose(fd);                                                             
        return packets;                                                         
    }                             
    int r = 0; //Remainder
    //If the data fits in packets with even lengths    
    if (*data_len % max_bytes == 0){    
        *total_packets = *data_len / max_bytes;                                 
        if ( (packets = malloc(sizeof(Packet*) * (*total_packets) + 1)) == NULL){ //Allocate memory for all even packets + terminating empty packet
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                       
    }else{                                                                      
        r = *data_len % max_bytes;                                              
        *total_packets = (*data_len - r)/max_bytes;                             
        if ( (packets = malloc(sizeof(Packet*) * (*total_packets + 2))) == NULL){ //Allocate memory for all evenly sized + 1 unevenly sized + 1 empty packet
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                       
    }                                                                           
    for(int i = 0; i < *total_packets; i++){ //Create all the even packets      
        Packet *p;                                                              
        if ( (p = malloc(sizeof(Packet) + max_bytes)) == NULL){                 
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                       
        if(i % 2 == 0){                                                         
            *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=max_bytes};
        }else{                                                                  
            *p = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=max_bytes};
        }                                                                       
        fread(p->payload, 1, max_bytes,fd);                                     
        packets[i] = p;                                                         
    }                                                                           
    if(r != 0){ //If the remainder is not 0, create the last uneven packet      
        Packet *p;                                                              
        if ( (p = malloc(sizeof(Packet) + r)) == NULL){                         
            perror("Malloc error: ");                                           
            exit(-1);                                                           
        }                                                                                                                                               
        if (packets[*total_packets - 1]->pktseq == 0)                           
            *p = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=r};
        else                                                                    
            *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=r};                                                       
        *total_packets += 1;                                                    
        fread(p->payload, 1, r, fd);                                            
        packets[*total_packets-1] = p;                                          
    }                                                                           
    //Add the final empty terminating packet                                    
    Packet *p;                                                                  
    if ( (p = malloc(sizeof(Packet))) == NULL){                                 
        perror("Malloc error: ");                                               
        exit(-1);                                                               
    }                                                                           
    if(packets[*total_packets-1]->pktseq == 0)                                  
        *p = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=0};
    else                                                                        
        *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=0};
    packets[*total_packets] = p;                                                
    *total_packets +=1;                                                         
    fclose(fd);                                 
    return packets;
}
   
void send_to_client(Connection *c){
    //Create the data packet to be sent    
    Packet *data = packs[(int)c->packet_nb];
    data->sender_id = c->server_id;
    data->recv_id = c->client_id;
    char* serial = serialize(data);
    rdp_send_data(&sockfd, &c->client_addr, serial);
    int ACK = rdp_check_ACK(&sockfd, data->recv_id, data->sender_id, data->pktseq); //Checks for ACK or terminate 
    free(serial);
    if (ACK == 0) //ACK recived from client
       c->packet_nb += 1;
    if (ACK == 1){ //Termination packet
        printf("\n%s[-] DISCONNECTED:%s %d --> %d\n", RED,NORM, c->server_id, c->client_id);
        c->state = 0; //Turn off connection
        served++;
    }
}
