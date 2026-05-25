# Lab 1: Simple Unix Shell

A custom Unix shell implemented in C that supports built-in commands, external command execution, background processes, and environment variable management.

---

## Features

- **Built-in commands:** `cd`, `echo`, `export`, `exit`
- **External commands:** executed via `fork()` + `execvp()`
- **Background execution:** append `&` to run a process in the background
- **Environment variables:** define with `export KEY=VALUE`, reference with `$KEY`
- **Child process cleanup:** uses `SIGCHLD` signal handler to reap zombie processes

---

## Build & Run

```bash
gcc -o shell simpleShell.c
./shell
```

---

## Supported Commands

### Built-in

| Command | Description |
|---------|-------------|
| `cd <path>` | Change directory (`~` goes to HOME) |
| `echo <args>` | Print arguments to stdout (supports quoted strings) |
| `export KEY=VALUE` | Define an environment variable |
| `exit` | Exit the shell |

### External
Any system command works:
```bash
ls -la
grep pattern file.txt
./myprogram arg1 arg2 &   # run in background
```

### Environment Variables
```bash
export NAME=John
echo $NAME      # prints: John
```

---

## Implementation Details

- `parse_input()` — tokenizes user input; handles special cases for `echo` (quoted strings) and `export` (key=value pairs)
- `evaluate_expression()` — replaces `$VAR` tokens with their stored values before execution
- `execute_external()` — forks a child process; parent waits only for foreground processes
- `on_child_exit()` — `SIGCHLD` handler using `waitpid(WNOHANG)` to clean up background processes without blocking
- Environment variables stored in a custom key-value array (upgradeable to hash table)
