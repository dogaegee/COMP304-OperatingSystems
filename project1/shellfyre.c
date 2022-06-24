/*
Betül DEMİRTAŞ - 68858
Doğa Ege İNHANLI - 69033
*/
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

const char *sysname = "shellfyre";
const char *pathfinder = "/usr/bin";
char project_path[1024];

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};

void pstraverse(int root_pid, char* flag);
char* find_path(struct command_t *command);
int search_file(char *arg, bool sub_directories_included, bool open_files);
int print_files(char *arg, bool open_files);
const char *get_filename_ext(const char *filename, char token);
int take(char *arg);
void my_ls(const char* directory, FILE* fp, bool sub_directories_included);
int print_directory_history();
void save_directory_history();
int isNumber(char s[]);
void schedule_joke();
void play_blackjack();
void move_directory(char * source, char *destination, char *current_dir);
/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}

void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	// FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;

	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

int main()
{
	
	char dir[128]; 
	getcwd(dir, sizeof(dir));
	strcpy(project_path, dir);
	save_directory_history();
	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0){
		return EXIT;
	}
	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			
			r = chdir(command->args[0]);
			
			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			else
				save_directory_history();
			return SUCCESS;
		}
	}
	
	// TODO: Implement your custom commands here
	//filesearch command
	if (strcmp(command->name, "filesearch") == 0){
    		if (command->arg_count > 0){
			if(command->arg_count == 1){ 
				//This is the situation with no flags. It searches the given argument in the current directory excluding the subdirectories thanks to two 'false' parameters.
				r = search_file(command->args[0], false, false);
			} else if (command->arg_count == 2){
				/*
				This is the situation with one flag. It checks whether the given flag is "-r" or "-o". Other arguments as flag are invalid.
				According to the "-r" or "-o" flag, it calls the function with corresponding boolean parameters.
				*/
				if(!strcmp(command->args[0], "-r")){
					r = search_file(command->args[1], true, false);
				} else if(!strcmp(command->args[0], "-o")){
					r = search_file(command->args[1], false, true);
				} else {
					printf("Flag not found for filesearch!");
				}
			} else if (command->arg_count == 3){
				//This is the situation with two flags. "-r" and "-o" are valid flags and the order is not important. 
				if((!strcmp(command->args[0], "-r") && !strcmp(command->args[1], "-o")) || (!strcmp(command->args[0], "-o") && !strcmp(command->args[1], "-r"))){
					r = search_file(command->args[2], true, true);
				} else {
					printf("Flags not found for filesearch!");
				}
			} else {
				printf("Too many arguments!");
			}
			if (r == -1)
				printf("No such file exists!");
			return SUCCESS;	
		}
		else{
			printf("Missing operand!");
			return SUCCESS;
		}
		return SUCCESS;
	}
	
	//take command
	if (strcmp(command->name, "take") == 0){
		r = take(command->args[0]);
		if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			else
				save_directory_history(); //saves the new directory to history for cdh command purposes
		return SUCCESS;
	}
	
	//cdh command
	if (strcmp(command->name, "cdh") == 0){
		r = print_directory_history();
		if (r == -1)
			printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
		else
			save_directory_history(); //saves the new directory to history for cdh command purposes
		return SUCCESS;
	}
	
	//joker command
	if (strcmp(command->name, "joker") == 0){
		schedule_joke();
		return SUCCESS;
	}
	
	//Our awesome commands
	
	//blackjack command
	if (strcmp(command->name, "blackjack") == 0){
		play_blackjack();
		return SUCCESS;
	}
	
	//movedir command
	if (strcmp(command->name, "movedir") == 0){
		char dir[128]; 
		getcwd(dir, sizeof(dir));
		move_directory(command->args[0], command->args[1], dir); //copies the first directory inside the second directory.
		chdir(dir);
		char *const args[] = {"/usr/bin/rm", "-rf", command->args[0], NULL}; 
		pid_t pid = fork();
		if(pid == 0){
			execv("/usr/bin/rm", args); //removes the original directory.
		} else {
			wait(NULL);
		}
        	
		return SUCCESS;
	}
	
	//Kernel module: pstraverse command
	
	if (strcmp(command->name, "pstraverse")==0) 	{
		pstraverse(atoi(command->args[0]),command->args[1]);
 	}
	
	pid_t pid = fork();

	if (pid == 0) // child
	{
		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		/// TODO: do your own exec with path resolving using execv()
		char* path = find_path(command);
		execv(path, command->args);
		exit(0);
		
	}
	else
	{
		/// TODO: Wait for child to finish if command is not running in background
		
		//The boolean named "background" stores the command ends with ampersand. So, if it is true, the parent waits for the child.
		if(!(command->background)){
			wait(NULL);
		}
		return SUCCESS;
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

//HELPER FUNCTIONS

/*
This function concatenates the "/usr/bin" (pathfinder variable) and command name itself to give the execv function to execute the UNIX commands.
*/
char* find_path(struct command_t *command){
	char* path;
	strcpy(path, pathfinder);
	size_t len = strlen(path);
	path[len] = '/';
	path[len + 1] = '\0';
	strcat(path, command->name);
	return path;
}

/*
This function divides the argument provided by the user into tokens which are splitted with “/” symbol, writes tokens to file and loops. If the current directory name exist, it goes into it and checks the next one. Otherwise, it first creates a new directory with the current directory name, goes into it, then checks the next one. 
*/
int take(char *arg){
	//Tokenize the argument.
	const char delimiter[2] = "/";
	char *token;
	token = strtok(arg,delimiter);

	//Loop through all of them and append.
	while(token != NULL){

		char dir[128]; 
		getcwd(dir, sizeof(dir));
		FILE *fp2;
		fp2 = fopen("files.txt", "a+");

		my_ls(dir, fp2, false);
		char line[128];
		int found = 0;
		//If found get into it and go to the next one.
		while (fgets(line, sizeof(line)-1, fp2)) {
			line[strlen(line) - 1] = '\0';
			if(!strcmp(line,token)){
				found = 1;
				remove("files.txt");
				chdir(token);
			}
		}
		//If not found creaate a new one.
		if(!found){
			mkdir(token, S_IRWXU | S_IRWXG | S_IRWXO);
			remove("files.txt");
			chdir(token);
		}
		
		fclose(fp2);
		token = strtok(NULL, delimiter);
	}
	return 0;

}
/*
- This function is called when the user enters "filesearch" command on shellfyre.
- It calls the my_ls function to write the names of the files and directories in the current directory to the text file.
- Fİnally, it calls the print_files to filter the names in the text file by considering the 'arg' given by the user.
*/
int search_file(char *arg, bool sub_directories_included, bool open_files){
	char direc[128]; 
	getcwd(direc, sizeof(direc));
	FILE *fp;
	fp = fopen("files.txt", "a+");
	my_ls(direc, fp, sub_directories_included);
	fclose(fp);
	return print_files(arg,open_files);
	
}
/*
- By considering the given 'arg', it filters the files/directories in the text file which we saved the names of the files/directories.
- It also handles with the "-o" flag by using open_files boolean.
- It prints the file/directory names in the wanted format in the pdf.
*/
int print_files(char *arg, bool open_files){
	FILE *fp;
	fp = fopen("files.txt", "r"); // We are going to read the names of files/directories in the 'files.txt' which was written by my_ls function.
	char line[128];
	int counter = 0;
	char dir[128];
	getcwd(dir, sizeof(dir));
    	while (fgets(line, sizeof(line), fp)) { //This reads the lines of the file one by one. Each line is a file or a directory.
    		//First, we are going to extract the file extension because the 'arg' will not be searched in the extension part.
        	const char * file_extension = get_filename_ext(line, '.');
        	size_t extension_length = strlen(file_extension);
        	size_t line_length = strlen(line);
        	char *file_name;
        	file_name = (char *)malloc(line_length);
        	strncpy(file_name, line, line_length - extension_length);
        	file_name[line_length - extension_length - 1] = '\0'; //This is the file/directory name without its extension.
        	strcpy(file_name, get_filename_ext(file_name, '/')); //This is the file/directory name without its full path.
        	char *p = strstr(file_name, arg); //It checks the name contains the given arg.
        	if(p != NULL){
        		counter++;
        		//Printing the file/directory name with the wanted format in the pdf.
			char * print_file = (char *)malloc(1024* sizeof(char));
			char * start = strstr(line, dir);
			strcpy(print_file,".");
			strcat(print_file,start+strlen(dir));
        		printf("%s", print_file);
        		if(open_files == true){
        			/*
        			- This part handles with the "-o" command which opens the found files/directories.
        			- Here, fork is important because we want to provide the continuation of the main process.
        			*/
        			pid_t pid = fork();
        			if(pid == 0){
        				line[strlen(line)-1] = '\0';
        				char *const args[] = {"/usr/bin/xdg-open", line, NULL};
        				execv("/usr/bin/xdg-open", args); //It executes the xdg-open command with the given file/directory name.
        				exit(0);
        			} else {
        				wait(NULL);
        			}
        		}
        		free(file_name);
        		free(print_file);
        	} else {
        		free(file_name);
        	}
    	}
    	fclose(fp);
	remove("files.txt");
    	if(counter == 0){
    		return -1;
    	} else {
    		return 0;
    	}
}

/*
This function returns the part which is after the given token in a string. We used this to obtain the file extensions.
*/
const char *get_filename_ext(const char *filename, char token) {
    const char *dot = strrchr(filename, token);
    if(!dot || dot == filename) return "";
    return dot + 1;
}

/*
- This function writes the names of the files and directories to the given text file.
*/
void my_ls(const char* directory, FILE* fp, bool sub_directories_included){
	DIR* current_directory = opendir(directory);
	struct dirent *d = malloc(sizeof(struct dirent));
	while((d = readdir(current_directory)) != NULL){ //This loop reads all the files and directories in the current directory one by one.
		if(d->d_type == DT_DIR){
			if(!strcmp(d->d_name,".") || !strcmp(d->d_name,"..")){
				continue;
			} else {
				/*
				- This part will work when the user enters "-r" flag, which is controlled by 'sub_directories_included' boolean.
				- In this situation, it continues to call itself recursively to get the content inside the subdirectories.
				*/
				if(sub_directories_included == true){
					chdir(d->d_name);
					char dir[128];
					getcwd(dir, sizeof(dir));
					chdir(dir);
					my_ls(dir, fp, sub_directories_included);
					chdir(directory);
				}
			}
		}
		//String concatenation for the path of the current file/directory.
		char *path = malloc(sizeof(char)*1024);
		char dir[128];
		getcwd(dir, sizeof(dir));
		strcpy(path,dir);
		strcat(path,"/");
		strcat(path,d->d_name);
		fputs(path, fp);
		fputs("\n", fp);
		free(path);
	}
	free(d);
	closedir(current_directory);
}

/*
- This function reads the history file, takes the last 10 entry, prints this information corresponding numbers and letters. Later takes input from user, finds the corresponding path of the letter or the number that user has provided and goes into that directory.
*/
int print_directory_history(){
	//This part goes to the directory of the history file and reads it
	chdir(project_path);
	FILE *fp;
	fp = fopen("beOS_shellfyre_directory_history.txt", "r");
	int line_number = 0;
	int total_line = 0;
	int print_line_number;
	char line[1024];
	char letters[10] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'};
	char directories[10][1024];
	//This part counts the line number in the file
	while(fgets(line, sizeof(line), fp) != NULL)
      		 line_number++;     
      	if(line_number == 0){
      		printf("No previous history!\n");
      		return -1;
      	}	   
      	                       
    	rewind(fp);
    	total_line = line_number;
    	//This part is to determine the starting index of printing
    	if(line_number<10){
    		line_number = 0;
    	} else {
    		line_number -= 10;
    	}
    	print_line_number = total_line - line_number;
    	//Goes through the file
    	while(line_number > 0) {
    		if(fgets(line, sizeof(line), fp) == NULL) {
			printf("%s", line);
    			return -1;
    			}
    			line_number--;		
    	}
    	int i = 0;
    	//Prints the wanted lines
    	while(fgets(line, sizeof(line), fp) != NULL){
    		printf("%c\t%d)\t~", letters[print_line_number - i-1], (print_line_number - i));
    		printf("%s", line);
    		strcpy(directories[i], line);
    		i++;
    	}
    	fclose(fp);	
    	//Asks the user to provide a selection
    	int go_to;
    	printf("Select directory by letter or number:");
    	char chosen[3];
	fgets(chosen, 3, stdin);
	chosen[strlen(chosen)-1] = '\0';

	//Converts the selection number or letter and decides which directory to go
	if(isNumber(chosen)){
		go_to = atoi(chosen);
		directories[print_line_number-go_to][strlen(directories[print_line_number-go_to])-1] = '\0';
		chdir(directories[print_line_number-go_to]);
		return 1;
	} else {
		int index = -1;
		int length = 10;
		if (chosen[0] >= 'a' && chosen[0] <= 'j')
  			index = chosen[0] - 'a' + 1;
		
		if(index > -1){
			go_to = index;
			directories[print_line_number-go_to][strlen(directories[print_line_number-go_to])-1] = '\0';
			//changes directory using chdir command with the given input 
			chdir(directories[print_line_number-go_to]);
			return 1;
		}
	}

	return -1;
}

/*
- This function determines whether a string is a number or not.
*/
int isNumber(char s[]){
    for (int i = 0; s[i]!= '\0'; i++){
        if (isdigit(s[i]) == 0)
              return 0;
    }
    return 1;
}

/*
- This function records the current directory path, goes in to the main directory the history file is, appends the recorded path, comes back to the original directory. 
*/
void save_directory_history(){
		//Records current history.
		char dir[1024];
		getcwd(dir, sizeof(dir));
		//Goes to the history dir.
		chdir(project_path);
		FILE *fp;
		fp = fopen("beOS_shellfyre_directory_history.txt", "a+");
		//Appends recorded.
		fputs(dir, fp);
		fputs("\n", fp);
		fclose(fp);
		//Goes back to the current dir.
		chdir(dir);
}

/*
This function opens a file, writes the command that we want to put into the crontab file. Then give this text file with the command to the crontab by making a system call.
*/
void schedule_joke(){
	//Open a file and write the crontab entry.
	FILE *fp;
	fp = fopen("joker.txt", "a+");
	char* command = malloc(sizeof(char)*1024);
	strcpy(command, "15 * * * *  XDG_RUNTIME_DIR=/run/user/$(id -u) notify-send \"Here is a joke for you:\" \"$(curl https://icanhazdadjoke.com)\"");
	fputs(command, fp);
	fputs("\n", fp);
	fclose(fp);
	//Give the file to the crontab.
	system("crontab joker.txt");
	remove("joker.txt");
	free(command);
	
}

/*
This function enables user to play a simple blackjack game.
*/
void play_blackjack(){
	printf("\nWelcome to BlackJack!.\n");
    	printf(".------..------.\n");
   	printf("|2.--. ||1.--. |\n");
    	printf("| (  ) || :  : |\n");
    	printf("| :  : || (__) |\n");
    	printf("| '--'2|| '--'1|\n");
    	printf("`------'`------'\n");
	srand(time(NULL));
	int player_first = rand() % 13+1;
	int player_second = rand() % 13+1;
	int dealer_first = rand() % 13+1;
	int dealer_second = rand() % 13+1;
	int new_card = 0;
	printf("\nYour cards: %d-%d", player_first, player_second);
	printf("\nDealers cards: %d-%d\n", dealer_first, dealer_second);
	int player_total = player_first + player_second;
	int dealer_total = dealer_first + dealer_second;
	int choice = 0;
	pid_t pid = fork();
	
	if(pid == 0){
		//players turn
		if(player_total != dealer_total){
			if(player_total == 21){
			printf("You won!\n");
			return;
			} else if(dealer_total == 21){
			printf("Dealer won!\n");
			exit(0);
			}
		}
		
		while(player_total < 21){
			printf("Type 1 if you want to Hit or Type 0 if you want to Stand? ");
			scanf("%d", &choice);
			fflush(stdin);
		
			if(choice == 1){
				new_card = rand() % 13+1;
				player_total += new_card;
				printf("\nYour new card: %d\t Total: %d\n", new_card, player_total);
			} else if(choice == 0) {
				break;
			} else {
				printf("Invalid entry!\n");
			}
		}
		
		//dealers turn
		while(dealer_total < 21){
				int will_draw = rand() % 2;
				if(will_draw == 0){
					printf("Dealer stood!\n");
					break;
				} else {
					new_card = rand() % 13+1;
					dealer_total += new_card;
					printf("Dealer hit!\n");
				}
		}
		
		if(player_total > 21 && dealer_total > 21){
			if(player_total < dealer_total) {
				printf("You won!\n");
			} else {
				printf("Dealer won!\n");
			}
		} else if(player_total < 21 && dealer_total < 21) {
			if(player_total > dealer_total) {
				printf("You won!\n");
			} else {
				printf("Dealer won!\n");
			}
		} else if(player_total > 21 && dealer_total < 21) {
			printf("Dealer won!\n");
		} else if(player_total < 21 && dealer_total > 21){
			printf("You won!\n");
		} else if (player_total == dealer_total){
			printf("It is a tie!\n");
		}
		
	} else {
		wait(NULL);
		printf("Game Over!\n");
	}
}
/*
- This function copies the source directory to the destination directory with its contents.
*/
void move_directory(char * source, char *destination, char *current_dir){
	chdir(destination);
	mkdir(source, S_IRWXU | S_IRWXG | S_IRWXO); //It creates the source directory in the destination directory with the permissions but without its content inside it.
	chdir(current_dir);
	DIR* current_directory = opendir(source);
	struct dirent *d = malloc(sizeof(struct dirent));
	while((d = readdir(current_directory)) != NULL){ //Now, we start the copy the contents of the source directory one by one in this loop.
		if(d->d_type == DT_DIR){ // We need to check subdirectories since they may have contents, too.
			if(!strcmp(d->d_name,".") || !strcmp(d->d_name,"..")){ //Directories reference hierarchy, we ignore them.
				continue;
			} else {
				/*
				- Basically, we obtain the full name of the subdirectory by string concatenation.
				- Since we need to copy the contents of the subdirectories to the new destionation, we call this function recursively with the new source and destionations.
				*/
				char * new_dest = malloc(sizeof(char)*1024);
				char * new_cur_dir = malloc(sizeof(char)*1024);
				strcpy(new_dest, destination);
				strcat(new_dest, "/");
				strcat(new_dest, source);
				strcpy(new_cur_dir, current_dir);
				strcat(new_cur_dir, "/");
				strcat(new_cur_dir, source);
				move_directory(d->d_name, new_dest,new_cur_dir); //Recursive call.
				free(new_cur_dir);
				strcpy(new_dest, destination);
				free(new_dest);
				chdir(destination);
			}
		} else if(d->d_type == DT_REG){ 
			/*
			- If it is a regular file such as text file, C file etc.
			- We obtain the full name of the subdirectory by string concatenation.
			- Then, we read the current file line by line and move them to a new file in the destionation place. That is why, this function is capable of copying only some specific files.
			*/
			char * new_dest = malloc(sizeof(char)*1024);
			char * new_cur_dir = malloc(sizeof(char)*1024);
			strcpy(new_cur_dir, current_dir);
			strcat(new_cur_dir, "/");
			strcat(new_cur_dir, source);
			chdir(new_cur_dir);
			strcpy(new_dest, destination);
			strcat(new_dest, "/");
			strcat(new_dest, source);
			FILE *fp;
			fp = fopen(d->d_name, "r");
			char line[1024];
			while(fgets(line, sizeof(line), fp) != NULL){ //It reads the file line by line and writes the content of it to the copy of it in the new destination.
				chdir(new_dest);
				FILE *fp2;
				fp2 = fopen(d->d_name, "a+");
				fputs(line, fp2);
				fclose(fp2);
			}
			fclose(fp);
			free(new_dest);
		}
	}
	free(d);
	closedir(current_directory);
}

void pstraverse(int root_pid, char *flag) {
	//Following lines prepare the pid and flag inputs coming from the user for the kernel module.
	char* root_command = malloc(sizeof(char)*1024);
	char* flag_command = malloc(sizeof(char)*1024);
	strcpy(root_command, "pid=");
	strcpy(flag_command, "flag=");
	char str[20];
	sprintf(str, "%d", root_pid);
	strcat(root_command, str);
	strcat(flag_command, flag);
	//Creating a child process is necessary since we want to continue using shellfyre after the execution of the module.
	pid_t pid = fork();
	if(pid == 0){
		char *args[] = {"/usr/bin/sudo","insmod","ps_traverse.ko", root_command, flag_command, NULL}; // commands for installing the module.
    		execv(args[0], args);
	}else{
		wait(NULL);
		pid_t pid2 = fork();
		if(pid2 == 0){
			char *args[] = {"/usr/bin/sudo","rmmod","ps_traverse.ko", NULL}; //commands for removing the module.
    			execv(args[0], args);
		} else {
			wait(NULL);
		}
		
	}
	
	free(root_command);
	free(flag_command);
}
