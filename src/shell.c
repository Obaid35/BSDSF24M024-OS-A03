#include "shell.h"

// ---------------- command history ----------------
// A circular array: command number k (1, 2, 3, ...) is stored at index (k-1) % HISTORY_SIZE.
// When more than HISTORY_SIZE commands are typed, the oldest one is overwritten.
static char* history[HISTORY_SIZE];
static int history_count = 0;          // total commands added so far

void add_to_history(const char* cmdline) {
    // do not store a line that is empty or only spaces/tabs
    if (cmdline[strspn(cmdline, " \t")] == '\0') return;

    int index = history_count % HISTORY_SIZE;
    free(history[index]);              // free the oldest command (NULL the first time round)
    history[index] = strdup(cmdline);  // strdup = malloc + strcpy
    history_count++;
}

// Returns a malloc'd copy of command number n, or NULL if it is not in the history.
char* get_history(int n) {
    int oldest = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE + 1 : 1;
    if (n < oldest || n > history_count) return NULL;
    return strdup(history[(n - 1) % HISTORY_SIZE]);
}

static void show_history(void) {
    int oldest = (history_count > HISTORY_SIZE) ? history_count - HISTORY_SIZE + 1 : 1;
    for (int n = oldest; n <= history_count; n++)
        printf("%5d  %s\n", n, history[(n - 1) % HISTORY_SIZE]);
}

// ---------------- reading and tokenizing ----------------
char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    fflush(stdout);   // show the prompt now; an unflushed buffer would also be copied into every forked child
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        if (pos < MAX_LEN - 1)          // never write past the buffer
            cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }

    cmdline[pos] = '\0';
    return cmdline;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace

        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        // allocate exactly len+1 bytes, so a long word (e.g. a long path) cannot overflow
        arglist[argnum] = (char*)malloc(len + 1);
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// ---------------- built-in commands ----------------
// Runs a built-in command inside the shell process.
// Returns 1 if arglist[0] is a built-in (it has been executed), 0 otherwise.
int handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "exit") == 0) {
        int code = (arglist[1] != NULL) ? atoi(arglist[1]) : 0;
        printf("Shell exited.\n");
        exit(code);                    // ends the shell process itself
    }

    if (strcmp(arglist[0], "cd") == 0) {
        char* dir = arglist[1];
        if (dir == NULL) dir = getenv("HOME");   // plain "cd" goes to the home directory
        if (dir == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
        } else if (chdir(dir) == -1) {           // chdir() changes the cwd of THIS process
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  cd <dir>   change the current directory\n");
        printf("  exit [n]   leave the shell\n");
        printf("  help       show this message\n");
        printf("  history    show the last %d commands (!n re-runs command n)\n", HISTORY_SIZE);
        printf("  jobs       list background jobs\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        show_history();
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;   // not a built-in
}
