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

/*Set NONBLOCK flag to fd*/
void set_nonblocking(int fd){
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
	int server, client, epfd, clients_fd, byte_count, resp_size, n;
	char request[MAX_BUF], buf[MAX_BUF], response[MAX_BUF];
	struct epoll_event ev, evlist[MAX_EVENTS];
	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;

	/*Create server socket*/
	if ((server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        	perror("socket error");
        	exit(EXIT_FAILURE);
	}

	/*Setup server settings*/
	struct sockaddr_in serv_addr;
	int portno = 7007;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	/*Make server socket reusable*/
	int yes = 1;
	if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	} 

	/*Bidn socket to serv_addr*/
	if (bind(server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
		perror("bind error");
		close(server);
		exit(EXIT_FAILURE);
	}

	/*
	 *Start listening server socket.
	 *Max count of connections = 10
	 */
	if (listen(server, 10) == -1) {
		perror("listen error");
		close(server);
		exit(EXIT_FAILURE);
	}

	/*Create epoll*/
	epfd = epoll_create(10);
	if (epfd == -1) {
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	/*Add server socket to epoll watcher*/
	ev.events = EPOLLIN;
	ev.data.fd = server;
	if(epoll_ctl(epfd, EPOLL_CTL_ADD, server, &ev) == -1){
		perror("epoll_ctl: server socket");
		exit(EXIT_FAILURE);
	}

	while (1) {
		/*Wait ready state from some of watched sockets*/
		clients_fd = epoll_wait(epfd, evlist, MAX_EVENTS, -1);
		if(clients_fd == -1){
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		/*Use all ready sockets*/
		for(n = 0; n < clients_fd; ++n){
			/*Watched socket == server socket*/
			if(evlist[n].data.fd == server){
				/*Recieve client socket*/
				client = accept(server, (struct sockaddr *) &their_addr, &addr_size);
				if(client == -1){
					printf("accept");
					exit(EXIT_FAILURE);
				}

				/*Add NONBLOCK flag to client socket*/
				set_nonblocking(client);

				/*Add client socket to epoll watcher*/
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = client;
				if(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev) == -1){
					printf("epoll_ctl: client");
					exit(EXIT_FAILURE);
				}
			} else {
				/*Read request*/
				byte_count = recv(client, buf, sizeof buf, 0);
				if (byte_count == -1) {
				    printf("recv error");
					exit(EXIT_FAILURE);
				}
				memcpy(request, buf, byte_count);
				puts(request);

				if(strstr(request, " /Hello ")>0)
					resp_size = sprintf(response,"Hello %d!", client);
				else 
					resp_size = sprintf(response,"Print \"/Hello\" in browser url");

				/*Send answer to client*/
				if (send(client, response, resp_size, 0) == -1) {
					printf("send error");
					exit(EXIT_FAILURE);
				}

				/*Done with this client - close its fd*/
				close(client);
			}
		}
	}

	/*Close server*/
	printf("Shutdown server... \n");
	close(server);
}
