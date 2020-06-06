#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#define SUCCESS 0
#define FAILURE 1
#define NUM_BUILT_IN 3
#define NUM_REDIR 2
#define LINE_MAX 514
int batch_mode = 0, redirection_mode = -1;
FILE* input;
char* output_path;
char *built_in_cmds[] = {
    "cd",
    "exit",
    "pwd"
};
void mycd(char* command);
void myexit(char* command);
void mypwd(char* command);
void (*built_in_fxns[])(char*) = {
    &mycd,
    &myexit,
    &mypwd
};
char *redir_cmds[] = {
    ">+",
    ">"
};
/* ========================================================================== */
/* HELPER FUNCTIONS ========================================================= */
/* ========================================================================== */
char* trimWhiteSpace(char* str) {
    if (!str) {
        return NULL;
    }
    char* trail = str + strlen(str) - 1;
    while (isspace(*str))
        str++;
    if (!(*str))
        return str;
    while (isspace(*trail) && trail > str)
        trail--;
    *(trail + 1) = 0;
    return str;
}
int isBlankLine(char* str) {
    char* str_dup = strdup(str);
    str_dup = trimWhiteSpace(str_dup);
    return !(strcmp(str_dup, ""));
}
int isBuiltIn(char* command) {
    unsigned int i;
    for (i = 0; i < NUM_BUILT_IN; i++) {
        if(!strncmp(command, built_in_cmds[i], strlen(built_in_cmds[i])))
            return i;
    }
    return -1;
}
int isFilePath(char* file_path) {
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
int isValidRedirection(char* command, char* redir_cmd) {
    char* rest = trimWhiteSpace(strstr(command, redir_cmd) + strlen(redir_cmd));
    if (!strcmp(rest, ""))
        return 0;
    if (strchr(rest, '>') || strchr(rest, ' '))
        return 0;
    if (!strcmp(redir_cmd, ">") && isFilePath(rest))
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
void myPrint(char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}
void myError() {
    char error_message[30] = "An error has occurred\n";
    myPrint(error_message);
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
        if (isFilePath(file_path)) {
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
    long cwdlen;
    char* cwd_buff;
    cwdlen = pathconf(".", _PC_PATH_MAX);
    if ((cwd_buff = (char*)malloc((size_t)cwdlen))) {
        getcwd(cwd_buff, (size_t)cwdlen);
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
    *p = fgets(*line_buff, LINE_MAX, input);
    if (!(*p)) {
        exit(0);
    }
    if (strchr(*p, '\n')) {
        // Command line is of valid length
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
        *p = fgets(*line_buff, LINE_MAX, input);
        if (!(*p)) {
            exit(0);
        }
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
        if (isValidRedirection(command, redir_cmd)) {
            redirection_mode = is_redir;
            output_path = trimWhiteSpace(strstr(command, redir_cmd) + strlen(redir_cmd));
            command = trimWhiteSpace(strtok_r(command, ">", &command));
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
int output_fd, temp_fd, read_bytes;
char buff[LINE_MAX];

int myRedirect() {
    if (redirection_mode == 1 || !isFilePath(output_path)) {
        if ((output_fd = creat(output_path, 0700)) < 0) {
            myError();
            return FAILURE;
        }
        dup2(output_fd, STDOUT_FILENO);
        return SUCCESS;
    }

    if ((temp_fd = creat("temp.txt", 0700)) < 0) {
        myError();
        return FAILURE;
    }
    if ((output_fd = open(output_path, O_RDWR, 0700)) < 0) {
        myError();
        return FAILURE;
    }

    read_bytes = read(output_fd, buff, LINE_MAX);
    while (read_bytes > 0) {
        write(temp_fd, buff, LINE_MAX);
        read_bytes = read(output_fd, buff, LINE_MAX);
    }
    dup2(output_fd, STDOUT_FILENO);
    return SUCCESS;
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
                if (redirection_mode != -1) {
                    if (myRedirect())
                        exit(0);
                }
                if (execvp(argv[0], argv) < 0)
                    myError();
                if (!redirection_mode) {
                    read_bytes = read(temp_fd, buff, LINE_MAX);
                    while (read_bytes > 0) {
                        write(output_fd, buff, LINE_MAX);
                        read_bytes = read(temp_fd, buff, LINE_MAX);
                    }
                }
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
    input = stdin;
    // Shell can read at most one batch file at a time
    if (argc > 2) {
        myError();
        exit(1);
    }
    // Open the batch file for reading, if provided
    if (argc > 1) {
        batch_mode = 1;
        input = fopen(argv[1], "r");
        if (!input) {
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
