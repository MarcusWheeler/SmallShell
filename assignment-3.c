/******************************************************************************

Online C Compiler.
Code, Compile, Run and Debug C program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h>
#include <err.h>
#include <sys/types.h>
#include <fcntl.h>
#define MAX_LINE_SIZE 2048
#define MAX_ARGUMENT_SIZE 512
#define MAX_PATH_LENGTH 4096

char** parse_string(char *argument_list, char **token_list);
char* process(char *token);
bool handle_exit(int *background_process_array, int background_process_index);
void handle_cd(char *destination);
void handle_status(int status);
void handle_SIGINT(int signal_number);
void handle_SIGTSTP(int signal_number);
//Global variable rip - strikes me as similar to interrupts tho
bool background_processes_enabled = true;
int main()
{
  bool ampersand_found = false;
  int status = 0;
  //The next four variables are error checking variables
  int dup_two_source_result;
  int dup_two_destination_result;
  int source_file_open_result;
  int destination_file_open_result;
  int trim_input;//index of where I need to insert a NULL
  int trim_output;//index of where I need to insert a NULL
  pid_t pid;
  bool exit_now = false;
  char **token_list;
  char *argument_list = malloc(sizeof(char) * MAX_LINE_SIZE + 1);
  //For now I'm not going to worry about the outrageously large swathes of memory I've claimed
  //I'm just going to get this working, and then I can remake my memory issues
  char *current_directory = malloc(sizeof(char) * MAX_PATH_LENGTH + 1);
  char *token;
  int number_of_tokens;
  char *destination = getenv("HOME");//I can set this to the home directory automatically for cd.
  char *redirect_input, *redirect_output;//Filenames for input and output redirection
  int *background_process_array = calloc(sizeof(int), 150); //Uber arbitrary number - I'll do a memory grower here too if I have time 
  int background_process_index = 0;
  
  while(!exit_now){
    //Reset my inventory keeping items
    redirect_input = redirect_output = NULL;
    number_of_tokens = 0;
    ampersand_found = false;
    trim_input = 0;
    trim_output = 0;
    
    //Now I can just execute whatever command I have because I know it isn't a built in
    
    //I'm going to set up y signal structs here - I'm going to do the structs in the body and then 
    //Divide up the installations into foreground/background/parent as needed
    struct sigaction SIGTSTP_action = {0}, SIGINT_action = {0}, ignore_action = {0};
    //Now I can register my actions and install only in the spots that I need to
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    ignore_action.sa_handler = SIG_IGN;            
    SIGINT_action.sa_handler = handle_SIGINT;
    
    
    
    getcwd(current_directory, MAX_PATH_LENGTH + 1);
    strcat(current_directory, "~");
    printf("%s", ":");
    fflush(stdout);
    fgets(argument_list, MAX_ARGUMENT_SIZE + 1, stdin);
    token_list = (char**)malloc(sizeof(char*) * MAX_PATH_LENGTH);
    token_list = parse_string(argument_list, token_list);
    if((token_list[0][0] != '#' && token_list[0][0])){
      for(int i = 0; token_list[i]; ++i){
        if(strstr(token_list[i], "$$")){
          token_list[i] = process(token_list[i]);
        }
        if(strcmp(token_list[i], ">") == 0){
          trim_output = i;
          redirect_output = token_list[i + 1];
        }
        if(strcmp(token_list[i], "<") == 0){
          trim_input = i; 
          redirect_input = token_list[i+1];
        }
        number_of_tokens++;
      }
      //This is the case if we have the last input as an ampersand
      if(strcmp(token_list[number_of_tokens - 1], "&") == 0){
        ampersand_found = true;
        if(!background_processes_enabled){
          token_list[number_of_tokens - 1] = NULL;
        }
        if(redirect_input == NULL){
          trim_input = number_of_tokens - 1;
        }
        if(redirect_output == NULL){
          trim_output = number_of_tokens  - 1;
        }
        
      }
      
      
      //Handle the actual tokens now
      if(strcmp(token_list[0], "exit") == 0){
        exit_now = handle_exit(background_process_array, background_process_index);
      }
      else if(strcmp(token_list[0], "cd") == 0){
        if(token_list[1]){
          handle_cd(token_list[1]);
        }
        else{
          handle_cd(destination);
        }
      }
      else if(strcmp(token_list[0], "status") == 0){
        handle_status(status);
      }
      
      else{
        pid = fork();
        //ERROR PORTION
        if(pid == -1){
          printf("Error forking rip\n");
        }
        
        
        
        
        //THIS IS CHILD PORTION   
        //The very first thing I can do is install my signal handlers depending on whether I'm in the foreground or background
        else if(pid == 0){
          
          if(ampersand_found && background_processes_enabled){
            //I'm in the background
            sigaction(SIGTSTP, &ignore_action, NULL);
            sigaction(SIGINT, &ignore_action, NULL); //If I'm in the background, I'm supposed to ignore both of the possible signals mentioned
          }
          else{ //I'm in the foreground/need to be treated as such.
            sigaction(SIGTSTP, &ignore_action, NULL);
            sigfillset(&SIGINT_action.sa_mask);
            SIGINT_action.sa_flags = 0;
            sigaction(SIGINT, &SIGINT_action, NULL);
          }
          //If I have an input redirection, I need to take care of it before I can execute my call
          if(redirect_input || (ampersand_found && background_processes_enabled)){
            //General idea is that I need to open the file that will be my source, point my stdin to it, and then execute
            //I think that afterwards I need to redirect it, but I could be wrong.
            //I also think that I need to trim the last few arguments - basically just keep the non ampersand/input/output ones
            //Have to set this to Null afterwards in order to trim the last few arguments.
            if(redirect_input){
              source_file_open_result = open(redirect_input, O_RDONLY, 0754);
              
              if(source_file_open_result == -1){
                perror("couldn't open source file");
                exit(1);
              }
            }
            else{
              source_file_open_result = open("/dev/null", O_RDONLY, 0754);
              if(source_file_open_result == -1){
                perror("couldn'to open /dev/null for reading");
                exit(1);
              }
              
            }
            dup_two_source_result = dup2(source_file_open_result, 0);
            if(dup_two_source_result == -1){
              perror("couldn't dup source file descriptor");
              exit(1);
            }    
            
            token_list[trim_input] = NULL;
          }
          //Now I take care of any possible input redirections. I only need to do this if I have an output specified or if I've decided to run in the background.
          if(redirect_output ||(ampersand_found && background_processes_enabled)){
            if(redirect_output){//If i have a file specified
              destination_file_open_result = open(redirect_output, O_RDWR | O_CREAT | O_TRUNC, 0754);
              if(destination_file_open_result == -1){
                perror("couldn't open destination file");
                exit(1);
              }
            }
            else{//Now I have nothing specified but it is a backgroudn process, so I have to send it to /dev/null
              destination_file_open_result = open("/dev/null", O_WRONLY, 0754);
              if(destination_file_open_result == -1){
                perror("couldn't open /dev/null for writing");
                exit(1);
              }
            }
            dup_two_destination_result = dup2(destination_file_open_result,1);
            if(dup_two_destination_result == -1){
              perror("couldn't dup destination file descriptor");
              exit(1);
            }    
            token_list[trim_output] = NULL;
            
          } 
          //int debug = 0;
          //while(token_list[debug]){
            //printf("Tokens being passed: %s\n", token_list[debug]);
            //debug++;
          //  }
          if(execvp(token_list[0], token_list) == -1){
            err(errno, "failed to exec");
            exit(1);
          }
        }
        
        
        
        
        else{//I'm the parent
          //I'm the parent, so I can register my signals as needed
          sigaction(SIGINT, &ignore_action, NULL);
          sigfillset(&SIGTSTP_action.sa_mask);
          SIGTSTP_action.sa_flags = 0;
          sigaction(SIGTSTP, &SIGTSTP_action, NULL);
          
          //If I'm running in the background, all I have to do is mark it down and mention it
          if(ampersand_found && background_processes_enabled){//Am I in the back?
            background_process_array[background_process_index] = pid;
            background_process_index++;
            printf("Background process %d started", pid);
          }
          //Otherwise, I have some bookkeeping to do.
          else{
            waitpid(pid, &status, 0); //wait for my foreground process
            
            //Now I need to check for any of my background processes
            if(background_process_index > 0 && background_processes_enabled){ //If we have some sort of background process, I need to deal with the children - also if they're enabled
              while(pid != -1 && pid != 0){ //I need to check all of the children so I don't get any zombies.
                pid = waitpid(-1, &status, WNOHANG);  //The -1 makes it wait for any child process and should hit all of them
                if(WIFEXITED(status) && pid != 0 && pid != -1){//I have to make sure that the pid isn't myself
                  status = WEXITSTATUS(status);
                  printf("Process %d exited with exit value %d\n", pid, status);
                }
                else if(WIFSIGNALED(status) && pid != 0 && pid != -1){
                  status = WTERMSIG(status);
                  printf("Process %d terminated by signal %d\n", pid, status);
                }
                if(pid != 0 && pid != -1){
                  //Now I need to go through my array and change my items
                  bool adjust = false;
                  for(int i= 0; i < background_process_index; ++i){
                    if(adjust){
                      background_process_array[i-1] = background_process_array[i];
                    }
                    if(background_process_array[i] == pid){
                      adjust = true;
                    }
                  }
                  background_process_index--; 
                }
              }
            }
          }
        }
      }
      for(int i = 0; token_list[i]; ++i){
        free(token_list[i]);
      }
      free(token_list);
    }
  }
  free(argument_list);
  free(current_directory);
  free(background_process_array);
  return 0;
}

char** parse_string(char *argument_list, char **token_list){
  char *token;
  char *location = argument_list;
  pid_t pid;
  //This idea for parsing is taken from the man pages example of strtok_r
  for(int i = 0; ; ++i, argument_list = NULL){
    token = strtok_r(argument_list, " ", &location);      
    
    //strcpy(token_list[i], token);
    if(token == NULL){
      //Assign my final arg to NULL
      token_list[i] = NULL;
      break;
    }
    //Clear the Newline from the last argument
    if(token[strlen(token) - 1] == '\n'){
      token[strlen(token) - 1] = '\0';
    }
    
    
    token_list[i] = (char*)malloc(sizeof(char) * strlen(token));
    strcpy(token_list[i], token);
  }
  
  return token_list;
}

//Right now I can just make it exit - later I'll change it
bool handle_exit(int *background_process_array, int background_process_index){
  //I can terminate any processes that I need to here before I return true
  for(int i = 0; i < background_process_index; ++i){
    if(background_process_array[background_process_index] != 0){
      kill(background_process_array[background_process_index], SIGKILL);
    }
  }
  return true;
}
void handle_cd(char *destination){
  if(chdir(destination) == -1){
    printf("Error changing directory to %s \n", destination);
    fflush(stdout);
  }
}

void handle_status(int status){
  if(WIFEXITED(status)){
    status = WEXITSTATUS(status);
  }
  else{
    status = WTERMSIG(status);
  }
  printf("%d", status);
  fflush(stdout);
}


char *process(char *token){
  char *processed_token = malloc(sizeof(char) * strlen(token) * 6);
  char *pid_string = malloc(sizeof(char) * 22);
  int token_index = 0;
  int processed_token_index = 0;
  int pid_index = 0;
  int pid = getpid();
  bool found = false;
  sprintf(pid_string, "%d", pid);
  
  for(token_index =0; token_index < strlen(token); ++token_index){
    if(token[token_index] == '$' && !found){
      processed_token[processed_token_index] = token[token_index];
      processed_token_index++;
      found = true;
    }
    else if(token[token_index] == '$' && found){
      processed_token_index--;
      for(pid_index = 0; pid_index < strlen(pid_string); ++pid_index){
        processed_token[processed_token_index] = pid_string[pid_index];
        processed_token_index++;
      }
      found = false;
    }
    else{
      if(found){
        found = false;
      }
      processed_token[processed_token_index] = token[token_index];
      processed_token_index++;
      found = false;
    }
  }
  processed_token[processed_token_index] = '\0';
  free(token);
  free(pid_string);
  return processed_token;
}



void handle_SIGTSTP(int signal_number){
  if(background_processes_enabled){
    write(1, "Entering foreground only mode\n", 30);
  }
  else{
    write(1, "Entering background only mode\n", 30);
  }
  background_processes_enabled = !background_processes_enabled;
}

void handle_SIGINT(int signal_number){
  write(1, "INT received", 12);
  raise(SIGKILL);
}
