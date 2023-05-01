#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXARGS   128
#define	MAXLINE	 8192  /* Max text line length */

void eval(char* cmdline);
int parseline(char* buf, char** argv);
int builtin_command(char** argv);
void changeDir(char** argv);
void pipeCmd(char* argv[][MAXARGS], int pos, int in_fd);
void handler(int sig);

#endif