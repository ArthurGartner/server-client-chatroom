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

//Text buffer to hold status text
char statusBuffer [1024];

//Error method to catch errors and close program
void error(const char *msg)
{
	printf("[SERVER-MAIN] ERROR: %s\n", msg);
};
//Function to deal with fata errors
void fatalError(const char *msg)
{
	printf("[SERVER-MAIN] FATAL ERROR: %s\n", msg);
	printf("[SERVER-MAIN] Shutting down...\n");
	exit(1);
};

//Function to display system messages
void systemStatus(const char *msg)
{
	printf("[SERVER-MAIN] %s\n", msg);
};

void joinStatus(const char *msg)
{
	printf("[SERVER-JOINHANDLER] %s\n", msg);
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
	int threadIndex;
	char ip[MAXNAME];
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

//Thread lock new user array
pthread_mutex_t new_user = PTHREAD_MUTEX_INITIALIZER;

//Thread lock for clien disconnect
pthread_mutex_t client_disconnect = PTHREAD_MUTEX_INITIALIZER;

//Registration table size depends on number of chatrooms allowed and number of users per chatroom allowed.
struct registrationTable table[MAX_CHATROOMS * MAX_PER_CHATROOM];

//Variable to maintain number of users within a chatroom
int currentUsers[MAX_CHATROOMS];

//Array to hold entries for new users that join chatroom
int newUser[MAX_CHATROOMS * MAX_PER_CHATROOM];

//Array for client disconnectd, handles the queue
int disconnect[MAX_CHATROOMS * MAX_PER_CHATROOM];

//Variable to maintain the data packet buffers for each chatroom
struct packet packetBuffer[MAX_CHATROOMS * PACKET_BUFFER_SIZE];

//Thread array to manage recieving messages from multiple clients
struct clientThread clients[MAX_PER_CHATROOM * MAX_CHATROOMS];

//Function that handles receipt of data packet from clients and assigns to appropriate chatid packet buffer
void *messageDelegate(void *regIndexPassed)
{
	//Initate local variables
	struct packet packet_chat_rcv, packet_chat_snd;
	int regIndex = *((int *) regIndexPassed);
	int sockID = table[regIndex].sockid;
	
	//Begin loop
	while (1)
	{
		//Wipe memory for packet_chat_rcv memory location
		bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
		
		//Loop for packet receiving from client
		int value = recv(sockID, &packet_chat_rcv, sizeof(packet_chat_rcv), 0);
		if (value > 0)
		{
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Data packet with type %d received.\n",packet_chat_rcv.mName, packet_chat_rcv.uName, ntohs(packet_chat_rcv.type));
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Locking packet buffer table...\n",packet_chat_rcv.mName, packet_chat_rcv.uName);
			//Lock buffer deload so other threads do not access this buffer while packets are being added
			pthread_mutex_lock(&buffer_deload);
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Packet buffer table locked.\n",packet_chat_rcv.mName, packet_chat_rcv.uName);
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Adding packet to chatroom %d packet buffer table...\n",packet_chat_rcv.mName, packet_chat_rcv.uName, ntohs(packet_chat_rcv.chatID));
			
			//Add to packet buffer at next available index
			int startIndex = ntohs(packet_chat_rcv.chatID) * PACKET_BUFFER_SIZE;
			for (int i = startIndex; i < startIndex + PACKET_BUFFER_SIZE; i++)
			{
				//Check if empty and if empty then add to buffer
				if (!ntohs(packetBuffer[i].type) > 0)
				{
					packetBuffer[i] = packet_chat_rcv;
					printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Packet added to chatroom %d packet buffer table at index %d.\n",packet_chat_rcv.mName, packet_chat_rcv.uName, ntohs(packet_chat_rcv.chatID), i);
					break;
				}
			}
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Unlocking packet buffer table...\n",packet_chat_rcv.mName, packet_chat_rcv.uName);
			//Unlock packet buffer so deload can occur by multicaster
			pthread_mutex_unlock(&buffer_deload);
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Packet buffer table unlocked.\n",packet_chat_rcv.mName, packet_chat_rcv.uName);
			
			//packet_chat_snd.type = htons(231);
			
			//printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Sending confirmation packet to client...\n",packet_chat_rcv.mName, packet_chat_rcv.uName);
			
			//Send packet back to client		 
			//send(sockID, &packet_chat_snd, sizeof(packet_chat_snd), 0);
			
			//printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Confirmation packet sent to client.\n",packet_chat_rcv.mName, packet_chat_rcv.uName);
			
			//Wipe memory for packet_chat_rcv location
			bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
			
			//Wipe memory for packet_chat_snd location
			bzero(&packet_chat_snd, sizeof(packet_chat_snd));
		}
		//Handle client disconnect
		else if (value == 0)
		{
			//Queue for disconnect by multicaster
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Socket disconnected.\n",table[regIndex].mName, table[regIndex].uName);
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Queuing for client information deletion...\n",table[regIndex].mName, table[regIndex].uName);
			//Add client reg index to disconnect array
			int startIndexNew = table[regIndex].chatID * MAX_PER_CHATROOM;
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Locking disconnect table...\n",table[regIndex].mName, table[regIndex].uName);
			//Lock new user array during write
			pthread_mutex_lock(&client_disconnect);
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Disconnect table locked.\n",table[regIndex].mName, table[regIndex].uName);
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Searching for available disconnect queue index...\n",table[regIndex].mName, table[regIndex].uName);
			for (int k = startIndexNew; k < (startIndexNew + MAX_PER_CHATROOM); k++)
			{
				if (disconnect[k] < 0)
				{
					disconnect[k] = regIndex;
					printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Client information queued for deletion on deletion array at index %d...\n",table[regIndex].mName, table[regIndex].uName, k);
					break;
				}
			}
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Unlocking disconnect table...\n",table[regIndex].mName, table[regIndex].uName);
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Disconnect table unlocked.\n",table[regIndex].mName, table[regIndex].uName);
			//Unlock disconnect array
			pthread_mutex_unlock(&client_disconnect);
			
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Closing socket...\n",table[regIndex].mName, table[regIndex].uName);
			close(sockID);
			printf("[SERVER-MESSAGEDELEGATE(%s | %s)] Socket closed. Thread terminating...\n",table[regIndex].mName, table[regIndex].uName);
			
			pthread_exit(NULL);
		}
	}
}

//Function for handling all join requests for potential connected clients. Executes within seperate thread then terminates thread.
void *join_handler(void *recievedClientData)
{
	joinStatus("Join handler initiated.");
	//Local variables
	int newsock;
	struct packet packet_reg, packet_conf, packet_chat_snd;
	struct registrationTable *clientData = (struct registrationTable*) recievedClientData;
	//Get the socket file descriptor which allows for continued data transfer between client and server even though the thread has changed and the newsockfd variable in main will
	//be overwritten with any new connections.
	newsock = clientData->sockid;
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Attempting to join client(%s:%d | %s) user(%s)...", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Awaiting registration packet (2/3) from client(%s:%d | %s) user(%s)...", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	//Receive the 2/3 packet from the client
	if(recv(newsock, &packet_reg, sizeof(packet_reg), 0) < 0)
	{
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Error with registration packet (2/3) from client(%s:%d | %s) user(%s)!", clientData->ip, clientData->port, clientData->mName, clientData->uName);
		fatalError(statusBuffer);
	}
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Registration packet (2/3) recieved from client(%s:%d | %s) user(%s).", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Awaiting registration packet (3/3) from client(%s:%d | %s) user(%s)...", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	//Receive the 3/3 packet from the client which completes the handshake between server and client
	if(recv(newsock, &packet_reg, sizeof(packet_reg), 0) < 0)
	{
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Error with registration packet (3/3) from client(%s:%d | %s) user(%s)!", clientData->ip, clientData->port, clientData->mName, clientData->uName);
		fatalError(statusBuffer);
	}
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Registration packet (3/3) recieved from client(%s:%d | %s) user(%s).", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	//Assign type to confirmation packet
	packet_conf.type = htons(221);
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Sending confirmation packet with type %d to client(%s:%d | %s) user(%s).", ntohs(packet_conf.type), clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	//Send confirmation packet to client
	send(newsock, &packet_conf, sizeof(packet_conf), 0);
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Confirmation packet sent to client(%s:%d | %s) user(%s).", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Adding client(%s:%d | %s) user(%s) to registration table...", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);
	
	joinStatus("Locking registration table for write access...");

	//Set lock to ensure other threads do not access table
	pthread_mutex_lock(&my_mutex);
	
	joinStatus("Registration table locked for write access.");
	//Add approved client to registration table so that multicaster can pull client info (socket id) to send broadcast to

	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Finding empty index in registration table for client(%s:%d | %s) user(%s)...", clientData->ip, clientData->port, clientData->mName, clientData->uName);
	joinStatus(statusBuffer);

	int startIndex = ntohs(packet_reg.chatID) * MAX_PER_CHATROOM;
	for (int i = startIndex; i < (startIndex + MAX_PER_CHATROOM); i++)
	{
		if (!table[i].occupied)
		{
			table[i].port = ((struct registrationTable*)clientData)->port;
			table[i].sockid = newsock;
			strcpy(table[i].uName, clientData->uName);
			strcpy(table[i].mName, clientData->mName);	
			strcpy(table[i].ip, clientData->ip);
			table[i].chatID = ntohs(packet_reg.chatID);
			table[i].occupied = true;
			
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Client(%s:%d | %s) user(%s) information saved in registration table index %d.", clientData->ip, clientData->port, table[i].mName, table[i].uName, i);
			joinStatus(statusBuffer);
			
			//Increment chatroom user count
			currentUsers[ntohs(packet_reg.chatID)]++;
			
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Chatroom %d now has %d users.", ntohs(packet_reg.chatID), currentUsers[ntohs(packet_reg.chatID)]);
			joinStatus(statusBuffer);
			
			int *regIndex = malloc(sizeof(regIndex));
			*regIndex = i;
			
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Finding empty index in client thread array for client(%s:%d | %s) user(%s)...", clientData->ip, clientData->port, table[i].mName, table[i].uName);
			joinStatus(statusBuffer);
			
			//Add client reg index to new user array to notify chatroom of new user
			int startIndexNew = ntohs(packet_reg.chatID) * MAX_PER_CHATROOM;
			
			//Lock new user array during write
			pthread_mutex_lock(&new_user);
			
			for (int k = startIndexNew; k < (startIndexNew + MAX_PER_CHATROOM); k++)
			{
				if (newUser[k] < 0)
				{
					newUser[k] = i;
					break;
				}
			}
			
			//Unlock new user array
			pthread_mutex_unlock(&new_user);
			
			//Setup thread to receive messages from this client
			for (int j = 0; j < (MAX_PER_CHATROOM * MAX_CHATROOMS); j++)
			{
				if (clients[j].available)
				{
					//Wipe memory for packet_chat_snd memory location
					bzero(&packet_chat_snd, sizeof(packet_chat_snd));
				
					//Setup welcome packet contents
					packet_chat_snd.type = htons(231);
					char welcomeStr[MAXNAME];
					if (currentUsers[ntohs(packet_reg.chatID)] == 2)
					{
						sprintf(welcomeStr, "You have joined chatroom %d. There is %d other user in this chatroom.", table[i].chatID, (currentUsers[ntohs(packet_reg.chatID)] - 1));
					}
					else
					{
						sprintf(welcomeStr, "You have joined chatroom %d. There are %d other users in this chatroom.", table[i].chatID, (currentUsers[ntohs(packet_reg.chatID)] - 1));
					}

					strcpy(packet_chat_snd.data, welcomeStr);
					
					//Send welcome packet to client
					send(newsock, &packet_chat_snd, sizeof(packet_chat_snd), 0);
					
					//Track index of thread to make available when client leaves
					table[i].threadIndex = j;
				
					pthread_create(&clients[j].thread, NULL, messageDelegate, regIndex);
					clients[j].available = false;
					
					bzero(&statusBuffer, sizeof(statusBuffer));
					sprintf(statusBuffer, "Client thread created for client(%s:%d | %s) user(%s) in client thread array index %d.", clientData->ip, clientData->port, table[i].mName, table[i].uName, j);
					joinStatus(statusBuffer);
					break;
				}
			}
			
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Client(%s:%d | %s) user(%s) successfully added to registration table and client thread created.", clientData->ip, clientData->port, table[i].mName, table[i].uName);
			joinStatus(statusBuffer);
			break;
		}
	}
	
	joinStatus("Unlocking registration table.");
	//Unlock to allow other threads access to table
	pthread_mutex_unlock(&my_mutex);
	
	joinStatus("Registration table unlocked.");
	
	joinStatus("Terminating...");
	
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
	struct packet packet_chat_snd;
	
	printf("[SERVER-CHATROOM%d] Thread initalized.\n", roomID);
	
	//This loop runs continuously
	while (1)
	{
		//Only execute if the chatroom is NOT empty
		if (currentUsers[roomID] > 0)
		{
			//Check client deletion queue for disconnected clients
			int startIndexNew = roomID * MAX_PER_CHATROOM;
			
			pthread_mutex_lock(&client_disconnect);
			
			for (int m = startIndexNew; m < (startIndexNew + MAX_PER_CHATROOM); m++)
			{
				if (disconnect[m] > -1)
				{	
					printf("[SERVER-CHATROOM%d] Disconnect request for client(%s:%d | %s) user(%s) found at index %d in disconnect array.\n", roomID, table[disconnect[m]].ip, table[disconnect[m]].port, table[disconnect[m]].mName, table[disconnect[m]].uName, m);
					
					bzero(&packet_chat_snd, sizeof(packet_chat_snd));
					
					printf("[SERVER-CHATROOM%d] Creating disconnect notification packet of type 445...\n", roomID);
					packet_chat_snd.type = htons(445);
					strcpy(packet_chat_snd.mName, "CHAT_SERVER");
					strcpy(packet_chat_snd.uName, "NOTIFICATION");
					strcpy(packet_chat_snd.data, table[disconnect[m]].uName);
					packet_chat_snd.chatID = htons(roomID);
					printf("[SERVER-CHATROOM%d] Disconnect notification packed created.\n", roomID);
					
					printf("[SERVER-CHATROOM%d] Sending disconnect notification packet to connected clients...\n", roomID);
					//Broadcast to all clients within chatroom iteratively
					for (int v = regStartIndex; v < (regStartIndex + MAX_PER_CHATROOM); v++)
					{
						if (table[v].sockid > 0)
						{
							send(table[v].sockid, &packet_chat_snd, sizeof(packet_chat_snd), 0);
						}
					}
					
					printf("[SERVER-CHATROOM%d] Disconnect notification packet sent.\n", roomID);
					
					printf("[SERVER-CHATROOM%d] Making client thread index available at index %d...\n", roomID, table[disconnect[m]].threadIndex);
					
					clients[table[disconnect[m]].threadIndex].available = true;
					
					printf("[SERVER-CHATROOM%d] Removing client(%s:%d | %s) user(%s) registration found at index %d in registration table...\n", roomID, table[disconnect[m]].ip, table[disconnect[m]].port, table[disconnect[m]].mName, table[disconnect[m]].uName, disconnect[m]);
					
					bzero(&table[disconnect[m]], sizeof(table[disconnect[m]]));
					
					printf("[SERVER-CHATROOM%d] Registration table cleared at registration table index %d...\n", roomID, disconnect[m]);
					
					disconnect[m] = -1;
					
					currentUsers[roomID]--;
				}
			}
			
			pthread_mutex_unlock(&client_disconnect);
		
			//Check for new users and announce	
			//Lock new user array during write
			pthread_mutex_lock(&new_user);
			
			for (int k = startIndexNew; k < (startIndexNew + MAX_PER_CHATROOM); k++)
			{
				if (newUser[k] > -1)
				{
					printf("[SERVER-CHATROOM%d] New client(%s:%d | %s) user(%s) found.\n", roomID, table[newUser[k]].ip, table[newUser[k]].port, table[newUser[k]].mName, table[newUser[k]].uName);
					bzero(&packet_chat_snd, sizeof(packet_chat_snd));
					
					printf("[SERVER-CHATROOM%d] Creating join notification packet of type 444...\n", roomID);
					packet_chat_snd.type = htons(444);
					strcpy(packet_chat_snd.mName, "CHAT_SERVER");
					strcpy(packet_chat_snd.uName, "NOTIFICATION");
					strcpy(packet_chat_snd.data, table[newUser[k]].uName);
					packet_chat_snd.chatID = htons(roomID);
					printf("[SERVER-CHATROOM%d] Join notification packed created.\n", roomID);
					
					printf("[SERVER-CHATROOM%d] Sending join notification packet to connected clients...\n", roomID);
					//Broadcast to all clients within chatroom iteratively
					for (int j = regStartIndex; j < (regStartIndex + MAX_PER_CHATROOM); j++)
					{
						if (table[j].sockid > 0)
						{
							send(table[j].sockid, &packet_chat_snd, sizeof(packet_chat_snd), 0);
						}
					}
					
					printf("[SERVER-CHATROOM%d] Join notification packet sent.\n", roomID);
				
					newUser[k] = -1;
				}
			}
			
			//Unlock new user array
			pthread_mutex_unlock(&new_user);
		
		
			//Lock packet buffer array during deload
			pthread_mutex_lock(&buffer_deload);
			
			for (int i = bufferStartIndex; i < (bufferStartIndex + PACKET_BUFFER_SIZE); i++)
			{
				//Check if buffer index has anything
				if (ntohs(packetBuffer[i].type) > 0)
				{
					printf("[SERVER-CHATROOM%d] Buffered packet found at index %d.\n", roomID, i);
					//Wipe memory for packet_chat_snd memory location
					bzero(&packet_chat_snd, sizeof(packet_chat_snd));
				
					//Setup send packet
					packet_chat_snd.type = htons(231);
					strcpy(packet_chat_snd.mName, packetBuffer[i].mName);
					strcpy(packet_chat_snd.uName, packetBuffer[i].uName);
					strcpy(packet_chat_snd.data, packetBuffer[i].data);
					packet_chat_snd.chatID = packetBuffer[i].chatID;
				
					if (currentUsers[roomID] == 1)
					{
						printf("[SERVER-CHATROOM%d] Sending buffered packet to %d connected client...\n", roomID, currentUsers[roomID]);
					}
					else
					{
						printf("[SERVER-CHATROOM%d] Sending buffered packet to %d connected clients...\n", roomID, currentUsers[roomID]);
					}
				

					//Broadcast to all clients within chatroom iteratively
					for (int j = regStartIndex; j < (regStartIndex + MAX_PER_CHATROOM); j++)
					{
						if (table[j].sockid > 0)
						{
							send(table[j].sockid, &packet_chat_snd, sizeof(packet_chat_snd), 0);
						}
					}
					
					printf("[SERVER-CHATROOM%d] Buffered packet sent.\n", roomID);
				
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
	systemStatus("Initializing...");
	
	//Initialize variables 
	int sockfd, newsockfd, clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct registrationTable client_info;
	struct packet packet_reg;
	pthread_t threads[1];
	pthread_t chatrooms[MAX_CHATROOMS];
	
	systemStatus("Local variables set.");
	
	//Set initial client list availablity to all true
	for (int i = 0; i < (MAX_PER_CHATROOM * MAX_CHATROOMS); i++)
	{
		clients[i].available = true;
		newUser[i] = -1;
		disconnect[i] = -1;
	}
	
	systemStatus("Client array availability set to true.");
	
	//Set initial packet buffer types to all 0
	for (int i = 0; i < (MAX_CHATROOMS * PACKET_BUFFER_SIZE); i++)
	{
		packetBuffer[i].type = 0;
	}
	
	systemStatus("Packet buffer types set to 0.");
	
	systemStatus("Creating socket...");
	
	//Socket creation using Address Family AF_INET, TCP SOCK_STREAM, and IP
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		fatalError("Error opening socket!\n");
	}
	
	systemStatus("Socket created.");
	
	systemStatus("Setting network properties...");
	
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
	
	systemStatus("Server network properties set.");
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Server address: %s:%d.", inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
	systemStatus(statusBuffer);
	
	systemStatus("Binding socket...");
	
	//Attempt to bind socket
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		fatalError("Binding failure!");
	}
	
	systemStatus("Socket succesfully binded.");
	
	//Set socket to listen mode, accept up to the MAX_PENDING constant value
	listen(sockfd, MAX_PENDING);
	
	//Get memory length of cli_addr structure
	clilen = sizeof(cli_addr);
	
	//Initiates multicasters which act as chatrooms. Each multicaster function will run continuously in this new thread.
	
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "Initializing chatroom threads with refresh rates of %d seconds...", CHATROOM_REFRESH_TIME);

	systemStatus(statusBuffer);	
	
	for (int i = 0; i < MAX_CHATROOMS; i++)
		{
			//Send data using sockfd value found in table for each client entry
			int *chatroomID = malloc(sizeof(chatroomID));
			*chatroomID = i;
			
			//Create chatroom thread
			pthread_create(&chatrooms[i], NULL, multicaster, chatroomID);
			
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Chatroom %d initialized!", i);
			
			systemStatus(statusBuffer);
		}
		
	bzero(&statusBuffer, sizeof(statusBuffer));
	sprintf(statusBuffer, "All chatrooms (%d/%d) initialized!", MAX_CHATROOMS, MAX_CHATROOMS);
	systemStatus(statusBuffer);	
	
	//Loop that runs continuously. Allows for multiple clients to join 
	while(1)
	{
		systemStatus("Listening for client connections...");
		
		//Zero out
		bzero(&client_info, sizeof(client_info));
		bzero(&newsockfd, sizeof(newsockfd));
	
		//Check for acceptance and assign to new socket
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		
		if (newsockfd < 0)
		{
			fatalError("Error on socket accept!");
		}
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Client(%s:%d) connected!", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		systemStatus(statusBuffer);
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Awaiting client(%s:%d) registration packet (1/3)...", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		systemStatus(statusBuffer);
	
		//Wait to receive registration packet from connected client
		if (recv(newsockfd, &packet_reg, sizeof(packet_reg), 0) < 0)
		{
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Error on client(%s:%d) registration packet (1/3) receive.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
			fatalError(statusBuffer);
		}
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Packet recieved from client(%s:%d) with type %d.", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), ntohs(packet_reg.type));
		systemStatus(statusBuffer);
	
		//Ensure that packet received has correct type
		if (ntohs(packet_reg.type) != 121)
		{
			bzero(&statusBuffer, sizeof(statusBuffer));
			sprintf(statusBuffer, "Incorrect packet type for registration request sent from client(%s:%d).", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
			fatalError(statusBuffer);
		}
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Registration packet (1/3) recieved from client(%s:%d).", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		systemStatus(statusBuffer);
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Client(%s:%d) paired with hostname (%s) user(%s).", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), packet_reg.mName, packet_reg.uName);
		systemStatus(statusBuffer);
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Caching client(%s:%d | %s) user(%s) information...", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),packet_reg.mName, packet_reg.uName);
		systemStatus(statusBuffer);
	
		//Save client info
		client_info.port = ntohs(cli_addr.sin_port);
		strcpy(client_info.ip, inet_ntoa(cli_addr.sin_addr));
		client_info.sockid = newsockfd;
		strcpy(client_info.uName, packet_reg.uName);
		strcpy(client_info.mName, packet_reg.mName);
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Client(%s:%d | %s) user(%s) information cached.", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), packet_reg.mName, packet_reg.uName);
		systemStatus(statusBuffer);
		
		bzero(&statusBuffer, sizeof(statusBuffer));
		sprintf(statusBuffer, "Initiating join handler thread for client(%s:%d | %s) user(%s)...", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), packet_reg.mName, packet_reg.uName);
		systemStatus(statusBuffer);

		//A new thread is created for the join_handler which accepts the client info just assigned by main. Join handler will wait for 2 additional registration packets from
		//the client and then add the client to the registration table. This is important because the uniqiue sockfd value for this specific client must be saved somewhere because
		//it will be overwritten within the main function while loop when a new client connects.
		pthread_create(&threads[0], NULL, join_handler, &client_info);
		
		//Wait for thread to complete. Main loop does not continue until the join handler terminates.
		pthread_join(threads[0], NULL);
		
		systemStatus("Join handler thread terminated.");
	}
}
