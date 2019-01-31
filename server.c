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
#include <signal.h>

long num_txt_files = 0;
char *title = NULL;

//
void signal_handler(int sig) {
	if(sig == SIGINT) {
		fprintf(stderr, "SIGINT Caught!\n");
		exit(111);
	}
	if(sig == SIGHUP) {
		fprintf(stderr, "%s is being transferred: %li files left.\n", title, num_txt_files);	
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

// Helper function receiving char data from the client socket
long receive_message(int client_fd, long size, char *buffer) {
	long received_bytes = 0;
	ssize_t remaining_bytes = (ssize_t) size;
	ssize_t read_bytes;
	while(remaining_bytes	 > 0) {
		read_bytes = read(client_fd, buffer+received_bytes, remaining_bytes);
		if(read_bytes > 0) {
			received_bytes += read_bytes;
			remaining_bytes -= read_bytes;
		}
		else if(read_bytes == 0) break;
		else {
			perror("read");
			exit(5);
		}
	}
	return received_bytes;
}

int main(int argc, char **argv) {
	// Print pid for testing signal-catching.	
	fprintf(stdout, "pid: %d\n", getpid());

	// Catch SIGINT and SIGHUP.
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);

	// server has 1 essential argument and 1 optional argument: port and directory name.
	char *port;
	char *folder = NULL;
	if(argc != 3 && argc != 2) {
		fprintf(stderr, "Wrong Arguments.\n");	
		exit(11);
	}
	else {
		port = argv[1];
		if(argc == 3) folder = argv[2];
	}
	
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

	// If port is "0", then bind() assigns any available port.
	// Print the port number.
	struct sockaddr_in serveraddr;
	socklen_t serveraddrlen = sizeof(serveraddr);
	if (getsockname(server_fd, (struct sockaddr *) &serveraddr, &serveraddrlen) == -1) {
		perror("getsockname");
		exit(5);
	}
	fprintf(stdout, "port %d is open for connections.\n", ntohs(serveraddr.sin_port));

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
	fprintf(stdout, "connected to %s:%s\n", client_host, client_port);

	// If directory is not specified, use the IP address of the client as a directory name.
	if(folder == NULL) folder = client_host;
	
	// Create a new directory
	struct stat st;
	// If the directory does not exist already, make a new directory.
	if(stat(folder, &st) == -1) {
		if(mkdir(folder, 0700) == -1) {
			perror("mkdir");
			exit(5);
		}
		if(chdir(folder) == -1) {
			perror("chdir");
			exit(5);
		}
	}
	// If exists, Check if the directory is empty.
	else {
		if(chdir(folder) == -1) {
			perror("chdir");
			exit(5);
		}
		DIR *d = opendir(".");
		if(d == NULL) {
			perror("opendir");
			exit(5);
		}
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

	int read_status = 0;
	
	// Total bytes that are successfully received by the server.
	long total_received_bytes = 0;

	// Receive the total number of .txt files to be sent by the client.
	read_status = read(client_fd, &num_txt_files, sizeof(num_txt_files));
	if(read_status < 0){
		perror("read");
		exit(5);
	}
	total_received_bytes += read_status;

	num_txt_files = ntohl(num_txt_files);
	int saved[num_txt_files];
	int count = 0;
	while(num_txt_files > 0) {
		// Receive length of title of file
		long title_length = 0;
		read_status = read(client_fd, &title_length, sizeof(title_length));
		if(read_status < 0) {
			perror("read");
			exit(5);
		}
		title_length = ntohl(title_length);
		total_received_bytes += read_status;

		// Retreive a file title from the client.
		title = malloc(title_length+1);
		if(title == NULL) {
			perror("malloc");
			exit(5);
		}
		title[title_length] = '\0';
		read_status = read(client_fd, title, title_length);
		if(read_status < 0) {
			perror("read");
			exit(5);	
		}
		fprintf(stdout, "%-15.15s", title);
		total_received_bytes += read_status;

		// Open a file with the filename.
		//If it does not exist, create a file with the name.
		FILE *fp = fopen(title, "a");
		if(fp == NULL) {
			perror("fopen");
			exit(5);
		}

		// Receive file size from the client.
		long filesize = 0;
		read_status = read(client_fd, &filesize, sizeof(filesize));
		if(read_status < 0) {
	   		perror("read");
			exit(5);
		}
		fprintf(stdout, "%7d bytes transferred ", ntohl(filesize));
		filesize = ntohl(filesize);
		total_received_bytes += read_status;

		// Receive content data from the client.
		char *buffer = malloc(filesize+1);
		if(buffer == NULL) {
			perror("malloc");
			exit(5);
		}
		buffer[filesize] = '\0';
		total_received_bytes += receive_message(client_fd, filesize, buffer);

		// Save data in buffer to the file.
		fprintf(fp, "%s", buffer);
		// What happens if the file is too big?

		// Compare the filesize and the actual saved size
		long sent_bytes = 0;
		fseek(fp, 0, SEEK_SET);
		fseek(fp, 0, SEEK_END);
		long saved_filesize = ftell(fp);
		sent_bytes += send_message_size(client_fd, saved_filesize);
		saved[count] = (saved_filesize == filesize);
		if(saved[count]) {
			fprintf(stdout, " Success\n");
		}
		else {
			fprintf(stdout, " Fail\n");
		}
		count += 1;
		fclose(fp);		
		
		// If a file is not saved successfully, it is removed.
		int remove_status = 0;
		if(!saved[count]) remove_status = remove(title);
		if(remove_status < 0) {
			perror("remove");
		}

		// Clean up.
		free(title);
		free(buffer);

		// Iterations
		num_txt_files -= 1;
		sleep(3);
	}
	
	// Print the total number of bytes received by the sever and close the client socket.
	fprintf(stdout, "total received bytes: %li bytes\n", total_received_bytes);
	shutdown(client_fd, SHUT_WR);

	// Check whether the entire files are saved successfully.
	int entirely_saved = 1;
	int partially_saved = 0;
	for(int i = 0; i < count; i++) {
		if(saved[i]) partially_saved = 1;
		else entirely_saved = 0;
	}
	if(!entirely_saved && partially_saved) {
		fprintf(stderr, "Files are partially saved.\n");
		exit(1);
	}
	if(!entirely_saved && !partially_saved) {
		fprintf(stderr, "No files are saved.\n");
		exit(2);
	}

	// Clean up
	freeaddrinfo(result);

	return 0;
}
