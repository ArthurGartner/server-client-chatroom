//Arthur Gartner & Jigyasa Suryawanshi
//CSCI 632 Project 3
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
#include <stdbool.h>

#define SERVER_PORT 6340
#define MAX_LINE 256
#define MAX_PENDING 5
#define MAX_CHATROOMS 5
#define MAX_PER_CHATROOM 5
#define MAXNAME 256
#define TABLE_SIZE 10
#define CHATROOM_REFRESH_TIME 2
#define PACKET_BUFFER_SIZE 5

//Error method to catch errors and close program
void error(const char *msg)
{
	printf("%s", msg);
	printf("Shutting down.");
	exit(1);
};

//Packet structure for all sent and received data
struct packet
{
	short type;
	char uName[MAXNAME];
	char mName[MAXNAME];
	char data[MAXNAME];
	int chatID;
};

//Registration table for server to hold info on connected clients
struct registrationTable
{
	bool occupied;
	int port;
	int sockid;
	char mName[MAXNAME];
	char uName[MAXNAME];
	int chatID;
};

struct clientThread
{
	bool available;
	pthread_t thread;
};

//Thread lock for registration table entries
pthread_mutex_t my_mutex = PTHREAD_MUTEX_INITIALIZER;

//Thread lock packet buffer array
pthread_mutex_t buffer_deload = PTHREAD_MUTEX_INITIALIZER;

//Registration table size depends on number of chatrooms allowed and number of users per chatroom allowed.
struct registrationTable table[MAX_CHATROOMS * MAX_PER_CHATROOM];

//Variable to maintain number of users within a chatroom
int currentUsers[MAX_CHATROOMS];

//Variable to maintain the data packet buffers for each chatroom
struct packet packetBuffer[MAX_CHATROOMS * PACKET_BUFFER_SIZE];

//Thread array to manage recieving messages from multiple clients
struct clientThread clients[MAX_PER_CHATROOM * MAX_CHATROOMS];

//Function that handles receipt of data packet from clients and assigns to appropriate chatid packet buffer
void *messageDelegate(void *socket)
{
	//Initate loca variables
	struct packet packet_chat_rcv, packet_chat_snd;
	int sockID = *((int *) socket);
	
	//Begin loop
	while (1)
	{
		//Wipe memory for packet_chat_rcv memory location
		bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
		
		//Loop for packet receiving from client
		if (recv(sockID, &packet_chat_rcv, sizeof(packet_chat_rcv), 0) > -1)
		{
			printf("Data packet with type %d received from Client: %s User: %s.\n", ntohs(packet_chat_rcv.type), packet_chat_rcv.mName, packet_chat_rcv.uName);
			
			//Setup data to show username
			char str[256];
			strcpy(str, "[");
			strcat(str, packet_chat_rcv.uName);
			strcat(str, "]: ");
			strcat(str, packet_chat_rcv.data); 	
			strcpy(packet_chat_rcv.data, str);
			
			//Lock buffer deload so other threads do not access this buffer while packets are being added
			pthread_mutex_lock(&buffer_deload);
			
			printf("Adding data packet to chatroom %d packet buffer...\n", ntohs(packet_chat_rcv.chatID));
			
			//Add to packet buffer at next available index
			int startIndex = ntohs(packet_chat_rcv.chatID) * PACKET_BUFFER_SIZE;
			for (int i = startIndex; i < startIndex + PACKET_BUFFER_SIZE; i++)
			{
				printf("%d\n", packetBuffer[i].type);
				//Check if empty and if empty then add to buffer
				if (!ntohs(packetBuffer[i].type) > 0)
				{
					packetBuffer[i] = packet_chat_rcv;
					printf("Packet succesfully added to chatroom %d packet buffer at index %d!\n", ntohs(packet_chat_rcv.chatID), i);
					break;
				}
			}
			
			//Unlock packet buffer so deload can occur by multicaster
			pthread_mutex_unlock(&buffer_deload);
			
			packet_chat_snd.type = htons(231);
			
			printf("Sending confirmation of receipt to client...\n");
			
			//Send packet back to client		 
			send(sockID, &packet_chat_snd, sizeof(packet_chat_snd), 0);
			
			printf("Receipt confirmation sent.\n");
			
			//Wipe memory for packet_chat_rcv location
			bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
			
			//Wipe memory for packet_chat_snd location
			bzero(&packet_chat_snd, sizeof(packet_chat_snd));
		}
		
		//Close socket
		//close(newsockfd);
	}
}

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

	int startIndex = ntohs(packet_reg.chatID) * MAX_PER_CHATROOM;
	for (int i = startIndex; i < (startIndex + MAX_PER_CHATROOM); i++)
	{
		if (!table[i].occupied)
		{
			table[i].port = ((struct registrationTable*)clientData)->port;
			table[i].sockid = newsock;
			strcpy(clientData->uName, table[i].uName);
			strcpy(clientData->mName, table[i].mName);	
			table[i].chatID = ntohs(packet_reg.chatID);
			
			//Increment chatroom user count
			currentUsers[ntohs(packet_reg.chatID)]++;
			
			int *socketID = malloc(sizeof(socketID));
			*socketID = newsock;
			
			//Setup thread to receive messages from this client
			for (int j = 0; j < (MAX_PER_CHATROOM * MAX_CHATROOMS); j++)
			{
				if (clients[j].available)
				{
					pthread_create(&clients[j].thread, NULL, messageDelegate, socketID);
					clients[j].available = false;
					printf("Client messageDelegate thread added at index %d.\n", j);
					break;
				}
			}
			
			printf("Client succesfully added to registration table at index %d.\n", i);
			break;
		}
	}
	
	//Unlock to allow other threads access to table
	pthread_mutex_unlock(&my_mutex);
	
	printf("Join handler thread terminating...\n");
	
	//Exit the thread for the join handler. A new joinhandler thread will be created by main when a new client connects.
	pthread_exit(NULL);
}

//Multicaster function that operates continuously in seperate thread (Each chatroom is represented as a multicast)
void *multicaster(void *chatroomID)
{
	//Local variable declarations
	int roomID = *((int *) chatroomID);
	int bufferStartIndex = roomID * PACKET_BUFFER_SIZE;
	int regStartIndex = roomID * MAX_PER_CHATROOM;
	char servername[MAXNAME];
	gethostname(servername, MAXNAME);
	char serveruser[MAXNAME];
	strcpy(serveruser, "SERVER");
	struct packet packet_chat_snd;
	
	printf("Chatroom %d created.\n", roomID);
	
	//This loop runs continuously
	while (1)
	{
		//Only execute if the chatroom is NOT empty
		if (currentUsers[roomID] > 0)
		{
			printf("USER FOUND IN CHATROOM %d\n", roomID);
			//Lock packet buffer array during deload
			pthread_mutex_lock(&buffer_deload);
			
			for (int i = bufferStartIndex; i < (bufferStartIndex + PACKET_BUFFER_SIZE); i++)
			{
				printf("BUFFERINDEX %d\n", i);
				printf("PACKET TYPE %d\n", ntohs(packetBuffer[i].type));
				//Check if buffer index has anything
				if (ntohs(packetBuffer[i].type) > 0)
				{
					//Wipe memory for packet_chat_snd memory location
					bzero(&packet_chat_snd, sizeof(packet_chat_snd));
				
					//Setup send packet
					packet_chat_snd.type = htons(231);
					strcpy(packet_chat_snd.mName, servername);
					strcpy(packet_chat_snd.uName, serveruser);
					strcpy(packet_chat_snd.data, packetBuffer[i].data);
					packet_chat_snd.chatID = packetBuffer[i].chatID;
				
					//Broadcast to all clients within chatroom iteratively
					for (int j = regStartIndex; j < (regStartIndex + MAX_PER_CHATROOM); j++)
					{
						if (table[j].sockid > 0)
						{
						printf("User found at index %d", j);
							send(table[j].sockid, &packet_chat_snd, sizeof(packet_chat_snd), 0);
						}
					}
				
					//Zero out packet buffer index since packet has been distributed to clients
					bzero(&packetBuffer[i], sizeof(packetBuffer[i]));
					
					packetBuffer[i].type = htons(0);
				}
			}
			
			//Unlock packet buffer to allow additional packets to buffer
			pthread_mutex_unlock(&buffer_deload);
		}
		
		//Pauses multicaster broadcast for set time in seconds
		sleep(CHATROOM_REFRESH_TIME);
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
	pthread_t threads[1];
	pthread_t chatrooms[MAX_CHATROOMS];
	
	//Set initial client list availablity to all true
	for (int i = 0; i < (MAX_PER_CHATROOM * MAX_CHATROOMS); i++)
	{
		clients[i].available = true;
	}
	
	//Set initial packet buffer to all NULL
	for (int i = 0; i < (MAX_CHATROOMS * PACKET_BUFFER_SIZE); i++)
	{
		packetBuffer[i].type = 0;
	}
	
	
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
	
	//Initiates multicasters which act as chatrooms. Each multicaster function will run continuously in this new thread.

	for (int i = 0; i < MAX_CHATROOMS; i++)
		{
			//Send data using sockfd value found in table for each client entry
			int *chatroomID = malloc(sizeof(chatroomID));
			*chatroomID = i + 1;
			
			//Create chatroom thread
			pthread_create(&chatrooms[i], NULL, multicaster, chatroomID);
		}
	
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
