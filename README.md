# A Simple Unix Shell: CMSC 15400 Project
Gaining exposure to the Linux programming environment and process interaction.

## Features
### Built-in Commands
* `cd [dir]` (leave `[dir]` blank to change to the home directory)
* `pwd`
* `exit`

### Multiple Commands
Run multiple jobs on a single command line using `;` to denote separate jobs
```
myshell> ls; ps; who
```

### Redirection of Standard Output `>`
```
myshell> ls -la /tmp > output
```

### Advanced Redirection of Standard Output `>+`
Functions similarly to redirection of standard output, except standard output is **prepended** to existing content if the file already exists.

### Batch Mode
Specify a "batch file" of command lines to execute instead of interacting with the shell to execute command lines, as demonstrated below.

## Getting Started
These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. Have fun!

### Run
1. Clone this repo into your folder of choice.
    ```
    git clone https://github.com/jackandthebean/shell.git
    ```
2. Change directories.
    ```
    $ cd shell
    ```
3. Compile the program.
    ```
    $ make
    ```
4. Run the program.

    *Interactive mode*
    ```
    ./myshell
    ```
    
    *Batch mode*
    ```
    ./myshell [file]
    ```

## Built With
* [Atom](https://atom.io/) – open-source text editor
* [GCC](https://gcc.gnu.org/) – C compiler
