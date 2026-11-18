#include "shell.h"

// ---------------- background jobs ----------------
typedef struct {
    int id;                 // job number shown as [1], [2], ...
    pid_t pid;              // PID of the (last) process of the job
    char cmd[MAX_LEN];      // the command text, for "jobs"
} job_t;

static job_t jobs[MAXJOBS];
static int njobs = 0;
static int next_job_id = 1;

// Builds a text like "cat f.txt | grep x" from the commands (only for showing it).
static void command_text(command_t cmds[], int ncmds, char* text) {
    text[0] = '\0';
    for (int i = 0; i < ncmds; i++) {
        if (i > 0) strncat(text, " | ", MAX_LEN - strlen(text) - 1);
        for (int j = 0; cmds[i].argv[j] != NULL; j++) {
            if (j > 0) strncat(text, " ", MAX_LEN - strlen(text) - 1);
            strncat(text, cmds[i].argv[j], MAX_LEN - strlen(text) - 1);
        }
    }
}

static void add_job(pid_t pid, command_t cmds[], int ncmds) {
    if (njobs == MAXJOBS) {
        fprintf(stderr, "myshell: job table full, job not listed\n");
        return;
    }
    jobs[njobs].id = next_job_id++;
    jobs[njobs].pid = pid;
    command_text(cmds, ncmds, jobs[njobs].cmd);
    printf("[%d] %d\n", jobs[njobs].id, (int)pid);   // like bash: "[1] 4321"
    njobs++;
}

// Called before every prompt. waitpid(-1, ..., WNOHANG) collects ANY child that has
// finished, but does not block if none has: so finished background jobs never stay zombies.
void reap_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < njobs; i++) {
            if (jobs[i].pid == pid) {
                printf("[%d]+  Done    %s\n", jobs[i].id, jobs[i].cmd);
                jobs[i] = jobs[njobs - 1];     // remove it: move the last job into its place
                njobs--;
                break;
            }
        }
    }
    if (njobs == 0)
        next_job_id = 1;                       // like bash, numbering restarts when no jobs are left
}

void print_jobs(void) {
    reap_jobs();                               // do not show jobs that have already finished
    if (njobs == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < njobs; i++)
        printf("[%d]  %-6d Running    %s &\n", jobs[i].id, (int)jobs[i].pid, jobs[i].cmd);
}

// ---------------- running commands ----------------

// Runs in the CHILD, after fork() and before execvp():
// "< file" -> the file becomes fd 0 (stdin),  "> file" -> the file becomes fd 1 (stdout).
static void setup_redirection(command_t* cmd) {
    if (cmd->infile != NULL) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd == -1) {
            perror(cmd->infile);
            exit(1);
        }
        dup2(fd, STDIN_FILENO);    // fd 0 now points to the file
        close(fd);                 // the original fd is not needed any more
    }
    if (cmd->outfile != NULL) {
        int fd = open(cmd->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);   // create or empty it
        if (fd == -1) {
            perror(cmd->outfile);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);   // fd 1 now points to the file
        close(fd);
    }
}

// Runs cmds[0] | cmds[1] | ... | cmds[ncmds-1], each one in its own child process.
// Foreground: waits for all of them and returns the exit status of the last one.
// Background (&): does NOT wait; the job is remembered and reaped later by reap_jobs().
int execute(command_t cmds[], int ncmds, int background) {
    pid_t pids[MAXCMDS];
    int prev_read = -1;   // read end of the pipe coming from the previous command (-1 = none)
    int fd[2];            // fd[0] = read end, fd[1] = write end of a new pipe

    for (int i = 0; i < ncmds; i++) {
        int is_last = (i == ncmds - 1);

        if (!is_last && pipe(fd) == -1) {   // a pipe joins command i to command i+1
            perror("pipe");
            return 1;
        }

        pid_t cpid = fork();
        if (cpid == -1) {
            perror("fork failed");
            return 1;
        }

        if (cpid == 0) {                    // ----- child: becomes command i -----
            if (!background) {              // the shell ignores Ctrl+C / Ctrl+\, but a
                signal(SIGINT, SIG_DFL);    // foreground command must be killable by them.
                signal(SIGQUIT, SIG_DFL);   // (a background job keeps ignoring them)
            }
            if (prev_read != -1) {          // read from the previous command's pipe
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            if (!is_last) {                 // write into the pipe to the next command
                close(fd[0]);               // this child never reads from its own pipe
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }
            setup_redirection(&cmds[i]);    // < and > (they win over a pipe)
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("Command not found");    // This line runs only if execvp fails
            exit(127);
        }

        // ----- parent (the shell) -----
        pids[i] = cpid;
        if (prev_read != -1)
            close(prev_read);               // the child has its own copy now
        if (!is_last) {
            close(fd[1]);                   // the shell never writes into the pipe
            prev_read = fd[0];              // the next child will read from here
        }
    }

    if (background) {                       // do not wait: give the prompt back at once
        add_job(pids[ncmds - 1], cmds, ncmds);
        return 0;
    }

    // Wait for every child, so none of them stays a zombie.
    int status = 0;
    for (int i = 0; i < ncmds; i++)
        waitpid(pids[i], &status, 0);       // after the loop: status of the last command

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WTERMSIG(status) == SIGINT)
        printf("\n");                       // after ^C, start the prompt on a new line
    return 128 + WTERMSIG(status);          // killed by a signal (like bash)
}
