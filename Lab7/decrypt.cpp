#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
using namespace std;
// the data structure for the oracles
typedef struct {
	int stage;
	int key;
	char *secret;
}	oracle_t;

char answer[33]="16a5ed366e9fb3be74601fcb2a51be7f";
static int oseed = -1;
static unsigned magic = 0xdeadbeef;
static oracle_t oracle[2];
static int current_pid;
char * gen_secret(int k1, int k2) {
	static int len;
	static char buf[64];
	if(oseed < 0) {
		oseed = (magic ^ k1 ^ k2) & 0x7fffffff;
		srand(oseed);
	}
	len  = snprintf(buf,     10, "%x", rand());
	len += snprintf(buf+len, 10, "%x", rand());
	len += snprintf(buf+len, 10, "%x", rand());
	len += snprintf(buf+len, 10, "%x", rand());
	buf[len] = '\0';
	return buf;
}

int	msg2key(char *msg, int len) {
	int i, n = len/4 + ((len%4) ? 1 : 0);
	unsigned int *v, key = 0;
	if(n == 0) return -1;
	for(i = 0, v = (unsigned int*) msg; i < n; i++, v++)
		key ^= *v;
	return key & 0x7fffffff;
}

void oracle_init(oracle_t *o) {
	int userkey;
	char *secret;
	char buf[128]="aaaa\n";


	if((userkey = msg2key(buf, strlen(buf))) < 0) {
		printf("** Invalid message.\n");
		return;
	}

	o->key = rand() ^ current_pid;
	if((secret = gen_secret(o->key, userkey)) != NULL) {
		o->secret = strdup(secret);
	}
}


int main() {
	int ch;

	setvbuf(stdin,  NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
    for(unsigned int t=time(0);t>time(0)-80;t--){
        for(int pid=0;pid<40000;pid++){
            current_pid=pid;
            srand(t^pid);
            magic = rand();
            memset(oracle, 0, sizeof(oracle));
            oracle_init(oracle);
            if(strcmp(answer,oracle[0].secret)){
                oseed=-1;
                continue;
            }
            else
            {
                oseed=-1;
                memset(oracle, 0, sizeof(oracle));
                oracle_init(oracle);
                cout<<oracle[0].secret<<endl;
                return 0;
            }
        }
    }
    //done:
    return 0;
}  