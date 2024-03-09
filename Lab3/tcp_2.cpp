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
#define PORT 10003
#define BUFFER_SIZE 1000000

static struct timeval _t0;
static unsigned long long bytesent = 0;

double tv2s(struct timeval *ptv) {
	return 1.0*(ptv->tv_sec) + 0.000001*(ptv->tv_usec);
}

void handler(int s) {
	struct timeval _t1;
	double t0, t1;
	gettimeofday(&_t1, NULL);
	t0 = tv2s(&_t0);
	t1 = tv2s(&_t1);
	fprintf(stderr, "\n%lu.%06lu %llu bytes sent in %.6fs (%.6f Mbps; %.6f MBps)\n",
		_t1.tv_sec, _t1.tv_usec, bytesent, t1-t0, 8.0*(bytesent/1000000.0)/(t1-t0), (bytesent/1000000.0)/(t1-t0));
	exit(0);
}

int main(int argc, char *argv[]) {
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
    //receive message
    read(sock, buffer, BUFFER_SIZE);
    cout<<buffer;

	signal(SIGINT,  handler);		//used to handle interrupt signal
	signal(SIGTERM, handler);		//used to handle terminal signal

	gettimeofday(&_t0, NULL);


	double rate=atof(argv[1]);		//MB/s:million bytes per second
	//double fixed_size=rate*1000000;
	//cout<<"rate: "<<rate<<endl;

	static struct timeval start;
	static struct timeval end;
	gettimeofday(&start, NULL);
	double diff=0,tmp=start.tv_usec;
	while(1) {
		struct timespec req;
		req.tv_sec = 0; 
		req.tv_nsec = 1000000;

		gettimeofday(&end, NULL);

		double end_time=1.0*(end.tv_sec) + 0.000001*(end.tv_usec);
		double start_time=1.0*(start.tv_sec) + 0.000001*(start.tv_usec);
		diff= (end_time-start_time)*1000000;
		//cout<<"diff: "<<diff<<endl;
		double send_byte=rate*diff;
		//cout<<send_byte<<endl;
		bytesent+=write(sock,buffer,send_byte);
		gettimeofday(&start, NULL);

		nanosleep(&req, NULL);
	}

	return 0;
}
