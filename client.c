#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv) {
	if(argc != 3) {
		fprintf(stderr, "Wrong Arguments.\n");	
		exit(11);
	}

	char *host = argv[1];
	char *port = argv[2];
	
	int s;
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	// Failed to return a file descriptor.
	if(sock_fd == -1) {
        	perror("socket");
        	exit(5);
	}

	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;	
	hints.ai_socktype = SOCK_STREAM;

	s = getaddrinfo(host, port, &hints, &result);
	// The given address is invalid.
	if(s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(5);
	}

	// Connect to the given address.
	if(connect(sock_fd, result->ai_addr, result->ai_addrlen) == -1) {
		perror("connect");
		exit(5);
	}
	
	FILE *fp = fopen("test.txt", "r");
	char *content;
	fseek(fp, 0, SEEK_END);
	long filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	content = malloc(filesize+1);
	content[filesize] = '\0';
	fread(content, filesize, 1, fp);
	fclose(fp);

	fprintf(stdout, "%s\n", content);
	//free(content);
	
	char *tracker = content;
	long sent_bytes = 0;
	while(sent_bytes < filesize) {
		if(filesize - sent_bytes < 256) {
			write(sock_fd, content, filesize - sent_bytes);
			content += filesize - sent_bytes;
			sent_bytes += filesize - sent_bytes; 
		}
		else {
			write(sock_fd, content, 256);
			content+=256;
			sent_bytes += 256;
		}
	}

	// Clean up.
	free(tracker);
	freeaddrinfo(result);

	return 0;
}
