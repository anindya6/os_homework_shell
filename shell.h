#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define READ_END 0
#define WRITE_END 1
#define MAX_PIPES 50


struct history;
int parse_command(char *cmd, struct history *hist, int mode);