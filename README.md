# Quash Shell Implementation

## Team Members
- Torien Mitchell
- Sarah Cole

## Project Overview
Quash is a custom shell implementation that provides command-line interface functionality similar to bash, csh, and other Unix shells. This project demonstrates process management, signal handling, and shell built-in commands.

## Features Implemented

### Task 1: Basic Shell with Built-in Commands
- **cd**: Change directory
- **pwd**: Print working directory  
- **echo**: Print messages and environment variables
- **env**: Display environment variables
- **setenv**: Set environment variables
- **exit**: Terminate the shell
- Variable expansion with `$VAR` syntax

### Task 2: Process Execution
- Forking and executing external commands
- Error handling for non-existent commands
- Process waiting and status reporting

### Task 3: Background Processes
- Support for `&` operator to run processes in background
- Background process tracking and management

### Task 4: Signal Handling
- SIGINT (Ctrl-C) handling for foreground processes
- Shell remains active when interrupting child processes

### Task 5: Process Timeout
- Automatic termination of foreground processes after 10 seconds
- Timer cancellation when process completes early

### Task 6: I/O Redirection and Piping (Extra Credit)
- Input redirection with `<`
- Output redirection with `>`
- Command piping with `|`

## Design Choices

### Tokenization and Parsing
- Used `strtok()` for command parsing with space/tab delimiters
- Implemented variable expansion during tokenization phase
- Handled special characters and operators separately

### Process Management
- Maintained array for background process tracking
- Used `waitpid()` with `WNOHANG` for non-blocking background process checks
- Implemented proper zombie process prevention

### Signal Handling
- Set up handlers for SIGINT and SIGCHLD
- Used global variables to track foreground process state
- Ensured proper signal masking during critical sections

### Built-in Commands
- Implemented as separate functions for modularity
- Used standard library functions (`chdir`, `getcwd`, `setenv`) where possible
- Added error checking and user-friendly error messages