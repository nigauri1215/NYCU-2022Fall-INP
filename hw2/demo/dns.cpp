#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include <sstream>

using namespace std;

#define err_quit(m) { perror(m); exit(-1); }
#define buf_size 1024
#define offset_size 32
#define DNS_HOST			0x01
#define DNS_CNAME			0x05
typedef unsigned char u_int8_t;

string foreign_server;
vector<string> my_domain;
vector<string> zonefile_path;

typedef struct header{
    unsigned short id;
    unsigned short flags;
    unsigned short questions;
    unsigned short answers;
    unsigned short authority;
    unsigned short additional;
}DNS_HEADER;

typedef struct question{
    unsigned short qtype;
    unsigned short qclass;
}DNS_QUESTION;

typedef struct response_header{
    int16_t ID;
    uint16_t flags;
    uint16_t QDCOUNT;
    uint16_t ANCOUNT;
    uint16_t NSCOUNT;
    uint16_t ARCOUNT;
}__attribute__((packed)) response_header;

typedef struct response_question{
    char name[50];
    int name_len;
    uint16_t qtype;
    uint16_t qclass;
}response_question;


typedef struct soa_auth
{
    char name[50];
    int name_len;
    uint16_t type;
    uint16_t classes;
    uint32_t ttl;
    uint16_t length;
    char primary_name[50];
    int primary_len;
    char auth_mailbox[50];
    int auth_len;
    uint32_t serial_num;
    uint32_t refresh_interval;
    uint32_t retry;
    uint32_t expire_limit;
    uint32_t min_ttl;
} SOA_AUTH;

typedef struct ans
{
    int ans_name_len;
    char sub_ans_name[50];
    uint16_t type;
    uint16_t classes;
    uint32_t ttl;
    uint16_t length;
    uint32_t address; 
    int addr_len;
} ANS;

typedef struct ans_ipv6
{
    int ans_name_len;
    char sub_ans_name[50];
    uint16_t type;
    uint16_t classes;
    uint32_t ttl;
    uint16_t length;
    unsigned char address[16]; 
    int addr_len;
} ANS_IPV6;

typedef struct auth
{
    char sub_auth_name[50];
    int auth_len;
    uint16_t type;
    uint16_t classes;
    uint32_t ttl;
    uint16_t length;
    char name_server[50];
    int server_len;
    uint8_t txt_len;
} AUTH;


int isthis_pointer(int in) {
	return ((in & 0xC0) == 0xC0);
}


void dns_parse_name(char *chunk, char *ptr, char *out, int *len) {

	int flag = 0, n = 0, alen = 0;
	char *pos = out + (*len);

	while (1) {

		flag = (int)ptr[0];
		if (flag == 0) break;

		if (isthis_pointer(flag)) {
			
			n = (int)ptr[1];
			ptr = chunk + n;
			dns_parse_name(chunk, ptr, out, len);
			break;
			
		} else {

			ptr ++;
			memcpy(pos, ptr, flag);
			pos += flag;
			ptr += flag;

			*len += flag;
			if ((int)ptr[0] != 0) {
				memcpy(pos, ".", 1);
				pos += 1;
				(*len) += 1;
			}
		}
	
	}
	
}

string strtopkt(string name){
    int n=name.length();
    string out;
    int start=0;
    int end=0;
    int len=0;
    while(name[end]!='\0' && name[end]!='\r' && name[end]!='\n' ){
        if(name[end]!='.'){
            end++;
            len++;
        }
        else{
            size_t charValue=len;
            out+=charValue;
            out+=name.substr(start,len);
            
            end++;
            start=end;
            len=0;
        }
    }
    
    out+='\0';

    return out;
}


int main(int argc, char *argv[]) {
    char config_path[50]={0};
    strcpy(config_path,argv[2]);
	int s;
    ssize_t sn;
	struct sockaddr_in sin;
    int port=atoi(argv[1]);

	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);

	if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		err_quit("socket");

    int reuseaddr=1;
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr))<0)
        err_quit("setsocket");
	if(bind(s, (struct sockaddr*) &sin, sizeof(sin)) < 0)
		err_quit("bind");
	long n=1024;
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));


    struct sockaddr_in csin;
    socklen_t csinlen = sizeof(csin);

    ifstream fp;
    fp.open(config_path);
    if(!fp.is_open()) {
        printf("cant read\n");
    }
    char line[128];
    int first_line=0;
    while (!fp.eof()) {
        fp.getline(line, sizeof(line));
        if(!first_line){
            foreign_server=line;
            first_line=1;
        }
        else{
            int current = 0;
            string tmp="";
            for(int i=0;line[i]!=',';i++){
                tmp+=line[i];
                current++;
            }
            my_domain.push_back(tmp);
            tmp="";
            for(int i=current+1;line[i]!='\0';i++){
                tmp+=line[i];
            }
            zonefile_path.push_back(tmp);
        }
    }
    fp.close();
    

    
   while(1){
        
        DNS_HEADER header;
        DNS_QUESTION question;
        bool is_my_domain=0;
        char buffer[1024]={0};
        int len=recvfrom(s, buffer, sizeof(buffer), 0, (struct sockaddr*) &csin, &csinlen);   //recv UDP payload length

        char tmp[100]={0};
        int j=0;
        memcpy(&header,buffer,sizeof(header));

        
        for(int i=sizeof(header);i<len;i++){
            if(buffer[i]=='\0') break;
            tmp[j]=buffer[i];
            j++;
        }
        
        //parse name
        char *ptr = tmp;
        int qname_len=0;
        char aname[128];
        bzero(aname, sizeof(aname));
        dns_parse_name(tmp, ptr, aname, &qname_len);
        aname[qname_len]='.';
        qname_len++;
        //cout<<aname<<endl;
        
        //qtype qclass
        char q[100]={0};
        j=0;
        for(int i=sizeof(header)+(qname_len)+1;i<len;i++){
            q[j]=buffer[i];
            j++;
        }
        memcpy(&question,q,sizeof(question));
        
        //cout<<aname<<endl;
        //查詢domain name
        int index=0;
        //string parse_name=aname;
        for(int i=0;i<my_domain.size();i++){
            char *pch;
            //cout<<my_domain[i]<<endl;
            //cout<<aname<<endl;
            pch=strstr(aname,const_cast<char*>(my_domain[i].c_str()));
            if(pch!=NULL){
                index=i;
                is_my_domain=1;
                break;
            }
        }

        if(is_my_domain){

            //zone file
            vector<string>zone_src;
            ifstream zp;
            string zone_path="";
            //zone_path.append("/home/sense/Desktop/hw2/");
            zone_path.append("/mnt/d/inp/hw2/demo/");
            zone_path.append(zonefile_path[index]);
            if(zone_path[zone_path.length()-1]=='\r')
                zone_path.pop_back();
            //cout<<zone_path<<endl;
            zp.open(zone_path);
            if(!zp.is_open()) {
                cerr << "Error: " << strerror(errno);
            }
            char line[128];
            int first_line=0;
            while (!zp.eof()) {
                zp.getline(line, sizeof(line));
                if(!first_line){
                    first_line=1;
                }
                else{
                    zone_src.push_back(line);
                }
            }
            zp.close();
            
            if(ntohs(question.qtype)==1){  //A
                //sub-domian
                //cout<<"sub domain"<<endl;
                //vector<vector<string>>split;
                ANS sub_answer;
                AUTH sub_auth;
                
                vector<vector<string>>split_with_my_domain;
                vector<vector<string>>split_with_sub_domain;
                
                for(int i=0;i<zone_src.size();i++){
                    if(zone_src[i][0]!='@'){
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_sub_domain.push_back(line);
                    }
                    else{
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_my_domain.push_back(line);
                    }
                }
                
                char* server_name;
                for(int i=0;i<split_with_my_domain.size();i++){
                    if(split_with_my_domain[i][3]=="NS"){
                        cout<<"match"<<endl;
                        server_name=const_cast<char*>(split_with_my_domain[i][4].c_str());    //dns.example1.org.
                        cout<<server_name<<endl;
                        break;
                    }
                }
                string server_name_to_send=server_name;
                const char* d = ".";
                char *auth_domain;
                int split_index_auth;
                
                auth_domain = strtok(server_name, d);    //dns
                for(int i=0;i<split_with_sub_domain.size();i++){
                    if(split_with_sub_domain[i][0]==auth_domain){
                        split_index_auth=i;
                        break;
                    }
                }
                char *sub_domain;
                sub_domain = strtok(aname, d);
                int split_index_sub;
                bool if_soa=true;
                for(int i=0;i<split_with_sub_domain.size();i++){
                    if(split_with_sub_domain[i][0]==sub_domain && split_with_sub_domain[i][3]=="A"){
                        //cout<<"match"<<endl;
                        //cout<<split[i][0]<<endl;
                        split_index_sub=i;
                        if_soa=0;
                        break;
                    }
                }
                
                if(!if_soa){
                    //SUB ANS
                    string sub_domain_name=sub_domain;
                    sub_domain_name+='.';
                    sub_domain_name+=my_domain[index];
                    //sub_answer.sub_ans_name=const_cast<char*>(strtopkt(sub_domain_name).c_str());
                    sprintf(sub_answer.sub_ans_name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                    
                    sub_answer.ans_name_len=strtopkt(sub_domain_name).length();
                    sub_answer.type=htons(0x0001);
                    sub_answer.classes=htons(0x0001);
                    sub_answer.ttl=htonl(stoi(split_with_sub_domain[split_index_sub][1]));
                    split_with_sub_domain[split_index_sub][4].pop_back();
                    const char* ip_address =split_with_sub_domain[split_index_sub][4].c_str();
                    
                    struct in_addr addr;
                    char str[INET_ADDRSTRLEN];
                    //printf("%x\n",ip_address);
                    int ret =inet_pton(AF_INET,ip_address , (void *)&addr);
                    //printf("%x\n",addr.s_addr);
                    sub_answer.address=addr.s_addr;
                    sub_answer.length=htons(sizeof(uint32_t));
                    sub_answer.addr_len=sizeof(uint32_t);

                    //SUB AUTH
                    sprintf(sub_auth.sub_auth_name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    sub_auth.auth_len=strtopkt(my_domain[index]).length();
                    sub_auth.type=htons(0x0002);    //ns
                    sub_auth.classes=htons(0x0001);
                    sub_auth.ttl=htonl(stoi(split_with_sub_domain[split_index_auth][1]));
                    
                    cout<<server_name_to_send<<endl;
                    sprintf(sub_auth.name_server,"%s",const_cast<char*>(strtopkt(server_name_to_send).c_str()));
                    sub_auth.server_len=strtopkt(server_name_to_send).length();
                    sub_auth.length=htons(strtopkt(server_name_to_send).length());
                    

                    response_header res_header;
                    res_header.ID=header.id;
                    res_header.flags=htons(0x8500);
                    res_header.QDCOUNT=htons(0x0001);
                    res_header.ANCOUNT=htons(0x0001);
                    res_header.NSCOUNT=htons(0x0001);
                    res_header.ARCOUNT=htons(0x0000);

                    response_question res_query;
                    //res_query.name=const_cast<char*>(strtopkt(sub_domain_name).c_str());
                    sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                    res_query.name_len=strtopkt(sub_domain_name).length();
                    res_query.qtype=htons(0x0001);
                    res_query.qclass=htons(0x0001);

                    char send_buffer[1024]={0};
                    char *send_p=send_buffer;
                    int offset=0;
                    memcpy(send_p,&res_header,sizeof(res_header));
                    offset+=sizeof(res_header);
                    memcpy(send_p+offset,&res_query.name,res_query.name_len);
                    offset+=res_query.name_len;
                    memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                    offset+=sizeof(res_query.qtype);
                    memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                    offset+=sizeof(res_query.qclass);
                    
                    memcpy(send_p+offset,&sub_answer.sub_ans_name,sub_answer.ans_name_len);
                    offset+=sub_answer.ans_name_len;
                    memcpy(send_p+offset,&sub_answer.type,sizeof(sub_answer.type));
                    offset+=sizeof(sub_answer.type);
                    memcpy(send_p+offset,&sub_answer.classes,sizeof(sub_answer.classes));
                    offset+=sizeof(sub_answer.classes);
                    memcpy(send_p+offset,&sub_answer.ttl,sizeof(sub_answer.ttl));
                    offset+=sizeof(sub_answer.ttl);
                    memcpy(send_p+offset,&sub_answer.length,sizeof(sub_answer.length));
                    offset+=sizeof(sub_answer.length);
                    memcpy(send_p+offset,&sub_answer.address,sub_answer.addr_len);
                    offset+=sub_answer.addr_len;

                    memcpy(send_p+offset,&sub_auth.sub_auth_name,sub_auth.auth_len);
                    offset+=sub_auth.auth_len;
                    memcpy(send_p+offset,&sub_auth.type,sizeof(sub_auth.type));
                    offset+=sizeof(sub_auth.type);
                    memcpy(send_p+offset,&sub_auth.classes,sizeof(sub_auth.classes));
                    offset+=sizeof(sub_auth.classes);
                    memcpy(send_p+offset,&sub_auth.ttl,sizeof(sub_auth.ttl));
                    offset+=sizeof(sub_auth.ttl);
                    memcpy(send_p+offset,&sub_auth.length,sizeof(sub_auth.length));
                    offset+=sizeof(sub_auth.length);
                    memcpy(send_p+offset,&sub_auth.name_server,sub_auth.server_len);
                    offset+=sub_auth.server_len;



                    sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));
                    
                    

                }
                else{
                    cout<<"SOA"<<endl;
                    SOA_AUTH soa_auth;
                    vector<string>split;
                    string info=zone_src[0];
                    const char* d = "  ,";
                    char *p;
                    p = strtok(const_cast<char*>(info.c_str()), d);
                    
                    while (p != NULL) {
                        //printf("%s\n", p);
                        split.push_back(p);
                        p = strtok(NULL, d);		   
                    }

                    

                    //soa_auth.name=const_cast<char*>(strtopkt(my_domain[index]).c_str());
                    sprintf(soa_auth.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    soa_auth.name_len=strtopkt(my_domain[index]).length();
                    soa_auth.type=htons(0x0006);
                    soa_auth.classes=htons(0x0001);
                    soa_auth.ttl=htonl(stoi(split[1]));
                    

                    int rdlength=0;
                    //soa_auth.primary_name = const_cast<char*>(strtopkt(split[4]).c_str());
                    sprintf(soa_auth.primary_name,"%s",const_cast<char*>(strtopkt(split[4]).c_str()));
                    soa_auth.primary_len=strtopkt(split[4]).length();
                    rdlength+=strtopkt(split[4]).length();

                    //soa_auth.auth_mailbox = const_cast<char*>(strtopkt(split[5]).c_str());
                    sprintf(soa_auth.auth_mailbox,"%s",const_cast<char*>(strtopkt(split[5]).c_str()));
                    soa_auth.auth_len=strtopkt(split[5]).length();
                    rdlength+=strtopkt(split[5]).length();

                    soa_auth.serial_num=htonl(stoi(split[6]));
                    soa_auth.refresh_interval=htonl(stoi(split[7]));
                    soa_auth.retry=htonl(stoi(split[8]));
                    soa_auth.expire_limit=htonl(stoi(split[9]));
                    soa_auth.min_ttl=htonl(stoi(split[10]));

                    rdlength+=sizeof(uint32_t)*5;
                    soa_auth.length=htons(rdlength);
                    
                    response_header res_header;
                    res_header.ID=header.id;
                    res_header.flags=htons(0x8500);
                    res_header.QDCOUNT=htons(0x0001);
                    res_header.ANCOUNT=htons(0x0000);
                    res_header.NSCOUNT=htons(0x0001);
                    res_header.ARCOUNT=htons(0x0000);
                    
                    response_question res_query;
                    
                    if(qname_len>my_domain[index].length()){
                        string sub_domain_name=sub_domain;
                        sub_domain_name+='.';
                        sub_domain_name+=my_domain[index];
                        //res_query.name=const_cast<char*>(strtopkt(sub_domain_name).c_str());
                        sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                        res_query.name_len=strtopkt(sub_domain_name).length();
                        res_query.qtype=htons(0x0001);
                        res_query.qclass=htons(0x0001);
                    }
                    else{
                        //res_query.name=const_cast<char*>(strtopkt(my_domain[index]).c_str());
                        sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                        res_query.name_len=strtopkt(my_domain[index]).length();
                        res_query.qtype=htons(0x0001);
                        res_query.qclass=htons(0x0001);
                    }

                    char send_buffer[1024]={0};
                    char *send_p=send_buffer;
                    int offset=0;
                    memcpy(send_p,&res_header,sizeof(res_header));
                    offset+=sizeof(res_header);

                    
                    memcpy(send_p+offset,&res_query.name,res_query.name_len);
                    offset+=res_query.name_len;
                    memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                    offset+=sizeof(res_query.qtype);
                    memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                    offset+=sizeof(res_query.qclass);

                    memcpy(send_p+offset,&soa_auth.name,soa_auth.name_len);
                    offset+=soa_auth.name_len;
                    memcpy(send_p+offset,&soa_auth.type,sizeof(soa_auth.type));
                    offset+=sizeof(soa_auth.type);
                    memcpy(send_p+offset,&soa_auth.classes,sizeof(soa_auth.classes));
                    offset+=sizeof(soa_auth.classes);
                    memcpy(send_p+offset,&soa_auth.ttl,sizeof(soa_auth.ttl));
                    offset+=sizeof(soa_auth.ttl);
                    memcpy(send_p+offset,&soa_auth.length,sizeof(soa_auth.length));
                    offset+=sizeof(soa_auth.length);
                    memcpy(send_p+offset,&soa_auth.primary_name,soa_auth.primary_len);
                    offset+=soa_auth.primary_len;
                    memcpy(send_p+offset,&soa_auth.auth_mailbox,soa_auth.auth_len);
                    offset+=soa_auth.auth_len;
                    memcpy(send_p+offset,&soa_auth.serial_num,sizeof(soa_auth.serial_num));
                    offset+=sizeof(soa_auth.serial_num);
                    memcpy(send_p+offset,&soa_auth.refresh_interval,sizeof(soa_auth.refresh_interval));
                    offset+=sizeof(soa_auth.refresh_interval);
                    memcpy(send_p+offset,&soa_auth.retry,sizeof(soa_auth.retry));
                    offset+=sizeof(soa_auth.retry);
                    memcpy(send_p+offset,&soa_auth.expire_limit,sizeof(soa_auth.expire_limit));
                    offset+=sizeof(soa_auth.expire_limit);
                    memcpy(send_p+offset,&soa_auth.min_ttl,sizeof(soa_auth.min_ttl));
                    offset+=sizeof(soa_auth.min_ttl);


                    sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));


                }
            }
            if(ntohs(question.qtype)==2){   //NS
                ANS ns_addi;
                AUTH ns_ans;
                vector<vector<string>>split_with_my_domain;
                vector<vector<string>>split_with_sub_domain;
                for(int i=0;i<zone_src.size();i++){
                    if(zone_src[i][0]!='@'){
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_sub_domain.push_back(line);
                    }
                    else{
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_my_domain.push_back(line);
                    }
                }
                
                char* server_name;
                bool if_soa=true;
                for(int i=0;i<split_with_my_domain.size();i++){
                    if(split_with_my_domain[i][3]=="NS"){
                        cout<<"match"<<endl;
                        server_name=const_cast<char*>(split_with_my_domain[i][4].c_str());    //dns.example1.org.
                        cout<<server_name<<endl;
                        if_soa=0;
                        break;
                    }
                }
                const char* d = ".";
                char *sub_domain;
                int split_index;
                sub_domain = strtok(server_name, d);    //dns
                for(int i=0;i<split_with_sub_domain.size();i++){
                    if(split_with_sub_domain[i][0]==sub_domain){
                        split_index=i;
                        if_soa=0;
                        break;
                    }
                }

                if(1){
                    string sub_domain_name=sub_domain;
                    sub_domain_name+='.';
                    sub_domain_name+=my_domain[index];
                    sprintf(ns_addi.sub_ans_name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                    
                    
                    //additional: dns.example1.org address
                    ns_addi.ans_name_len=strtopkt(sub_domain_name).length();
                    ns_addi.type=htons(0x0001);
                    ns_addi.classes=htons(0x0001);
                    ns_addi.ttl=htonl(stoi(split_with_sub_domain[split_index][1]));
                    split_with_sub_domain[split_index][4].pop_back();
                    const char* ip_address =split_with_sub_domain[split_index][4].c_str();
                    
                    struct in_addr addr;
                    char str[INET_ADDRSTRLEN];
                    int ret =inet_pton(AF_INET,ip_address , (void *)&addr);
                    ns_addi.address=addr.s_addr;
                    ns_addi.length=htons(sizeof(uint32_t));
                    ns_addi.addr_len=sizeof(uint32_t);

                    //answer: example1.org name server
                    sprintf(ns_ans.sub_auth_name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    ns_ans.auth_len=strtopkt(my_domain[index]).length();
                    ns_ans.type=htons(0x0002);    //ns
                    ns_ans.classes=htons(0x0001);
                    ns_ans.ttl=htonl(stoi(split_with_my_domain[split_index][1]));
                    
                    sprintf(ns_ans.name_server,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                    ns_ans.server_len=strtopkt(sub_domain_name).length();
                    ns_ans.length=htons(strtopkt(sub_domain_name).length());
                    

                    response_header res_header;
                    res_header.ID=header.id;
                    res_header.flags=htons(0x8500);
                    res_header.QDCOUNT=htons(0x0001);
                    res_header.ANCOUNT=htons(0x0001);
                    res_header.NSCOUNT=htons(0x0000);
                    res_header.ARCOUNT=htons(0x0001);

                    response_question res_query;
                    sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    res_query.name_len=strtopkt(my_domain[index]).length();
                    res_query.qtype=htons(0x0002);  //TYPE:NS
                    res_query.qclass=htons(0x0001);

                    char send_buffer[1024]={0};
                    char *send_p=send_buffer;
                    int offset=0;
                    memcpy(send_p,&res_header,sizeof(res_header));
                    offset+=sizeof(res_header);
                    memcpy(send_p+offset,&res_query.name,res_query.name_len);
                    offset+=res_query.name_len;
                    memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                    offset+=sizeof(res_query.qtype);
                    memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                    offset+=sizeof(res_query.qclass);
                    
                    //answer
                    memcpy(send_p+offset,&ns_ans.sub_auth_name,ns_ans.auth_len);
                    offset+=ns_ans.auth_len;
                    memcpy(send_p+offset,&ns_ans.type,sizeof(ns_ans.type));
                    offset+=sizeof(ns_ans.type);
                    memcpy(send_p+offset,&ns_ans.classes,sizeof(ns_ans.classes));
                    offset+=sizeof(ns_ans.classes);
                    memcpy(send_p+offset,&ns_ans.ttl,sizeof(ns_ans.ttl));
                    offset+=sizeof(ns_ans.ttl);
                    memcpy(send_p+offset,&ns_ans.length,sizeof(ns_ans.length));
                    offset+=sizeof(ns_ans.length);
                    memcpy(send_p+offset,&ns_ans.name_server,ns_ans.server_len);
                    offset+=ns_ans.server_len;

                    //additional
                    memcpy(send_p+offset,&ns_addi.sub_ans_name,ns_addi.ans_name_len);
                    offset+=ns_addi.ans_name_len;
                    memcpy(send_p+offset,&ns_addi.type,sizeof(ns_addi.type));
                    offset+=sizeof(ns_addi.type);
                    memcpy(send_p+offset,&ns_addi.classes,sizeof(ns_addi.classes));
                    offset+=sizeof(ns_addi.classes);
                    memcpy(send_p+offset,&ns_addi.ttl,sizeof(ns_addi.ttl));
                    offset+=sizeof(ns_addi.ttl);
                    memcpy(send_p+offset,&ns_addi.length,sizeof(ns_addi.length));
                    offset+=sizeof(ns_addi.length);
                    memcpy(send_p+offset,&ns_addi.address,ns_addi.addr_len);
                    offset+=ns_addi.addr_len;

                    sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));
                    
                }
                
            }
            if(ntohs(question.qtype)==6){
                SOA_AUTH soa_ans;
                AUTH soa_auth;
                vector<string>split_soa;
                string info=zone_src[0];
                const char* d = "  ,";
                char *p;
                p = strtok(const_cast<char*>(info.c_str()), d);
                
                while (p != NULL) {
                    //printf("%s\n", p);
                    split_soa.push_back(p);
                    p = strtok(NULL, d);		   
                }
                vector<vector<string>>split_with_my_domain;
                vector<vector<string>>split_with_sub_domain;
                for(int i=0;i<zone_src.size();i++){
                    if(zone_src[i][0]!='@'){
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_sub_domain.push_back(line);
                    }
                    else{
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_my_domain.push_back(line);
                    }
                }
                
                char* server_name;
                for(int i=0;i<split_with_my_domain.size();i++){
                    if(split_with_my_domain[i][3]=="NS"){
                        cout<<"match"<<endl;
                        server_name=const_cast<char*>(split_with_my_domain[i][4].c_str());    //dns.example1.org.
                        cout<<server_name<<endl;
                        break;
                    }
                }
                
                string server_name_to_send=server_name;
                const char* dn = ".";
                char *sub_domain;
                int split_index_auth;
                sub_domain = strtok(server_name, dn);    //dns
                for(int i=0;i<split_with_sub_domain.size();i++){
                    if(split_with_sub_domain[i][0]==sub_domain){
                        split_index_auth=i;
                        break;
                    }
                }

                //answer
                sprintf(soa_ans.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                soa_ans.name_len=strtopkt(my_domain[index]).length();
                soa_ans.type=htons(0x0006);    //SOA
                soa_ans.classes=htons(0x0001);
                soa_ans.ttl=htonl(stoi(split_soa[1]));
                

                int rdlength=0;
                sprintf(soa_ans.primary_name,"%s",const_cast<char*>(strtopkt(split_soa[4]).c_str()));
                soa_ans.primary_len=strtopkt(split_soa[4]).length();
                rdlength+=strtopkt(split_soa[4]).length();

                sprintf(soa_ans.auth_mailbox,"%s",const_cast<char*>(strtopkt(split_soa[5]).c_str()));
                soa_ans.auth_len=strtopkt(split_soa[5]).length();
                rdlength+=strtopkt(split_soa[5]).length();

                soa_ans.serial_num=htonl(stoi(split_soa[6]));
                soa_ans.refresh_interval=htonl(stoi(split_soa[7]));
                soa_ans.retry=htonl(stoi(split_soa[8]));
                soa_ans.expire_limit=htonl(stoi(split_soa[9]));
                soa_ans.min_ttl=htonl(stoi(split_soa[10]));

                rdlength+=sizeof(uint32_t)*5;
                soa_ans.length=htons(rdlength);


                //authority NS
                sprintf(soa_auth.sub_auth_name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                soa_auth.auth_len=strtopkt(my_domain[index]).length();
                soa_auth.type=htons(0x0002);    //ns
                soa_auth.classes=htons(0x0001);
                soa_auth.ttl=htonl(stoi(split_with_sub_domain[split_index_auth][1]));
                
                sprintf(soa_auth.name_server,"%s",const_cast<char*>(strtopkt(server_name_to_send).c_str()));
                soa_auth.server_len=strtopkt(server_name_to_send).length();
                soa_auth.length=htons(strtopkt(server_name_to_send).length());
                
                response_header res_header;
                res_header.ID=header.id;
                res_header.flags=htons(0x8500);
                res_header.QDCOUNT=htons(0x0001);
                res_header.ANCOUNT=htons(0x0001);
                res_header.NSCOUNT=htons(0x0001);
                res_header.ARCOUNT=htons(0x0000);
                
                response_question res_query;
                sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                res_query.name_len=strtopkt(my_domain[index]).length();
                res_query.qtype=htons(0x0006);  //SOA
                res_query.qclass=htons(0x0001);


                char send_buffer[1024]={0};
                char *send_p=send_buffer;
                int offset=0;
                memcpy(send_p,&res_header,sizeof(res_header));
                offset+=sizeof(res_header);

                
                memcpy(send_p+offset,&res_query.name,res_query.name_len);
                offset+=res_query.name_len;
                memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                offset+=sizeof(res_query.qtype);
                memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                offset+=sizeof(res_query.qclass);

                //ANSWER SECTION
                memcpy(send_p+offset,&soa_ans.name,soa_ans.name_len);
                offset+=soa_ans.name_len;
                memcpy(send_p+offset,&soa_ans.type,sizeof(soa_ans.type));
                offset+=sizeof(soa_ans.type);
                memcpy(send_p+offset,&soa_ans.classes,sizeof(soa_ans.classes));
                offset+=sizeof(soa_ans.classes);
                memcpy(send_p+offset,&soa_ans.ttl,sizeof(soa_ans.ttl));
                offset+=sizeof(soa_ans.ttl);
                memcpy(send_p+offset,&soa_ans.length,sizeof(soa_ans.length));
                offset+=sizeof(soa_ans.length);
                memcpy(send_p+offset,&soa_ans.primary_name,soa_ans.primary_len);
                offset+=soa_ans.primary_len;
                memcpy(send_p+offset,&soa_ans.auth_mailbox,soa_ans.auth_len);
                offset+=soa_ans.auth_len;
                memcpy(send_p+offset,&soa_ans.serial_num,sizeof(soa_ans.serial_num));
                offset+=sizeof(soa_ans.serial_num);
                memcpy(send_p+offset,&soa_ans.refresh_interval,sizeof(soa_ans.refresh_interval));
                offset+=sizeof(soa_ans.refresh_interval);
                memcpy(send_p+offset,&soa_ans.retry,sizeof(soa_ans.retry));
                offset+=sizeof(soa_ans.retry);
                memcpy(send_p+offset,&soa_ans.expire_limit,sizeof(soa_ans.expire_limit));
                offset+=sizeof(soa_ans.expire_limit);
                memcpy(send_p+offset,&soa_ans.min_ttl,sizeof(soa_ans.min_ttl));
                offset+=sizeof(soa_ans.min_ttl);

                //AUTH SECTION
                memcpy(send_p+offset,&soa_auth.sub_auth_name,soa_auth.auth_len);
                offset+=soa_auth.auth_len;
                memcpy(send_p+offset,&soa_auth.type,sizeof(soa_auth.type));
                offset+=sizeof(soa_auth.type);
                memcpy(send_p+offset,&soa_auth.classes,sizeof(soa_auth.classes));
                offset+=sizeof(soa_auth.classes);
                memcpy(send_p+offset,&soa_auth.ttl,sizeof(soa_auth.ttl));
                offset+=sizeof(soa_auth.ttl);
                memcpy(send_p+offset,&soa_auth.length,sizeof(soa_auth.length));
                offset+=sizeof(soa_auth.length);
                memcpy(send_p+offset,&soa_auth.name_server,soa_auth.server_len);
                offset+=soa_auth.server_len;



                sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));
            }
            if(ntohs(question.qtype)==28){  //AAAA
                //sub-domian
                //cout<<"sub domain"<<endl;
                //vector<vector<string>>split;
                ANS_IPV6 sub_answer;
                AUTH sub_auth;
                
                vector<vector<string>>split_with_my_domain;
                vector<vector<string>>split_with_sub_domain;
                
                for(int i=0;i<zone_src.size();i++){
                    if(zone_src[i][0]!='@'){
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_sub_domain.push_back(line);
                    }
                    else{
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_my_domain.push_back(line);
                    }
                }
                
                char* server_name;
                for(int i=0;i<split_with_my_domain.size();i++){
                    if(split_with_my_domain[i][3]=="NS"){
                        cout<<"match"<<endl;
                        server_name=const_cast<char*>(split_with_my_domain[i][4].c_str());    //dns.example1.org.
                        cout<<server_name<<endl;
                        break;
                    }
                }
                string server_name_to_send=server_name;
                const char* d = ".";
                char *auth_domain;
                int split_index_auth;
                
                auth_domain = strtok(server_name, d);    //dns
                for(int i=0;i<split_with_sub_domain.size();i++){
                    if(split_with_sub_domain[i][0]==auth_domain){
                        split_index_auth=i;
                        break;
                    }
                }
                char *sub_domain;
                sub_domain = strtok(aname, d);
                int split_index_sub;
                bool if_soa=true;
                for(int i=0;i<split_with_sub_domain.size();i++){
                    if(split_with_sub_domain[i][0]==sub_domain && split_with_sub_domain[i][3]=="AAAA"){
                        split_index_sub=i;
                        if_soa=0;
                        break;
                    }
                }
                
                if(!if_soa){
                    //SUB ANS
                    cout<<"AAAA\n";
                    string sub_domain_name=sub_domain;
                    sub_domain_name+='.';
                    sub_domain_name+=my_domain[index];
                    //sub_answer.sub_ans_name=const_cast<char*>(strtopkt(sub_domain_name).c_str());
                    sprintf(sub_answer.sub_ans_name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                    
                    sub_answer.ans_name_len=strtopkt(sub_domain_name).length();
                    sub_answer.type=htons(28);  //AAAA
                    sub_answer.classes=htons(0x0001);
                    sub_answer.ttl=htonl(stoi(split_with_sub_domain[split_index_sub][1]));
                    split_with_sub_domain[split_index_sub][4].pop_back();
                    const char* ip_address =split_with_sub_domain[split_index_sub][4].c_str();
                    
                    struct in6_addr addr;
                    unsigned char buf[sizeof(struct in6_addr)];
                    char str[INET_ADDRSTRLEN];
                    //printf("%s\n",ip_address);
                    int ret =inet_pton(AF_INET6,ip_address , buf);
                    //cout<<*addr.s6_addr<<endl;
                    //sub_answer.address=buf;
                    memcpy(sub_answer.address,buf,sizeof(struct in6_addr)+1);
                    //sprintf(sub_answer.address,"%s",addr.s6_addr);
                    
                    sub_answer.length=htons(sizeof(sub_answer.address));
                    sub_answer.addr_len=sizeof(sub_answer.address);
                    

                    response_header res_header;
                    res_header.ID=header.id;
                    res_header.flags=htons(0x8180);
                    res_header.QDCOUNT=htons(0x0001);
                    res_header.ANCOUNT=htons(0x0001);
                    res_header.NSCOUNT=htons(0x0000);
                    res_header.ARCOUNT=htons(0x0000);

                    response_question res_query;
                    //res_query.name=const_cast<char*>(strtopkt(sub_domain_name).c_str());
                    sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                    res_query.name_len=strtopkt(sub_domain_name).length();
                    res_query.qtype=htons(28);      //AAAA
                    res_query.qclass=htons(0x0001);

                    char send_buffer[1024]={0};
                    char *send_p=send_buffer;
                    int offset=0;
                    memcpy(send_p,&res_header,sizeof(res_header));
                    offset+=sizeof(res_header);
                    memcpy(send_p+offset,&res_query.name,res_query.name_len);
                    offset+=res_query.name_len;
                    memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                    offset+=sizeof(res_query.qtype);
                    memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                    offset+=sizeof(res_query.qclass);

                    
                    memcpy(send_p+offset,&sub_answer.sub_ans_name,sub_answer.ans_name_len);
                    offset+=sub_answer.ans_name_len;
                    memcpy(send_p+offset,&sub_answer.type,sizeof(sub_answer.type));
                    offset+=sizeof(sub_answer.type);
                    memcpy(send_p+offset,&sub_answer.classes,sizeof(sub_answer.classes));
                    offset+=sizeof(sub_answer.classes);
                    memcpy(send_p+offset,&sub_answer.ttl,sizeof(sub_answer.ttl));
                    offset+=sizeof(sub_answer.ttl);
                    memcpy(send_p+offset,&sub_answer.length,sizeof(sub_answer.length));
                    offset+=sizeof(sub_answer.length);
                    memcpy(send_p+offset,&sub_answer.address,sizeof(sub_answer.address));
                    offset+=16;
                    

                    sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));

                }
                else{
                    cout<<"SOA"<<endl;
                    SOA_AUTH soa_auth;
                    vector<string>split;
                    string info=zone_src[0];
                    const char* d = "  ,";
                    char *p;
                    p = strtok(const_cast<char*>(info.c_str()), d);
                    
                    while (p != NULL) {
                        //printf("%s\n", p);
                        split.push_back(p);
                        p = strtok(NULL, d);		   
                    }

                    

                    //soa_auth.name=const_cast<char*>(strtopkt(my_domain[index]).c_str());
                    sprintf(soa_auth.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    soa_auth.name_len=strtopkt(my_domain[index]).length();
                    soa_auth.type=htons(0x0006);
                    soa_auth.classes=htons(0x0001);
                    soa_auth.ttl=htonl(stoi(split[1]));
                    

                    int rdlength=0;
                    //soa_auth.primary_name = const_cast<char*>(strtopkt(split[4]).c_str());
                    sprintf(soa_auth.primary_name,"%s",const_cast<char*>(strtopkt(split[4]).c_str()));
                    soa_auth.primary_len=strtopkt(split[4]).length();
                    rdlength+=strtopkt(split[4]).length();

                    //soa_auth.auth_mailbox = const_cast<char*>(strtopkt(split[5]).c_str());
                    sprintf(soa_auth.auth_mailbox,"%s",const_cast<char*>(strtopkt(split[5]).c_str()));
                    soa_auth.auth_len=strtopkt(split[5]).length();
                    rdlength+=strtopkt(split[5]).length();

                    soa_auth.serial_num=htonl(stoi(split[6]));
                    soa_auth.refresh_interval=htonl(stoi(split[7]));
                    soa_auth.retry=htonl(stoi(split[8]));
                    soa_auth.expire_limit=htonl(stoi(split[9]));
                    soa_auth.min_ttl=htonl(stoi(split[10]));

                    rdlength+=sizeof(uint32_t)*5;
                    soa_auth.length=htons(rdlength);
                    
                    response_header res_header;
                    res_header.ID=header.id;
                    res_header.flags=htons(0x8500);
                    res_header.QDCOUNT=htons(0x0001);
                    res_header.ANCOUNT=htons(0x0000);
                    res_header.NSCOUNT=htons(0x0001);
                    res_header.ARCOUNT=htons(0x0000);
                    
                    response_question res_query;
                    
                    if(qname_len>my_domain[index].length()){
                        string sub_domain_name=sub_domain;
                        sub_domain_name+='.';
                        sub_domain_name+=my_domain[index];
                        //res_query.name=const_cast<char*>(strtopkt(sub_domain_name).c_str());
                        sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(sub_domain_name).c_str()));
                        res_query.name_len=strtopkt(sub_domain_name).length();
                        res_query.qtype=htons(0x0001);
                        res_query.qclass=htons(0x0001);
                    }
                    else{
                        //res_query.name=const_cast<char*>(strtopkt(my_domain[index]).c_str());
                        sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                        res_query.name_len=strtopkt(my_domain[index]).length();
                        res_query.qtype=htons(0x0001);
                        res_query.qclass=htons(0x0001);
                    }

                    char send_buffer[1024]={0};
                    char *send_p=send_buffer;
                    int offset=0;
                    memcpy(send_p,&res_header,sizeof(res_header));
                    offset+=sizeof(res_header);

                    
                    memcpy(send_p+offset,&res_query.name,res_query.name_len);
                    offset+=res_query.name_len;
                    memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                    offset+=sizeof(res_query.qtype);
                    memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                    offset+=sizeof(res_query.qclass);

                    memcpy(send_p+offset,&soa_auth.name,soa_auth.name_len);
                    offset+=soa_auth.name_len;
                    memcpy(send_p+offset,&soa_auth.type,sizeof(soa_auth.type));
                    offset+=sizeof(soa_auth.type);
                    memcpy(send_p+offset,&soa_auth.classes,sizeof(soa_auth.classes));
                    offset+=sizeof(soa_auth.classes);
                    memcpy(send_p+offset,&soa_auth.ttl,sizeof(soa_auth.ttl));
                    offset+=sizeof(soa_auth.ttl);
                    memcpy(send_p+offset,&soa_auth.length,sizeof(soa_auth.length));
                    offset+=sizeof(soa_auth.length);
                    memcpy(send_p+offset,&soa_auth.primary_name,soa_auth.primary_len);
                    offset+=soa_auth.primary_len;
                    memcpy(send_p+offset,&soa_auth.auth_mailbox,soa_auth.auth_len);
                    offset+=soa_auth.auth_len;
                    memcpy(send_p+offset,&soa_auth.serial_num,sizeof(soa_auth.serial_num));
                    offset+=sizeof(soa_auth.serial_num);
                    memcpy(send_p+offset,&soa_auth.refresh_interval,sizeof(soa_auth.refresh_interval));
                    offset+=sizeof(soa_auth.refresh_interval);
                    memcpy(send_p+offset,&soa_auth.retry,sizeof(soa_auth.retry));
                    offset+=sizeof(soa_auth.retry);
                    memcpy(send_p+offset,&soa_auth.expire_limit,sizeof(soa_auth.expire_limit));
                    offset+=sizeof(soa_auth.expire_limit);
                    memcpy(send_p+offset,&soa_auth.min_ttl,sizeof(soa_auth.min_ttl));
                    offset+=sizeof(soa_auth.min_ttl);


                    sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));


                }
            }
            if(ntohs(question.qtype)==16){   //TXT
                
                AUTH txt_ans;
                vector<vector<string>>split_with_my_domain;
                vector<vector<string>>split_with_sub_domain;
                for(int i=0;i<zone_src.size();i++){
                    if(zone_src[i][0]!='@'){
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_sub_domain.push_back(line);
                    }
                    else{
                        vector<string>line;
                        string info=zone_src[i];
                        const char* d = "  ,";
                        char *p;
                        p = strtok(const_cast<char*>(info.c_str()), d);
                        
                        while (p != NULL) {
                            line.push_back(p);
                            p = strtok(NULL, d);		   
                        }
                        split_with_my_domain.push_back(line);
                    }
                }
                
                char* txt_content;
                bool if_soa=true;
                int split_index;
                uint8_t txt_length=0;
                for(int i=0;i<split_with_my_domain.size();i++){
                    if(split_with_my_domain[i][3]=="TXT"){
                        cout<<"match"<<endl;
                        txt_content=const_cast<char*>(split_with_my_domain[i][4].c_str());    //"Th1s_1s_D3m0_2."
                        cout<<txt_content<<endl;
                        split_index=i;
                        txt_length=split_with_my_domain[i][4].length();
                        if_soa=0;
                        break;
                    }
                }
                if(1){

                    //answer: example1.org txt_content
                    sprintf(txt_ans.sub_auth_name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    txt_ans.auth_len=strtopkt(my_domain[index]).length();
                    txt_ans.type=htons(16);    //txt
                    txt_ans.classes=htons(0x0001);
                    txt_ans.ttl=htonl(stoi(split_with_my_domain[split_index][1]));
                    
                    sprintf(txt_ans.name_server,"%s",txt_content);
                    txt_ans.server_len=txt_length+1;
                    txt_ans.length=htons(txt_length+1);

                    txt_ans.txt_len=txt_length;
                    

                    response_header res_header;
                    res_header.ID=header.id;
                    res_header.flags=htons(0x8500);
                    res_header.QDCOUNT=htons(0x0001);
                    res_header.ANCOUNT=htons(0x0001);
                    res_header.NSCOUNT=htons(0x0000);
                    res_header.ARCOUNT=htons(0x0000);

                    response_question res_query;
                    sprintf(res_query.name,"%s",const_cast<char*>(strtopkt(my_domain[index]).c_str()));
                    res_query.name_len=strtopkt(my_domain[index]).length();
                    res_query.qtype=htons(16);  //TYPE:TXT
                    res_query.qclass=htons(0x0001);

                    char send_buffer[1024]={0};
                    char *send_p=send_buffer;
                    int offset=0;
                    memcpy(send_p,&res_header,sizeof(res_header));
                    offset+=sizeof(res_header);
                    memcpy(send_p+offset,&res_query.name,res_query.name_len);
                    offset+=res_query.name_len;
                    memcpy(send_p+offset,&res_query.qtype,sizeof(res_query.qtype));
                    offset+=sizeof(res_query.qtype);
                    memcpy(send_p+offset,&res_query.qclass,sizeof(res_query.qclass));
                    offset+=sizeof(res_query.qclass);
                    
                    //answer
                    memcpy(send_p+offset,&txt_ans.sub_auth_name,txt_ans.auth_len);
                    offset+=txt_ans.auth_len;
                    memcpy(send_p+offset,&txt_ans.type,sizeof(txt_ans.type));
                    offset+=sizeof(txt_ans.type);
                    memcpy(send_p+offset,&txt_ans.classes,sizeof(txt_ans.classes));
                    offset+=sizeof(txt_ans.classes);
                    memcpy(send_p+offset,&txt_ans.ttl,sizeof(txt_ans.ttl));
                    offset+=sizeof(txt_ans.ttl);
                    memcpy(send_p+offset,&txt_ans.length,sizeof(txt_ans.length));
                    offset+=sizeof(txt_ans.length); //data length
                    memcpy(send_p+offset,&txt_ans.txt_len,sizeof(uint8_t));
                    offset+=sizeof(uint8_t); //txt length
                    memcpy(send_p+offset,&txt_ans.name_server,txt_ans.server_len);
                    offset+=txt_ans.server_len;


                    sendto(s,send_buffer, offset, 0,(struct sockaddr*)&csin, sizeof(sin));
                    
                }
            }
        }

        //===Forwarding to foreign domain===
        else if(!is_my_domain){
            int sockfd=socket(AF_INET,SOCK_DGRAM,0);
            if(sockfd<0){
                return -1;
            }
            char request[1024]={0};
            memset(request,0,1024);
            memcpy(request,buffer,sizeof(buffer));
            int length=len;

            sockaddr_in servaddr={0};
            servaddr.sin_family=AF_INET;
            servaddr.sin_port=htons(53);
            servaddr.sin_addr.s_addr=inet_addr(foreign_server.c_str());

            int slen=sendto(sockfd,request,length,0,(sockaddr*)&servaddr,sizeof(sockaddr));

            char response[1024]={0};
            sockaddr_in addr;
            size_t addr_len=sizeof(sockaddr_in);
            int response_len = recvfrom(sockfd,response,sizeof(response),0,(sockaddr*)&addr,(socklen_t*)&addr_len);

            sendto(s,response, response_len, 0,(struct sockaddr*)&csin, sizeof(sin));
        }
		
        
    }
	close(s);
}