//Arthur Gartner & Jigyasa Suryawanshi
//CSCI 632 Project 2
//Client.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_PORT 6328
#define MAX_LINE 256
#define MAXNAME 256

//Error method to catch errors and close program
void error(const char *msg)
{
	printf("%s", msg);
	printf("Shutting down.\n");
	exit(1);
}

//Packet structure for all sent a received data
struct packet
{
	short type;
	char uName[MAXNAME];
	char mName[MAXNAME];
	char data[MAXNAME];
	short seqNumber;
};

//Main function
int main (int argc, char* argv[])
{	
	//Initialize variables
	struct hostent *server;
	struct sockaddr_in serv_addr;
	struct packet packet_reg, packet_conf, packet_chat_rcv, packet_chat_snd;
	char buffer[MAX_LINE];
	int sockfd, len;
	
	//Argument check
	if (argc < 3)
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
	
	printf("Attempting connection to server %s:%d...\n", argv[1], ntohs(serv_addr.sin_port));
	
	//Attempt connection to server
	if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		error("Connection Failed.\n");
	}
	
	printf("Connection successful.");
	
	//Setup client info
	packet_reg.type = htons(121);
	strcpy(packet_reg.uName, argv[2]);
	char clientname[MAXNAME];
	gethostname(clientname, MAXNAME);
	strcpy(packet_reg.mName, clientname);
	printf("Sending 1st registration packet with type %d...\n", ntohs(packet_reg.type));
	
	//Send registration packet
	send(sockfd, &packet_reg, sizeof(packet_reg), 0);
	
	printf("Registration packet (1/3) sent.\n");
	
	//Send registration packet
	send(sockfd, &packet_reg, sizeof(packet_reg), 0);
	
	printf("Registration packet (2/3) sent.\n");
	
	//Send registration packet
	send(sockfd, &packet_reg, sizeof(packet_reg), 0);
	
	printf("Registration packet (3/3) sent.\n");
	printf("Awaiting response from server...\n");
	
	//Receive confirmation packet
	recv(sockfd, &packet_conf, sizeof(packet_conf), 0);
	
	printf("Packet received from server with type %d.\n", ntohs(packet_conf.type));
	
	
	//Ensure packet type is correct
	if (ntohs(packet_conf.type) != 221)
	{
		printf("%d", packet_conf.type);
		error("Confirmation packet not received.\n");
	}
	
	printf("Acknowledgement received from server. Tuning in...\n");
	
	//Send & receive loop
	while (1)
	{
		//Wipe memory of packer_char_rcv memory locations
		bzero(&packet_chat_rcv, sizeof(packet_chat_rcv));
	
		//Receive packet
		recv(sockfd, &packet_chat_rcv, sizeof(packet_chat_rcv), 0);
		
		//Print received packet info
		printf("Packet with type %d received from server. Data received:\n", ntohs(packet_chat_rcv.type));
	
		//Print data from received packet
		printf("Sequence(%d): %s\n",packet_chat_rcv.seqNumber, packet_chat_rcv.data);
	}
}
