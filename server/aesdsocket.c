#include <stdlib.h>
#include <stdio.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define DATA_PATH "/var/tmp/aesdsocketdata"
#define MAXIMUM_RECV_BUF_SIZE 128

int sockfd;

void cleanup_exit(int fd){
        remove(DATA_PATH);
        close(sockfd);
        exit(fd);
}


void write_completed_packet_to_file(int fd, char* buf){
	FILE* fp;
	fp = fopen(DATA_PATH, "a+");
	if(fp == NULL){
		syslog(LOG_ERR, "Error on opening data file: %s", strerror(errno));
		cleanup_exit(EXIT_FAILURE);
	}
	if(fprintf(fp, "%s\n", buf) < 0){
		syslog(LOG_ERR, "Error on writing to file: %s", strerror(errno));
		fclose(fp);
		cleanup_exit(EXIT_FAILURE);
	}
	// move cursor to begin of data file.
	if(fseek(fp, 0, SEEK_SET) != 0){
		syslog(LOG_ERR, "Error on seeking file: %s", strerror(errno));
		fclose(fp);
		cleanup_exit(EXIT_FAILURE);
	}

	char* line = NULL;
	// size of line
	size_t len = 0;
	//the number of characters read, including the delimiter character
	ssize_t read;

	// send full content of data file to client
	while((read = getline(&line, &len, fp)) != -1){
		if(send(fd, line, read, MSG_NOSIGNAL) == -1){
			syslog(LOG_ERR, "Error on sending: %s", strerror(errno));
			cleanup_exit(EXIT_FAILURE);
		}
	}
	free(line);
	fclose(fp);
}


static void signal_handler(int signal_number){
	syslog(LOG_INFO, "Caught signal, exiting");
	shutdown(sockfd, SHUT_RDWR);
}

void create_daemon(){
	pid_t pid = fork();
	if(pid == -1){
		syslog(LOG_ERR, "Error on forking: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	else if(pid > 0){
		// parent process
		exit(EXIT_SUCCESS);
	}
	if(setsid() == -1){
		syslog(LOG_ERR, "Error on setsid(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	if(chdir("/") == -1){
		syslog(LOG_ERR, "Error on chdir(): %s", strerror(errno));
                exit(EXIT_FAILURE);
	}
	for(int i = 0; i < 3; i++){
		close(i);
	}
	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
	umask(0);
}

int main(int argc, char* argv[]){
	// open syslog file
	openlog("aesdsocket", LOG_PERROR, LOG_USER);

	// register signals
	struct sigaction saction;
	memset(&saction, 0, sizeof(saction));
	saction.sa_handler = signal_handler;
	if(sigaction(SIGTERM, &saction, NULL) != 0){
		syslog(LOG_ERR, "Error on SIGTERM signal register: %s", strerror(errno));
	}
	if(sigaction(SIGINT, &saction, NULL) != 0){
		syslog(LOG_ERR, "Error on SIGINT signal register: %s", strerror(errno));
	}
	
	// addrinfo
	struct addrinfo hints, *res;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if(getaddrinfo(NULL, "9000", &hints, &res) != 0){
		syslog(LOG_ERR, "Error on getaddrinfo: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	// make a socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	
	// bind to port
	if(bind(sockfd, res->ai_addr, res->ai_addrlen) != 0){
		syslog(LOG_ERR, "Error on bind: %s", strerror(errno));
		freeaddrinfo(res);
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);
	
	//listen
	syslog(LOG_INFO, "Bounded to port 9000");

	if(listen(sockfd, 10) != 0){
		syslog(LOG_ERR, "Error on listening: %s", strerror(errno));
		cleanup_exit(EXIT_FAILURE);
	}

	int opt;
	if((opt = getopt(argc, argv, "d")) != -1){
		if(opt != 'd'){
			syslog(LOG_ERR, "Invalid option: %c", opt);
			exit(EXIT_FAILURE);
		}
		syslog(LOG_INFO, "Option -d detected");
		create_daemon();
	}

	// Wait, accept and handle connection.
	while(1){
		syslog(LOG_INFO, "Listening...");

		//accept connection
		struct sockaddr_in addr;
		socklen_t addr_len = sizeof(addr);
		memset(&addr, 0, sizeof(addr));
		int fd = accept(sockfd, (struct sockaddr*)&addr, &addr_len);
		if(fd == -1){
			// interrupted by signal.
			if(errno != EINTR){
				syslog(LOG_ERR, "Error on accepting: %s", strerror(errno));
				cleanup_exit(EXIT_FAILURE);
			}else{cleanup_exit(EXIT_SUCCESS);}
		}
		
		char client_ip[INET_ADDRSTRLEN] = "";
		//extract client ip
		inet_ntop(AF_INET,&(addr.sin_addr) , client_ip, INET_ADDRSTRLEN);
		syslog(LOG_INFO, "Accepted connection from %s", client_ip);

		// handle packet
		char* buf = malloc(sizeof(char));
		*buf = '\0';
		int size = 1;
		while(1){
			char recv_buf[MAXIMUM_RECV_BUF_SIZE + 1];
			// return the number of bytes received
			int receive = recv(fd, recv_buf, MAXIMUM_RECV_BUF_SIZE, 0);
			if(receive == -1){
				syslog(LOG_ERR, "Error on receiving: %s", strerror(errno));
				close(fd);
				free(buf);
				cleanup_exit(EXIT_FAILURE);
			}else if(receive == 0){
				syslog(LOG_INFO, "Closed connection from %s", client_ip);
				break;
			}
			recv_buf[receive] = '\0';
			char* str = recv_buf;
			char* token = strsep(&str, "\n");
			if(str == NULL){
				// packet is not completed.
				size += receive;
				buf = realloc(buf, size * sizeof(char));
				strncat(buf, token, receive);
			}else{
				size += strlen(token);
				buf = realloc(buf, size * sizeof(char));
				strcat(buf, token);
				syslog(LOG_INFO, "Received packet: %s", buf);
				// write buffer to file and send
				write_completed_packet_to_file(fd, buf);
				// store the remaining characters in buffer
				size = strlen(str) + 1;
				buf = realloc(buf, size * sizeof(char));
				strcpy(buf, str);		
			}
		}
		free(buf);
		close(fd);
	}
	return 0;
}
