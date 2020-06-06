#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

/* MACROS */
#define SUCCESS 0
#define FAILURE 1
#define NUM_BUILT_IN 3
#define NUM_REDIR 2
#define LINE_MAX 514

/* GLOBALS */
int batch_mode = 0;
FILE* input_stream;

char* output_path;
int redirection_mode = -1, output_fd, temp_fd, read_bytes;
char file_buff[LINE_MAX];

/* BUILT-IN COMMANDS */
char *built_in_cmds[] = {
    "cd",
    "exit",
    "pwd"
};

/* BUILT-IN COMMAND FUNCTIONS */
void mycd(char* command);
void myexit(char* command);
void mypwd(char* command);

void (*built_in_fxns[])(char*) = {
    &mycd,
    &myexit,
    &mypwd
};

/* REDIRECTION COMMANDS */
char *redir_cmds[] = {
    ">+",
    ">"
};

/* ========================================================================== */
/* HELPER FUNCTIONS ========================================================= */
/* ========================================================================== */
char* trimWhiteSpace(char* string) {
    if (!string) {
        return NULL;
    }

    char* leading = string;
    char* trailing = string + strlen(string) - 1;

    while (isspace(*leading))
        leading++;

    while (isspace(*trailing) && trailing > leading)
        trailing--;
    *(trailing + 1) = '\0';

    while ((*string++ = *leading++));

    return string;
}

int isBlankLine(char* string) {
    int is_blank_line;
    char* string_dup = strdup(string);
    trimWhiteSpace(string_dup);
    is_blank_line = !(strcmp(string_dup, ""));
    free(string_dup);
    return is_blank_line;
}

int isBuiltIn(char* command) {
    unsigned int i;
    for (i = 0; i < NUM_BUILT_IN; i++) {
        if(!strncmp(command, built_in_cmds[i], strlen(built_in_cmds[i])))
            return i;
    }
    return -1;
}

int isExistingFilePath(char* file_path) {
    struct stat file_stats;
    return !stat(file_path, &file_stats);
}

int isRedirection(char* command) {
    unsigned int i;
    for (i = 0; i < NUM_REDIR; i++) {
        if (strstr(command, redir_cmds[i]))
            return i;
    }
    return -1;
}

char* strrst(char* string, char* first) {
    return strstr(string, first) + strlen(first);
}

int isValidRedirection(char* redir_path, char* redir_cmd) {
    if (!strcmp(redir_path, ""))
        return 0;
    if (strchr(redir_path, '>') || strchr(redir_path, ' '))
        return 0;
    if (!strcmp(redir_cmd, ">") && isExistingFilePath(redir_path))
        return 0;
    return 1;
}

unsigned int getargc(char* command) {
    unsigned int argc = 0;
    while (*command) {
        if (*command == ' ')
            argc++;
        command++;
    }
    return argc + 1;
}

void myPrint(char* string) {
    write(STDOUT_FILENO, string, strlen(string));
    return;
}

void myError() {
    char error_message[30] = "An error has occurred\n";
    myPrint(error_message);
    return;
}


/* ========================================================================== */
/* BUILT-IN COMMANDS ======================================================== */
/* ========================================================================== */
void mycd(char* command) {
    char* file_path;
    if (!command[2]) {
        chdir(getenv("HOME"));
        return;
    }

    if ((file_path = trimWhiteSpace(strchr(command, ' ')))) {
        if (isExistingFilePath(file_path)) {
            chdir(file_path);
            return;
        }
    }

    myError();
    return;
}

void myexit(char* command) {
    if (strcmp(command, "exit")) {
        myError();
        return;
    }

    exit(0);
}

void mypwd(char* command) {
    if (strcmp(command, "pwd")) {
        myError();
        return;
    }

    size_t cwd_len = (size_t)pathconf(".", _PC_PATH_MAX);
    char* cwd_buff;

    if ((cwd_buff = (char*)malloc(cwd_len))) {
        getcwd(cwd_buff, cwd_len);
        myPrint(cwd_buff);
        myPrint("\n");
        free(cwd_buff);
        return;
    }
}

/* ========================================================================== */
/* THE SHELL ================================================================ */
/* ========================================================================== */
int myReadLine(char (*line_buff)[LINE_MAX], char** p) {
    // Read command line
    *p = fgets(*line_buff, LINE_MAX, input_stream);
    if (!(*p))
        exit(0);

    // Check if command line is of valid length
    if (strchr(*p, '\n')) {
        if (isBlankLine(*p))
            return FAILURE;

        if (batch_mode)
            myPrint(*p);

        *p = trimWhiteSpace(*p);
        return SUCCESS;
    }

    // Handle command line of invalid length
    while (!strchr(*p, '\n')) {
        myPrint(*p);
        *p = fgets(*line_buff, LINE_MAX, input_stream);
        if (!(*p))
            exit(0);
    }
    myPrint(*p);
    myError();
    return FAILURE;
}

char** myParseCommand(char* command) {
    unsigned int argc, i;
    int is_redir;
    char* redir_cmd;
    char** argv;
    if ((is_redir = isRedirection(command)) != -1) {
        redir_cmd = redir_cmds[is_redir];
        output_path = trimWhiteSpace(strrst(command, redir_cmd));
        if (isValidRedirection(output_path, redir_cmd)) {
            redirection_mode = is_redir;
            command = trimWhiteSpace(strtok_r(command, redir_cmd, &command));
        } else {
            myError();
            return NULL;
        }
    }
    argc = getargc(command);
    argv = (char**)malloc(sizeof(char*) * (argc + 1));
    for (i = 0; i < argc; i++) {
        argv[i] = strtok_r(command, " ", &command);
    }
    argv[i] = NULL;
    return argv;
}

int myRedirect() {
    // Standard redirection and advanced redirection to a nonexistent file
    if (redirection_mode == 1 || !isExistingFilePath(output_path)) {
        if ((output_fd = creat(output_path, 0700)) < 0) {
            myError();
            return FAILURE;
        }
        dup2(output_fd, STDOUT_FILENO);
        return SUCCESS;
    }
    return FAILURE;
}

void myExecuteCommandLine(char* line) {
    char* command = trimWhiteSpace(strtok_r(line, ";", &line));
    int is_built_in;
    char** argv;
    pid_t pid;
    int status;
    while (command) {
        if ((is_built_in = isBuiltIn(command)) != -1) {
            built_in_fxns[is_built_in](command);
        } else {
            argv = myParseCommand(command);
            pid = fork();
            if (pid == 0) {
                /*if (redirection_mode != -1) {
                    if (myRedirect())
                        exit(0);
                }*/
                if (execvp(argv[0], argv) < 0)
                    myError();
                /*if (!redirection_mode) {
                    // check if you get here
                    read_bytes = read(temp_fd, buff, LINE_MAX);
                    while (read_bytes > 0) {
                        write(output_fd, buff, LINE_MAX);
                        read_bytes = read(temp_fd, buff, LINE_MAX);
                    }
                }*/
                exit(0);
            } else {
                while (wait(&status) != pid);
                free(argv);
            }
        }
        redirection_mode = -1;
        command = trimWhiteSpace(strtok_r(line, ";", &line));
    }
}

int main(int argc, char *argv[]) {
    char line_buff[LINE_MAX];
    char *p;
    input_stream = stdin;
    // Shell can read at most one batch file at a time
    if (argc > 2) {
        myError();
        exit(1);
    }
    // Open the batch file for reading, if provided
    if (argc > 1) {
        batch_mode = 1;
        input_stream = fopen(argv[1], "r");
        if (!input_stream) {
            myError();
            exit(1);
        }
    }
    // Shell loop
    while (1) {
        // Shell prompt
        if (!batch_mode)
            myPrint("myshell> ");
        // Read command line
        if (!myReadLine(&line_buff, &p)) {
            // Execute command line
            myExecuteCommandLine(p);
        }
    }
}
