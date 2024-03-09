// Client side C/C++ program to demonstrate Socket
// programming
#include <arpa/inet.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define PORT 10002
#define BUFFER_SIZE 1800000
using namespace std;
int main(int argc, char* argv[])
{
	int sock = 0, valread, client_fd;
	struct sockaddr_in serv_addr;
	char buffer[BUFFER_SIZE] = { 0 };
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

    int bytesRead = 0;
    int n;
    //receive message
    n=0;
    read(sock, buffer, BUFFER_SIZE);
    cout<<buffer;
    bzero(buffer,sizeof(buffer));

    buffer[0]='G';
    buffer[1]='O';
    buffer[2]='\n';
    
    int send_first=write(sock,buffer,strlen(buffer));
    bzero(buffer,sizeof(buffer));
    sleep(1);
    for(;;){
        bzero(buffer,sizeof(buffer));
        valread = read(sock, buffer, BUFFER_SIZE);
        //cout<<buffer;
       if(buffer[strlen(buffer)-2]=='?'){
        bytesRead+=strlen(buffer);
        break;
       }
        bytesRead+=strlen(buffer);
    }
    bytesRead-=86;
    //cout<<"\nBytes read: "<<bytesRead<<endl;
    bzero(buffer,sizeof(buffer));
    
    string tmp=to_string(bytesRead);
    for(int i=0;i<tmp.length();i++){
        buffer[i]=tmp[i];
    }
    
    //cout<<strlen(buffer)<<endl;
    int send=write(sock,buffer,10000);
    //cout<<"send byte: "<<send<<endl;
    sleep(1);
    //cout<<"Send result to sever, my result: "<<buffer<<endl;

    bzero(buffer,sizeof(buffer));
    sleep(1);
    int recv=read(sock, buffer, BUFFER_SIZE);
    //cout<<"receive byte: "<<recv<<endl;
    cout<<buffer<<endl;

	// closing the connected socket
	close(client_fd);
	return 0;
}
