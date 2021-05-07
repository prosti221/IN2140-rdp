#ifndef SERVER_H                                                                
#define SERVER_H                                                                

#include "../rdp/rdp.h"                                                                
                                                                                
void send_to_client(Connection *c); //Send packet to a connected client and check for ACK or terminate         
                                                                                
Packet **create_packets(char *filename, int *data_len, int max_bytes, int *total_packets); //Read file and store data packets in memory with flags 0x04 and correct pktseq.
                                                                                
#endif       
