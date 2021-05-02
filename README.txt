RUN:
    ./server <port> <filename> <number of files> <loss probability>
    ./client <ip> <port> <loss probability>

NOTE:
    I decided to set the maximum amount of connections to N, 
    since i could not think of a scenario where the amount of connections would be greater than the amount of files being transmitted before the server terminates.
    
    All the client output files get stored in /out and will get deleted when calling "make clean"    
