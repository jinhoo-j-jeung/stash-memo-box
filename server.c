#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
	
	char *port = argv[1];

	int s;
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	s = getaddrinfo(NULL, port, &hints, &result);
	// The given address is invalid.
	if(s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(5);
	}

	if(bind(server_fd, result->ai_addr, result->ai_addrlen)) {
		perror("bind");
		exit(5);
	}

	if(listen(server_fd, 1)) {
		perror("listen:");
		exit(5);
	}
/*
	struct sockaddr_storage clientaddr;
	socklen_t clientaddrsize = sizeof(clientaddr);
	int client_id = accept(passive_socket, (struct sockaddr *) &clientaddr, &clientaddrsize);
*/
	int client_fd = accept(server_fd, NULL, NULL);
    	printf("Connection made: client_fd=%d\n", client_fd);
	
	return 0;
}
