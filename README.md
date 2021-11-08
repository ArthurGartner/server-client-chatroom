# CSCI632_Project3
<h2>Summary</h2>
This chat program demonstrates a client-server relationship using basic IP sockets. Once instantiated the server application is capable of connecting to multiple clients. The server consists of seperate chatrooms which clients connect to. Clients can only connect to one chatroom at a time and any messages sent within a chatroom can only be seen by other clients in the same chatroom. This program handles client disconnects and allows those clients to reconnect. The program runs indefinitely until a fatal error occurs or the user terminates the program.
<h2>How to use</h2>
<h3>Server.c</h3>
<h4>Code</h4>
This program consists of multiple constants that can be changed by the user before the program is executed.
SERVER_PORT #### - This is the port number. The port number must be the same port number in Client.c. Default value is 6341.
MAX_PENDING # - This is the number of clients than can be pending for the socket accept(). Default value is 5.
MAX_CHATROOMS # - This is the number of available chatrooms that are running within the server. Default value is 5.
MAX_PER_CHATROOM # - This is maximum number of users allowed in each chatroom. Default value is 5.
MAXNAME 256 - This is the value in bytes of the allowed size for structure components within a data packet. Default value is 256.
CHATROOM_REFRESH_TIME # - This is the number of seconds that a chatroom will pause before checking the packet buffer and distributing the pending packets. Once distributed the packet is removed from the buffer. Default value is 2.
PACKET_BUFFER_SIZE # - This is the number of packets that are allowed to be buffered, or pending, for each chatroom. Each buffer is emptied each time a chatroom refreshes. Default value is 5.
<h4>Executing</h4>
./Server
(No parameters are needed to execute Server.c.)
After execution Server.c will run indefinitely until a fatal error occurs or the user terminates the program.
<h3>Client.c</h3>
