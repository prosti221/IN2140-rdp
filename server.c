#include <stdio.h>
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
#define MAX_PACKET_BYTES 1000
#define TIMEOUT 1000 //in microseconds 

#define NORM "\x1B[0m"
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"

//Global variables
Packet **packs;
Connection **connections;
int connected = 0;
int served = 0;
int sockfd;

char *file_buffer(char *filename, int *datalen){    //Creates a buffer for file data and stores it in memory
    FILE *fd;
    if ( ( fd = fopen(filename, "rb") ) == NULL ){
        printf("\nFile error: \n");
        exit(-1);
    }
    fseek(fd, 0, SEEK_END); //Go to end of file
    *datalen = ftell(fd);   //get the size of buffer
    rewind(fd);             //rewind
    char *data_buffer;
    if ( (data_buffer = (char*)malloc(*datalen)) == NULL){
        perror("malloc error: ");
        exit(-1);
    }
    fread(data_buffer, *datalen, 1, fd);
    fclose(fd);
    return data_buffer;
}

Packet **create_packets(char* data_bfr, int data_len, int max_bytes, int *total_packets){
    if(data_len < 1){                                                           
        printf("\nEmpty data buffer. Cannot create packets \n");                
        exit(-1);                                                               
    }                                                                           
    Packet **packets;                                                           
    if(data_len < max_bytes){ //If data buffer fits in a single packet          
        *total_packets = 2;                                                     
        if ( (packets = malloc(sizeof(Packet *) * *total_packets)) == NULL){
            perror("Malloc error: ");
            exit(-1); 
        }            
        Packet *p;
        if ( (p = malloc(sizeof(Packet) + data_len)) == NULL){
            perror("Malloc error: ");
            exit(-1);
        }            
        *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=data_len};
        memcpy(p->payload, data_bfr , data_len);                                
        packets[0] = p;                                                         
        //Add final empty packet                                                
        Packet *final;
        if ( (final = malloc(sizeof(Packet))) == NULL){
            perror("Malloc error: ");
            exit(-1);
        } 
        *final = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=0};
        packets[1] = final;                                                     
        return packets;                                                         
    }                                                                           
    int r = 0; //Remainder                                                      
    if (data_len % max_bytes == 0){ //If the data buffer can be evenly split    
        *total_packets = data_len / max_bytes;                                  
        if ( (packets = malloc(sizeof(Packet*) * (*total_packets) + 1)) == NULL){ //Allocate memory for all even packets + terminating empty packet
            perror("Malloc error: ");
            exit(-1);
        }
    }else{                                                                      
        r = data_len % max_bytes;                                               
        *total_packets = (data_len - r)/max_bytes;                              
        if ( (packets = malloc(sizeof(Packet*) * (*total_packets + 2))) == NULL){ //Total packets = all evenly sized + 1 unevenly sized + 1 empty packet
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
        memcpy(p->payload, data_bfr + (i * max_bytes) , max_bytes);             
        packets[i] = p;                                                         
    }                                                                           
    if(r != 0){ //If the remainder is not 0, create the last uneven packet      
        *total_packets += 1;                                                    
        Packet *p;
        if ( (p = malloc(sizeof(Packet) + r)) == NULL){
            perror("Malloc error: ");
            exit(-1);
        } 
        *p = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=r};
        memcpy(p->payload, data_bfr + (data_len - r), r);                       
        packets[*total_packets-1] = p;                                          
    }                                                                           
    //Add the final empty terminating packet                                    
    Packet *p;
    if ( (p = malloc(sizeof(Packet))) == NULL){
        perror("Malloc error: ");
        exit(-1);
    } 
    if(*total_packets + 1 % 2 == 0)                                             
        *p = (Packet){.flags=0x04, .pktseq=1, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=0};
    else                                                                        
        *p = (Packet){.flags=0x04, .pktseq=0, .ackseq=0, .unassigned=0, .sender_id=0, .recv_id=0, .metadata=0};
    packets[*total_packets] = p;                                                
    *total_packets +=1;                                                         
    return packets;                                                             
}                                                 


void send_to_client(Connection *c){
   Packet *data = packs[(int)c->packet_nb]; //Data to be sent to connected client
   data->sender_id = c->server_id;
   data->recv_id = c->client_id;
   char* serial = serialize(data);
   //print_packet(data);
   send_data(&sockfd, &c->client_addr, serial);
   int ACK = wait_ACK(&sockfd, serial,data->metadata, data->recv_id, data->sender_id, data->pktseq, &c->client_addr); //Waits until it recinves either ACK or terminat 
   free(serial);
   if (ACK == 0) //ACK recived from client
       c->packet_nb += 1;
   if (ACK == 1){ //Termination packet from client. When a client is disconnected, state is set to 0.
       printf("\n%s[-] DISCONNECTED:%s %d --> %d\n", RED,NORM, c->server_id, c->client_id);
       c->state = 0; //Turn off connection
       served++;
    }
}

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
    unsigned PORT = atoi(argv[1]);
    struct sockaddr_in server_addr;
    
    if ( (connections = malloc(sizeof(Connection*) * MAX_CONNECTIONS)) == NULL){
        perror("Malloc error: ");
        exit(-1);
    }
    //Storing file in databuffer and allocating memory to it
    int datalen = 0;
    char *data_buffer = file_buffer(argv[2], &datalen);
    //Number of packets needed is datalen/MAX_PACKET_BYTES
    int packet_num = datalen/MAX_PACKET_BYTES; 
    packs = create_packets(data_buffer, datalen, MAX_PACKET_BYTES, &packet_num);
    //N is number of files to transfer before terminating
    int N = atoi(argv[3]);
    //Setting loss probability
    set_loss_probability(prob);
    //setting up the socket
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
   
    while(1){
    Connection *c;
        int wait = 1;
        if (served < N && (c = rdp_accept(&sockfd, connections, &connected, &N)) != NULL ){ //Listen for incoming connections
            printf("\n%s[+] CONNECTED:%s %d --> %d\n",GREEN, NORM, c->server_id, c->client_id);
            connections[connected] = c; //Add connection to current connections 
            connected++;
            wait = 0;
        }
        for(int i = 0; i < connected; i++){ //For every ongoing connecions
            if(connections[i]->state != 0 && connections[i]->packet_nb < packet_num){ //if there are still packets to send and if connection is not terminated
               wait = 0;
               send_to_client(connections[i]); //Send packet and wait for ACK or terminate notice
            }
        }
        if (wait){ //While there is nothing to do
            if (served >= N) //If we have served N files, then end. 
                break;
            sleep(1);
            printf("\nWaiting for requests... \n");
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
    free(data_buffer);
}

