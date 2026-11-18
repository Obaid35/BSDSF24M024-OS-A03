#include "shell.h"

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

// Runs cmds[0] | cmds[1] | ... | cmds[ncmds-1], each one in its own child process,
// and waits for all of them. With ncmds == 1 it is the normal fork-exec-wait.
// Returns the exit status of the last command.
int execute(command_t cmds[], int ncmds) {
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

    // Wait for every child, so none of them stays a zombie.
    int status = 0;
    for (int i = 0; i < ncmds; i++)
        waitpid(pids[i], &status, 0);       // after the loop: status of the last command

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 128 + WTERMSIG(status);          // killed by a signal (like bash)
}
