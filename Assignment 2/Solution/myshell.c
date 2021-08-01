//
// Created by Dvir Ben Asuli on 17/11/2020.
// Tel-Aviv University, Operating Systems Course Assignment 2, 2021a
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

/***** Enums *****/
typedef enum command_type_enum {
    REGULAR = 0,
    PIPE,
    PARALLEL
} cmd_typ;

typedef enum result_s {
    SUCCESS,
    FAIL
} result;

/***** Declarations *****/

/**
 * @purpose The skeleton calls this function before the first invocation of process_arglist()
 *          for any initialization and setup that are necessary for this function
 * @return 0 on success, something else otherwise
 */
int prepare(void);

/**
 * @purpose Receives the parsed command line and executes it.
 * @param count => Number of arguments in arglist minus 1
 * @param arglist => Array with #count non-NULL words and one NULL value at the end. Commands specified
 *                   in the arglist should be executed as a child process using fork() and execvp()
 * @return 1 if no error occured, 0 otherwise
 */
int process_arglist(int count, char **arglist);

/**
 * @purpose The skeleton calls this function before exiting for any cleanups
 *           related to process_arglist() that are necessary
 * @return 0 on success, something else otherwise
 */
int finalize(void);

/**
 * @purpose Initializes a sigaction struct to handle SIGINT for current proccess.
 * @param mode => 0 -> Process needs to ignore SIGINT
 *                1- > Process handles the SIGINT by termination using default action
 */
void handle_sigint(int mode);

/**
 * @purpose Received command line and finds special '|' or '&' symbols in it.
 * @param count =>  Number of values in the arglist, for iterations
 * @param arglist =>  Command line
 * @return REGULAR if no special symbol was found,
 *         PIPE if a "|" symbol was identified
 *         PARALLEL if a "& " symbol was identified
 */
cmd_typ disclose_pipe_or_parallel(int count, char** arglist);

/**
 * @purpose Called Handler for SIGCLD and ECHILD Signals
 */
void valyrian_steel ();

/**
 * @purpose Called when we need to perform a single command, not pipe.
 * @param arglist =>  Command line
 * @param type => indicates if we perform the new task in background or not
 */
void execute_single_command(char** arglist, cmd_typ type);

/**
 * @purpose Called when we need to perform a two command and pipe their results
 * @param command1 =>  'arglist'-type command line of first command
 * @param command2 =>  'arglist'-type command line of second command
 */
void pipe_processes(char** command1, char** command2);

/***** Function Implementations *****/

cmd_typ disclose_pipe_or_parallel(int count, char** arglist) {
    for (int i = 0; i < count; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            return PIPE;
        }

        if (((i == (count - 1)) && (strcmp(arglist[i], "&") == 0))) {
            arglist[i] = NULL;
           return PARALLEL;
        }
    }
    return REGULAR; //No special symbol was found
}

void valyrian_steel (){
    //Handler for wights
    int pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {}
    if (pid < 0 && errno != ECHILD){
        fprintf(stderr, "Error dealing with wights");
        exit(1);
    }
}


void execute_single_command(char** arglist, cmd_typ type) {
    pid_t pid = fork();
    int status;

    if (pid == -1) {
        fprintf(stderr, "Fork action went wrong");
        exit(1);

    } else if (pid == 0){
        //Child process
        if (type == REGULAR){
            //Foreground child processes should terminate upon SIGINT.
            handle_sigint(1);
        }

        //Background child processes should not terminate upon SIGINT
        execvp(arglist[0], arglist);

        //If we reached here EXECVP failed and we need to exit
        fprintf(stderr, "Command initiation failed for %%s", arglist[0]);
        exit(1);

    } else {
        //Parent Process
        if (type == REGULAR) {
            int wait_res;
            //If we are in "regular" mode the parent proccess need to wait for the child to finish
            wait_res = waitpid(pid, &status, WUNTRACED);
            if (wait_res < 0 && errno != ECHILD && errno != EINTR){
                fprintf(stderr, "Error Waiting for child");
                exit(1);
            }
        }

        if (type == PARALLEL) {

            struct sigaction longclaw = {
                    //To kill Wights
                    .sa_handler = valyrian_steel,
                    .sa_flags = SA_NOCLDSTOP | SA_RESTART
            };

            memset(&longclaw, 0, sizeof(longclaw));
            // signal(SIGCHLD, SIG_IGN);

            if (sigaction(SIGCHLD, &longclaw, NULL) == -1) {
                fprintf(stderr, "SIGCHLD error occurred");
                exit(1);
            }
        }

        //If we are running a PARALLEL process the parent process shouldn't wait for anything
    }
}

void pipe_processes(char** command1, char** command2){
    int descriptors[2];
    int pipe_res;
    pid_t first_split;
    pid_t second_split;
    int status;

    pipe_res = pipe(descriptors);
    if (pipe_res == -1){
        fprintf(stderr, "Pipe creation went wrong");
        exit(1);
    }

    // If we reached here the pipe is ok,
    // Descriptors[0] contains the descriptor of reading side
    // Descriptors[1] contains the file descriptor of writing side
    first_split = fork();

    if (first_split == -1) {
        close(descriptors[0]);
        close(descriptors[1]);
        fprintf(stderr, "Fork action during pipe went wrong");
        exit(1);

    } else if (first_split == 0){
        //First child process, needs to write to the pipe
        handle_sigint(1);
        close(descriptors[0]); //This side is writing so we are closing reading side
        dup2(descriptors[1], 1); //Redirecting standard output of
                                // process to the writing side of pipe
        close(descriptors[1]);
        execvp(command1[0], command1);

        //If we reached here EXECVP failed and we need to exit
        fprintf(stderr, "Command 1 from pipe initiation failed");
        exit(1);

    } else {
        //We are in the parent process, need to create another child
        second_split = fork();

        if (second_split == -1) {
            fprintf(stderr, "Fork action during pipe went wrong");
            close(descriptors[0]);
            close(descriptors[1]);
            exit(1);

        } else if (second_split == 0){
            //Second child process, needs to read to the pipe
            handle_sigint(1);
            close(descriptors[1]);
            dup2(descriptors[0], 0); //Redirecting standard input of
                                     // process to the reading side of pipe
            close(descriptors[0]);
            execvp(command2[0], command2);

            //If we reached here EXECVP failed and we need to exit
            fprintf(stderr, "Command 2 from pipe initiation failed");
            exit(1);

        } else {
            //Arch-parent of both children
            int wait_res1;
            int wait_res2;

            close(descriptors[0]);
            close(descriptors[1]);
            wait_res1 = waitpid(first_split, &status, WUNTRACED);
            wait_res2= waitpid(second_split, &status, WUNTRACED);

            if (((wait_res1 < 0) || (wait_res2 < 0)) && errno != ECHILD && errno != EINTR){
                fprintf(stderr, "Error Waiting for child");
                exit(1);
            }
        }
    }
}

void handle_sigint(int mode) {
    struct sigaction sigaction1 = {
            .sa_flags = SA_RESTART
    };

    if (mode == 0) {
        sigaction1.sa_handler = SIG_IGN;
    } else {
        sigaction1.sa_handler = SIG_DFL;
    }

    if (sigaction(SIGINT, &sigaction1, NULL) == -1) {
        fprintf(stderr, "SIGINT error occurred");
    }
}

int prepare(void) {
    return 0;
}

int process_arglist(int count, char **arglist){
  cmd_typ command_type;

    //In this scope we ignore sigint signals
    handle_sigint(0);

    command_type = disclose_pipe_or_parallel(count, arglist);

  if (command_type == PIPE ) {
    char **command1;
    char **command2;

    for (int i = 0; i < count; i++) {
        if (strcmp(arglist[i], "|") == 0){
            arglist[i] = NULL; //Now will be considered the end of first command
            command1 = arglist;
            command2 = &arglist[i+1];
            break;
        }
    }

      pipe_processes(command1, command2);

  } else {
      //Running a single command, could be in the background
      execute_single_command(arglist, command_type);
  }

  return 1;

}

int finalize(void) {
    return 0;
}