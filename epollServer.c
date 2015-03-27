#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAX_EVENTS 10
#define MAX_BUF    1024

void setnonblocking(int fd){
	int val;

	if((val = fcntl(fd, F_GETFL, 0)) < 0){
		printf("fcntl: F_GETFL");
		exit(EXIT_FAILURE);
	}

	val |= O_NONBLOCK;

	if(fcntl(fd, F_SETFL, val) < 0){
		printf("fcntl: F_SETFL");
		exit(EXIT_FAILURE);
	}
}

int main() {
    int server, client, count_fds;
    // create socket
    if ((server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("socket error");
        exit(1);
    }
    
    struct sockaddr_in serv_addr;
    int portno = 7007;
    serv_addr.sin_family = AF_INET; // don't care IPv4 or IPv6
    serv_addr.sin_addr.s_addr = INADDR_ANY; // receive packets destined to any of the interfaces
    serv_addr.sin_port = htons(portno);
 
    // by default, system doesn't close socket, but set it's status to TIME_WAIT; allow socket reuse   
    int yes = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        printf("setsockopt");
        exit(1);
    } 
    
    // bind socket to address
    if (bind(server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
        printf("bind error");
        close(server);
        exit(1);
    }
    
    // start listening on port 7000, allow 10 connection in the incominq queue
    if (listen(server, 1) == -1) {
        printf("listen error");
        close(server);
        exit(1);
    }

    //Create event for server socket
    struct epoll_event ev;
    //Create event list
    struct epoll_event evlist[MAX_EVENTS];
    
    // create epoll instance
    int epfd;
    epfd = epoll_create(10);
    if (epfd == -1) {
        printf("epoll_create");
		exit(EXIT_FAILURE);
    }

    //Setup event for server socket
    //Wait for input events
    ev.events = EPOLLIN;
    //Set file descriptor = server socket fd
    ev.data.fd = server;
    //Add event to epoll
    if(epoll_ctl(epfd, EPOLL_CTL_ADD, server, &ev) == -1){
		printf("epoll_ctl: server socket");
		exit(EXIT_FAILURE);
    }
    
    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof their_addr;
    int n;
    
    char request[MAX_BUF], buf[MAX_BUF];
	int byte_count;
	char response[64];
	int resp_size;
    while (1) {
		printf("---------------WHILE--------------\n");
		//Get number of ready descriptors
		count_fds = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
		if(count_fds == -1){
			printf("epoll_wait");
			exit(EXIT_FAILURE);
		}
		printf("---------------EPOLL_WAIT--------------\n");

		for(n = 0; n < count_fds; ++n){
			if(evlist[n].data.fd == server){
				printf("---------------SERVER FD--------------\n");
				//Recieve data
				client = accept(server, (struct sockaddr *) &their_addr, &addr_size);
				printf("---------------ACCEPT CLIENT--------------\n");
				if(client == -1){
					printf("accept");
					exit(EXIT_FAILURE);
				}
				setnonblocking(client);
				printf("---------------NONBLOCK--------------\n");
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = client;
				if(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev) == -1){
					printf("epoll_ctl: client");
					exit(EXIT_FAILURE);
				}
				printf("---------------ADDED CLIENT--------------\n");
			} else {
				printf("---------------CLIENT FD--------------\n");
				byte_count = recv(client, buf, sizeof buf, 0);
				printf("---------------CLIENT RECV--------------\n");
				if (byte_count == -1) {
				    printf("recv error");
					exit(EXIT_FAILURE);
				}

				memcpy(request, buf, byte_count);
				puts(request);

				if(strstr(request, " /Hello ")>0){
					resp_size = sprintf(response,"Hello %d!", client);
				} else {
					resp_size = sprintf(response,"Print \"/Hello\" in browser url");
				}
				printf("Printf: %s\n", response);
				if (send(client, response, resp_size, 0) == -1) {
					printf("send error");
					exit(EXIT_FAILURE);
				}
				printf("---------------ANSWER SENDED--------------\n");
				close(client);
			}
		}
    }
    
    printf("Shutdown server... \n");
    close(server);
}
