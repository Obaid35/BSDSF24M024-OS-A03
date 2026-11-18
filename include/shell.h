#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <readline/readline.h>   // readline(): prompt + line editing + Tab completion
#include <readline/history.h>    // add_history(): Up/Down arrow history

#define MAX_LEN 512
#define MAXARGS 10
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20   // how many recent commands are remembered

// Function prototypes
char** tokenize(char* cmdline);
int execute(char** arglist);
int handle_builtin(char** arglist);   // returns 1 if arglist[0] was a built-in (and ran it), else 0
void add_to_history(const char* cmdline);
char* get_history(int n);             // copy of command number n, or NULL if n is out of range
void init_completion(void);           // tell readline how to complete words on Tab

#endif // SHELL_H
