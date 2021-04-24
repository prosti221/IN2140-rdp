TO DO:
    Make Disconnect client on empty data packet function
    Figure out how to terminate server on N-th bein served. If N-th file is being served, stop accepting connections.
    Once Client has all packets and disconnects write to file.
    Do not wait for ACK but resend the same packet if not recived, then client resends ACK if packet number is the same as the last one.

