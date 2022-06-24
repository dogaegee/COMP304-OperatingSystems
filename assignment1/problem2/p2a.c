#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 20

void writeMessage(int currentFd[], char current_write_msg[]){
	close(currentFd[READ_END]);
	write(currentFd[WRITE_END], current_write_msg, strlen(current_write_msg) + 1);
	close(currentFd[WRITE_END]);
}

void readMessage(int currentFd[], char current_read_msg[]){
	close(currentFd[WRITE_END]);
	read(currentFd[READ_END], current_read_msg, BUFFER_SIZE);
	close(currentFd[READ_END]);
}

int main(){
	int fd[2];
	int fd2[2];
	int fd3[2];
	int fd4[2];
	char write_msg[BUFFER_SIZE];
	char read_msg[BUFFER_SIZE];
	char read_msg2[BUFFER_SIZE];
	char read_msg3[BUFFER_SIZE];
	char read_msg4[BUFFER_SIZE];
	
	if (pipe(fd) == -1 || pipe(fd2) == -1 ||
		pipe(fd3) == -1 || pipe(fd4) == -1) {
		fprintf(stderr,"Pipe failed");
		return 1;
	}
	
	int carNum = 1;
	pid_t pid;
	pid_t pid2;
	pid_t pid3;
	pid_t pid4;
	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "Fork failed");
		return 1;
	} else if(pid > 0){
		sprintf(write_msg, "%d", carNum);
		printf("Started washing car %s\n", write_msg);
		writeMessage(fd, write_msg);
		wait(NULL);
		sleep(4);
		readMessage(fd4, read_msg4);
		printf("Finished washing car %s\n", read_msg4);	
	} else {
		sleep(4);
		pid2 = fork();
		if (pid2 < 0) {
			fprintf(stderr, "Fork failed");
			return 1;
		} else if(pid2 > 0){
			readMessage(fd, read_msg);
			printf("Washing the windows of car %s\n", read_msg);
			writeMessage(fd2, read_msg);
			wait(NULL);
		} else {
			sleep(4);
			pid3 = fork();
			if (pid3 < 0) {
				fprintf(stderr, "Fork failed");
				return 1;
			} else if(pid3 > 0){
				readMessage(fd2, read_msg2);
				printf("Washing the interior of car %s\n", read_msg2);
				writeMessage(fd3, read_msg2);
				wait(NULL);
			} else {
				sleep(4);
				pid4 = fork();
				if (pid4 < 0) {
					fprintf(stderr, "Fork failed");
					return 1;
				} else if(pid4 > 0){
					readMessage(fd3, read_msg3);
					printf("Washing the wheels of car %s\n", read_msg3);
					writeMessage(fd4, read_msg3);
				}
			}
		}
	}
}


