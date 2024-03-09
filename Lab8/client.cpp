#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<math.h>


//#define buf_size 32768
#define buf_size 1024
#define offset_size 32

int offset=1024;
/*/client <path-to-read-files> <total-number-of-files> <port> <server-ip-address>*/
char file_name[buf_size]={0};
char file_data[35000]={0};

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


void strmncpy(char* s, int m, int n, char* t)
{
    char* p = s;
    char* q = t;
    int a = 0;
    p = p + m;
    while (a < n)
    {
        *q++ = *p++;
        a++;
    }
    *q = '\0';

}


int main(int argc, char *argv[]) {
    char path[50];
    strcpy(path,argv[1]);
	const char* server_name = "localhost";
	const int server_port = atoi(argv[3]);
    int num_of_file=atoi(argv[2]);
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET, server_name, &server_address.sin_addr);
    server_address.sin_port = htons(server_port);
    int sock;

	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("could not create socket\n");
		return 1;
	}
	struct timeval read_timeout;
	read_timeout.tv_sec = 0;
	read_timeout.tv_usec = 1;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);
	struct sockaddr_in csin;
	socklen_t csinlen = sizeof(csin);

	
	while(1){
			for(int i=0;i<num_of_file;i++){
				char target[100];
				memset(target,0,sizeof(target));
				snprintf(target,sizeof(target),"%s/%06d",path,i);
				FILE *p = fopen(target,"rb");
				double bound,sz;
			
				fseek(p, 0L, SEEK_END);
				sz = ftell(p);
				fseek(p, 0L, SEEK_SET);
				if(!p) {
					printf("cant read\n");
					continue;
				}
				memset(file_name,0,sizeof(file_name));
				snprintf(file_name,sizeof(file_name),"%06d",i);
				memset(file_data,0,sizeof(file_data));
				fread(file_data,sizeof(file_data),1,p);

				Packinfo pkt;
				Ackinfo ack;
				memset(pkt.content,0,sizeof(pkt.content));
				memset(pkt.name,0,sizeof(pkt.name));
				pkt.seg_id=-1;
				pkt.file_id=-1;
				ack.file_id=-1;
				ack.seg_id=-1;
				strcpy(pkt.name,file_name);

				for(int j=0;j<sz;j+=offset){
					if(check_list[i][j/offset]==1){
						continue;
					}
					memset(pkt.content,0,sizeof(pkt.content));
					ack.seg_id=-1;
					ack.file_id=-1;
					
					pkt.file_id=i;
					pkt.seg_id=j/offset;
					pkt.total_size=sz;
					strmncpy(file_data,j,offset,pkt.content);
					sendto(sock, (char*)&pkt, sizeof(pkt), 0,(struct sockaddr*)&server_address, sizeof(server_address));
					if(recvfrom(sock, (char*)&ack, sizeof(ack), 0, (struct sockaddr*) &server_address, &csinlen)>0){
						if(ack.file_id>=0 && ack.seg_id>=0){
							check_list[ack.file_id][ack.seg_id]=1;
						}
					}
				}

				fclose(p);
			}
			
		
    }

	return 0;
}