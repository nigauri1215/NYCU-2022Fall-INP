#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include<cstdlib>
#include<string.h>
#include<math.h>
#include<map>
#include<iostream>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include<sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<errno.h>
#include<bitset>
using namespace std;
#define MAXLINE 10000
char buf[MAXLINE];

string strtopkt(string name){
    int n=name.length();
    string out;
    int start=0;
    int end=0;
    int len=0;
    while(name[end]!='\0'){
        if(name[end]!='.'){
            end++;
            len++;
            
        }
        else{
            cout<<"s:"<<start<<endl;
            cout<<"e:"<<end<<endl;
            cout<<"len:"<<len<<endl;
            char charValue=len;
            out+=charValue;
            cout<<"out:"<<out<<endl;
            cout<<"sub:"<<name.substr(start,len)<<endl;
            out.append(name.substr(start,len));
            cout<<"result:"<<out<<endl;
            
            end++;
            start=end;
            len=0;
        }
    }
    out+='\0';
    

    return out;
}

int main(){
        char question[50];
        string domain="appple.ball.www.example1.org.";
        cout<<strtopkt(domain)<<endl;
        
}