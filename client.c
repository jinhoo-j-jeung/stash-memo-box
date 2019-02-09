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
#include <signal.h>

// Linked list to save a filename and its length
typedef struct node {
	long message_size;
	char *message;
	struct node *next;
} node_t;

// Global vaiables
int sock_fd;	
struct addrinfo hints, *result;
node_t *head = NULL;
long num_txt_files = 0;
char title[256], *content;

// Helper function cleaning up before exiting.
void close_client(int exit_code) {
	shutdown(sock_fd, SHUT_WR);
	if(result != NULL) {
		freeaddrinfo(result);
	}
	if(head != NULL) {
		node_t *iter = head;
		while(iter) {
			node_t *temp = iter->next;
			free(iter->message);
			free(iter);
			iter = temp;
		}
	}
	if(content != NULL) {
		free(content);
	}
	exit(exit_code);
}

// Custom signal handler
void signal_handler(int sig) {
	if(sig == SIGINT) {
		fprintf(stderr, "SIGINT Caught!\n");
		close_client(5);
	}
	if(sig == SIGPIPE) {
		fprintf(stderr, "SIGPIPE Caught!\n");
		close_client(5);
	}
	if(sig == SIGHUP) {
		fprintf(stderr, "'%s' is being transferred: %li files left.\n", title, num_txt_files-1);	
	}
}

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

// Helper function sending filename to the server socket
long send_title(int sock_fd, long size, char *content) {
	long sent_bytes = 0;

	// Print the name of the file to be sent to the server.
	char *message = content;
	fprintf(stdout, "%-30.30s ", content);

	ssize_t written_bytes;
	while(sent_bytes < size) {
		if(size - sent_bytes < 1024) {
			written_bytes = write(sock_fd, message, size - sent_bytes);
			if(written_bytes < 0) {
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
	return sent_bytes;
}

// Helper function sending content of the file to the server socket
long send_message(int sock_fd, long size, char *content) {
	long sent_bytes = 0;
	ssize_t written_bytes;
	char *message = content;
	while(sent_bytes < size) {
		if(size - sent_bytes < 1024) {
			written_bytes = write(sock_fd, message, size - sent_bytes);
			if(written_bytes < 0) {
				perror("write");
				exit(5);
			}
			message += size - sent_bytes;
			sent_bytes += size - sent_bytes;
		}
		else {
			written_bytes = write(sock_fd, message, 1024);
			if(written_bytes < 0) {
				perror("write");
				exit(5);
			}
			message+=1024;
			sent_bytes += 1024;
		}
	}
	fprintf(stdout, "%7li bytes trnasferred", sent_bytes);
	return sent_bytes;
}

int main(int argc, char **argv) {
	// Catch SIGINT and SIGHUP.
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	// client requires 2 arguments: host and port.
	if(argc != 3) {
		fprintf(stderr, "%s: usage: %s server_addr server_port\n", argv[0], argv[0]);	
		exit(11);
	}
	char *host = argv[1];
	char *port = argv[2];

	// Print pid for testing signal-catching.	
	fprintf(stderr, "pid: %d\n", getpid());

	// Socket
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	// Failed to return a file descriptor.
	if(sock_fd == -1) {
        	perror("socket");
        	exit(5);
	}

	// Client address information
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;	
	hints.ai_socktype = SOCK_STREAM;
	int s = getaddrinfo(host, port, &hints, &result);
	// The given address is invalid.
	if(s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		shutdown(sock_fd, SHUT_WR);
		if(result != NULL) {
			freeaddrinfo(result);
		}
		exit(5);
	}

	// Connect
	if(connect(sock_fd, result->ai_addr, result->ai_addrlen) == -1) {
		perror("connect");
		shutdown(sock_fd, SHUT_WR);
		if(result != NULL) {
			freeaddrinfo(result);
		}
		exit(5);
	}

	// Total bytes that are successfully sent to the server.	
	long total_sent_bytes = 0;

	// Linked list for saving .txt filename and its length.
	head = malloc(sizeof(node_t));
	node_t *tail = head;

	// Open the current direcotry and find all .txt files.
	DIR *d = opendir(".");
	if(!d) {
		perror("opendir");
		close_client(5);
	}
	struct dirent *dir;
	while((dir = readdir(d)) != NULL) {
		char *filename = dir->d_name;
		long len_filename = strlen(dir->d_name);
		if(dir->d_type == DT_REG && strncmp(filename + len_filename - 4, ".txt", 4) == 0) {		
			num_txt_files += 1;
			tail->message_size = len_filename;
			tail->message = malloc(len_filename+1);
			strncpy(tail->message, filename, len_filename);
			tail->message[len_filename] = '\0';
			tail->next = malloc(sizeof(node_t));
			tail = tail->next;
			tail->message_size = 0;
			tail->message = NULL;
			tail->next = NULL;
		}
	}
	closedir(d);

	// Send the total number of text files to be sent.
	total_sent_bytes += send_message_size(sock_fd, num_txt_files);

	// Send the actual content of txt files saved in the linked list.
	int saved[num_txt_files];
	int count = 0;
	node_t *iter = head;
	while(iter->message) {
		// Send a filename and its length.
		strncpy(title, iter->message, iter->message_size);
		total_sent_bytes += send_message_size(sock_fd, iter->message_size);		
		total_sent_bytes += send_title(sock_fd, strlen(iter->message), iter->message);

		FILE *fp = fopen(iter->message, "r");
		fseek(fp, 0, SEEK_END);
		long filesize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		content = malloc(filesize+1);
		content[filesize] = '\0';
		fread(content, filesize, 1, fp);
		fclose(fp);	

		// Send the content of the file and its size.
		total_sent_bytes += send_message_size(sock_fd, filesize);
		total_sent_bytes += send_message(sock_fd, filesize, content);

		// Receive the size of the file saved in the server from the server.
		long saved_content_size = 0;
		int read_status = read(sock_fd, &saved_content_size, sizeof(saved_content_size));
		while(read_status < 0) {
			// If read gets interrupted by SIGHUP, try again.
			if(errno == EINTR) {
				read_status = read(sock_fd, &saved_content_size, sizeof(saved_content_size));
				sleep(2);
			}
			else {
	   			perror("read");
				close_client(5);
			}
		}
		saved_content_size = (long) ntohl(saved_content_size);

		// Print whether the file is successfully transfered or not.
		saved[count] = (filesize == saved_content_size);
		if(saved[count]) {
			fprintf(stdout, " Success\n");
		}
		else {
			fprintf(stdout, " Fail\n");
		}

		// Clean up
		free(content);
		content = NULL;

		// Iteration
		num_txt_files -= 1;
		count += 1;
		iter = iter->next;
	}

	// Print the total number of bytes sent to the sever and close the socket.
	fprintf(stdout, "total sent bytes: %li bytes\n", total_sent_bytes);

	// Check whether the entire files are saved successfully.
	int entirely_saved = 1;
	int partially_saved = 0;
	for(int i = 0; i < count; i++) {
		if(saved[i]) partially_saved = 1;
		else entirely_saved = 0;
	}
	if(!entirely_saved && partially_saved) {
		fprintf(stderr, "Files are partially saved.\n");
		close_client(1);
	}
	if(!entirely_saved && !partially_saved) {
		fprintf(stderr, "No files are saved.\n");
		close_client(2);
	}

	// Clean up.
	iter = head;
	while(iter) {
		node_t *temp = iter->next;
		free(iter->message);
		free(iter);
		iter = temp;
	}
	freeaddrinfo(result);
	shutdown(sock_fd, SHUT_WR);

	return 0;
}
