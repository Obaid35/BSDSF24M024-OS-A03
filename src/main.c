#include "shell.h"

int main() {
    char* cmdline;
    char** tokens;
    command_t cmds[MAXCMDS];

    init_completion();

    // readline() prints the prompt, lets the user edit the line, and returns it in
    // malloc'd memory. It returns NULL on Ctrl+D (end of input).
    while ((cmdline = readline(PROMPT)) != NULL) {
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
            int ncmds = parse_pipeline(tokens, cmds);   // -1 = syntax error (already printed)

            // A single command may be a built-in (cd, exit, ...): it runs inside the shell.
            // Everything else (external commands, pipelines) runs in child processes.
            if (ncmds > 0 && !(ncmds == 1 && handle_builtin(cmds[0].argv))) {
                execute(cmds, ncmds);
            }
            free_tokens(tokens);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
