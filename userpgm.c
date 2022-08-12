#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>

#define BUFLEN 100
int quit = 0;
int fd;

void *readmsg(){
	char buf[BUFLEN];
	while(1){
		bzero(buf, BUFLEN);
		if(quit == 1)
            break;
		if(read(fd, buf, BUFLEN) < 0) {
			perror("/dev/chatroom read failed : ");
			exit(2);
		}
		else
			if(strlen(buf)!=0)
				printf("\n%s\n", buf);
	    sleep(1);	
    }	
}

void *sendmsg(){
	char buf[BUFLEN];
	while(1){
		bzero(buf, BUFLEN);
      	fgets(buf, sizeof(buf), stdin);
		if(write(fd, buf, strlen(buf)+1) < 0) {
			exit(3);
		}
		if(strcmp("Bye!\n", buf) == 0){
			quit = 1;
            break;
        }
	}
}

int main() {
	fd = open("/dev/chatroom", O_RDWR);
	if( fd < 0) {
		exit(1);
	}
	pthread_t readThread;
	pthread_t sendThread;
    pthread_create(&readThread, NULL, readmsg, NULL);
    pthread_create(&sendThread, NULL, sendmsg, NULL);
    pthread_join(readThread, NULL);
    pthread_join(sendThread, NULL);
	return 0;
}


