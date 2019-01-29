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

// Linked list to save a filename and its length
typedef struct node {
	long message_size;
	char *message;
	struct node *next;
} node_t;

// Helper function sending long data to the server socket
long send_message_size(int sock_fd, long message_size) {
	long size = htonl(message_size);
	long sent_bytes = write(sock_fd, &size, sizeof(size));
	if(sent_bytes <0) {
		perror("write");
		exit(5);
	}
	return sent_bytes;
}

// Helper function sending char data to the server socket
long send_string(int sock_fd, long size, char *content) {
	long sent_bytes = 0;
	char *message = content;
	fprintf(stdout, "message size : %li\n", size);
	while(sent_bytes < size) {
		fprintf(stdout, "sent bytes: %li\n", sent_bytes);
		if(size - sent_bytes < 1024) {
			if(write(sock_fd, message, size - sent_bytes) < 0) {
				perror("write");
				exit(5);
			}
			message += size - sent_bytes;
			sent_bytes += size - sent_bytes; 
		}
		else {
			if(write(sock_fd, message, 1024) < 0) {
				perror("write");
				exit(5);
			}
			message+=1024;
			sent_bytes += 1024;
		}
	}
	fprintf(stdout, "Successfully sent bytes: %li\n", sent_bytes);
	return sent_bytes;
}

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

	// Total bytes that are successfully sent to the server.	
	long total_sent_bytes = 0;

	// Total number of text files that will be sent to the server.
	long num_txt_files = 0;

	// Linked list for saving .txt filename and its length.
	node_t *head = malloc(sizeof(node_t));
	node_t *tail = head;

	// Open the current direcotry and find all .txt files.
	DIR *d = opendir(".");
	if(!d) {
		perror("opendir");
		exit(5);
	}
	struct dirent *dir;
	while((dir = readdir(d)) != NULL) {
		char *filename = dir->d_name;
		long len_filename = strlen(dir->d_name);
		if(dir->d_type == DT_REG && strncmp(filename + len_filename - 4, ".txt", 4) == 0) {		
			num_txt_files += 1;
			tail->message_size = len_filename;
			tail->message = filename;
			tail->next = malloc(sizeof(node_t));
			tail = tail->next;
			tail->message_size = 0;
			tail->message = NULL;
		}
	}
	closedir(d);

	// Send the total number of text files to be sent.
	//fprintf(stdout, "%li\n", num_txt_files);
	total_sent_bytes += send_message_size(sock_fd, num_txt_files);

	// Send the actual content of txt files saved in the linked list.
	node_t *iter = head;
	while(iter->message) {
		total_sent_bytes += send_message_size(sock_fd, iter->message_size);
		total_sent_bytes += send_string(sock_fd, strlen(iter->message), iter->message);

		FILE *fp = fopen(iter->message, "r");
		char *content;
		fseek(fp, 0, SEEK_END);
		long filesize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		content = malloc(filesize+1);
		content[filesize] = '\0';
		fread(content, filesize, 1, fp);
		fclose(fp);

		total_sent_bytes += send_message_size(sock_fd, filesize);
		total_sent_bytes += send_string(sock_fd, filesize, content);

		free(content);
		iter = iter->next;
	}

	fprintf(stdout, "total sent bytes: %li\n", total_sent_bytes);

	// Clean up.
	iter = head;
	while(iter) {
		node_t *temp = iter->next;
		free(iter);
		iter = temp;
	}
	shutdown(sock_fd, SHUT_WR);
	freeaddrinfo(result);

	return 0;
}
