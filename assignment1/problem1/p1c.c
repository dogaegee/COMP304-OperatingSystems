#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>

int main(int argc, char *argv[]){
	pid_t pid = fork();
	if(pid < 0){
		fprintf(stderr, "Fork Failed\n");
		return 1;
	} else if(pid == 0){
		printf("%s %d %s %d\n", "I am the zombie process with PID:", getpid(), "Parent PID:", getppid());
	} else {
		time_t now;
		printf("Parent started sleeping\n");
		printf("The return value of time(&now) call: %ld\n", time(&now));
		sleep(6);
		printf("Parent finished sleeping\n");
		printf("The return value of time(&now) call: %ld\n", time(&now));
		wait(NULL);
	}
	return 0;
}
