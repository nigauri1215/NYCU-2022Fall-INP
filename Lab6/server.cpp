/* include fig01 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <ctime>
#include <set>
#include <vector>

using namespace std;

#define PORT1 9998
#define PORT2 9999
#define MAX_CLIENT_NUM 100
//#define FD_SETSIZE 1024
#define NAME_LEN 50
#define BUFFER_LEN 4096
#define	LISTENQ		1024

int counter=0,current_client_num=0;


int main(int argc, char **argv)
{
	int					i, maxi, maxfd, listenfd,listenfd_2, connfd_cmd, connfd_sink, sockfd;
	int					nready;
	ssize_t				n;
	fd_set				rset, allset;
	char				buf_cmd[BUFFER_LEN],buf_sink[BUFFER_LEN];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;

    //first socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(PORT1);	//9998

	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

    //second socket
    listenfd_2 = socket(AF_INET, SOCK_STREAM, 0);
	servaddr.sin_port        = htons(PORT2);	//9999

	bind(listenfd_2, (struct sockaddr*) &servaddr, sizeof(servaddr));


	listen(listenfd, LISTENQ);
	listen(listenfd_2, LISTENQ);

	maxfd = listenfd;			/* initialize */
	maxi = -1;					/* index into client[] array */

	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);


	for ( ; ; ) {
		rset = allset;		/* structure assignment */
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(listenfd, &rset)) {	/* command channel */
			clilen = sizeof(cliaddr);
			connfd_cmd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);

			printf("* client connected from %s:%d\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));

			/*initiate client info*/
			for (i = 0; i < FD_SETSIZE; i++){
				if (client[i].fd < 0) {
					client[i].id = i;
					client[i].fd=connfd_cmd;
					client[i].address=inet_ntoa(cliaddr.sin_addr);
					client[i].port=ntohs(cliaddr.sin_port);	

					current_client_num++;
					break;
				}
			}
			if (i == FD_SETSIZE){
				printf("too many clients\n");
                break;
            }

			FD_SET(connfd, &allset);
			if (connfd > maxfd)
				maxfd = connfd;			
			if (i > maxi)
				maxi = i;			

			if (--nready <= 0)
				continue;				
		}
		if (FD_ISSET(listenfd_2, &rset)) {	/* data sink server */
			clilen = sizeof(cliaddr);
			connfd_sink = accept(listenfd_2, (struct sockaddr*) &cliaddr, &clilen);

			printf("* client connected from %s:%d\n",inet_ntoa(cliaddr.sin_addr),ntohs(cliaddr.sin_port));

			/*initiate client info*/
			for (i = 0; i < FD_SETSIZE; i++){
				if (client[i].fd < 0) {
					client[i].id = i;
					client[i].fd=connfd;		/* save descriptor */
					client[i].address=inet_ntoa(cliaddr.sin_addr);
					client[i].port=ntohs(cliaddr.sin_port);	

					current_client_num++;
					break;
				}
			}
			if (i == FD_SETSIZE){
				printf("too many clients\n");
                break;
            }

			FD_SET(connfd, &allset);	/* add new descriptor to set */
			if (connfd > maxfd)
				maxfd = connfd;			/* for select */
			if (i > maxi)
				maxi = i;				/* max index in client[] array */

			if (--nready <= 0)
				continue;				/* no more readable descriptors */
		}

		for (i = 0; i <= maxi; i++) {	/* check all clients for data */
			if ( (sockfd = client[i]) < 0)
				continue;
			if (FD_ISSET(sockfd, &rset)) {
                /**
				if ( (n = Read(sockfd, buf, MAXLINE)) == 0) {
					Close(sockfd);
					FD_CLR(sockfd, &allset);
					client[i] = -1;
				} else
					Writen(sockfd, buf, n);
                    */
                n = read(sockfd, buf, BUFFER_LEN);
				//cout<<"Recv msg:"<<buf<<endl;
				string cmd="";
                for(int j=0;j<n;j++){
					cmd+=buf[j];
				}
                /* /reset */
                if(cmd.substr(0,6)=="/reset"){
					
				}
                /* /ping */
                if(cmd.substr(0,5)=="/ping"){
					
				}
                /* /report */
                if(cmd.substr(0,7)=="/report"){
					
				}
                /* /clients */
                if(cmd.substr(0,8)=="/clients"){
					
				}


                memset(buf,0,sizeof(buf));

				if (--nready <= 0)
					break;				/* no more readable descriptors */
			}


			
		}
	}
}