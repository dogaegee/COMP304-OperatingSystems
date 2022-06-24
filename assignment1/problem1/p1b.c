#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]){
	if(argc >= 2){
		int i;
		char *commandName[argc];
		for(i = 1; i < argc; i++){
			commandName[i-1] = argv[i];
		}
		commandName[argc-1] = NULL;
		
		pid_t pid = fork();
		if(pid < 0){
			fprintf(stderr, "Fork Failed\n");
			return 1;
		} else if(pid == 0){
			execvp(commandName[0] , commandName);
		} else {
			wait(NULL);
			printf("%s ", "Child finished executing");
			for(i = 0; i < argc - 1; i++){
				printf("%s ", commandName[i]);
			}
			printf("\n");
		}
	}else {
		printf("%s", "Give a command name as an argument!\n");
	}
	return 0;
}
