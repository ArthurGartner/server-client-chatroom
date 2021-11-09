# CSCI632_Project3
<h2>Summary</h2>
This chat program demonstrates a client-server relationship using basic IP sockets. Once instantiated the server application is capable of connecting to multiple clients. The server consists of seperate chatrooms which clients connect to. Clients can only connect to one chatroom at a time and any messages sent within a chatroom can only be seen by other clients in the same chatroom. This program handles client disconnects and allows those clients to reconnect. The program runs indefinitely until a fatal error occurs or the user terminates the program.
<h2>How to use</h2>
<h3>Server.c</h3>
<h4>Code</h4>
Server.c consists of multiple constants that can be changed by the user before the program is executed.
<ul>
<li>SERVER_PORT #### - This is the port number. This value must be the same value in Client.c. Default value is 6341.</li>
<li>MAX_PENDING # - This is the number of clients than can be pending for the socket accept(). Default value is 5.</li>
<li>MAX_CHATROOMS # - This is the number of available chatrooms that are running within the server. Default value is 5.</li>
<li>MAX_PER_CHATROOM # - This is maximum number of users allowed in each chatroom. Default value is 5.</li>
<li>MAXNAME ### - This is the value in bytes of the allowed size for structure components within a data packet. This value must be the same value in Client.c. Default value is 256.</li>
<li>CHATROOM_REFRESH_TIME # - This is the number of seconds that a chatroom will pause before checking the packet buffer and distributing the pending packets. Once distributed the packet is removed from the buffer. Default value is 2.</li>
<li>PACKET_BUFFER_SIZE # - This is the number of packets that are allowed to be buffered, or pending, for each chatroom. Each buffer is emptied each time a chatroom refreshes. Default value is 5.</li>
 </ul>
<h4>Executing</h4>
./Server<br>
(No parameters are needed to execute Server.c.)<br>
After execution Server.c will run indefinitely until a fatal error occurs or the user terminates the program.
<h3>Client.c</h3>
<h4>Code</h4>
Client.c consists of only two constants that can be changed.
<ul>
<li>SERVER_PORT #### - This is the port number. This value must be the same value in Server.c. Default value is 6341.</li>
<li>MAXNAME ### - This is the value in bytes of the allowed size for structure components within a data packet. This value must be the same value in Server.c. Default value is 256.</li>
 </ul>
 <h4>Executing</h4>
 ./Client [IP_ADDRESS] [USERNAME] [CHATROOM_ID]<br>
 <ul>
 <li>[IP_ADDRESS] - IP address for Server.c. Example: 127.0.0.1</li>
 <li>[USERNAME] - This is the username that will be used to identify the user within the chatroom. Example: John</li>
 <li>[CHATROOM_ID] - This is the ID of the chatroom which is an integer. Chatroom ID's start at 0 and increment by one. If 5 chatrooms are allowed in Server.c then the available chatroom IDs would be 0,1,2,3 and 4. Example: 1</li>
</ul>
<h2>How it works</h2>
Once executed the Server.c program begins going through the main function. The main function creates a seperate thread for each chatroom and runs the multicaster function in each one of the threads. A socket is created and set to listen to connecting clients. Once a client requests connection the socket accepts the connection and waits for a registration packet (1/3) from the client. The client sends the registration packet (1/3) to the server. Upon receipt of this registration packet (1/3) the Server creates a new thread to execute the join_handler function. The client information is then sent to the join_handler function and awaits for registration packet (2/3) and registration packet (3/3) from the client which the client sends. Upon receipt of the final registration packet (3/3) a check is done to ensure that the requested chat ID is valid. If not a disconnect packet is sent to the client. If the chat ID does exists then a check to see if the chatroom is full is done. If the chatroom is full then a disconnect packet is sent to the client.<br><br>After the checks are made the join_handler then adds the clien information to the registration table and assigns a new thread to the client which executes the messageDelegate function. This function runs in a loop waiting to receive data froma client. After assigning the client to a thread the join_handler thread terminates and the main loop continues. When a client sends a data packet the respective client thread running the messageDelegate function adds the datapacket to a data buffer for the respective chatroom. At each refresh that the multicaster function exhibits for each chatroom the respective data packet buffer is emptied and the data is broadcast to each client.<br><br>Each client instance runs a seperate thread to receive data packets. This allows the client to listen for packets while also waiting for the user to input some text to send. When a client disconnects the associated messageDelegate function puts this update into an deletion queue and then terminates the thread. At next refresh the chatroom then removes the client information from the registration table and client thread array freeing up a spot for another potential client.
 
  
 
 
 
