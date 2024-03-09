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
#include <queue>
#include <time.h>
#include <ctime>
#include <map>
#include <vector>
#include <set>

using namespace std;

#define SERV_PORT 10004
#define MAX_CLIENT_NUM 100
//#define FD_SETSIZE 1024
#define NAME_LEN 50
#define BUFFER_LEN 4096
#define	LISTENQ		1024

struct Client{
	int id=-1;		//user id number
	int fd=-1;		//discripter
	string name;		//user name
	char* address;		//user addr
	int port;		//user port
	string cur_ch;

}client[MAX_CLIENT_NUM]={0};

struct Channel{
	string ch_name;
	int users=0;
	string topic;

};

vector<pair<int,string>> ch_map;			//client id,ch name
vector<Channel> ch_list;		//users,ch name
int current_client_num=0;

void nick(char buf[BUFFER_LEN],int i,int sockfd){
	string user_name;
	for(int k=5;k<strlen(buf)-1;k++){
		user_name+=buf[k];
	}
	client[i].name=user_name.c_str();
	char temp[1024];
	string message_buffer;
	sprintf(temp,":mircd 421  CAP :Unknown command\r\n\
:mircd 001 %s :Welcome to the minimized IRC daemon!\r\n\
:mircd 251 %s :There are %d users and 0 invisible on 1 server\r\n\
:mircd 375 %s :- mircd Message of the day -\r\n\
:mircd 372 %s :-  Hello, World!\r\n\
:mircd 372 %s :-               @                    _ \r\n\
:mircd 372 %s :-   ____  ___   _   _ _   ____.     | |\r\n\
:mircd 372 %s :-  /  _ `'_  \\ | | | '_/ /  __|  ___| |\r\n\
:mircd 372 %s :-  | | | | | | | | | |   | |    /  _  |\r\n\
:mircd 372 %s :-  | | | | | | | | | |   | |__  | |_| |\r\n\
:mircd 372 %s :-  |_| |_| |_| |_| |_|   \\____| \\___,_|\r\n\
:mircd 372 %s :-  minimized internet relay chat daemon\r\n\
:mircd 372 %s :-\r\n\
:mircd 376 %s :End of message of the day\r\n",const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,current_client_num
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str())
	,const_cast<char*>(client[i].name.c_str()));
	message_buffer+=temp;
	write(sockfd, message_buffer.c_str(), message_buffer.length());

}

void ping(char buf[BUFFER_LEN],int sockfd){
	string message_buffer="PONG ";
	for(int k=5;k<strlen(buf);k++){
		message_buffer+=buf[k];
	}
	message_buffer+='\n';
	write(sockfd, message_buffer.c_str(), message_buffer.length());
	message_buffer.clear();
	
	return;
}

void list(char buf[BUFFER_LEN],int i,int sockfd){
	char temp[1024];
	string message_buffer;
	sprintf(temp,":mircd 321 %s Channel :Topic\r\n",const_cast<char*>(client[i].name.c_str()));
	message_buffer+=temp;
	for (int j=0;j<ch_list.size();j++) {
		sprintf(temp,":mircd 322 %s #%s %d :%s\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(ch_list[j].ch_name.c_str()),ch_list[j].users,const_cast<char*>(ch_list[j].topic.c_str()));
		message_buffer+=temp;
		
	}
	sprintf(temp,":mircd 323 %s :End of List\r\n",const_cast<char*>(client[i].name.c_str()));
	message_buffer+=temp;
	write(sockfd, message_buffer.c_str(), message_buffer.length());

	message_buffer.clear();
	return;
}

void join(char buf[BUFFER_LEN],int i,int sockfd){
	string channel_name;
	for(int k=5;k<strlen(buf)-1;k++){
		channel_name+=buf[k];
	}

	char temp[1024];
	string message_buffer;
	for(int j=0;j<ch_list.size();j++){
		if(ch_list[j].ch_name==channel_name && client[i].cur_ch!=channel_name){
			cout<<"Join channel "<<ch_list[j].ch_name<<"\n";
			ch_list[j].users++;
			client[i].cur_ch=channel_name;
			ch_map.push_back(pair<int,string>(i,channel_name));

			sprintf(temp,":%s JOIN %s \r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()));
			message_buffer+=temp;
			write(sockfd, message_buffer.c_str(), message_buffer.length());

			message_buffer.clear();

			return;
		}
		else if(ch_list[j].ch_name==channel_name && client[i].cur_ch==channel_name){
			
			sprintf(temp,"Already in channel #%s \r\n",const_cast<char*>(channel_name.c_str()));
			message_buffer+=temp;
			write(sockfd, message_buffer.c_str(), message_buffer.length());

			message_buffer.clear();
			return;
		}
	}

	cout<<"Add new channel\n"<<endl;
	Channel new_ch;
	new_ch.ch_name=channel_name;
	new_ch.topic="";
	new_ch.users=1;
	ch_list.push_back(new_ch);

	client[i].cur_ch=channel_name;
	ch_map.push_back(pair<int,string>(i,channel_name));

	sprintf(temp,":%s JOIN %s \r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()));
	message_buffer+=temp;
	write(sockfd, message_buffer.c_str(), message_buffer.length());

	message_buffer.clear();
	return;

}

void part(char buf[BUFFER_LEN],int i,int sockfd){
	string channel_name;
	for(int k=6;buf[k]!=' ';k++){
		channel_name+=buf[k];
	}

	if(client[i].cur_ch==channel_name){
		client[i].cur_ch="";
	}

	for(int j=0;j<ch_list.size();j++){
		if(ch_list[j].ch_name==channel_name){
			cout<<client[i].name<<" leaving "<<channel_name<<"\n";	//client i leave channel
			if(ch_list[j].users>0)
				ch_list[j].users--;
			break;
		}
	}

	//TODO:erase
	vector<pair<int,string>>::iterator it;

	for(it=ch_map.begin();it!=ch_map.end();){
		if(it->second==channel_name){
			it=ch_map.erase(it);
		}
		else
			it++;
	}
	
	char temp[1024];
	string message_buffer;
	sprintf(temp,":%s PART :#%s\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()));
	message_buffer+=temp;
	write(sockfd, message_buffer.c_str(), message_buffer.length());

	message_buffer.clear();

	return;
	

}

void topic(char buf[BUFFER_LEN],int i,int sockfd){
	//TOPIC <channel> [<topic>]
	string channel_name;
	string topic_str;
	int index;
	bool show=true;
	for(int k=7;buf[k]!='\n';k++){
		if(buf[k]==' '){
			show=false;
			break;
		}
		channel_name+=buf[k];
		index=k;
	}
	if(show){
		//show the topic
		string tpc;
		for(int p=0;p<ch_list.size();p++){
			if(ch_list[p].ch_name==channel_name){
				tpc=ch_list[p].topic;
				break;
			}
		}
		char tmp[1024];
		string message_buffer;
		if(tpc==""){
			sprintf(tmp,":mircd 332 %s #%s :No topic is set\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()));
		}
		else{
			sprintf(tmp,":mircd 332 %s #%s :%s\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()),const_cast<char*>(tpc.c_str()));
		}
		message_buffer+=tmp;
		write(sockfd, message_buffer.c_str(), message_buffer.length());
		return;
	}
	index+=3;
	for(int k=index;buf[k]!='\n';k++){
		topic_str+=buf[k];
	}
	for(int j=0;j<ch_list.size();j++){
		if(ch_list[j].ch_name==channel_name){
			ch_list[j].topic=topic_str;
			break;
		}
	}
	char temp[1024];
	string message_buffer;
	sprintf(temp,":mircd 332 %s #%s :%s\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()),const_cast<char*>(topic_str.c_str()));
	message_buffer+=temp;
	write(sockfd, message_buffer.c_str(), message_buffer.length());

	message_buffer.clear();
	return;

}

void users(char buf[BUFFER_LEN],int i,int sockfd){
	char temp[1024];
	string message_buffer;
	sprintf(temp,":mircd 392 %s :UserID					Terminal  Host\r\n",const_cast<char*>(client[i].name.c_str()));
	message_buffer+=temp;
	for(int k=0;k<MAX_CLIENT_NUM;k++){
		if(client[k].fd>0){
			sprintf(temp,":mircd 393 %s :%s					-	%s\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(client[k].name.c_str()),client[k].address);
			message_buffer+=temp;
		}
	}
	sprintf(temp,":mircd 394 %s :End of users\r\n",const_cast<char*>(client[i].name.c_str()));
	message_buffer+=temp;
	write(sockfd, message_buffer.c_str(), message_buffer.length());

	message_buffer.clear();

	return;
}

void quit(int i,int sockfd){
	client[i].fd=-1;
	client[i].name="";
	client[i].id=-1;
	client[i].address=NULL;
	client[i].port=0;
	current_client_num--;

	vector<pair<int,string>>::iterator it;
	for(it=ch_map.begin();it!=ch_map.end();){
		if(it->first==i){
			for(int k=0;k<ch_list.size();k++){
				if(ch_list[k].ch_name==it->second){
					ch_list[k].users--;
				}
			}
			
			it=ch_map.erase(it);
		}
		else
			it++;
	}


}

void privmsg(char buf[BUFFER_LEN],int i,int sockfd){
	string channel_name;
	string msg_str;
	int index;
	for(int k=9;buf[k]!=' ';k++){
		channel_name+=buf[k];
		index=k;
	}
	index+=3;
	for(int k=index;buf[k]!='\n';k++){
		msg_str+=buf[k];
	}

	char temp[1024];
	string message_buffer;
	//username,channelname,message
	sprintf(temp,":%s PRIVMSG #%s :%s\r\n",const_cast<char*>(client[i].name.c_str()),const_cast<char*>(channel_name.c_str()),const_cast<char*>(msg_str.c_str()));
	message_buffer+=temp;
	//send to all users in same channel
	vector<pair<int,string>>::iterator it;
	vector<int>user;
	for(it=ch_map.begin();it!=ch_map.end();it++){
		if(it->second==channel_name && it->first!=i){
			user.push_back(it->first);
			cout<<"now in channel: "<<it->first<<endl;
		}
	}

	for(int j=0;j<user.size();j++){
		int index=user[j];

		if(client[index].fd>0){
			write(client[index].fd, message_buffer.c_str(), message_buffer.length());
		}

	}



}

void debug(int i,int sockfd){
	cout<<endl;
	cout<<endl;
	cout<<"===============Debug Area=============\n";
	cout<<"Current user: "<<client[i].name<<" id:"<<client[i].id<<" current channel: "<<client[i].cur_ch<<endl;
	cout<<endl;
	//users
	cout<<"-------------user list-------------\n";
	for(int j=0;j<MAX_CLIENT_NUM;j++){
		if(client[j].fd>0){
			cout<<"user name:"<<client[j].name<<" user id:"<< client[j].id<<" current channel: "<<client[j].cur_ch <<endl;
		}
	}

	//channel
	cout<<"-------------Channel list-------------\n";
	for(int j=0;j<ch_list.size();j++){
		cout<<"channel name: "<<ch_list[j].ch_name <<" online users: "<<ch_list[j].users<<" topic:"<<ch_list[j].topic<<endl;
	}
	//client - channel=
	cout<<"-------------Client/Channel-------------\n";
	for(int j=0;j<ch_map.size();j++){
		cout<<"client id: "<<ch_map[j].first <<" channel: "<<ch_map[j].second<<endl;
	}

	cout<<"===============Debug End==============\n";
	cout<<endl;
	cout<<endl;

}

int main(int argc, char **argv)
{
	int					i, maxi, maxfd, listenfd, connfd, sockfd;
	int					nready;
	ssize_t				n;
	fd_set				rset, allset;
	char				buf[BUFFER_LEN];
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(SERV_PORT);

	bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);

	maxfd = listenfd;			/* initialize */
	maxi = -1;					/* index into client[] array */
	//for (i = 0; i < FD_SETSIZE; i++){
	//	client[i].fd_id=-1;			/* -1 indicates available entry */

	//}
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);


	for ( ; ; ) {
		rset = allset;		/* structure assignment */
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);

		if (FD_ISSET(listenfd, &rset)) {	/* new client connection */
			clilen = sizeof(cliaddr);
			connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);

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
			if ( (sockfd = client[i].fd) < 0)
				continue;
			if (FD_ISSET(sockfd, &rset)) {
				/*read data from client*/
				n = read(sockfd, buf, BUFFER_LEN);
				string cmd="";

				for(int i=0;i<n;i++){
					cmd+=buf[i];
				}
				int trans_len=cmd.length();

				/*NICK <nickname>:change name*/
				if(cmd.substr(0,4)=="NICK"){
					nick(buf,i,sockfd);
				}
				/*USER <username> <hostname> <servername> <realname>*/
				if(cmd.substr(0,4)=="USER"){
					/*ignore*/
				}
				/*PING <server1> [<server2>]*/
				if(cmd.substr(0,4)=="PING"){
					ping(buf,sockfd);
				}
				/*LIST [<channel>]*/
				if(cmd.substr(0,4)=="LIST"){
					list(buf,i,sockfd);
				}
				/*JOIN <channel>*/
				if(cmd.substr(0,4)=="JOIN"){
					join(buf,i,sockfd);
				}
				/*TOPIC <channel> [<topic>]*/
				if(cmd.substr(0,5)=="TOPIC"){
					topic(buf,i,sockfd);
				}
				/*NAMES [<channel>]*/
				/*PART <channel>*/
				if(cmd.substr(0,4)=="PART"){
					part(buf,i,sockfd);
				}
				/*USERS*/
				if(cmd.substr(0,5)=="USERS"){
					users(buf,i,sockfd);
				}
				/*PRIVMSG <channel> <message>*/
				if(cmd.substr(0,7)=="PRIVMSG"){
					privmsg(buf,i,sockfd);
				}
				/*QUIT*/
				if(cmd.substr(0,4)=="QUIT"){
					quit(i,sockfd);
					close(sockfd);
					FD_CLR(sockfd, &allset);
					
				}
				/*DEBUG*/
				if(cmd.substr(0,5)=="DEBUG"){
					debug(i,sockfd);
				}

				memset(buf,0,sizeof(buf));


				if (--nready <= 0)
					break;				/* no more readable descriptors */
			}
		}
	}
}
