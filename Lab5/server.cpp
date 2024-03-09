#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <queue>
#include <sys/wait.h>
#include "animals.h"
#include <time.h>
#include <ctime>

using namespace std;

// PORT number
#define PORT 10005
#define MAX_CLIENT_NUM 2048
#define NAME_LEN 50
#define BUFFER_LEN 4096

struct Client{
	int valid=0;		//1 for user who is online
	int fd_id;		//user id number
	int socket;		//socket to this user
	string name;		//user name
	char* address;		//user addr
	int port;		//user port

}client[MAX_CLIENT_NUM]={0};

queue<string> message_q[MAX_CLIENT_NUM];
int current_client_num = 0;

pthread_mutex_t num_mutex = PTHREAD_MUTEX_INITIALIZER;

// 2 kinds of threads
pthread_t chat_thread[MAX_CLIENT_NUM] = {0};
pthread_t send_thread[MAX_CLIENT_NUM] = {0};

// used to sync
pthread_mutex_t mutex[MAX_CLIENT_NUM] = {0};
pthread_cond_t cv[MAX_CLIENT_NUM] = {0};


//change name
void change_name(void *data,char buffer[BUFFER_LEN]){
	//cout<<"enter change name"<<endl;
	struct Client *pipe=(struct Client *)data;
	char tt[100];
	time_t now = time(nullptr);
    // message buffer
    string message_buffer;
    int message_len = 0;
	//read buffer
	string read;
	for(int i=6;i<strlen(buffer)-1;i++){
		read+=buffer[i];
	}

	string old_name=pipe->name;
	string new_name = read.c_str();

	now = time(nullptr);
	tm* tm_info = localtime(&now);
	strftime(tt, 100, "%Y-%m-%d %H:%M:%S", tm_info);

	//send to this->user
	char to_user[100];
	sprintf(to_user,"%s *** Nickname changed to <%s>\n",tt,const_cast<char*>(new_name.c_str()));

	//cout<<"old name:"<<

	pthread_mutex_lock(&mutex[pipe->fd_id]);
	client[pipe->fd_id].name=new_name;
	message_q[pipe->fd_id].push(to_user);
	pthread_cond_signal(&cv[pipe->fd_id]);
    pthread_mutex_unlock(&mutex[pipe->fd_id]);
	
	//send to every client
	char to_clients[100];
	sprintf(to_clients,"%s *** User <%s> renamed to <%s>\n",tt,const_cast<char*>(old_name.c_str()),const_cast<char*>(new_name.c_str()));
	message_buffer=to_clients;
	message_len=message_buffer.length();
	for(int j=0;j<MAX_CLIENT_NUM;j++){
		if(client[j].valid&&client[j].socket!=pipe->socket){
			pthread_mutex_lock(&mutex[j]);
			message_q[j].push(message_buffer);
			pthread_cond_signal(&cv[j]);		//對應到handle_send的con_wait
			pthread_mutex_unlock(&mutex[j]);
		}
	}
	return;

}

//list who are online
void who(void *data){
	struct Client *pipe=(struct Client *)data;
	char temp[100];
	string message_buffer;
	sprintf(temp,"--------------------------------------------------\n");
	message_buffer+=temp;
	for(int i=0;i<MAX_CLIENT_NUM;i++){
		if(client[i].valid&&client[i].socket!=pipe->socket){
			//other clients
			sprintf(temp,"  %s       %s:%d\n",const_cast<char*>(client[i].name.c_str()),client[i].address,client[i].port);
			message_buffer+=temp;
		}
		if(client[i].valid&&client[i].socket==pipe->socket){
			//this user
			sprintf(temp,"* %s       %s:%d\n",const_cast<char*>(client[i].name.c_str()),client[i].address,client[i].port);
			message_buffer+=temp;
		}
	}
	sprintf(temp,"--------------------------------------------------\n");
	message_buffer+=temp;

	pthread_mutex_lock(&mutex[pipe->fd_id]);
	message_q[pipe->fd_id].push(message_buffer);
	pthread_cond_signal(&cv[pipe->fd_id]);
	pthread_mutex_unlock(&mutex[pipe->fd_id]);

	return;
	
}

//send message
void* handle_send(void *data){
	struct Client *pipe=(struct Client *)data;
	while(1){
		pthread_mutex_lock(&mutex[pipe->fd_id]);		//wait前要上鎖，避免race condition
		//wait until new message recv
		while(message_q[pipe->fd_id].empty()){
			pthread_cond_wait(&cv[pipe->fd_id], &mutex[pipe->fd_id]);		//等待client端的訊息，如果有新訊息->被signal，boardcast new message to other users
		}
		
		//send message
		while(!message_q[pipe->fd_id].empty()){
			string message_buffer=message_q[pipe->fd_id].front();
			int n=message_buffer.length();

			int trans_len=BUFFER_LEN>n?n:BUFFER_LEN;
			//send message
			while(n>0){
				int len=send(pipe->socket,message_buffer.c_str(),trans_len,MSG_NOSIGNAL);
				if(len<0){
					perror("send");
					return NULL;
				}
				//if(len<0) break;
				n-=len;
				message_buffer.erase(0,len);
				trans_len = BUFFER_LEN > n ? n : BUFFER_LEN;
			}
			// delete the message that has been sent
            message_buffer.clear();
            message_q[pipe->fd_id].pop();
		}
		pthread_mutex_unlock(&mutex[pipe->fd_id]);
	}
	return NULL;


}

//get client message
void handle_recv(void *data){
	struct Client *pipe = (struct Client *)data;
	char tt[100];
	time_t now = time(nullptr);
	tm* tm_info = localtime(&now);
    // message buffer
    string message_buffer;
    int message_len = 0;

	//transfer buffer 
	char buffer[BUFFER_LEN+1];
	int buffer_len=0;
	bool cont_reading=true;

	//receive message
	while((buffer_len=recv(pipe->socket,buffer,BUFFER_LEN,0))>0){
		string check_cmd;
		for(int k=0;k<buffer_len-1;k++){
			check_cmd+=buffer[k];
		}
		if(check_cmd[0]=='/'){
			if(check_cmd.substr(0,6)=="/name "){
				change_name(data,buffer);
				buffer_len=0;
				memset(buffer,0,sizeof(buffer));
				
				continue;
			}
			else if(check_cmd.substr(0,4)=="/who"){
				who(data);
				continue;
			}
			else{
				//unknown command
				char error[100];
				//get time
				now = time(nullptr);
				tm_info = localtime(&now);
				strftime(tt, 100, "%Y-%m-%d %H:%M:%S", tm_info);
				
				sprintf(error,"%s *** Unknown or incomplete command <%s>\n",tt,const_cast<char*>(check_cmd.c_str()));
				pthread_mutex_lock(&mutex[pipe->fd_id]);
				message_q[pipe->fd_id].push(error);
				pthread_cond_signal(&cv[pipe->fd_id]);
				pthread_mutex_unlock(&mutex[pipe->fd_id]);

				continue;
			}
		}
		
		
		for(int i=0;i<buffer_len;i++){
			if(message_len==0){
				char temp[100];
				now = time(nullptr);
				tm_info = localtime(&now);
				//new chatting message
				strftime(tt, 100, "%Y-%m-%d %H:%M:%S", tm_info);
				sprintf(temp,"%s <%s> ",tt,const_cast<char*>(pipe->name.c_str()));
				message_buffer=temp;
				message_len=message_buffer.length();
	
			}

			message_buffer+=buffer[i];
			message_len++;

			//end of reading message
			if(buffer[i]=='\n'){
				//send to every client
				for(int j=0;j<MAX_CLIENT_NUM;j++){
					if(client[j].valid&&client[j].socket!=pipe->socket){
						pthread_mutex_lock(&mutex[j]);
						message_q[j].push(message_buffer);
						pthread_cond_signal(&cv[j]);		//對應到handle_send的con_wait
						pthread_mutex_unlock(&mutex[j]);
					}
				}

				//other new message
				message_len=0;
				message_buffer.clear();
			}
		}
		//clear buffer
		buffer_len=0;
		memset(buffer,0,sizeof(buffer));
		
		cont_reading=true;
	}


	return;
}


void *chat(void *data){
	struct Client *pipe=(struct Client *)data;
	char tt[100];
	time_t now = time(nullptr);

	//printf hello message
	char hello[100];
	//get time
	now = time(nullptr);
	tm* tm_info = localtime(&now);
	strftime(tt, 100, "%Y-%m-%d %H:%M:%S", tm_info);
	
	sprintf(hello,"%s *** Welcome to the simple CHAT server\n%s *** Total %d users online now. Your name is <%s>\n",tt,tt,current_client_num,const_cast<char*>(pipe->name.c_str()));
	pthread_mutex_lock(&mutex[pipe->fd_id]);
	message_q[pipe->fd_id].push(hello);
	pthread_cond_signal(&cv[pipe->fd_id]);
    pthread_mutex_unlock(&mutex[pipe->fd_id]);

	memset(hello,0,sizeof(hello));
	now = time(nullptr);
	tm_info = localtime(&now);
	strftime(tt, 100, "%Y-%m-%d %H:%M:%S", tm_info);
	sprintf(hello,"%s *** User <%s> has just landed on the server\n",tt,const_cast<char*>(pipe->name.c_str()));
	//send message to all the other users
	for(int i=0;i<MAX_CLIENT_NUM;i++){
		if(client[i].valid&&client[i].socket!=pipe->socket){
			pthread_mutex_lock(&mutex[i]);
			message_q[i].push(hello);
			pthread_cond_signal(&cv[i]);
			pthread_mutex_unlock(&mutex[i]);
		}
	}

	// create a new thread to handle send messages for this socket
    pthread_create(&send_thread[pipe->fd_id], NULL, handle_send, (void *)pipe);

    // receive message
    handle_recv(data);

	//handle_recv is return -> user was disconnected
	pthread_mutex_lock(&num_mutex);
	pipe->valid=0;
	current_client_num--;
	pthread_mutex_unlock(&num_mutex);

	//print goodbye message
	printf("* client %s:%d disconnected\n",pipe->address,pipe->port);
	printf("---Online users: %d ---\n\n",current_client_num);
	now = time(nullptr);
	tm_info = localtime(&now);
	strftime(tt, 100, "%Y-%m-%d %H:%M:%S", tm_info);
	char bye[100];
	sprintf(bye,"%s *** User <%s> has left the server\n",tt, const_cast<char*>(pipe->name.c_str()));
	//send offline message to all the other user
	for(int i=0;i<MAX_CLIENT_NUM;i++){
		if(client[i].valid&&client[i].socket!=pipe->socket){
			pthread_mutex_lock(&mutex[i]);
			message_q[i].push(bye);
			pthread_cond_signal(&cv[i]);
			pthread_mutex_unlock(&mutex[i]);
		}
	}
	
	int this_client_id=pipe->fd_id;
	int this_client_socket=pipe->socket;
	/*
	pthread_mutex_lock(&mutex[pipe->fd_id]);
	client[this_client_id].name="";
	client[this_client_id].valid=0;
	client[this_client_id].fd_id=0;
	client[this_client_id].socket=0;
	client[this_client_id].address=NULL;
	client[this_client_id].port=0;
	pthread_mutex_unlock(&mutex[pipe->fd_id]);*/

    pthread_mutex_destroy(&mutex[pipe->fd_id]);
    pthread_cond_destroy(&cv[pipe->fd_id]);
    pthread_cancel(send_thread[pipe->fd_id]);
	

	return NULL;

}


int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in serverAddr;
	int clientSocket;
	struct sockaddr_in cliAddr;
	socklen_t addr_size;
	signal(SIGPIPE, SIG_IGN);

    int opt = 1;
    int addrlen = sizeof(serverAddr);
    char buffer[1024] = { 0 };

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error in connection.\n");
		exit(1);
	}

	printf("Server Socket is created.\n");
    if (setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    int nport=atoi(argv[1]);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(nport);

	if (bind(sockfd,(struct sockaddr*)&serverAddr,sizeof(serverAddr)) < 0) {
		printf("Error in binding.\n");
		exit(1);
	}
	addr_size=sizeof(serverAddr);

	if (listen(sockfd, 100) == 0) {
		printf("Waiting for clients...\n\n");
	}

	// waiting for new client to join in
	while (1) {

		clientSocket = accept(sockfd, (struct sockaddr*)&cliAddr,&addr_size);
		if (clientSocket < 0) {
            perror ("accept");
			if((errno == ENETDOWN || errno == EPROTO || errno == ENOPROTOOPT || errno == EHOSTDOWN ||
				errno == ENONET || errno == EHOSTUNREACH || errno == EOPNOTSUPP || errno == ENETUNREACH)) {
				continue;
			}
			exit (EXIT_FAILURE);
		}

		//update client array
		for(int i=0;i<MAX_CLIENT_NUM;i++){
			if(!client[i].valid){
				pthread_mutex_lock(&num_mutex);
				//set initial random name
				string test_name,test_adj;
				srand(clock());
				int ani_id=rand()%889;
				int adj_id=rand()%3434;
				test_adj=adjs[adj_id];
				test_name=animals[ani_id];
				string my_name=test_adj+" "+test_name;
				//char* c = const_cast<char*>(my_name.c_str());

				client[i].name=my_name;
				client[i].valid=1;
				client[i].fd_id=i;
				client[i].socket=clientSocket;
				client[i].address=inet_ntoa(cliAddr.sin_addr);
				client[i].port=ntohs(cliAddr.sin_port);

				mutex[i] = PTHREAD_MUTEX_INITIALIZER;
                cv[i] = PTHREAD_COND_INITIALIZER;
				current_client_num++;
				
				pthread_mutex_unlock(&num_mutex);
				pthread_create(&chat_thread[i], NULL, chat, (void *)&client[i]);
                printf("* client connected from %s:%d\n",inet_ntoa(cliAddr.sin_addr),ntohs(cliAddr.sin_port));
				printf("---Online users: %d ---\n\n",current_client_num);

				break;
			}
		}

		
	}
	// close socket
    for (int i = 0; i < MAX_CLIENT_NUM; i++)
        if (client[i].valid)
            close(client[i].socket);
    close(sockfd);
	return 0;
}
