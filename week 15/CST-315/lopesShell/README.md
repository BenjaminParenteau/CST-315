# Unix/Linux Command Line Interpreter (lopeShell)

## Build
gcc -Wall -Wextra -O2 -o lopeShell lopeShell.c fs.c

## Run Interactive
./lopeShell

## Run Batch Mode
./lopeShell batch.txt

## Key Combos
Ctrl-C  → End execution (terminate running child processes)
Ctrl-\ → Exit shell

## Built-in Commands

### Shell Control
| Command | Description |
|---------|-------------|
| end | End execution (terminate running children) |
| quit / exit | Exit the shell |

### File System (Project 6)
| Command | Description |
|---------|-------------|
| cd \<path\> | Change current directory |
| pwd | Print working directory |
| ls [dir] | List directory contents |
| mkdir \<path\> | Create a directory |
| rmdir [-r] \<path\> | Delete a directory (-r for non-empty) |
| touch [-s \<bytes\>] \<name\> | Create a file (-s generates random data) |
| rm \<file\> | Delete a file |
| rename \<old\> \<new_name\> | Rename a file or directory |
| edit \<file\> | Edit a file (line-based, type END to finish) |
| mv \<file\> \<dest_dir\> | Move a file to another directory |
| cp [-r] \<src\> \<dest\> | Copy a file or directory (-r for recursive) |
| find \<name\> [start_dir] | Search for a file in the directory tree |
| tree [dir] | Display the directory tree |
| stat [-l] \<file\> | File info (-l for detailed) |
| dirinfo [-l] \<dir\> | Directory info (-l for detailed) |
