//Arthur Gartner & Jigyasa Suryawanshi
//CSCI 632 Project 3
//Client.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define SERVER_PORT 6341
#define MAXNAME 256

//Error method to catch errors and close program
void error(const char *msg)
{
	printf("[CLIENT] %s\n", msg);
	printf("[CLIENT] Shutting down...\n");
	exit(1);
}

//Packet structure for all sent a received data
struct packet
{
	short type;
	char uName[MAXNAME];
	char mName[MAXNAME];
	char data[MAXNAME];
	int chatID;
};


//Thread to recieve messages
void *receiveMessage(void *socket)
{
	int sockID = *((int *) socket);
	struct packet packet_chat_rcv;
	
	
	//Wipe memory of packer_char_rcv memory locations
	bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
	
	//Loop that runs to continuously receive message
	while (1)
	{
		if (recv(sockID, &packet_chat_rcv, sizeof(packet_chat_rcv), 0) > -1)
		{
			if (ntohs(packet_chat_rcv.type) == 444)
			{
				//Print data from received packet
				printf("[CHATROOM%d][%s]: %s has joined the chatroom.\n", ntohs(packet_chat_rcv.chatID), packet_chat_rcv.uName, packet_chat_rcv.data);
			}
			else if (ntohs(packet_chat_rcv.type) == 445)
			{
				//Print data from received packet
				printf("[CHATROOM%d][%s]: %s has left the chatroom.\n", ntohs(packet_chat_rcv.chatID), packet_chat_rcv.uName, packet_chat_rcv.data);
			}
			else
			{
				//Print data from received packet
				printf("[CHATROOM%d][%s]: %s", ntohs(packet_chat_rcv.chatID), packet_chat_rcv.uName, packet_chat_rcv.data);
			}
			
			//Wipe memory of packet_chat_rcv memory locations
			bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
		}
		
		sleep(1);
	}
}

//Main function
int main (int argc, char* argv[])
{	
	//Initialize variables
	struct hostent *server;
	struct sockaddr_in serv_addr;
	struct packet packet_reg, packet_conf, packet_chat_rcv, packet_chat_snd;
	pthread_t readMessage;
	int sockfd, len;
	
	//Argument check
	if (argc < 4)
	{
		error("Argument missing.\n");
	}
	
	//Create socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		error("Error opening socket.\n");
	}
	
	//Get host name from argument
	server = gethostbyname(argv[1]);
	
	if (server == NULL)
	{
		error("No host found.\n");
	}
	
	//Setup server to connect to info
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(SERVER_PORT);
	
	printf("[CLIENT] Attempting connection to server %s:%d...\n", argv[1], ntohs(serv_addr.sin_port));
	
	//Attempt connection to server
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		error("Connection Failed.\n");
	}
	
	printf("[CLIENT] Connection to server %s:%d successful.\n", argv[1], ntohs(serv_addr.sin_port));
	
	//Setup client info
	packet_reg.type = htons(121);
	strcpy(packet_reg.uName, argv[2]);
	packet_reg.chatID = htons(atoi(argv[3]));
	char clientname[MAXNAME];
	gethostname(clientname, MAXNAME);
	strcpy(packet_reg.mName, clientname);
	printf("[CLIENT] Sending 1st registration packet with type %d...\n", ntohs(packet_reg.type));
	
	//Send registration packet
	send(sockfd, &packet_reg, sizeof(packet_reg), 0);
	
	printf("[CLIENT] Registration packet (1/3) sent.\n");
	
	//Send registration packet
	send(sockfd, &packet_reg, sizeof(packet_reg), 0);
	
	printf("[CLIENT] Registration packet (2/3) sent.\n");
	
	//Send registration packet
	send(sockfd, &packet_reg, sizeof(packet_reg), 0);
	
	printf("[CLIENT] Registration packet (3/3) sent.\n");
	printf("[CLIENT] Awaiting response from server...\n");
	
	//Receive confirmation packet
	recv(sockfd, &packet_conf, sizeof(packet_conf), 0);
	
	printf("[CLIENT] Packet received from server with type %d.\n", ntohs(packet_conf.type));
	
	
	//Handler for full chatroom response from server.
	if (ntohs(packet_conf.type) == 501)
	{
		printf("[CHAT-SERVER] Chatroom%d is full!\n", atoi(argv[3]));
		error("Connected chatroom was full.");

	}
	else if (ntohs(packet_conf.type) == 502)
	{
		printf("[CHAT-SERVER] Chatroom ID %d does not exist!\n", atoi(argv[3]));
		error("ChatroomID does not exist!");

	}
	else if (ntohs(packet_conf.type) != 221)
	{
		error("Confirmation packet not received.\n");	
	}
	
	//Wait for notification packet from connected chatroom
	if (recv(sockfd, &packet_chat_rcv, sizeof(packet_chat_rcv), 0) > -1)
	{
		//Print data from received packet
		printf("[CHATROOM%d][NOTIFICATION] %s\n", ntohs(packet_chat_rcv.chatID), packet_chat_rcv.data);
			
		//Wipe memory of packet_chat_rcv memory locations
		bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
	}
	
	int *socketID = malloc(sizeof(sockfd));
	*socketID = sockfd;
	
	//Create a thread to be constantly listening for data packets sent from the connected server
	pthread_create(&readMessage, NULL, receiveMessage, socketID);
	
	//Send & receive loop
	while (1)
	{
		//Clear packet_chat_snd memory location
		bzero(&packet_chat_snd, sizeof(packet_chat_snd));
		
		//Setup data packet
		packet_chat_snd.type = htons(131);
		strcpy(packet_chat_snd.mName, clientname);
		strcpy(packet_chat_snd.uName, argv[2]);
		packet_chat_snd.chatID = htons(atoi(argv[3]));
		
		//Add user input to data
		fgets(packet_chat_snd.data, MAXNAME, stdin);
		
		//Send packet
		send(sockfd, &packet_chat_snd, sizeof(packet_chat_snd), 0);
	}
}
