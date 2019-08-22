#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_THREADS 512
#define DEFAULT_THREADS 5 

#define MAX_HOST_LEN 256

#define DEBUG true 

// https://stackoverflow.com/questions/22697407/reading-text-file-into-char-array/22697886


// $ r3solve hosts.txt 120 
// loads hosts from hosts.txt in current dir with 120 threads

void *lookup_host(void *data);

pthread_t thread_id[MAX_THREADS];
pthread_mutex_t a_mutex = PTHREAD_MUTEX_INITIALIZER;
char **hosts;
int nexthost = 0;
int hosts_count;

void* lookup_host(void* data) {
    int rc;
    while(nexthost <= hosts_count) {
        rc = pthread_mutex_lock(&a_mutex);
        char *host = hosts[nexthost];
        nexthost++;
        rc = pthread_mutex_unlock(&a_mutex);

        if (host) {
            printf("processing %s\n", host);
        } else {
            printf("exiting\n");
            pthread_exit(NULL);
        }
    }
    printf("exiting at end\n");
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
        //printf("host: %s", hosts[l]);
        l++;
        hosts_count = l;
    }
    fclose(fp);

    int r, t;
    for (t = 0; t <= threads; t++) {
        r = pthread_create(&thread_id[t], NULL, lookup_host, NULL);
        pthread_join(thread_id[t], NULL);
    }
    pthread_exit(NULL);
    return 0;
}
