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

// ---------------- Tab completion (GNU Readline) ----------------
// The line itself is now read by readline() in main.c (read_cmd() is no longer needed).

static char* builtin_names[] = { "cd", "exit", "help", "history", "jobs", NULL };

// Readline calls this again and again for ONE word: state == 0 on the first call,
// then 1, 2, 3 ... It must return one matching name (malloc'd) per call, and NULL when done.
// Matches are the built-in names first, then the programs in every directory of $PATH.
static char* command_generator(const char* text, int state) {
    static int builtin_index;
    static size_t len;
    static char* path_copy = NULL;   // our own copy of $PATH, cut into pieces by strtok_r()
    static char* saveptr;
    static char* dir_name;           // the PATH directory being searched now
    static DIR* dirp = NULL;
    struct dirent* entry;

    if (state == 0) {                // a new word: start from the beginning
        builtin_index = 0;
        len = strlen(text);
        if (dirp != NULL) { closedir(dirp); dirp = NULL; }
        free(path_copy);
        path_copy = getenv("PATH") ? strdup(getenv("PATH")) : NULL;
        dir_name = path_copy ? strtok_r(path_copy, ":", &saveptr) : NULL;
    }

    // 1. built-in command names
    while (builtin_names[builtin_index] != NULL) {
        const char* name = builtin_names[builtin_index++];
        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }

    // 2. file names in each PATH directory (e.g. /usr/bin), one directory at a time
    while (dir_name != NULL) {
        if (dirp == NULL)
            dirp = opendir(dir_name);
        if (dirp != NULL) {
            while ((entry = readdir(dirp)) != NULL) {
                if (entry->d_name[0] != '.' && strncmp(entry->d_name, text, len) == 0)
                    return strdup(entry->d_name);    // the next call continues from here
            }
            closedir(dirp);
            dirp = NULL;
        }
        dir_name = strtok_r(NULL, ":", &saveptr);    // move to the next PATH directory
    }
    return NULL;                     // no more matches
}

// Called by readline when Tab is pressed. start = position of the word in the line.
static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0)                  // first word of the line -> complete a command name
        return rl_completion_matches(text, command_generator);
    return NULL;                     // any other word -> readline's default: file names
}

void init_completion(void) {
    rl_attempted_completion_function = shell_completion;
}

// ---------------- tokenizing and parsing ----------------

// characters that are a token of their own even without spaces: "ls>out" = ls  >  out
static int is_special(char c) {
    return c == '<' || c == '>' || c == '|';
}

static int is_operator(const char* tok) {
    return strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0 || strcmp(tok, "|") == 0;
}

// Splits the line into words and operators. Text inside "..." or '...' stays one word
// (the quotes are removed), so  grep "hello world" f.txt  gives 3 tokens.
// Returns a NULL-terminated array of malloc'd strings, or NULL if the line has no tokens.
char** tokenize(char* cmdline) {
    char** tokens = malloc(sizeof(char*) * (MAXTOKENS + 1));
    char word[MAX_LEN];
    int ntok = 0;
    char* cp = cmdline;

    while (*cp != '\0') {
        while (*cp == ' ' || *cp == '\t') cp++;        // skip spaces between tokens
        if (*cp == '\0') break;
        if (ntok == MAXTOKENS) {
            fprintf(stderr, "myshell: too many words, the rest is ignored\n");
            break;
        }

        int len = 0;
        if (is_special(*cp)) {                         // an operator: < > |
            word[len++] = *cp++;
        } else {                                       // a normal word
            char quote = 0;                            // 0, or the quote we are inside
            while (*cp != '\0') {
                if (quote != 0) {
                    if (*cp == quote) quote = 0;       // closing quote: not copied
                    else if (len < MAX_LEN - 1) word[len++] = *cp;
                } else if (*cp == '"' || *cp == '\'') {
                    quote = *cp;                       // opening quote: not copied
                } else if (*cp == ' ' || *cp == '\t' || is_special(*cp)) {
                    break;                             // end of this word
                } else if (len < MAX_LEN - 1) {
                    word[len++] = *cp;
                }
                cp++;
            }
        }
        word[len] = '\0';
        tokens[ntok++] = strdup(word);
    }

    if (ntok == 0) {                                   // empty line or only spaces
        free(tokens);
        return NULL;
    }
    tokens[ntok] = NULL;
    return tokens;
}

void free_tokens(char** tokens) {
    for (int i = 0; tokens[i] != NULL; i++)
        free(tokens[i]);
    free(tokens);
}

// Turns the tokens into an array of commands separated by "|".
// "<" and ">" take the next token as a file name. The argv pointers point INTO tokens,
// so nothing new is allocated (free_tokens() frees everything later).
// Returns the number of commands, or -1 after printing a syntax error.
int parse_pipeline(char** tokens, command_t cmds[]) {
    int ncmds = 0;     // commands finished so far
    int argc = 0;      // words in the current command
    command_t* cmd = &cmds[0];
    memset(cmd, 0, sizeof(command_t));                 // argv[] all NULL, no files

    for (int i = 0; tokens[i] != NULL; i++) {
        char* tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            if (argc == 0) {
                fprintf(stderr, "myshell: syntax error near '|'\n");
                return -1;
            }
            cmd->argv[argc] = NULL;                    // finish this command
            ncmds++;
            if (ncmds == MAXCMDS) {
                fprintf(stderr, "myshell: too many commands in the pipeline\n");
                return -1;
            }
            cmd = &cmds[ncmds];                        // start the next command
            memset(cmd, 0, sizeof(command_t));
            argc = 0;
        } else if (strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0) {
            char* file = tokens[i + 1];
            if (file == NULL || is_operator(file)) {
                fprintf(stderr, "myshell: syntax error: file name missing after '%s'\n", tok);
                return -1;
            }
            if (tok[0] == '<') cmd->infile = file;
            else               cmd->outfile = file;
            i++;                                       // the file name is used, skip it
        } else {
            if (argc == MAXARGS) {
                fprintf(stderr, "myshell: too many arguments\n");
                return -1;
            }
            cmd->argv[argc++] = tok;
        }
    }

    if (argc == 0) {                                   // e.g. "ls |"  or  "> out.txt"
        fprintf(stderr, "myshell: syntax error: command missing\n");
        return -1;
    }
    cmd->argv[argc] = NULL;
    return ncmds + 1;
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
