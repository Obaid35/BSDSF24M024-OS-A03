# myshell — a custom UNIX shell (Operating Systems, PA-03)

A simple command-line interpreter written in C, built feature by feature.

## Build and run

```bash
make            # builds bin/myshell
./bin/myshell   # starts the shell (Ctrl+D or "exit" to quit)
make clean      # removes obj/*.o and bin/myshell
```

## Directory structure

- `src/` — C source files (`main.c`, `shell.c`, `execute.c`)
- `include/` — header file (`shell.h`)
- `Makefile` — build script
- `bin/`, `obj/` — created by `make` (not stored in git)

## Versions

| Tag | Feature |
|---|---|
| v1.0-base | Base shell (starter code): runs external commands with fork-exec-wait |
| v2 | Built-in commands: `cd`, `exit`, `help`, `jobs` |
| v3 | Command history: `history` and `!n` (last 20 commands) |
