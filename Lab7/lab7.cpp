#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;
#define PORT 10008
#define BUFFER_SIZE 50

static struct timeval _t0;
static unsigned long long bytesent = 0;

int global_sock=0;
double tv2s(struct timeval *ptv) {
	return 1.0*(ptv->tv_sec) + 0.000001*(ptv->tv_usec);
}

void interrupt(int s) {
	string message_buffer="r\nlocalhost/10000\n";
	write(global_sock, message_buffer.c_str(), message_buffer.length());
	
	cout<<"send: "<<message_buffer<<endl;
	message_buffer.clear();
	exit(0);
}

int main(int argc, char *argv[]) {
	int sock = 0, valread, client_fd;
	struct sockaddr_in serv_addr;
	char buffer[BUFFER_SIZE] = { 0 };
	char read_buf[10000]={0};
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\n Socket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	// Convert IPv4 and IPv6 addresses from text to binary
	// form
	if (inet_pton(AF_INET, "140.113.213.213", &serv_addr.sin_addr)<= 0) {
		printf("Invalid address/ Address not supported \n");
		return -1;
	}

	if ((client_fd= connect(sock, (struct sockaddr*)&serv_addr,sizeof(serv_addr)))< 0) {
		printf("Connection Failed \n");
		return -1;
	}

	global_sock=sock;
    //receive message

	bzero(read_buf,sizeof(read_buf));
	read(sock, read_buf, 10000);
	cout<<read_buf<<endl;

	char ans[20];
	cout<<"input PoW ans:";
	fgets(ans,20,stdin);
	write(sock,ans,sizeof(ans));
	

	//signal(SIGINT,  handler);		//used to handle interrupt signal
	//signal(SIGTERM, handler);		//used to handle terminal signal

	
	read(sock, buffer, BUFFER_SIZE);
    cout<<buffer;

	int cnt=0;


	while(1) {

		signal(SIGALRM,interrupt);
		alarm(10);

		bzero(read_buf,sizeof(read_buf));
		read(sock, read_buf, 10000);
		cout<<read_buf<<endl;

		cout<<"user start sending"<<endl;

		//send: r

		string message_buffer="r\nlocalhost/10000\n";
		write(sock, message_buffer.c_str(), message_buffer.length());
		
		cout<<"send: "<<message_buffer<<endl;
		message_buffer.clear();

		/*
		bzero(read_buf,sizeof(read_buf));
		read(sock, read_buf, 10000);
		cout<<read_buf<<endl;
		
		
		//send: localhost/10000
		message_buffer="localhost/10000";
		message_buffer+='\n';
		write(sock, message_buffer.c_str(), message_buffer.length());
		
		cout<<"send: "<<message_buffer<<endl;
		message_buffer.clear();

		*/
		
		bzero(read_buf,sizeof(read_buf));
		read(sock, read_buf, 10000);
		cout<<read_buf<<endl;

		if(cnt%5==0){
			//send: v ,checking

			message_buffer="v";
			message_buffer+='\n';
			write(sock, message_buffer.c_str(), message_buffer.length());

			cout<<"send: "<<message_buffer<<endl;
			message_buffer.clear();

			bzero(read_buf,sizeof(read_buf));
			read(sock, read_buf, 10000);
			cout<<read_buf<<endl;
			
			sleep(2);
		}
		
		bzero(buffer,BUFFER_SIZE);
		
		cnt++;
	}

	return 0;
}
