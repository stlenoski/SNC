#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <getopt.h>

#include <pthread.h>


#define MAX_BUFFER 1024
#define MIN_PORT 1025
#define MAX_PORT 65535
typedef enum { false, true } bool;

struct arg_struct
{
	int  socket;
};

struct CLI_args {
	bool listen;
	bool isUDP;
	bool hasSource;
	int port;
	char * sourceName;
	struct hostent * st;
	struct hostent * ht;
};

struct arg_struct_UDP {
	int socket;
	struct sockaddr_in  myaddr;
};


void print_error() {
	printf("invalid or missing options\n");
	printf("usage: snc [-l] [-u] [-s source_ip_address] [hostname] port\n");
	fflush(stdout);
}


void print_internal_error() {
		printf("internal error\n");
		fflush(stdout);
		exit(1);
}


struct CLI_args * initCLI_args() {
	struct CLI_args * inputArgs = NULL;
	inputArgs = (struct CLI_args *)malloc(sizeof(struct CLI_args));
	inputArgs->listen = false;
	inputArgs->isUDP = false;
	inputArgs->hasSource = false;
	inputArgs->port = 8080;
	inputArgs->st = NULL;
	inputArgs->sourceName = NULL;
	inputArgs->ht = NULL;
	return inputArgs;
}

struct CLI_args * getCliArgs(int argc,char * argv[]) {
	struct CLI_args * inputArgs;

	int c;
	inputArgs = initCLI_args();


	while((c = getopt(argc,argv,"lus:")) != -1) {
		switch(c)
		{
			case 'l':
				inputArgs->listen = true;
				break;
			case 'u':
				inputArgs->isUDP = true;
				break;
			case 's':
				inputArgs->hasSource = true;
				inputArgs->sourceName = optarg;
				inputArgs->st = gethostbyname(optarg);
				break;
			case ':':
			if(optopt == 's')
				print_error();
				exit(1);
			default:
				print_error();
				exit(1);
		}
	}

	// checks if port number is valid
	if(atoi(argv[argc - 1]) < MIN_PORT || atoi(argv[argc - 1]) > MAX_PORT) {
		print_error();
		exit(1);
	}

	// i feel like this can be better
	if(optind < argc - 1) {
		inputArgs->ht = gethostbyname(argv[optind]);
		inputArgs->port = atoi(argv[optind+1]);
	}
	else if( optind == argc - 1 && inputArgs->listen == false) {
		print_error();
		exit(1);
	}
	else {
		inputArgs->port = atoi(argv[optind]);
	}
	
	return inputArgs;
}

struct sockaddr_in makeSockAddr(short family, unsigned short port, struct in_addr * addr) {
	struct sockaddr_in sockAddr;
	sockAddr.sin_family = family;
	sockAddr.sin_port = port;
	if(addr != NULL)
		sockAddr.sin_addr = *addr;
	else
		sockAddr.sin_addr.s_addr = INADDR_ANY;
	return sockAddr;


}
//create connection
//int create_connection(int net_socket,int port,char * addr, bool sflag,char * adder) {
int create_connection(int net_socket,struct CLI_args * cliArgs) {
    struct sockaddr_in server_address;
	struct hostent * at = NULL;
	struct sockaddr_in newaddr;

	if(cliArgs->ht == NULL)
		server_address = makeSockAddr(AF_INET, htons(cliArgs->port), NULL);
	else
		server_address = makeSockAddr(AF_INET, htons(cliArgs->port), (struct in_addr *)(cliArgs->ht->h_addr_list[0]));

	if(cliArgs->hasSource == true) {
		at = gethostbyname(cliArgs->sourceName);
		newaddr = makeSockAddr(AF_INET, htons(8080), (struct in_addr *)at->h_addr_list[0]);
		if(bind(net_socket,(struct sockaddr *)&newaddr,sizeof(newaddr)) < 0)
			print_internal_error();	
	}
	int status = connect(net_socket,(struct sockaddr *)&server_address,sizeof(server_address));
	if(status == -1)
		print_internal_error();
	return status;
}

//tcp thread recieve
void * get_Recv(void * arg) {
	while(true) {
		char server_response[MAX_BUFFER];
		struct arg_struct * my_arg;
		my_arg = (struct arg_struct *)arg;

		if(recv(my_arg->socket,server_response,MAX_BUFFER,0) <= 0)
			exit(0);
		printf("%s", server_response);
		fflush(stdout);
		memset(server_response,0,MAX_BUFFER);
	}
	return NULL;
}

//tcp thread send
void * get_Sent(void * arg) {
	while(true) {
		char client_message[MAX_BUFFER];
		struct arg_struct * my_arg;
		my_arg = (struct arg_struct *)arg;
		if(fgets(client_message,MAX_BUFFER,stdin) == NULL)
			exit(0);

		if(send(my_arg->socket,client_message,strlen(client_message),0) <= 0)
			break;
		memset(client_message,0,MAX_BUFFER);
	}
	
	return NULL;
}

//udp send
void * get_Sent_U(void * arg) {
	while(true) {
		char client_message[MAX_BUFFER];
		struct arg_struct_UDP * my_arg;
		my_arg = (struct arg_struct_UDP *)arg;
		if(fgets(client_message, MAX_BUFFER, stdin) == NULL)
			break;
		if(sendto(my_arg->socket,client_message,strlen(client_message), 0, (struct sockaddr *)&my_arg->myaddr, sizeof(my_arg->myaddr)) < 0)
			break;
		memset(client_message,0,MAX_BUFFER);
	}
	exit(0);
	return NULL;
}

//udp recv
void * get_Recv_U(void * arg) {
	while(true) {
		char server_response[MAX_BUFFER];
		struct arg_struct_UDP * my_arg;
		socklen_t addrlen = sizeof(my_arg->myaddr);
		my_arg = (struct arg_struct_UDP *)arg;
		if(recvfrom(my_arg->socket,server_response,MAX_BUFFER,0,(struct sockaddr *)&my_arg->myaddr,&addrlen) <= 0)
			break;
		printf("%s", server_response);
		fflush(stdout);
		memset(server_response,0,MAX_BUFFER);
	}
	exit(0);
	return NULL;
}


void tcpClient(struct CLI_args * cliArgs) {
	int net_socket;
	pthread_t stdo;
	pthread_t stdi;
	int err1;
	int err2;
	int status;
	struct arg_struct * args;
	args = (struct arg_struct *)malloc(sizeof(struct arg_struct));
	
	net_socket = socket(AF_INET,SOCK_STREAM,0);
	if(net_socket == -1) 
		print_internal_error();
	
	if(cliArgs->ht != NULL)
		status = create_connection(net_socket, cliArgs);
	else
		status = create_connection(net_socket, cliArgs);

	//for recieving
	args->socket = net_socket;

	int errSent = pthread_create(&stdi,NULL,get_Sent,(void *)args);
	int errRev = pthread_create(&stdo,NULL,get_Recv,(void *)args);
	
	if(errRev != 0 || errSent != 0)
		print_internal_error();

	// join the threads
	err2 = pthread_join(stdi,(void*)args);
	err1 = pthread_join(stdo,(void*)args);
	
	//check if there is an error with join
	if(err2 != 0 || err1 != 0)
		print_internal_error();
	
	close(net_socket);
}


void tcpServer(struct CLI_args * cliArgs) {
	int ser_socket;
	int accepted_socket;
	pthread_t stdo;
	pthread_t stdi;
	int err1;
	int err2;
	struct arg_struct * args;
	struct sockaddr_in server_address;
	args = (struct arg_struct *)malloc(sizeof(struct arg_struct));

	ser_socket = socket(AF_INET,SOCK_STREAM,0);

	if(ser_socket == -1)
		print_internal_error();

	
	if(cliArgs->ht != NULL)
		server_address = makeSockAddr(AF_INET,htons(cliArgs->port), (struct in_addr *)cliArgs->ht->h_addr_list[0]); // buz = (struct in_addr *)ht->h_addr_list[0];
	else
		server_address = makeSockAddr(AF_INET,htons(cliArgs->port), NULL);

	bind(ser_socket,(struct sockaddr *)&server_address,sizeof(server_address));
	listen(ser_socket,5);

	accepted_socket = accept(ser_socket,NULL,NULL);
	args->socket = accepted_socket;
	int errSent = pthread_create(&stdi,NULL,get_Sent,(void *)args);
	int errRevs = pthread_create(&stdo,NULL,get_Recv,(void *)args);
	
	if(errRevs != 0 || errSent != -)
		print_internal_error();
	
	err2 = pthread_join(stdi,(void*)args);
	err1 = pthread_join(stdo,(void*)args);
	if(err2 != 0 || err1 != 0)
		print_internal_error();

	close(ser_socket);

}


void udpClient(struct CLI_args * cliArgs) {
	int net_socket;
	pthread_t stdo;
	pthread_t stdi;
	int err1;
	int err2;
	struct arg_struct_UDP * arg_udp;
	struct sockaddr_in server_address;

	arg_udp = (struct arg_struct_UDP *)malloc(sizeof(struct arg_struct_UDP));
	net_socket = socket(AF_INET, SOCK_DGRAM, 0);
		
	if(net_socket == -1)
		print_internal_error();

	
	if(cliArgs->ht == NULL)
		server_address = makeSockAddr(AF_INET, htons(cliArgs->port), NULL);
	else
		server_address = makeSockAddr(AF_INET, htons(cliArgs->port), (struct in_addr *)cliArgs->ht->h_addr_list[0]);

	//to connect to another location 
	if(cliArgs->hasSource == true) {
		struct sockaddr_in newaddr;
		newaddr = makeSockAddr(AF_INET, htons(8080), ((struct in_addr *)cliArgs->st->h_addr_list[0]));
		if(bind(net_socket,(struct sockaddr *)&newaddr,sizeof(newaddr)) < 0)
			print_internal_error();
	}

	char client_message[MAX_BUFFER];
	if(fgets(client_message, MAX_BUFFER, stdin) == NULL)
		exit(0);

	if(sendto(net_socket,client_message,strlen(client_message), 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
		print_internal_error();	
	memset(client_message,0,MAX_BUFFER);

	//create threads
	arg_udp->socket = net_socket;
	arg_udp->myaddr = server_address;
	int udpSend = pthread_create(&stdo,NULL,get_Sent_U,(void *)arg_udp);
	int udpRecv = pthread_create(&stdi,NULL, get_Recv_U,(void *)arg_udp);
	if(udpSend != 0 || udpRecv != 0) {
		print_internal_error();		
	}

	err2 = pthread_join(stdo,(void*)arg_udp);
	err1 = pthread_join(stdi,(void*)arg_udp);
	if(err2 != 0 || err1 != 0) {
		print_internal_error();	
	}
}

void udpServer(struct CLI_args * cliArgs) {
	int ser_socket;
	pthread_t stdo;
	pthread_t stdi;
	struct sockaddr_in client_addr;
	int get_length;
	int err1;
	int err2;
	struct arg_struct_UDP * arg_udp;
	char sendUDP[MAX_BUFFER];
	struct sockaddr_in server_address;
	socklen_t addrlen = sizeof(client_addr);
	
	arg_udp = (struct arg_struct_UDP *)malloc(sizeof(struct arg_struct_UDP));
	ser_socket = socket(AF_INET,SOCK_DGRAM,0);
	if(ser_socket == -1)
		print_internal_error();
	
	if(cliArgs->ht == NULL)
		server_address = makeSockAddr(AF_INET, htons(cliArgs->port), NULL);
	else
		server_address = makeSockAddr(AF_INET, htons(cliArgs->port), (struct in_addr *)cliArgs->ht->h_addr_list[0]);

	if(bind(ser_socket,(struct sockaddr *)&server_address,sizeof(server_address))< 0)
		print_internal_error();

	 get_length = recvfrom(ser_socket,sendUDP,MAX_BUFFER,0,(struct sockaddr *)&client_addr,&addrlen);
	 printf("%s",sendUDP);
	 memset(sendUDP,0,MAX_BUFFER);
	 arg_udp->socket = ser_socket;
	 arg_udp->myaddr = client_addr;
	 int udpSendS = pthread_create(&stdo,NULL,get_Sent_U,(void *)arg_udp);
	 int udpRecvS = pthread_create(&stdi,NULL,get_Recv_U,(void *)arg_udp);
	 

	if(udpSendS != 0 || udpRecvS != 0)
		print_internal_error();
	
	err2 = pthread_join(stdo,(void*)arg_udp);
	err1 = pthread_join(stdi,(void*)arg_udp);
	if(err2 != 0 || err1 != 0)
		print_internal_error();
}


void snc(struct CLI_args * cliArgs) {
	//tcp client
	if(cliArgs->listen == false && cliArgs->isUDP == false) {
		tcpClient(cliArgs);
	}
	//tcp server
	else if(cliArgs->listen == true && (cliArgs->hasSource == false && cliArgs->isUDP == false)) {
		tcpServer(cliArgs);
	}
	// error, should never happend
	else if(cliArgs->listen == true && cliArgs->hasSource == true) {
		print_error();
		exit(1);
	}
	// udp client
	else if(cliArgs->listen == false && cliArgs->isUDP == true) {
		udpClient(cliArgs);
	}
	//udp server
	else if(cliArgs->listen == true && cliArgs->isUDP == true) {
		udpServer(cliArgs);
	}

}


int main(int argc, char *argv[]) {
	struct CLI_args * cliArgs;
	cliArgs = getCliArgs(argc, argv);
	snc(cliArgs);
}