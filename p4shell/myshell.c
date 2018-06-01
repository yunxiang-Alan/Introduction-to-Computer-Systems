// Author: Renyi Tang
// Last date modified: 05/24/2018

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
int MAXCHAR = 512;

void myPrint(char *msg);
void print_error();
int parse_cmd_buff(char* cmd_buff, char** indiv_cmds);
int check_redirection(char* indiv_cmd);
int parse_no_redir(char* indiv_cmd_no_redir, char** argv);
int parse_indiv_cmd(char* indiv_cmd, char** argv, char** file);
void execute(int argc, char** argv, char** file, char* indiv_cmd);

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
    fflush(stdout);
}

void print_error()
{
    char error_message[] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
    fflush(stdout);
}

int parse_cmd_buff(char* cmd_buff, char** indiv_cmds)
{
    int i = 0;
    char *token = strtok(cmd_buff, ";\n");
    while (token != NULL && i < MAXCHAR){
        indiv_cmds[i] = strdup(token);
        i++;
        token = strtok(NULL, ";\n");
    }

    return i; // return the number of commands
}

// check if there exists a valid redirection in a command
// return 0 if there is no redirection
// return -1 if there are two or more redirections or advanced redirections
// return 1 if there is one redirection
// return 2 if there is one advanced redirection
// it also implicitly checks that the redirection is not the last character
int check_redirection(char* indiv_cmd)
{
    int l = strlen(indiv_cmd);
    int count_of_redir = 0; //count of '>' symbol
    int return_value = -1;
    int i;

    for (i=0; i<l; i++) {
        if (indiv_cmd[i] == '>')
            count_of_redir++;
    }
    if (count_of_redir > 1)
        return return_value;
    for (i=0; i<l; i++) {
        if (indiv_cmd[i]=='>' && indiv_cmd[i+1]!='+')
            return_value = 1;
    }
    for (i=0; i<l-1; i++){
		if (indiv_cmd[i]=='>' && indiv_cmd[i+1]=='+')
            return_value = 2;
    }
    if (return_value == -1)
        return_value = 0;

    return return_value;
}

int parse_no_redir(char* indiv_cmd_no_redir, char **argv)
{
    int i = 0;
    char* indiv_cmd_no_redir_dup = strdup(indiv_cmd_no_redir);
    // make a copey b/c strtok changes the original string
    char *token = strtok(indiv_cmd_no_redir_dup, " \n\t");

    while (token != NULL && i < MAXCHAR){
        argv[i] = strdup(token);
        i++;
        token = strtok(NULL, " \n\t");
    }
    argv[i] = NULL; //set the last argument to be NULL

    return ++i; //return argc
}

// parse an individual command
// register the arguments in argv and the destination file
// return -1 if
// 1. more than 1 redirection symbols
// 2. no destination file or more than 1 destination files
// return argc
// argc 1 if the only argument is 0
int parse_indiv_cmd(char* indiv_cmd, char **argv, char **file)
{
    int redirect = check_redirection(indiv_cmd);
    int argc = -1;
    char* first_half;
    char* second_half;
    char* token1_second_half;
    char* token2_second_half;
    char* indiv_cmd_dup = strdup(indiv_cmd);

    switch(redirect) {
        case -1 :
            print_error();
            break;
        case 0 :
            argc = parse_no_redir(indiv_cmd, argv);
            file = NULL;
            break;
        case 1 :
            if (indiv_cmd[0] == '>'){
                first_half = NULL;
                second_half = strtok(indiv_cmd_dup, ">\n\t");
            } else {
                first_half = strtok(indiv_cmd_dup, ">\n\t");
                second_half = strtok(NULL, ">\n\t");
            }
            if (second_half == NULL){
                print_error();
            } else {
                token1_second_half = strtok(second_half, " \n\t");
                token2_second_half = strtok(NULL, " \n\t");
                if ((token1_second_half == NULL) || token2_second_half){
                    print_error();
                } else {
                    file[0] = strdup(token1_second_half);
                    argc = parse_no_redir(first_half, argv);
                }
            }
            break;
        case 2 :
            if (indiv_cmd[0] == '>' && indiv_cmd[1] == '+'){
                first_half = NULL;
                second_half = strtok(indiv_cmd_dup, ">+\n\t");
            } else {
                first_half = strtok(indiv_cmd_dup, ">+\n\t");
                second_half = strtok(NULL, ">+\n\t");
            }
            if (second_half == NULL){
                print_error();
            } else {
                token1_second_half = strtok(second_half, " \n\t");
                token2_second_half = strtok(NULL, " \n\t");
                if ((token1_second_half == NULL) || token2_second_half){
                    print_error();
                } else {
                    file[0] = strdup(token1_second_half);
                    argc = parse_no_redir(first_half, argv);
                }
            }
    }
    return argc;
}

void execute(int argc, char** argv, char** file, char* indiv_cmd)
{
    if (argc == -1 || argc == 1) return;
    int redirect = check_redirection(indiv_cmd);
    char* first_arg = argv[0];
    char* second_arg = argv[1];

    if (strcmp(first_arg, "exit") == 0){
        if (second_arg == NULL && redirect == 0)
            exit(0);
        else{
            print_error();
            return;
        }
    } else if (strcmp(first_arg, "pwd") == 0){
        if (second_arg == NULL && redirect == 0){
            char directory[1024];
            getcwd(directory, 1024);
            myPrint(directory);
            myPrint("\n");
        } else {
            print_error();
            return;
        }
    } else if (strcmp(first_arg, "cd") == 0){
        if (argc == 2 && redirect == 0)
            chdir(getenv("HOME"));
        else if (argc == 3 && redirect == 0){
            int x = chdir(second_arg);
            if (x)
                print_error();
        } else {
            print_error();
            return;
        }
    } else {
        int forkret;
        int fd;
		FILE* file_copy;
		FILE* temp_one;

        if ((forkret = fork()) == 0){
        	switch (redirect){
        		case 0:
	        		if (execvp(argv[0], argv) == -1){
	              		print_error();
	        			exit(0); //terminate the child process!!!
	        		}
	        		break;
        		case 1:
	                fd = open(file[0], O_WRONLY|O_TRUNC|O_EXCL|O_CREAT,
	                	S_IRUSR|S_IRGRP|S_IWGRP|S_IWUSR);
	                if (fd != -1){
	                    dup2(fd, 1);
	                    close(fd);
	                } else {
	                    print_error();
	                    exit(0);
	                }
	                if (execvp(argv[0], argv) == -1){
	                	print_error();
	                    exit(0);
	                }
	                break;
                case 2:
                    file_copy = fopen(file[0], "r");
                    temp_one = fopen("temp1", "w+");
                    char buff[MAXCHAR];
                    if (file_copy) {
                    	while (fgets(buff, MAXCHAR, file_copy)){
                            if (fputs(buff, temp_one) < 0){
                                print_error();
                            }
                        }
                        fclose(file_copy);
                        remove(file[0]);
                    }
                	fclose(temp_one);

                    int fork_ret = fork();
                    // have to do this in a new process
                    // need new private address space
                    if (fork_ret == 0){
                        int temp_two = open("temp2", O_RDWR|O_EXCL|O_CREAT, 0666);
                        dup2(temp_two, 1);
                        if (execvp(argv[0], argv) == -1){
                            print_error();
                            exit(0);
                        }
                        close(temp_two);
                        exit(0);
                    } else {
                        wait(NULL);
                    }
                    int new_file_fd = open(file[0], O_RDWR|O_EXCL|O_CREAT, 0640);
                    // create a new file with the same name as the original one
                    // has to use system call open b/c fopen can't create a new one
                    if (new_file_fd < 0) {
                        print_error();
                        close(new_file_fd);
                        exit(0);
                    }
                    close(new_file_fd);

                    // now the grand operations start...
                    FILE* new_file = fopen(file[0], "w+");
                    FILE* temp_one_p = fopen("temp1", "r");
                    FILE* temp_two_p = fopen("temp2", "r");
                    
                    if (temp_two_p == NULL) {
                        print_error();
                    } else {
                        while(fgets(buff, MAXCHAR, temp_two_p)) {
                            fputs(buff, new_file);
                        }
                    }
                    if (temp_one_p == NULL) {
                        print_error();
                    } else {
                        while(fgets(buff, MAXCHAR, temp_one_p)) {
                            fputs(buff, new_file);
                        }
                    }
                    remove("temp1");
                    remove("temp2");
                    exit(0);
	            	break;
            }
        } else 
        	wait(NULL);
    }
}

int main(int argc, char *argv[])
{
    char cmd_buff[2 * MAXCHAR];
    char cmd_buff_batch[2 * MAXCHAR];
    char* pinput;
    char* indiv_cmds[MAXCHAR];
    int n_indiv_cmds;
    char* cmd_argv[MAXCHAR];
    int cmd_argc;
    char* file[1];

    if (argc == 1){
        while (1) {
            myPrint("myshell> ");
            pinput = fgets(cmd_buff, 2 * MAXCHAR, stdin);
            if (!pinput){
                exit(0);
            }
            if (strlen(cmd_buff) > MAXCHAR + 1){
            	myPrint(cmd_buff);
                print_error();
                continue;
            } else {
                n_indiv_cmds = parse_cmd_buff(cmd_buff, indiv_cmds);
                for (int i = 0; i < n_indiv_cmds; i++){
                    cmd_argc = parse_indiv_cmd(indiv_cmds[i], cmd_argv, file);
                    execute(cmd_argc, cmd_argv, file, indiv_cmds[i]);
                }
            }
        }
    } else if (argc == 2) {
        FILE* batch_file = fopen(argv[1], "r");
        if (batch_file == NULL){
            print_error();
            exit(0);
        }
        while(fgets(cmd_buff_batch, 2*MAXCHAR, batch_file) != NULL){
        	int l = strlen(cmd_buff_batch);
        	char *indiv_line = strdup(cmd_buff_batch);
        	char *indiv_line_real = strtok(indiv_line, " \n\t");

        	if (indiv_line_real == NULL){
        		continue;
        	} else if (l > (MAXCHAR + 1) && indiv_line_real != NULL){
        		myPrint(cmd_buff_batch);
        		print_error();
        		continue;
        	} else {
        		myPrint(cmd_buff_batch);
        		n_indiv_cmds = parse_cmd_buff(cmd_buff_batch, indiv_cmds);
                for (int i = 0; i < n_indiv_cmds; i++){
                	remove("temp1");
                	remove("temp2");
                    cmd_argc = parse_indiv_cmd(indiv_cmds[i], cmd_argv, file);
                    execute(cmd_argc, cmd_argv, file, indiv_cmds[i]);
                }
        	}
        }
    } else {
    	print_error();
    	exit(0);
    }
}