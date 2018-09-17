#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#define READ_END 0
#define WRITE_END 1

struct history_entry
{
    int index;
    char *command;
    struct history_entry *next;
};


struct history
{
    struct history_entry *start;
    struct history_entry *last;
    int history_size;
};


struct history *initialize_history()
{
    struct history_entry* start = NULL;
    start = (struct history_entry*)malloc(sizeof(struct history_entry));
    start->index = -1;
    start->next = NULL;
    struct history* hist = NULL;
    hist = (struct history*)malloc(sizeof(struct history));
    hist->start = start;
    hist->last = start;
    hist->history_size = 0;
    return hist;
}


int parse_command(char *cmd, struct history *hist, int mode);
int parse_args(char **args, struct history *hist);


char *pointer_to_first_occurence(char *cmd, char delimiter)
{
    int l = strlen(cmd);
    int i;
    for(i=0; i<l; i++)
    {
        if ((cmd+i)[0] == delimiter)
            return cmd+i;
    }
    return NULL;
}


char **split_on_first_occurence(char *cmd, char delimiter)
{
    int i, j, l1, l2, size;
    char *first_occurence = pointer_to_first_occurence(cmd, delimiter);
    l1 = strlen(cmd);
    l2 = strlen(first_occurence);
    char *pre_occurence = (char *)malloc(sizeof(cmd));
    strncpy(pre_occurence, cmd, l1-l2);
    pre_occurence[l1-l2]= '\0';
    char **split = (char**)malloc(2*sizeof(char *));
    split[0] = pre_occurence;
    split[1] = first_occurence + 1;
    return split;
}


int piped_command(char *cmd, struct history *hist)
{
    char **split = split_on_first_occurence(cmd, '|');
    char *cmd1 = split[0];
    char *cmd2 = split[1];
    pid_t pid;
    int fd[2];
    pipe(fd);
    pid = fork();
    if(pid==0)
    {
        dup2(fd[WRITE_END], STDOUT_FILENO);
        close(fd[READ_END]);
        close(fd[WRITE_END]);
        parse_command(cmd1, hist, 0);
        return 0;
    }
    else
    { 
        pid=fork();
        if(pid==0)
        {
            dup2(fd[READ_END], STDIN_FILENO);
            close(fd[WRITE_END]);
            close(fd[READ_END]);
            parse_command(cmd2, hist, 0);
            return 0;
        }
        else
        {
            int status;
            close(fd[READ_END]);
            close(fd[WRITE_END]);
            waitpid(pid, &status, 0);
        }
    }
    return 1;
}


int execute_non_builtin(char **args)
{
    int status;
    int i;
    pid_t pid;
    pid = fork();
    if (pid == 0)
    {
        i = execv(args[0], args);
        if (i == -1)
            fprintf(stderr, "error: %s\n", strerror(errno));
        return 0;
    }
    else
        waitpid(pid, &status, 0);
    return 1;
}


int is_number(char *str)
{
    int i;
    for (i=0; str[i]!= '\0'; i++)
    {
        if (isdigit(str[i]) == 0) 
            return 0;
    }
    return 1;
}


int is_prefix(const char *prefix, const char *str)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}


void add_command_to_history(char *command, struct history *hist)
{
    if(hist->history_size == 100)
    {
        hist->start = hist->start->next;
        hist->history_size--;
    }
    struct history_entry* current_entry = NULL;
    current_entry = (struct history_entry*)malloc(sizeof(struct history_entry));
    current_entry->index = hist->last->index + 1;
    current_entry->command = (char *)malloc(sizeof(command));
    strcpy(current_entry->command, command);
    current_entry->next = NULL;
    hist->last->next = current_entry;
    hist->last = current_entry;
    hist->history_size++;
}


void free_history_entry(struct history_entry *entry)
{
    free(entry->command);
    entry->next = NULL;
    free(entry);
}


void clear_history(struct history *hist)
{
    struct history_entry *temp1;
    struct history_entry *temp2;
    temp1 = hist->start;
    while(temp1->next != NULL)
    {
        temp2 = temp1;
        temp1 = temp1->next;
        free_history_entry(temp2);
    }
    hist->start=hist->last;
    hist->history_size = 0;
}


void free_entire_history(struct history *hist)
{
    clear_history(hist);
    free_history_entry(hist->start);
    free(hist);
}


void show_history(struct history *hist, char **args)
{
    int skip_entries;
    if (args[1] != NULL && !is_number(args[1]))
    {
        fprintf(stderr, "error: %s\n", "Invalid argument for history command");
        return;
    }
    else if (args[1] == NULL)
        skip_entries = 0;
    else
        skip_entries = hist->history_size - atoi(args[1]);
    struct history_entry *iterator = NULL;
    iterator = hist->start;
    while(iterator->next != NULL)
    {
        iterator = iterator->next;
        if(skip_entries <= 0)
            printf("%d %s\n", iterator->index, iterator->command);
        skip_entries--;
    }
}


char *search_history(char *search_str, struct history *hist)
{
    const char *failure = "@search_str_not_found";
    char *command = (char *)malloc(sizeof(failure));
    strcpy(command, failure);
    if(hist->history_size == 0)
        return command;
    struct history_entry *iterator;
    iterator = hist->start;
    while (iterator->next != NULL)
    {
        iterator = iterator->next;
        if (is_prefix(search_str, iterator->command))
        {
            command = (char *)malloc(sizeof(iterator->command));
            strcpy(command, iterator->command);
        }
    }
    return command;
}


void change_directory(char **args)
{
    if (args[1] == NULL)
    {
        fprintf(stderr, "error: %s\n", "Missing directory argument for cd command");
        return;
    }
    else if (args[2] != NULL)
    {
        fprintf(stderr, "error: %s\n", "Too many args for cd command");
        return;
    }
    int status = chdir(args[1]); 
    if (status == -1)
        perror("error");
}


char **input_tokenizer(char *buffer, char *delimiters)
{
    int max_args = 11;
    /* The final token is chosen to be null.
    This done so that the token parser knows when to stop.
    So the actual number of max tokens will be max_tokens-1
    */
    char **args = malloc(max_args*sizeof(char *));
    char *arg;
    int index = 0;
    arg = strtok(buffer, delimiters);
    while(arg != NULL)
    {
        args[index++] = arg;
        if (index >= max_args-1)
            break;
        arg = strtok(NULL, delimiters);
    }
    args[index] = NULL;
    return args;
}


char *get_input_cmd()
{
    int index = 0;
    int initial_buffer_size = 100;
    int current_buffer_size = initial_buffer_size;
    char *buffer = malloc(current_buffer_size*sizeof(char));
    for(;;)
    {
        char c = getchar();
        if (c!=EOF && c!='\n')
        {
            buffer[index++] = c;
            if (index >= current_buffer_size)
            {
                current_buffer_size += initial_buffer_size;
                buffer = realloc(buffer, current_buffer_size);
            }
        }
        else
        {
            buffer[index] = '\0';
            return buffer;
        }
    }
}


void print_args(char **args)
{
    int i=0;
    while(args[i]!=NULL)
    {
        printf("%s ", args[i]);
        i++;
    }
}


void append_args_to_command(char *cmd, char **args)
{
    int i = 1;
    const char *space = " ";
    while(args[i]!=NULL)
    {
        char *temp = (char *)malloc(sizeof(space)+sizeof(args[i]));
        strcpy(temp, space);
        strcat(temp, args[i]);
        cmd = realloc(cmd, sizeof(cmd)+sizeof(temp));
        strcat(cmd, temp);
        i++;
        free(temp);
    }
}


int parse_command(char *cmd, struct history *hist, int mode)
{
    char **args;
    if(strlen(cmd) && cmd[0] != '!' && cmd[0] != '@' && mode != 0)
        add_command_to_history(cmd, hist);
    if(pointer_to_first_occurence(cmd, '|') != NULL)
    {
        return piped_command(cmd, hist);
    }
    args = input_tokenizer(cmd, " ");
    int flag = parse_args(args, hist);
    return flag;
}


int parse_args(char **args, struct history *hist)
{
    if(args[0]==NULL)
        return 1;
    else if (strcmp(args[0], "history") == 0)
    {
        if (args[1] != NULL && strcmp(args[1], "-c") == 0)
            clear_history(hist);
        else if (args[1] != NULL && args[2] != NULL)
            fprintf(stderr, "error: %s\n", "Too many arguments for history command");
        else
            show_history(hist, args);
    }
    else if (strcmp(args[0], "!!") == 0)
    {
        if(hist->history_size == 0)
            return 1;
        else
        {
            char *cmd = (char *)malloc(sizeof(hist->last->command));
            strcpy(cmd, hist->last->command);
            return parse_command(cmd, hist, 1);
        }
    }
    else if (args[0][0] == '!')
    {
        char *search_str = (char *)malloc(sizeof(args[0]));
        strcpy(search_str, args[0]);
        search_str = search_str + 1;
        char *cmd = search_history(search_str, hist);
        append_args_to_command(cmd, args);
        return parse_command(cmd, hist, 1);
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        change_directory(args);
    }
    else if(strcmp(args[0], "exit") == 0)
    {
        if (args[1] != NULL)
            fprintf(stderr, "error: %s\n", "Too Many Arguments to Exit Command");
        else
            return 0;
    }
    else if (strcmp(args[0], "@search_str_not_found") == 0)
    {
        fprintf(stderr, "error: %s\n", "Search String Not Found in History");
    }
    else
    {
        return execute_non_builtin(args);
    }
    return 1;
}


void continuous_run()
{   
    char *cmd;
    int flag = 1;   
    struct history* hist = initialize_history();
    while (flag)
    {
        printf("$");
        cmd = get_input_cmd();
        flag = parse_command(cmd, hist, 1);
        free(cmd);
    }
    free_entire_history(hist);
}


int main()
{
    continuous_run();
    return 0;
}