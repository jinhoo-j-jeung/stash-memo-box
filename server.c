#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>

int main(int argc, char **argv) {	
	char *port = argv[1];

	// Socket
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	// Failed to return a file descriptor.
	if(server_fd == -1) {
        	perror("socket");
        	exit(5);
	}

	// Server address information
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	int s = getaddrinfo(NULL, port, &hints, &result);
	// The given address is invalid.
	if(s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(5);
	}

	// Make the port instantly reusable.
	int optval = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	// Bind
	if(bind(server_fd, result->ai_addr, result->ai_addrlen)) {
		perror("bind");
		exit(5);
	}

	// Listen
	if(listen(server_fd, 1)) {
		perror("listen");
		exit(5);
	}

	// Accept
	struct sockaddr_storage clientaddr;
	socklen_t clientaddrsize = sizeof(clientaddr);
	memset(&clientaddr, 0, sizeof(struct sockaddr));
	int client_fd = accept(server_fd, (struct sockaddr *) &clientaddr, &clientaddrsize);
	// Failed to connect.
	if(client_fd == -1) {
		perror("accept");
		exit(5);		
	}

	// Client address information
	char client_host[256], client_port[256];
	getnameinfo((struct sockaddr *) &clientaddr, clientaddrsize, client_host, sizeof(client_host), client_port, sizeof(client_port), NI_NUMERICHOST | NI_NUMERICSERV);
	fprintf(stdout, "connected to %s:%s.\n", client_host, client_port);
	
	// Create a new directory with the client's address
	struct stat st;
	// If the directory does not exist already, make a new directory.
	if(stat(client_host, &st) == -1) {
		if(mkdir(client_host, 0700) == -1) {
			perror("mkdir");
			exit(5);
		}
		chdir(client_host);
	}
	// If exists, Check if the directory is empty.
	else {
		chdir(client_host);
		DIR *d = opendir(".");
		struct dirent *dir;
		int count = 0;	
		while((dir = readdir(d)) != NULL) {
			count++;
			if(count > 2) break;
		}
		// If the directory is not empty, exit.
		if(count > 2) {
			fprintf(stderr, "The target directory is not empty.\n");
			exit(3);
		}

	}
	
	// Retreive a filename from the client.
/*
	char file_title[1024];
	int file_title_status = read(client_fd, file_title, 1024);
	fprintf(stdout, "title: %d\n", strlen(ntohl(file_title)));
*/

	// Open a file with the filename.
	//If it does not exist, create a file with the name.
	FILE *fp = fopen("1.txt", "a");

	// Receive file size from the client.
	long filesize = 0;
	int status = read(client_fd, &filesize, sizeof(filesize));
	if(status < 0) {
   		perror("read");
		exit(5);
	}
	fprintf(stdout, "%d\n", ntohl(filesize));
	filesize = ntohl(filesize);

	// Receive content data from the client.
	char *buffer = malloc(filesize+1);
	buffer[filesize] = '\0';
	char *tracker = buffer;
	ssize_t remaining_bytes = (ssize_t) filesize;
	ssize_t read_bytes = 0;
	ssize_t received_bytes;
	while(remaining_bytes > 0) {
		received_bytes = read(client_fd, buffer, remaining_bytes);
		fprintf(stdout, "received bytes: %zu\n", received_bytes);
		fprintf(stdout, "remaining bytes: %zu\n", remaining_bytes);
		fprintf(stdout, "read bytes: %zu\n", read_bytes);
		if(received_bytes > 0) {
			read_bytes += received_bytes;
			remaining_bytes -= received_bytes;
			fprintf(fp, "%s", buffer);
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

	// Clean up.
	fclose(fp);
	//free(tracker);
	return 0;
}
