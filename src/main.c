#include "shell.h"

// Runs one part of the line (the part between ; or &): a built-in or a pipeline.
static void run_segment(char** seg, int background) {
    command_t cmds[MAXCMDS];
    int ncmds = parse_pipeline(seg, cmds);   // -1 = syntax error (already printed)
    if (ncmds <= 0)
        return;

    // A single command may be a built-in (cd, exit, ...): it runs inside the shell.
    // Everything else (external commands, pipelines) runs in child processes.
    if (ncmds == 1 && handle_builtin(cmds[0].argv))
        return;
    execute(cmds, ncmds, background);
}

// Splits the tokens at ";" and "&" and runs the parts one after the other.
//   a ; b   -> run a, wait for it, then run b
//   a & b   -> start a in the background, then run b at once
static void run_line(char** tokens) {
    char* seg[MAXTOKENS + 1];   // pointers to the tokens of the current part
    int n = 0;

    for (int i = 0; ; i++) {
        char* tok = tokens[i];
        if (tok == NULL || strcmp(tok, ";") == 0 || strcmp(tok, "&") == 0) {
            seg[n] = NULL;
            if (n > 0) {
                run_segment(seg, tok != NULL && strcmp(tok, "&") == 0);
            } else if (tok != NULL) {          // e.g. "; ls"  or  "ls ; ; pwd"
                fprintf(stderr, "myshell: syntax error near '%s'\n", tok);
                return;
            }
            if (tok == NULL)
                return;
            n = 0;                             // start the next part
        } else {
            seg[n++] = tok;
        }
    }
}

int main() {
    char* cmdline;
    char** tokens;

    init_completion();

    // Ctrl+C and Ctrl+\ must not kill the shell itself (only the command it is running).
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    while (1) {
        reap_jobs();   // collect background jobs that finished (prevents zombies)

        // readline() prints the prompt, lets the user edit the line, and returns it in
        // malloc'd memory. It returns NULL on Ctrl+D (end of input).
        if ((cmdline = readline(PROMPT)) == NULL)
            break;

        // "!n" -> replace the line with command number n from the history,
        // BEFORE it is tokenized or stored in the history.
        if (cmdline[0] == '!') {
            int n = atoi(cmdline + 1);
            char* old = get_history(n);
            free(cmdline);
            if (old == NULL) {
                fprintf(stderr, "!%d: event not found\n", n);
                continue;
            }
            cmdline = old;
            printf("%s\n", cmdline);   // show which command is being re-run (like bash)
            fflush(stdout);
        }

        // Store only non-empty lines: in readline's list (Up/Down arrows) and in ours (history, !n).
        if (cmdline[strspn(cmdline, " \t")] != '\0') {
            add_history(cmdline);
            add_to_history(cmdline);
        }

        if ((tokens = tokenize(cmdline)) != NULL) {
            run_line(tokens);
            free_tokens(tokens);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
