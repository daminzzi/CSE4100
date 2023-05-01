/* $begin shellmain */
#include "myshell.h"
#define READ 0
#define WRITE 1

int main()
{
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
        /*Signal Control*/
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        if (signal(SIGTSTP, handler) == 0)
            continue;
        if (signal(SIGINT, handler) == 0) // terminal interrupt -> handler
            continue;
        if (signal(SIGQUIT, handler) == 0) // terminal quit -> handler
            continue;
        if (signal(SIGCHLD, handler) == 0) // terminal quit -> handler
            continue;

        /* Read */
        printf("CSE4100-SP-P#1> ");
        char * i = fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        eval(cmdline);
    }
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char* cmdline)
{
    char* argv[MAXARGS][MAXARGS]; /* Argument list execvp() */
    char buf[MAXLINE];   /* Holds modified command line */
    char cmd[MAXARGS][MAXLINE]; /*pipe를 기준으로 나눈 명령어를 저장*/
    char* delim;         /* Points to first space delimiter */
    int cmdnum;          /*pipe로 분류되는 command의 수*/
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    if (delim = strchr(buf, '|')) {
        char* temp = buf;
        /*parsing command line by pipe*/
        temp[strlen(temp) - 1] = '|';
        while (delim = strchr(temp, '|')) {
            *delim = '\0';
            strcpy(cmd[cmdnum], temp);
            strcat(cmd[cmdnum++], "\n");
            temp = delim + 1;
        }
        cmd[cmdnum][0] = '\0';
        
        for (int i = 0; i < cmdnum; i++) {
            bg = parseline(cmd[i], argv[i]);
        }
        argv[cmdnum][0] = NULL;
        
        pid = fork();
        
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0) {
            pipeCmd(argv, 0, STDIN_FILENO);
        }

        /* Parent waits for foreground job to terminate */
        if (!bg) {
            int status;
            if (waitpid(pid, &status, 0) < 0) {
                fprintf(stderr, "%s: %s\n", "waitfg: waitpid error", strerror(errno));
                exit(0);
            }
        }
        else//when there is background process!
            printf("%d %s", pid, cmdline);
    }
    else {
        bg = parseline(buf, argv[0]);
        if (argv[0][0] == NULL)
            return;   /* Ignore empty lines */
        
        /*command excute*/
        if (!builtin_command(argv[0])) { //quit -> exit(0), & -> ignore, other -> run
            pid = fork();
            if (pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (pid == 0) {
                if (execvp(argv[0][0], argv[0]) < 0) {
                    printf("%s: Command not found.\n", argv[0][0]);
                    exit(1);
                }
            }

            /* Parent waits for foreground job to terminate */
            if (!bg) {
                int status;
                if (waitpid(pid, &status, 0) < 0) {
                    fprintf(stderr, "%s: %s\n", "waitfg: waitpid error", strerror(errno));
                    exit(0);
                }
            }
            else//when there is backgrount process!
                printf("%d %s", pid, cmdline);
        }
    
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char** argv)
{
    if (!strcmp(argv[0], "exit")) { /* quit command */
        exit(1);
    }
    else if (!strcmp(argv[0], "&")) {    /* Ignore singleton & */
        return 1;
    }
    else if (!strcmp(argv[0], "cd")) {
        changeDir(argv);
        return 1;
    }
    return 0;                     /* Not a builtin command */
}

void changeDir(char** argv) { //excute for cd.
    int temp;
    if (argv[1] == NULL || !strcmp(argv[1], "~")) {
        temp = chdir(getenv("HOME"));
    }
    else if (argv[2] != NULL)
    {
        fprintf(stderr, "usage: cd [directory name] ...\n");
    }
    else if (temp = (chdir(argv[1]) < 0))
        fprintf(stderr, "%s: %s: No such file or directory\n", argv[0], argv[1]);
}
/* $end eval */



/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char* buf, char** argv)
{
    char* delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf) - 1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;

        int flag = 0;
        char* temp = buf;
        char* start = buf;
        char* end = buf;


        while (*temp && (*temp != ' ')) { //따옴표 여부를 탐색하며 위치를 저장
            if (*temp == '\"')
            {
                start = temp++;
                end = strchr(temp, '\"');
                temp = end;
                delim = strchr(temp, ' ');
                flag = 1;
                break;
            }
            if (*temp == '\'')
            {
                start = temp++;
                end = strchr(temp, '\'');
                temp = end;
                delim = strchr(temp, ' ');
                flag = 1;
                break;
            }
            temp++;
        }

        *delim = '\0';

        if (flag == 1) {
            temp = buf;
            while (temp <= delim) {
                if (buf > delim) {
                    *temp = ' ';
                    temp++;
                }
                else if (buf != start && buf != end) {
                    *temp = *buf;
                    temp++;
                }
                buf++;
            }
        }

        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0)  /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


/*running command with pipe*/
//input: argv, pos: i번째 command, in_fd: 입출력 표시
void pipeCmd(char* argv[][MAXARGS], int pos, int in_fd) {
    int fd[2];
    pid_t pid;           /* Process id */

    if (argv[pos + 1][0] == NULL) {	//last command
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);
        if (!builtin_command(argv[pos])) { //quit -> exit(0), & -> ignore, other -> run
            if (execvp(argv[pos][0], argv[pos]) < 0) {	//ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[pos][0]);
                exit(0);
            }
        }
        exit(0);
    }
    else {
        if (pipe(fd) < 0) {
            perror("pipe");
            exit(0);
        }
        if ((pid = fork()) == 0) {
            close(fd[READ]); //unused
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
            dup2(fd[WRITE], STDOUT_FILENO);
            close(fd[WRITE]);
            if (!builtin_command(argv[pos])) { //quit -> exit(0), & -> ignore, other -> run
                if (execvp(argv[pos][0], argv[pos]) < 0) {	//ex) /bin/ls ls -al &
                    printf("%s: Command not found.\n", argv[pos][0]);
                    exit(0);
                }
            }
            exit(0);
        }


        close(fd[WRITE]);
        pipeCmd(argv, pos + 1, fd[READ]);
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            fprintf(stderr, "%s: %s\n", "waitfg: waitpid error", strerror(errno));
            exit(0);
        }
    }
}

void handler(int sig)
{
    // terminal interrupt
    if (sig == SIGINT)
    {
        fflush(stdout);
        signal(SIGINT, SIG_DFL);
    }
    // terminal stop
    else if (sig == SIGTSTP)
    {
        fflush(stdout);
        signal(SIGTSTP, SIG_DFL);
    }
    // terminal quit
    else if (sig == SIGQUIT)
    {
        fflush(stdout);
        signal(SIGQUIT, SIG_DFL);
    }
    // child quit
    else if (sig == SIGCHLD)
    {
        fflush(stdout);
        signal(SIGCHLD, SIG_DFL);
    }
}


