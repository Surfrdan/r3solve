#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MAX_THREADS 512
#define DEFAULT_THREADS 5 

#define MAX_HOST_LEN 256
#define T_A 1
#define T_CNAME 5

#define DEBUG true 

struct DNS_HEADER
{
    unsigned short id; // identification number

    unsigned char rd :1; // recursion desired
    unsigned char tc :1; // truncated message
    unsigned char aa :1; // authoritive answer
    unsigned char opcode :4; // purpose of message
    unsigned char qr :1; // query/response flag

    unsigned char rcode :4; // response code
    unsigned char cd :1; // checking disabled
    unsigned char ad :1; // authenticated data
    unsigned char z :1; // its z! reserved
    unsigned char ra :1; // recursion available

    unsigned short q_count; // number of question entries
    unsigned short ans_count; // number of answer entries
    unsigned short auth_count; // number of authority entries
    unsigned short add_count; // number of resource entries
};

struct QUESTION
{
    unsigned short qtype;
    unsigned short qclass;
};

struct R_DATA
{
    unsigned short type;
    unsigned short _class;
    unsigned int ttl;
    unsigned short data_len;
};

struct RES_RECORD
{
    unsigned char *name;
    struct R_DATA *resource;
    unsigned char *rdata;
};

typedef struct
{
    unsigned char *name;
    struct QUESTION *ques;
} QUERY;

// $ r3solve hosts.txt 120 
// loads hosts from hosts.txt in current dir with 120 threads

void *process_hosts(void *data);
bool lookup_host(char *host);

pthread_t thread_id[MAX_THREADS];
pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;
char **hosts;
int nexthost = 0;
int hosts_count;

bool lookup_host(char *host) {
    int sockfd, portno, n;
    char recvBuff[512];
    char *qname,*reader;
    struct sockaddr_in serv_addr;
    
    memset(recvBuff, '0',sizeof(recvBuff));    
    if((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        return 1;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(53);
    if(inet_pton(AF_INET, "8.8.8.8", &serv_addr.sin_addr)<=0) {
        return 1;
    }
    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        return 1; 
    }

    struct DNS_HEADER *dns = NULL;
    struct QUESTION *qinfo = NULL;

    dns = (struct DNS_HEADER *) &recvBuff;
    dns->id = 1;
    dns->qr = 0;
    dns->opcode = 0;
    dns->aa = 0;
    dns->tc = 0;
    dns->rd = 1;
    dns->ra = 0;
    dns->z = 0;
    dns->ad = 0;
    dns->cd = 0;
    dns->rcode = 0;
    dns->q_count = htons(1);
    dns->ans_count = 0;
    dns->auth_count = 0;
    dns->add_count = 0;

    qname = (unsigned char*)&recvBuff[sizeof(struct DNS_HEADER)];
    int lock = 0, i; ;
    for (i = 0; i<strlen(host); i++ ) {
        if(host[i]=='.') {
            *qname++ = i - lock;
            for(;lock<i; lock++) {
                *qname++=host[lock];
            }
        }
    }
    *qname++='\0';
    qinfo =(struct QUESTION*)&recvBuff[sizeof(struct DNS_HEADER) + (strlen((const char*)qname) + 1)];
    qinfo->qtype = htons(T_A);
    qinfo->qclass = htons(1);
    int packet_size = sizeof(struct DNS_HEADER) + (strlen((const char*)qname)+1) + sizeof(struct QUESTION); 
    sendto(sockfd, (char*)recvBuff, packet_size, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    i = sizeof(serv_addr);
	if(recvfrom (sockfd,(char*)recvBuff , 65536 , 0 , (struct sockaddr*)&serv_addr , (socklen_t*)&i ) < 0) {
		return 1;
    }
	dns = (struct DNS_HEADER*) recvBuff;
	reader = &recvBuff[sizeof(struct DNS_HEADER) + (strlen((const char*)qname)+1) + sizeof(struct QUESTION)];
	unsigned short answers = ntohs(dns->ans_count);
	printf("answers %d\n", answers);
}

void* process_hosts(void* data) {
    int rc;
    while(nexthost <= hosts_count) {
        rc = pthread_mutex_lock(&a_mutex);
        char *host = hosts[nexthost];
        nexthost++;
        rc = pthread_mutex_unlock(&a_mutex);

        if (host) {
            lookup_host(host);
        } else {
            pthread_exit(NULL);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    
    char *hostsFile = argv[1];
    FILE *fp = fopen(hostsFile, "r");
    
    int threads = DEFAULT_THREADS;
    if (argv[2] != NULL) {
        threads = strtol(argv[2], NULL, 10);
    }
    
    int l = 0;
    char line[MAX_HOST_LEN];
    while(fgets(line, (MAX_HOST_LEN - 1), fp)) {
        line[strlen(line)-1]='\0';  
        hosts = realloc(hosts, (l+1) * sizeof(char*));
        int host_length = strlen(line);
        hosts[l] = calloc(sizeof(char), host_length+1);
        strcpy(hosts[l], line);
        l++;
        hosts_count = l;
    }
    fclose(fp);

    int r, t;
    for (t = 0; t <= threads; t++) {
        r = pthread_create(&thread_id[t], NULL, process_hosts, NULL);
        pthread_join(thread_id[t], NULL);
    }
    pthread_exit(NULL);
    return 0;
}
