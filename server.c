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

	// Receive file size from the client.
	long filesize = 0;
	int status = read(client_fd, &filesize, sizeof(filesize));
	if(status < 0) {
   		perror("read");
		exit(5);
	}
	fprintf(stdout, "%d\n", ntohl(filesize));
	filesize = ntohl(filesize);
	char *buffer = malloc(filesize+1);
	buffer[filesize] = '\0';
	char *tracker = buffer;
	fprintf(stdout, "debug1\n");
	ssize_t remaining_bytes = (ssize_t) filesize;
	fprintf(stdout, "debug2\n");
	ssize_t read_bytes = 0;

	ssize_t received_bytes;
	fprintf(stdout, "debug3\n");
	while(remaining_bytes > 0) {
		received_bytes = read(client_fd, buffer, remaining_bytes);
		fprintf(stdout, "received bytes: %zu\n", received_bytes);
		fprintf(stdout, "remaining bytes: %zu\n", remaining_bytes);
		fprintf(stdout, "read bytes: %zu\n", read_bytes);
		if(received_bytes > 0) {
			read_bytes += received_bytes;
			fprintf(stdout, "read bytes: %zu\n", read_bytes);
			remaining_bytes -= received_bytes;
			fprintf(stdout, "%s", buffer);
			buffer += read_bytes;
			
		}
		else if(received_bytes == 0) {
			break;
		}
		else {
			perror("read");
			exit(5);
		}
	}	

	free(tracker);
//	fflush(stdout);
	return 0;
}
