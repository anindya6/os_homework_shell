#include "shell.h"


struct history_entry {
	int index;
	char *command;
	struct history_entry *next;
};


struct history {
	struct history_entry *start;
	struct history_entry *last;
	int history_size;
};


struct history *initialize_history()
{
	struct history_entry *start = NULL;
	struct history *hist = NULL;

	start = (struct history_entry *)malloc(sizeof(struct history_entry));
	start->index = -1;
	start->next = NULL;
	hist = (struct history *)malloc(sizeof(struct history));
	hist->start = start;
	hist->last = start;
	hist->history_size = 0;
	return hist;
}


char *pointer_to_first_occurence(char *cmd, char delimiter)
{
	int l = strlen(cmd);
	int i;

	for (i = 0; i < l; i++) {
		if ((cmd+i)[0] == delimiter)
			return cmd+i;
	}
	return NULL;
}


char **split_on_first_occurence(char *cmd, char delimiter)
{
	int l1, l2;
	char *first_occurence = pointer_to_first_occurence(cmd, delimiter);
	char *pre_occurence = (char *)malloc(1 + strlen(cmd));
	char **split = (char **)malloc(2*sizeof(char *));

	l1 = strlen(cmd);
	l2 = strlen(first_occurence);
	strncpy(pre_occurence, cmd, l1-l2);
	pre_occurence[l1-l2] = '\0';
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
	int status;
	int syscall;

	pipe(fd);
	pid = fork();
	if (pid == 0) {
		syscall = dup2(fd[WRITE_END], STDOUT_FILENO);
		if (syscall == -1) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			return 0;
		}
		close(fd[READ_END]);
		close(fd[WRITE_END]);
		parse_command(cmd1, hist, 0);
		free(cmd1);
		free(split);
		return 0;
	}
	pid = fork();
	if (pid == 0) {
		syscall = dup2(fd[READ_END], STDIN_FILENO);
		if (syscall == -1) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			return 0;
		}
		close(fd[WRITE_END]);
		close(fd[READ_END]);
		parse_command(cmd2, hist, 0);
		free(split);
		return 0;
	}
	free(split);
	close(fd[READ_END]);
	close(fd[WRITE_END]);
	waitpid(pid, &status, 0);
	return 1;
}


int execute_non_builtin(char **args)
{
	int status;
	int i;
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		i = execv(args[0], args);
		if (i == -1)
			fprintf(stderr, "error: %s\n", strerror(errno));
		return 0;
	}
	waitpid(pid, &status, 0);
	return 1;
}


int is_number(char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++) {
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
	struct history_entry *current_entry = NULL;

	if (hist->history_size == 100) {
		hist->start = hist->start->next;
		hist->history_size--;
	}
	current_entry = (struct history_entry *)
				malloc(sizeof(struct history_entry));
	current_entry->index = hist->last->index + 1;
	current_entry->command = (char *)malloc(1+strlen(command));
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
	while (temp1->next != NULL) {
		temp2 = temp1;
		temp1 = temp1->next;
		free_history_entry(temp2);
	}
	hist->start = hist->last;
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
	struct history_entry *iterator = NULL;

	if (args[1] != NULL && !is_number(args[1])) {
		fprintf(stderr, "error: %s\n",
				"Invalid argument for history command");
		return;
	} else if (args[1] == NULL)
		skip_entries = 0;
	else
		skip_entries = hist->history_size - atoi(args[1]);
	iterator = hist->start;
	while (iterator->next != NULL) {
		iterator = iterator->next;
		if (skip_entries <= 0)
			printf("%d %s\n", iterator->index, iterator->command);
		skip_entries--;
	}
}


char *search_history(char *search_str, struct history *hist)
{
	const char *failure = "@search_str_not_found";
	char *command = (char *)malloc(1 + strlen(failure));
	struct history_entry *iterator;

	strcpy(command, failure);
	if (hist->history_size == 0)
		return command;
	iterator = hist->start;
	while (iterator->next != NULL) {
		iterator = iterator->next;
		if (is_prefix(search_str, iterator->command)) {
			free(command);
			command = (char *)
					malloc(1 + strlen(iterator->command));
			strcpy(command, iterator->command);
		}
	}
	return command;
}


void change_directory(char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr,
			"error: %s\n",
			"Missing directory argument for cd command");
		return;
	} else if (args[2] != NULL) {
		fprintf(stderr, "error: %s\n", "Too many args for cd command");
		return;
	}
	int status = chdir(args[1]);

	if (status == -1)
		perror("error");
}



char **input_tokenizer(char *buffer, char *delimiters)
{
	/* The final token is chosen to be null.
	* This done so that the token parser knows when to stop.
	* So the actual number of max tokens will be max_tokens-1
	* execv calls require this list to be null terminated
	*/
	char **args = malloc(MAX_ARGS*sizeof(char *));
	char *arg;
	int index = 0;

	arg = strtok(buffer, delimiters);
	while (arg != NULL) {
		args[index++] = arg;
		if (index >= MAX_ARGS-1)
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
	char *cmd = malloc(current_buffer_size*sizeof(char));

	for (;;) {
		char c = getchar();

		if (c != EOF && c != '\n') {
			cmd[index++] = c;
			if (index >= current_buffer_size) {
				current_buffer_size += initial_buffer_size;
				cmd = realloc(cmd, current_buffer_size);
			}
		} else {
			if (c == EOF) {
				cmd[0] = EOF;
				cmd[1] = '\0';
			} else
				cmd[index] = '\0';
			return cmd;
		}
	}
}


int args_has_bang_at_i(char **args)
{
	int i = 0;

	while (args[i] != NULL) {
		if (args[i][0] == '!')
			return i;
		i++;
	}
	return -1;
}


void print_args(char **args)
{
	int i = 0;

	while (args[i] != NULL) {
		printf("%s ", args[i]);
		i++;
	}
	printf("\n");
}


int is_command_non_empty(char *cmd)
{
	int i;

	for (i = 0; i < strlen(cmd); i++) {
		if (cmd[i] != ' ')
			return 1;
	}
	return 0;
}


char *last_command(struct history *hist)
{
	char *cmd = (char *)malloc(1 + strlen(hist->last->command));

	strcpy(cmd, hist->last->command);
	return cmd;
}


char *find_pattern_to_match(const char *s)
{
	int i = 0;
	char *pattern = (char *)malloc(1 + strlen(s));

	while (s[i] != ' ' && s[i] != '\0') {
		pattern[i] = s[i];
		i++;
	}
	pattern[i] = '\0';
	return pattern;
}



char *replace_pattern
	(const char *s, const char *old_word, const char *new_word)
{
	char *new_s;
	int i, count = 0;
	int new_word_len = strlen(new_word);
	int old_word_len = strlen(old_word);

	for (i = 0; s[i] != '\0'; i++) {
		if (strstr(s+i, old_word) == s+i) {
			count++;
			i += old_word_len - 1;
		}
	}
	new_s = (char *)malloc(i + count * (new_word_len - old_word_len) + 1);
	i = 0;
	while (*s) {
		if (strstr(s, old_word) == s) {
			strcpy(new_s + i, new_word);
			i += new_word_len;
			s += old_word_len;
		} else
			new_s[i++] = *s++;
	}
	new_s[i] = '\0';
	return new_s;
}


int parse_args(char **args, struct history *hist)
{
	if (args[0] == NULL)
		return 1;
	else if (strcmp(args[0], "history") == 0) {
		if (args[1] != NULL && strcmp(args[1], "-c") == 0)
			clear_history(hist);
		else if (args[1] != NULL && args[2] != NULL)
			fprintf(stderr,
				"error: %s\n",
				"Too many arguments for history command");
		else
			show_history(hist, args);
	} else if (strcmp(args[0], "cd") == 0)
		change_directory(args);
	else if (strcmp(args[0], "exit") == 0) {
		if (args[1] != NULL)
			fprintf(stderr,
				"error: %s\n",
				"Too Many Arguments to Exit Command");
		else
			return 0;
	} else
		return execute_non_builtin(args);
	return 1;
}


int parse_command(char *cmd, struct history *hist, int mode)
{
	char **args;
	int flag;
	int flag_2 = 0;
	char *ptr;
	char *search_str;
	char *match;
	char *copy;

	ptr = strstr(cmd, "!!");
	if (ptr != NULL && hist->history_size == 0) {
		fprintf(stderr, "error: %s\n", "History is empty");
		return 1;
	}
	if (ptr != NULL) {
		char *last_cmd = last_command(hist);

		cmd = replace_pattern(cmd, "!!", last_cmd);
		free(last_cmd);
		flag_2 = 1;
	}
	ptr = strstr(cmd, "!");
	if (ptr != NULL) {
		search_str = find_pattern_to_match(ptr);
		match = search_history(search_str + 1, hist);
		if (strcmp(match, "@search_str_not_found") == 0) {
			free(search_str);
			free(match);
			fprintf(stderr,
				"error: %s\n",
				"Search String Not Found in History");
			return 1;
		}
		cmd = replace_pattern(cmd, search_str, match);
		free(search_str);
		free(match);
		flag_2 = 1;
	}
	if (is_command_non_empty(cmd) && mode != 0)
		add_command_to_history(cmd, hist);
	if (pointer_to_first_occurence(cmd, '|') != NULL)
		return piped_command(cmd, hist);
	copy = (char *)malloc(1 + strlen(cmd));
	strcpy(copy, cmd);
	args = input_tokenizer(copy, " ");
	flag = parse_args(args, hist);
	free(args);
	free(copy);
	if (flag_2 == 1)
		free(cmd);
	return flag;
}


void continuous_run(void)
{
	char *cmd;
	int flag = 1;
	struct history *hist = initialize_history();

	while (flag) {
		printf("$");
		cmd = get_input_cmd();
		if (cmd[0] == EOF)
			flag = 0;
		else
			flag = parse_command(cmd, hist, 1);
		free(cmd);
	}
	free_entire_history(hist);
}


int main(void)
{
	continuous_run();
	return 0;
}
