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
	// Failed to return a file descriptor.
	if(server_fd == -1) {
        	perror("socket");
        	exit(5);
	}

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

	// Make the port instantly reusable.
	int optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	if(bind(server_fd, result->ai_addr, result->ai_addrlen)) {
		perror("bind");
		exit(5);
	}

	if(listen(server_fd, 1)) {
		perror("listen");
		exit(5);
	}

	struct sockaddr_storage clientaddr;
	socklen_t clientaddrsize = sizeof(clientaddr);
	memset(&clientaddr, 0, sizeof(struct sockaddr));

	int client_fd = accept(server_fd, (struct sockaddr *) &clientaddr, &clientaddrsize);
	// Failed to connect.
	if(client_fd == -1) {
		perror("accept");
		exit(5);		
	}

	char client_host[256], client_port[256];
	getnameinfo((struct sockaddr *) &clientaddr, clientaddrsize, client_host, sizeof(client_host), client_port, sizeof(client_port), NI_NUMERICHOST | NI_NUMERICSERV);
	fprintf(stdout, "connected to %s:%s.\n", client_host, client_port);

	char buffer[1024];
	while(read(client_fd, buffer, sizeof(buffer)-1)) {
		fprintf(stdout, "%s\n", buffer);
		fflush(stdout);
	}

	return 0;
}
