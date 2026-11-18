#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>               // open() flags: O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>              // signal(), SIGINT, SIGQUIT
#include <readline/readline.h>   // readline(): prompt + line editing + Tab completion
#include <readline/history.h>    // add_history(): Up/Down arrow history

#define MAX_LEN 512
#define MAXARGS 64        // words in one command
#define MAXTOKENS 128     // words + operators on one line
#define MAXCMDS 10        // commands joined by |
#define MAXJOBS 20        // background jobs remembered by "jobs"
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20   // how many recent commands are remembered

// One simple command of a pipeline, e.g. for  "sort < in.txt | head > out.txt"
// there are two:  {argv = sort, infile = in.txt}  and  {argv = head, outfile = out.txt}
typedef struct {
    char* argv[MAXARGS + 1];   // NULL-terminated, ready for execvp()
    char* infile;              // file after "<" (NULL = keyboard / previous pipe)
    char* outfile;             // file after ">" (NULL = screen / next pipe)
} command_t;

// Function prototypes
char** tokenize(char* cmdline);
void free_tokens(char** tokens);
int parse_pipeline(char** tokens, command_t cmds[]);   // number of commands, or -1 on a syntax error
int execute(command_t cmds[], int ncmds, int background);
void reap_jobs(void);                 // collect finished background jobs (no zombies)
void print_jobs(void);                // the "jobs" built-in
int handle_builtin(char** arglist);   // returns 1 if arglist[0] was a built-in (and ran it), else 0
void add_to_history(const char* cmdline);
char* get_history(int n);             // copy of command number n, or NULL if n is out of range
void init_completion(void);           // tell readline how to complete words on Tab

#endif // SHELL_H
