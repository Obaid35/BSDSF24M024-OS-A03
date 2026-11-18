#include "shell.h"

int last_status = 0;   // exit status of the last command (0 = success)

// Runs one part of the line (the part between ; or &): a built-in or a pipeline.
// Returns its exit status.
static int run_segment(char** seg, int background) {
    command_t cmds[MAXCMDS];
    int ncmds = parse_pipeline(seg, cmds);   // -1 = syntax error (already printed)
    if (ncmds <= 0)
        return 2;                            // 2 = syntax error (like bash)

    // A single command may be a built-in (cd, exit, ...): it runs inside the shell.
    // Everything else (external commands, pipelines) runs in child processes.
    if (ncmds == 1 && handle_builtin(cmds[0].argv))
        return last_status;                  // handle_builtin() has set it
    return execute(cmds, ncmds, background);
}

// Splits the tokens at ";" and "&" and runs the parts one after the other.
//   a ; b   -> run a, wait for it, then run b
//   a & b   -> start a in the background, then run b at once
// Returns the exit status of the last part (also kept in last_status).
static int run_line(char** tokens) {
    char* seg[MAXTOKENS + 1];   // pointers to the tokens of the current part
    int n = 0;

    for (int i = 0; ; i++) {
        char* tok = tokens[i];
        if (tok == NULL || strcmp(tok, ";") == 0 || strcmp(tok, "&") == 0) {
            seg[n] = NULL;
            if (n > 0) {
                last_status = run_segment(seg, tok != NULL && strcmp(tok, "&") == 0);
            } else if (tok != NULL) {          // e.g. "; ls"  or  "ls ; ; pwd"
                fprintf(stderr, "myshell: syntax error near '%s'\n", tok);
                last_status = 2;
                return last_status;
            }
            if (tok == NULL)
                return last_status;
            n = 0;                             // start the next part
        } else {
            seg[n++] = tok;
        }
    }
}

// Tokenizes one stored line of a then/else block and runs it.
static void run_text(char* line) {
    char** tokens = tokenize(line);
    if (tokens == NULL)
        return;
    if (strcmp(tokens[0], "if") == 0)
        fprintf(stderr, "myshell: an if inside an if is not supported\n");
    else
        run_line(tokens);
    free_tokens(tokens);
}

// True if the line is exactly the keyword (spaces around it are allowed): "then", "else", "fi".
static int is_keyword(const char* line, const char* kw) {
    line += strspn(line, " \t");
    size_t n = strlen(kw);
    return strncmp(line, kw, n) == 0 && line[n + strspn(line + n, " \t")] == '\0';
}

// The line started with "if". Read the rest of the block with a "> " prompt:
//     if <command>          <- tokens: "if" + the condition command
//     then
//       commands...         <- stored in then_part[]
//     else                  (optional)
//       commands...         <- stored in else_part[]
//     fi
// Then run the condition and, by its exit status, the then-part (0) or the else-part (not 0).
static void run_if(char** tokens) {
    char* then_part[MAX_BLOCK];
    char* else_part[MAX_BLOCK];
    int nthen = 0, nelse = 0;
    int state = 0;            // 0 = waiting for "then", 1 = inside then-part, 2 = inside else-part
    int found_fi = 0;
    char* line;

    if (tokens[1] == NULL) {
        fprintf(stderr, "myshell: syntax error: 'if' needs a command\n");
        return;
    }

    while (!found_fi && (line = readline("> ")) != NULL) {
        if (state == 0) {
            if (is_keyword(line, "then")) {
                state = 1;
            } else {
                fprintf(stderr, "myshell: syntax error: expected 'then'\n");
                free(line);
                break;
            }
        } else if (state == 1 && is_keyword(line, "else")) {
            state = 2;
        } else if (is_keyword(line, "fi")) {
            found_fi = 1;
        } else if (line[strspn(line, " \t")] != '\0') {      // a command line (skip blank lines)
            if (state == 1 && nthen < MAX_BLOCK)
                then_part[nthen++] = strdup(line);
            else if (state == 2 && nelse < MAX_BLOCK)
                else_part[nelse++] = strdup(line);
            else
                fprintf(stderr, "myshell: if-block too long, line ignored\n");
        }
        free(line);
    }

    if (found_fi) {
        int status = run_line(tokens + 1);       // run the condition (everything after "if")
        if (status == 0) {                       // success -> then-part
            for (int i = 0; i < nthen; i++) run_text(then_part[i]);
        } else {                                 // failure -> else-part (may be empty)
            for (int i = 0; i < nelse; i++) run_text(else_part[i]);
        }
    } else if (state != 0) {
        fprintf(stderr, "myshell: syntax error: 'fi' missing\n");
    }

    for (int i = 0; i < nthen; i++) free(then_part[i]);
    for (int i = 0; i < nelse; i++) free(else_part[i]);
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
            if (strcmp(tokens[0], "if") == 0)
                run_if(tokens);        // reads more lines until "fi"
            else
                run_line(tokens);
            free_tokens(tokens);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
