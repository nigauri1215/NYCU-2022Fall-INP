#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>


#define err_quit(m) { perror(m); exit(-1); }
#define buf_size 1024
#define offset_size 32
//#define buf_size 32768

/* /server <path-to-store-files> <total-number-of-files> <port>*/

typedef struct packinfo{
    char name[10];
    char content[buf_size];
    int seg_id;
	int file_id;
	int total_size=0;
}Packinfo;

typedef struct ackinfo{
	int file_id;
	int seg_id;
}Ackinfo;

int check_list[1005][offset_size]={0};


char file_tmp[1005][offset_size][32768]={0};
int file_len[1005]={0};
bool file_fin[1005]={0};

int main(int argc, char *argv[]) {
    //alarm(300);
    char path[50]={0};
    strcpy(path,argv[1]);
    int num_of_files=atoi(argv[2]);
    int port=atoi(argv[3]);
	int s;
    ssize_t sn;
	struct sockaddr_in sin;

	if(argc < 2) {
		return -fprintf(stderr, "usage: %s ... <port>\n", argv[0]);
	}

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");

	if(bind(s, (struct sockaddr*) &sin, sizeof(sin)) < 0)
		err_quit("bind");
	long n=23372036854775807;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));


    struct sockaddr_in csin;
    socklen_t csinlen = sizeof(csin);
    
    
   while(1){

		Packinfo pkt;
		Ackinfo ack;

		memset(pkt.content,0,sizeof(pkt.content));
		memset(pkt.name,0,sizeof(pkt.name));
		pkt.seg_id=-1;
		pkt.file_id=-1;
		ack.file_id=-1;
		ack.seg_id=-1;

		if(recvfrom(s, (char*)&pkt, sizeof(pkt), 0, (struct sockaddr*) &csin, &csinlen)){
			ack.seg_id=pkt.seg_id;
			ack.file_id=pkt.file_id;
			sendto(s,(char*)&ack, sizeof(ack), 0,(struct sockaddr*)&csin, sizeof(sin));
		}
		
		if(check_list[pkt.file_id][pkt.seg_id]){
			sendto(s,(char*)&ack, sizeof(ack), 0,(struct sockaddr*)&csin, sizeof(sin));
			continue;
		}
		strcpy(file_tmp[pkt.file_id][pkt.seg_id],pkt.content);
		file_len[pkt.file_id]+=strlen(pkt.content);
		check_list[pkt.file_id][pkt.seg_id]=1;
		
		if(file_len[pkt.file_id]==pkt.total_size && !file_fin[pkt.file_id]){
			
			char target[100];
			memset(target,0,sizeof(target));
			snprintf(target,sizeof(target),"%s/%s",path,pkt.name);
			FILE* p = fopen(target,"a+");
			if(!p) {printf("cant write\n");
				continue;
			}
			for(int j=0;j<offset_size;j++){
				fwrite(file_tmp[pkt.file_id][j],strlen(file_tmp[pkt.file_id][j]),1,p);
			}
			fclose(p);
			file_fin[pkt.file_id]=1;
		}
	
	
}
	close(s);
}