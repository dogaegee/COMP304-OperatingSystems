#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 100

void writeMessage(int currentFd[], struct timeval *time){
	close(currentFd[READ_END]);
	write(currentFd[WRITE_END], time, sizeof(*time) + 1);
	close(currentFd[WRITE_END]);
}

void readMessage(int currentFd[], struct timeval *time){
	close(currentFd[WRITE_END]);
	read(currentFd[READ_END], time, sizeof(*time) + 1);
	close(currentFd[READ_END]);
}
int main(int argc, char *argv[]){

	if(argc >= 2){
		int fd[2];
		if (pipe(fd) == -1) {
			fprintf(stderr,"Pipe failed");
			return 1;
		}
		int i;
		char *commandName[argc];
		for(i = 1; i < argc; i++){
			commandName[i-1] = argv[i];
		}
		commandName[argc-1] = NULL;
		pid_t pid;
		pid = fork();
		if(pid < 0){
			fprintf(stderr, "Fork Failed\n");
			return 1;
		} else if(pid == 0){
			struct timeval timeFirst;
			gettimeofday(&timeFirst,NULL);
			writeMessage(fd, &timeFirst);
			execvp(commandName[0] , commandName);
		} else {
			wait(NULL);
			struct timeval timeLast;
			struct timeval timeRead;
			long int passingSeconds;
			float passingMilliSeconds;
			gettimeofday(&timeLast,NULL);
			readMessage(fd,&timeRead);
			passingSeconds = timeLast.tv_sec - timeRead.tv_sec;
			passingMilliSeconds = ((float) (timeLast.tv_usec - timeRead.tv_usec))/1000;
			printf("Passing time: %ld seconds and %.4f milliseconds\n", passingSeconds, passingMilliSeconds);
		}
	} else {
		printf("Please give a command!\n");
	}
}
