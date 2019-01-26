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
#include <dirent.h>

int main(int argc, char **argv) {
	if(argc != 3) {
		fprintf(stderr, "Wrong Arguments.\n");	
		exit(11);
	}

	char *host = argv[1];
	char *port = argv[2];
	
	// Socket
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	// Failed to return a file descriptor.
	if(sock_fd == -1) {
        	perror("socket");
        	exit(5);
	}

	// Client address information
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;	
	hints.ai_socktype = SOCK_STREAM;
	int s = getaddrinfo(host, port, &hints, &result);
	// The given address is invalid.
	if(s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(5);
	}

	// Connect
	if(connect(sock_fd, result->ai_addr, result->ai_addrlen) == -1) {
		perror("connect");
		exit(5);
	}
	
	// Open the current direcotry and find all .txt files.
	DIR *d = opendir(".");
	if(!d) {
		perror("opendir");
		exit(5);
	}
	struct dirent *dir;
	int num_txt_files = 0;
	while((dir = readdir(d)) != NULL) {
		int len_filename = strlen(dir->d_name);
		if(dir->d_type == DT_REG && strncmp(dir->d_name + len_filename - 4, ".txt", 4) == 0) {
			fprintf(stdout, "%d\n", len_filename);
			fprintf(stdout, "%s\n", dir->d_name);
			num_txt_files += 1;
			//char *filename = dir->d_name;
			//write(sock_fd, &filename, strlen(filename)+1);
		}
	}
	closedir(d);
	fprintf(stdout, "%d\n", num_txt_files);
	
	// Open a text file to send to the server.
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

	// Send file size to the server.
	filesize = htonl(filesize);
	write(sock_fd, &filesize, sizeof(filesize));

	// Send content data to the server
	char *tracker = content;
	filesize = ntohl(filesize);
	long sent_bytes = 0;
	fprintf(stdout, "file size : %li\n", filesize);
	while(sent_bytes < filesize) {
		fprintf(stdout, "sent bytes: %li\n", sent_bytes);
		if(filesize - sent_bytes < 1024) {
			if(write(sock_fd, content, filesize - sent_bytes) < 0) {
				perror("write");
				exit(5);
			}
			content += filesize - sent_bytes;
			sent_bytes += filesize - sent_bytes; 
		}
		else {
			if(write(sock_fd, content, 1024) < 0) {
				perror("write");
				exit(5);
			}
			content+=1024;
			sent_bytes += 1024;
		}
	}
	fprintf(stdout, "out of while loop: %li\n", sent_bytes);

	// Clean up.
	free(tracker);
	freeaddrinfo(result);

	return 0;
}
