#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#define BUFFER_SIZE 100
char buffer[BUFFER_SIZE] = {0};
char path[] = "/sudoku.sock";

int map[9][9]={0};
int ifsend[9][9]={0};
int sock;

bool isPlace(int count){
	int row = count / 9;
	int col = count % 9;
	int j;
	//同一行
	for(j = 0; j < 9; ++j){
		if(map[row][j] == map[row][col] && j != col){
            //fprintf(stderr,"FAILED\n");
			return false;
		}
	}
	//同一列
	for(j = 0; j < 9; ++j){
		if(map[j][col] == map[row][col] && j != row){
            //fprintf(stderr,"FAILED\n");
			return false;
		}
	}
	//同一小格
	int tempRow = row / 3 * 3;
	int tempCol = col / 3 * 3;
	for(j = tempRow; j < tempRow + 3;++j){
		for(int k = tempCol; k < tempCol + 3; ++k){
			if(map[j][k] == map[row][col] && j != row && k != col){
                //fprintf(stderr,"FAILED\n");
				return false;
			}
		}
	}
	return true;
}
void backtrace(int count){
	if(count == 81){
        for(int i=0;i<9;i++){
            for(int j=0;j<9;j++){
                if(ifsend[i][j]==1){
                    snprintf(buffer,sizeof(buffer),"v %d %d %d",i,j,map[i][j]);
                    send(sock, buffer, strlen(buffer), 0);
                    memset(buffer,0,sizeof(buffer));
                    recv(sock, buffer, BUFFER_SIZE, 0);
                    memset(buffer,0,sizeof(buffer));
                }
            }
        }
        
		return;
	}
	int row = count / 9;
	int col = count % 9;
	if(map[row][col] == 0){
        //fprintf(stderr,"Solve...\n");
		for(int i = 1; i <= 9; ++i){
			map[row][col] = i;//赋值
			if(isPlace(count)){//可以放
				backtrace(count+1);//进入下一层
			}
		}
		map[row][col] = 0;//回溯
	}else{
		backtrace(count+1);
	}
}

int main(void) {
    int clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        perror("create socket fail");
        exit(-1);
    }
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path));

    if (connect(clientSocket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect to server fail");
        exit(-1);
    }
    fprintf(stderr,"Connect success\n");
    //get board
    char current_board[BUFFER_SIZE]={0};
    buffer[0]='s';
    send(clientSocket, buffer, strlen(buffer), 0);
    memset(buffer,0,sizeof(buffer));
    recv(clientSocket, buffer, BUFFER_SIZE, 0);
    //fprintf(stderr,"\n Recv:%s\n",buffer);
    strcpy(current_board,buffer);

    memset(buffer,0,sizeof(buffer));
    //fprintf(stderr,"\n current_board:%s\n",current_board);
    
    
    for(int i=0;i<81;i++){
        if(current_board[i+4]=='.'){
            map[i/9][i%9]=0;
            ifsend[i/9][i%9]=1;
        }
        else{
            map[i/9][i%9]=current_board[i+4]-'0';
        }
    }

    sock=clientSocket;

    backtrace(0);   


    memset(buffer,0,sizeof(buffer));

    fprintf(stderr,"\nEnd solving.\n");

    snprintf(buffer,sizeof(buffer),"c");
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer,0,sizeof(buffer));
    recv(sock, buffer, BUFFER_SIZE, 0);
}