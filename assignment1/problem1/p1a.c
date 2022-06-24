#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
	if(argc == 2){
		printf("Main Process ID: %d, Level: %d\n", getpid(), 0);
		int i = 0;
		int n = atoi(argv[1]);
		int level = 0;
  		for(i; i < n; i++){
	  		pid_t pid = fork();
	  		if(pid == 0){
	  			level++;
        			printf("Process ID: %d, Parent ID: %d, Level: %d\n", getpid(), getppid(), level); 
	   		}
		}
  	} else if(argc >= 2) {
  		printf("%s\n", "Too many arguments!");
  	} else {
  		printf("%s\n", "Please, give arguments!");
  	}
  	return 0;
}
