#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

// PORT number
#define PORT 11111

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in serverAddr;
	int clientSocket;
	struct sockaddr_in cliAddr;
	socklen_t addr_size;

    int opt = 1;
    int addrlen = sizeof(serverAddr);
    char buffer[1024] = { 0 };

	// Error handling if socket id is not valid
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error in connection.\n");
		exit(1);
	}

	printf("Server Socket is created.\n");
	// Assign port number and IP address
	// to the socket created
        // Forcefully attaching socket to the port 8080
    if (setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    int nport=atoi(argv[1]);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(nport);

	// Error handling
	if (bind(sockfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr)) < 0) {
		printf("Error in binding.\n");
		exit(1);
	}
	addr_size=sizeof(serverAddr);

	// Listening for connections (upto 10)
	if (listen(sockfd, 10) == 0) {
		printf("Listening...\n\n");
	}

	int cnt = 0;
    pid_t childpid;
    int status;
	while (1) {

		// Accept clients and store their information in cliAddr
		clientSocket = accept(sockfd, (struct sockaddr*)&cliAddr,&addr_size);

		// Error handling
		if (clientSocket < 0) {
            printf("Error handling...");
			exit(1);
		}

		// Displaying information of connected client
		printf("Connection accepted from %s:%d\n",inet_ntoa(cliAddr.sin_addr),ntohs(cliAddr.sin_port));

		// Print number of clients
		printf("Clients connected: %d\n\n",++cnt);

		if ((childpid = fork()) == 0) {
			close(sockfd);

			int handler=0;
			dup2(STDERR_FILENO,handler);
			//cout<<handler<<endl;
			
            dup2(clientSocket,STDOUT_FILENO);
			dup2(clientSocket,STDERR_FILENO);

            //TODOs
			string cmd;
			for(int i=2;i<argc;i++){
				cmd+=argv[i];
				if(i!=argc-1){
					cmd+=" ";
				}
			}
			const char *file=cmd.data();
			//cout<<system(file)<<endl;
			int return_value=system(file);
			//cout<<return_value<<endl;
            if(return_value==32512){
				dup2(handler,STDERR_FILENO);
				system(file);
			}
            exit(0);
		}
        else{
            close(clientSocket);

            waitpid((pid_t)childpid, &status, 0);
        }
	}

	// Close the client socket id
	close(clientSocket);
	return 0;
}
