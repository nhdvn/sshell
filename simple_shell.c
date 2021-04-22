#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

pid_t child[64];
int pipes[64];
int pipes_size = 0;
int user_read;
int user_write;


void io_redirect(char sign, char *file)
{
	if (sign == '<')
	{
		user_read = open(file, O_RDONLY);
	}
	else if (sign == '>')
	{
		user_write = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	}

	// chmod 0644: user (read + write), group and other (read only)
}


void parse_iostream(char **argv, bool *background)
{
	char *sign = *argv;

	// argv: pointer to array of pointer of char string (argument)

	while (sign != NULL)
	{
		if (*sign == '<' || *sign == '>')
		{
			io_redirect(*sign, *(argv + 1));

			*argv = '\0'; // remove the sign from argument list

			argv += 1;

			*argv = '\0'; // remove file name from argument list
		}
		else if (*sign == '&')
		{
			*background = true;

			*argv = '\0'; // remove the sign
		}

		sign = *(argv += 1); // move to the next argument
	}
}


void parse_command(char *args, char **argv)
{
	// argv: pointer to array of pointer of char string (argument)

	char chr = *args;

	while (chr != '\0')
	{
		while (chr == ' ' || chr == '\t' || chr == '\n') // remove seperator between arguments
		{
			*args = '\0'; // end of the previous argument (char string)

			chr = *(args += 1);
		}

		if (chr != '\0')
		{
			*argv = args; // add argument to the list of arguments

			argv += 1; // argv is a pointer to the array of pointers of char string (arguments)
		}
		else break;

		while (chr != ' ' && chr != '\t' && chr != '\n')
		{
			chr = *(args += 1);
		}
	}

	*argv = '\0'; // end of the arguments list, the last pointer in the list is now null
}


void open_pipes()
{
	if (pipes_size == 0) return;

	for (int i = 0; i < pipes_size; i++)
	{
		if (pipe(pipes + i * 2) < 0)
		{
			printf("*** Error: Initilizing pipe array failed !!!\n");

			exit(1);
		}
	}
}


void close_pipes()
{
	if (pipes_size == 0) return;

	for (int i = 0; i < pipes_size * 2; i++)
	{
		close(pipes[i]);
	}
}


pid_t execute_command(char **argv, int pipe_read, int pipe_write)
{
	pid_t pid;

	if ((pid = fork()) < 0) // fork() return 0 to child, return pid of child to parent
	{
		printf("*** Error: Forking child process failed !!!\n");

		exit(1);
	}
	else if (pid == 0) // if process is child
	{
		dup2(pipe_read, STDIN_FILENO); // read input from last command output or from user

		dup2(pipe_write, STDOUT_FILENO); // write output for next command input or to user

		if (execvp(*argv, argv) < 0)
		{
			printf("*** Error: Executing commandline failed !!!\n");

			exit(1);
		}
	}
	else // if process is parent
	{
		printf("Child PID: %d\n\n", pid);

		if (pipe_read != user_read) close(pipe_read); 

		if (pipe_write != user_write) close(pipe_write);

		// only close file discriptor from pipes, not from user

		return pid; // if background then wait here

		// run child process in background or wait it to end
	}
}


int count_command(char *str, char **cmd)
{
	*cmd = str; // str is also point to the first command

	cmd = cmd + 1; // init pointer for next command

	int cnt = 1;  // user input has at least 1 command

	while (*str != '\0')
	{
		if (*str == '|') // pipe sign
		{
			*str = '\0'; // replace with null to finish the last command string

			*cmd = str + 1; // assign next command to a pointer

			cmd = cmd + 1; // next pointer for the next command

			cnt = cnt + 1;
		}
		
		str = str + 1;
	}

	*cmd = '\0'; // no more command

	return cnt; // total number of command to pipe
}


void pipe_command(int index, int *pipe_read, int *pipe_write)
{
	if (index != 0) // not the first command 
	{
		*pipe_read = pipes[index * 2 - 2];
	}
	else *pipe_read = user_read; // is first command: read from user input

	if (index != pipes_size) // not the last command
	{
		*pipe_write = pipes[index * 2 + 1];
	}
	else *pipe_write = user_write; // is last command: write to user output
}


bool is_internal_command(char **argv)
{
	if (strcmp(argv[0], "exit") == 0) 
	{
		printf("\n");

		exit(0);
	}

	char dir[100];

	if (strcmp(argv[0], "cd") == 0)
	{
		chdir(argv[1]);

		printf("%s\n", getcwd(dir, 100));

		return true;
	}

	if (strcmp(argv[0], "pwd") == 0)
	{
		printf("%s\n", getcwd(dir, 100));

		return true;
	}

	return false;
}


void main_process(char *line) // a line with multiple command
{
	char *list[64]; // list of command

	char *argv[64]; // arguments list of each command

	bool background = false; // flag to enable child process running background

	int n = count_command(line, list);

	pipes_size = n - 1;

	int pipe_read, pipe_write;

	open_pipes(); // initialize n - 1 pipes

	for (int i = 0; i < n; i++)
	{
		parse_command(list[i], argv);

		if (is_internal_command(argv)) break;

		parse_iostream(argv, &background);

		pipe_command(i, &pipe_read, &pipe_write);

		pid_t pid = execute_command(argv, pipe_read, pipe_write);

		child[i] = pid;
	}

	int status;

	if (background != true)
	{
		for (int i = 0; i < n; i++)
		{
			while (wait(&status) != child[i]);
		}
	}
}


bool check_last_command(char *line, char *hist)
{
	if (strcmp(line, "!!\n") != 0) // this is a new command
	{
		strncpy(hist, line, 1024); // copy it to history buffers

		return true;
	}
	else if (hist[0] == '\0') // no previous command to copy
	{
		printf("No commands in history \n");

		return false;
	}
	else // there is previous command, then copy it
	{
		strncpy(line, hist, 1024);

		printf(line);

		return true;
	}
}


void main(void)
{
	char line[1024], hist[1024];

	int std_read = dup(STDIN_FILENO);

	int std_write = dup(STDOUT_FILENO);

	line[0] = hist[0] = '\0';

	while (1)
	{
		user_read = std_read;

		user_write = std_write;

		// user can change to IO from files in the command

		printf("ssh -> ");

		fgets(line, 1024, stdin);

		if (check_last_command(line, hist))
		{
			main_process(line);
		}

		// restore default IO to the terminal

		dup2(std_read, STDIN_FILENO);

		dup2(std_write, STDOUT_FILENO);

		printf("\n");
	}
}
