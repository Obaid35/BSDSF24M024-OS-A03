#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
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

        add_to_history(cmdline);

        if ((arglist = tokenize(cmdline)) != NULL) {
            // Built-ins (cd, exit, help, jobs, history) run inside the shell itself.
            // Only if it is NOT a built-in do we fork a child with execute().
            if (handle_builtin(arglist) == 0) {
                execute(arglist);
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
