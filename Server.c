//Arthur Gartner & Jigyasa Suryawanshi
//CSCI 632 Project 2
//Server.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 6328
#define MAX_LINE 256
#define MAX_PENDING 5
#define MAXNAME 256
#define TABLE_SIZE 10

//Error method to catch errors and close program
void error(const char *msg)
{
	printf("%s", msg);
	printf("Shutting down.");
	exit(1);
}

//Packet structure for all sent and received data
struct packet
{
	short type;
	char uName[MAXNAME];
	char mName[MAXNAME];
	char data[MAXNAME];
	short seqNumber;
};

//Registration table for server to hold info on connected clients
struct registrationTable
{
	int port;
	int sockid;
	char mName[MAXNAME];
	char uName[MAXNAME];
};

pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;
int indexCount;
struct registrationTable table[10];

//Function for handling all join requests for potential connected clients. Executes within seperate thread then terminates thread.
void *join_handler(void *recievedClientData)
{
	//Local variables
	int newsock;
	struct packet packet_reg, packet_conf;
	struct registrationTable *clientData = (struct registrationTable*) recievedClientData;
	//Get the socket file descriptor which allows for continued data transfer between client and server even though the thread has changed and the newsockfd variable in main will
	//be overwritten with any new connections.
	newsock = clientData->sockid;
	
	printf("Join handler initiatiated. Waiting for registration packets...\n");
	
	//Receive the 2/3 packet from the client
	if(recv(newsock, &packet_reg, sizeof(packet_reg), 0) < 0)
	{
		error("Error with second registration packet.\n");
	}
	
	printf("Registration packet (2/3) received. Waiting for 3rd registration packet...\n");
	
	//Receive the 3/3 packet from the client which completes the handshake between server and client
	if(recv(newsock, &packet_reg, sizeof(packet_reg), 0) < 0)
	{
		error("Error with third registration packet.\n");
	}
	
	printf("Registration packet (3/3) received.\n");
	
	//Assign type to confirmation packet
	packet_conf.type = htons(221);
	
	printf("Confirming connection to Client %s on Port %d for User %s\n", packet_reg.mName, clientData->port, packet_reg.uName);
	printf("Sending acknowledgement packet with type %d...\n", ntohs(packet_conf.type));
	
	//Send confirmation packet to client
	send(newsock, &packet_conf, sizeof(packet_conf), 0);
	
	printf("Acknowledgement packet sent.\n");
	
	printf("Adding client to registration table...\n");
	
	//Set lock to ensure other threads do not access table
	pthread_mutex_lock(&my_mutex);
	
	//Add approved client to registration table so that multicaster can pull client info (socket id) to send broadcast to
	table[indexCount].port = ((struct registrationTable*)clientData)->port;
	table[indexCount].sockid = newsock;
	strcpy(clientData->uName, table[indexCount].uName);
	strcpy(clientData->mName, table[indexCount].mName);	
	
	//Unlock to allow other threads access to table
	pthread_mutex_unlock(&my_mutex);
	
	//Increment index counter to keep track of available indexes in the registration table
	indexCount++;
	
	printf("Client added to table. Join handler thread terminating.\n");
	
	//Exit the thread for the join handler. A new joinhandler thread will be created by main when a new client connects.
	pthread_exit(NULL);
}

//Multicaster function that operates continuously in seperate thread
void *multicaster()
{
	//Local variable declarations
	char *filename;
	char text[100];
	int fd, nread;
	short seqNum = 0;
	struct packet packet_chat_snd;
	
	//Setup text file location. Text file generated with lorem ipsum text
	filename = "input.txt";
	
	//Open the file for reading
	fd = open(filename, O_RDONLY, 0);
	
	//This loop runs continuously
	while (1)
	{
		//Clear packet_chat_snd memory
		bzero(&packet_chat_snd, sizeof(packet_chat_snd));
		
		//Activate lock which ensures other threads do not access the registration table until unlocked
		pthread_mutex_lock(&my_mutex);
		
		//Only executes when atleast 1 client has been added to the registration table
		if (indexCount > 0)
		{
			//Read 100bytes of data from the text file and store in text variable
			nread = read(fd, text, 100);
			
			//Set packet type
			packet_chat_snd.type = htons(231);
			
			//Set sqeNum which is incremented each time a broadcast is made
			packet_chat_snd.seqNumber = seqNum;
			
			//Copy the text to the data portion of the packet to be sent
			strcpy(packet_chat_snd.data, text);
			
			//Increase sequence number since 100bytes have been read from the file
			seqNum++;
			
			//Iterate through clients within the registration table and send the data packet using the socketfd for each client found in the registration table
			for (int i = 0; i < TABLE_SIZE; i++)
			{
				//Send data using sockfd value found in table for each client entry
				send(table[i].sockid, &packet_chat_snd, sizeof(packet_chat_snd), 0);
			}
		}
		
		//Unlock allowing other threads to access the registration table
		pthread_mutex_unlock(&my_mutex);
		
		//Pause this thread for 5 seconds otherwise this function would essentially print the entire text document at once. 
		sleep(5);
	}
}

//Main funtion
int main (int argc, char* argv[])
{
	printf("Server initalizing...\n");
	
	//Initialize variables 
	int sockfd, newsockfd, clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct registrationTable client_info;
	struct packet packet_reg;
	pthread_t threads[2];
	indexCount = 0;
	
	printf("Creating socket...\n");
	
	//Socket creation using Address Family AF_INET, TCP SOCK_STREAM, and IP
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		error("Error opening socket.\n");
	}
	
	printf("Socket created.\n");
	printf("Setting up server network properties...\n");
	
	//Wipe memory addresses
	bzero((char *) &serv_addr, sizeof(serv_addr));
	
	//Setup server properties, INADDR_ANY used to accept connection on local host.
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(SERVER_PORT);
	char servername[MAXNAME];
	gethostname(servername, MAXNAME);
	char serveruser[MAXNAME];
	strcpy(serveruser, "SERVER");
	
	printf("Server network properties set. Server address set to: %s:%d.\n", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
	printf("Binding socket...\n");
	
	//Attempt to bind socket
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		error("Binding failure.\n");
	}
	
	printf("Socket binded.\n");
	
	//Set socket to listen mode, accept up to the MAX_PENDING constant value
	listen(sockfd, MAX_PENDING);
	
	//Get memory length of cli_addr structure
	clilen = sizeof(cli_addr);
	
	indexCount = 0;
	
	//Assigns the multicaster function to a thread. The multicaster function will run continuously in this new thread
	pthread_create(&threads[1], NULL, multicaster, NULL);
	
	//Loop that runs continuously. Allows for multiple clients to join 
	while(1)
	{
		printf("Listening for connections...\n");
		
		//Zero out
		bzero(&client_info, sizeof(client_info));
		bzero(&newsockfd, sizeof(newsockfd));
	
		//Check for acceptance and assign to new socket
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		
		if (newsockfd < 0)
		{
			error("Error on accept.\n");
		}

		printf("Connection from client %s:%d accepted.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		printf("Awaiting client registration packet...\n");
	
		//Wait to receive registration packet from connected client
		if (recv(newsockfd, &packet_reg, sizeof(packet_reg), 0) < 0)
		{
			error("Error on registration receive.\n");
		}
	
		printf("Packet received from client with type %d.\n", ntohs(packet_reg.type));
	
		//Ensure that packet received has correct type
		if (ntohs(packet_reg.type) != 121)
		{
			error("Incorrect packet type for registration request sent from client.\n");
		}
		
		printf("Received registration packet (1/3) from Client: %s on Port %d for User %s\n", packet_reg.mName, ntohs(cli_addr.sin_port), packet_reg.uName);
		
		printf("Saving client information...\n");
	
		//Save client info
		client_info.port = ntohs(cli_addr.sin_port);
		client_info.sockid = newsockfd;
		
		printf("Creating join handler thread...\n");

		//A new thread is created for the join_handler which accepts the client info just assigned by main. Join handler will wait for 2 additional registration packets from
		//the client and then add the client to the registration table. This is important becuase the uniqiue sockfd value for this specific client must be saved somewhere because
		//it will be overwritten within the main function while loop when a new client connects.
		pthread_create(&threads[0], NULL, join_handler, &client_info);
		
		//Wait for thread to complete. Main loop does not continue until the join handler terminates.
		pthread_join(threads[0], NULL);
	}
}
