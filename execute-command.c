// UCLA CS 111 Lab 1 command execution

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Regular command execution functions */

int
command_status (command_t c)
{
	return c->status;
}

void
input_and_output_redirection(command_t c)
{
	/* Try to extract the file into stdin if it was specified */
	if(c->input != NULL)
	{
		int input_file = open(c->input, O_RDONLY);
		if(input_file < 0)
			error(1, 0, "Error! %s could not be read as standard input.\n", c->input);

		dup2(input_file, 0);
		close(input_file);
	}
	/* Try to extract stdout into the file that was specified */
	if(c->output != NULL)
	{
		/* If the file name does not exist, then create one. Or open the existing
		   one and clear its contents */
		int output_file = open(c->output, O_CREAT | O_WRONLY | O_TRUNC, 0664);
		if(output_file < 0)
			error(1, 0, "Error! %s could not be created or overwritten.\n", c->output);

		dup2(output_file, 1);
		close(output_file);
	}
}

void
execute_simple_command(command_t c)
{
	pid_t pid = fork();

	/* Parent process waits for the child process to finish */
	if(pid > 0)
	{
		int exit_status;
		waitpid(pid, &exit_status, 0);
		c->status = WEXITSTATUS(exit_status);
	}
	/* Child process executes the command */
	else if(pid == 0)
	{
		/* If the command begins with ':', then simple execute it with 'true' command */
		if(c->u.word[0][0] != ':')
		{
			input_and_output_redirection(c);
			/* Execute the command, if the command returns, then the execution must have error */
			execvp(c->u.word[0], c->u.word);
		}
		else
		{
			c->u.word = NULL;
			execvp("true", c->u.word);
		}
		error(1, 0, "Error! Command %s exited with error.\n", c->u.word[0]);
	}
	else
		error(1, 0, "Error! Forking failed.\n");
}

/* Declaration of function below execute_subshell_command() */
void execute_compound_command(command_t c);

void
execute_pipe_command(command_t c)
{
	int pipefd[2];
	int exit_status;
	pid_t pid1, pid2;
	pid_t wait_pid;

	if(pipe(pipefd) < 0)
		error(1, 0, "Error! Failed to create pipe.\n");

	pid1 = fork();
	/* Parent process executes the left command */
	if(pid1 > 0)
	{
		pid2 = fork();
		/* Wait for other process to finish */
		if(pid2 > 0)
		{
			/* Process that does not execute command does not need the pipe */
			close(pipefd[0]);
			close(pipefd[1]);
			
			wait_pid = waitpid(-1, &exit_status, 0);
			/* Right command waits for the left command to finish */
			if(wait_pid == pid1)
			{
				c->status = WEXITSTATUS(exit_status);
				waitpid(pid2, &exit_status, 0);
			}
			else if(wait_pid == pid2)
			{
				waitpid(pid1, &exit_status, 0);
				c->status = WEXITSTATUS(exit_status);
			}
		}
		/* Execute the left command */
		else if(pid2 == 0)
		{
			/* Take standard output of the left command into the pipe */
			close(pipefd[0]);
			dup2(pipefd[1], 1);
			/* execute_simple_command() will close pipefd[1] */
			execute_compound_command(c->u.command[0]);
			/* Terminate the process when the execution finishes */
			exit(c->u.command[0]->status);
		}
		else
			error(1, 0, "Error! Forking failed.\n");
	}
	/* Child process executes the right command */
	else if(pid1 == 0)
	{
		/* Take standard input from the standard output of the left command */
		close(pipefd[1]);
		/* execute_simple_command() will close pipefd[0] */
		dup2(pipefd[0], 0);
		execute_compound_command(c->u.command[1]);		
		exit(c->u.command[1]->status);
	}
	else
		error(1, 0, "Error! Forking failed.\n");
}

void
execute_subshell_command(command_t c)
{
	pid_t pid = fork();

	/* Parent process waits for the child process to finish */
	if(pid > 0)
	{
		int exit_status;
		waitpid(pid, &exit_status, 0);
		c->status = WEXITSTATUS(exit_status);
	}
	/* Child process executes the subshell command */
	else if(pid == 0)
	{
		input_and_output_redirection(c);
		execute_compound_command(c->u.subshell_command);
		/* Terminate the child process after the subshell command has been executed */
		exit(c->u.subshell_command->status);
	}
	else
		error(1, 0, "Error! Forking failed.\n");
}

void 
execute_compound_command(command_t c)
{
	switch(c->type)
	{
		case AND_COMMAND:
			execute_compound_command(c->u.command[0]);
			/* If the first command exits WITH error, then the second command will not be executed */
			if(c->u.command[0]->status == 0)
			{
				execute_compound_command(c->u.command[1]);
				c->status = c->u.command[1]->status;
			}
			else
				c->status = c->u.command[0]->status;
		break;
		case OR_COMMAND:
			execute_compound_command(c->u.command[0]);
			/* If the first command exits WITHOUT error, then the second command will not be executed */
			if(c->u.command[0]->status != 0)
			{
				execute_compound_command(c->u.command[1]);
				c->status = c->u.command[1]->status;
			}
			else
				c->status = c->u.command[0]->status;
		break;
		case SEQUENCE_COMMAND:
			/* If there are multiple sequence commands, then they will be executed recursively */
			execute_compound_command(c->u.command[0]);
			execute_compound_command(c->u.command[1]);
			c->status = c->u.command[1]->status;
		break;
		case PIPE_COMMAND:
			execute_pipe_command(c);
		break;
		case SIMPLE_COMMAND:
			execute_simple_command(c);
		break;
		case SUBSHELL_COMMAND:
			execute_subshell_command(c);
		break;
	}
}

/* Time travel execution functions */

int
number_of_processes_to_fork(command_stream_t s, int number_of_commands, int* waiting)
{
	int result = 0;
	int i;
	for(i = 0; i < number_of_commands; i++, s = s->next_command)
		/* Create a new process for a command that does not depend on other commands,
		   and it has not yet been executed */
		if(waiting[i] == 0 && s->command->status == -1)
			result++;
	return result;
}

int /* bool */
check_dependency(char** earlier_io, int earlier_io_size, char** later_io, int later_io_size)
{
	int i, j;
	for(i = 0; i < earlier_io_size; i++)
		for(j = 0; j < later_io_size; j++)
			if(strcmp(earlier_io[i], later_io[j]) == 0)
				return 1;
	return 0;
}

void
retrieve_command_io(command_t c, char*** result_command_io, int* result_command_io_size)
{
	if(c->type == SIMPLE_COMMAND || c->type == SUBSHELL_COMMAND)
	{
		/* Standard input redirection */
		if(c->input != NULL)
		{
			(*result_command_io)[(*result_command_io_size)++] = c->input;
			*result_command_io = checked_realloc(*result_command_io, ((*result_command_io_size)+1)*sizeof(char*));
			(*result_command_io)[*result_command_io_size] = NULL;
		}

		/* Standard output redirection */
		if(c->output != NULL)
		{
			(*result_command_io)[(*result_command_io_size)++] = c->output;
			*result_command_io = checked_realloc(*result_command_io, ((*result_command_io_size)+1)*sizeof(char*));
			(*result_command_io)[*result_command_io_size] = NULL;
		}

		/* Part of the arguments are possibly input or output as well */
		int arg = 1;
		while(c->u.word[arg] != NULL)
		{
			(*result_command_io)[(*result_command_io_size)++] = c->u.word[arg];
			*result_command_io = checked_realloc(*result_command_io, ((*result_command_io_size)+1)*sizeof(char*));
			(*result_command_io)[*result_command_io_size] = NULL;
			arg++;
		}
	}
	/* Since the function is recursive, so the function must pass the result array by reference */
	else
	{
		retrieve_command_io(c->u.command[0], result_command_io, result_command_io_size);
		retrieve_command_io(c->u.command[1], result_command_io, result_command_io_size);
	}
}

/*
*	Execute the list of commands with parallelism using dependency graph,
*	it returns the pointer to the last command of the stream
*/
command_t
execute_commands_with_time_travel(command_stream_t head)
{
	/* Count the number of command lines for producing the dependency graph */
	int number_of_commands = 0;
	command_stream_t current = head->next_command;
	while(current != head)
	{
		number_of_commands++;
		current = current->next_command;
	}

	/* If there are no commands, then nothing would be performed */
	if(number_of_commands == 0)
		return NULL;

	/* Construct the dependency graph and initialize it with zero at the same time */
	int** dependency_graph = (int**) checked_malloc(number_of_commands * sizeof(int*));
	int i;
	for(i = 0; i < number_of_commands; i++)
	{
		dependency_graph[i] = (int*) checked_malloc(number_of_commands * sizeof(int));
		memset(dependency_graph[i], 0, number_of_commands * sizeof(int));
	}

	/* Construct an array counting the number waiting commands of a command. 
	   Assume all of them initially have no waiting commands (no dependency exists) */
	int* waiting = checked_malloc(number_of_commands * sizeof(int));
	memset(waiting, 0, number_of_commands * sizeof(int));

	/* Mark input/output dependencies */
	int command_shift = 0, compare_command_shift = 0;
	command_stream_t compare = head->next_command;
	current = head->next_command;
	int current_index = 0, compare_index = 0;
	while(current != head && compare != head)
	{
		/* Avoid repeated comparison */
		while(compare_command_shift != command_shift && compare != head)
		{
			compare = compare->next_command;
			compare_index++;
			compare_command_shift++;
		}

		/* Two identical commands don't need to check dependency */
		if(current == compare)
		{
			compare = compare->next_command;
			compare_index++;
			continue;
		}

		while(compare != head)
		{
			char** current_command_io = checked_malloc(sizeof(char*));
			char** compare_command_io = checked_malloc(sizeof(char*));
			int current_command_io_size = 0, compare_command_io_size = 0;
			/* Get the input and output from two commands and check if the command being compared depends on
			   the current command */
			retrieve_command_io(current->command, &current_command_io, &current_command_io_size);
			retrieve_command_io(compare->command, &compare_command_io, &compare_command_io_size);
			if(check_dependency(current_command_io, current_command_io_size, 
								compare_command_io, compare_command_io_size))
			{
				dependency_graph[compare_index][current_index] = 1;
				waiting[compare_index]++;
			}
			compare = compare->next_command;
			compare_index++;
			/* Clean up the dynamcially allocated memory */
			free(current_command_io);
			free(compare_command_io);
		}

		current = current->next_command;
		current_index++;
		/* Reinitialize the variables keeping track of the command being compared */
		compare = head->next_command;
		compare_index = 0;
		compare_command_shift = 0;
		command_shift++;
	}

	/* Execute the commands in time travel mode */
	int number_of_child_processes;
	while((number_of_child_processes = number_of_processes_to_fork(head->next_command, number_of_commands, waiting)) >= 1)
	{
		command_stream_t temp = head->next_command;
		int temp_index, process_index;

		pid_t* process_id = checked_malloc(number_of_child_processes * sizeof(pid_t));
		int* exit_status = checked_malloc(number_of_child_processes * sizeof(int));
		/* Used to map the process ID to the corresponding command */
		int* map_process_id_to_command = checked_malloc(number_of_child_processes * sizeof(int));

		/* Map the corresponding command to a process ID */
		for(temp_index = 0, process_index = 0; temp_index < number_of_commands; 
			temp_index++, temp = temp->next_command)
			/* This should get the same commands as from the number_of_processes_to_fork() function */
			if(waiting[temp_index] == 0 && temp->command->status == -1)
			{
				map_process_id_to_command[process_index] = temp_index;
				process_index++;
			}

		/* Execute the available processes in time travel mode */
		for(process_index = 0; process_index < number_of_child_processes; process_index++)
		{
			/* Get the correct command */
			for(temp = head->next_command, temp_index = 0; temp_index != map_process_id_to_command[process_index];
				temp = temp->next_command, temp_index++)
				;
			process_id[process_index] = fork();
			/* The parent process will wait for the child processes to finish after they are all forked */
			if(process_id[process_index] == 0)
			{
				execute_compound_command(temp->command);
				exit(temp->command->status);
			}
			else if(process_id[process_index] == -1)
				error(1, 0, "Error! Forking failed.\n");
		}

		/* Let the parent process wait for all child processes to finish */
		for(process_index = 0; process_index < number_of_child_processes; process_index++)
			waitpid(process_id[process_index], &exit_status[process_index], 0);

		/* Update the status of all the commands that have already run */
		for(process_index = 0; process_index < number_of_child_processes; process_index++)
		{
			/* Get the correct command */
			for(temp = head->next_command, temp_index = 0; temp_index != map_process_id_to_command[process_index];
				temp = temp->next_command, temp_index++)
				;
			temp->command->status = WEXITSTATUS(exit_status[process_index]);
			/* Update the dependency graph and waiting array */
			int i;
			for(i = 0; i < number_of_commands; i++)
				if(dependency_graph[i][map_process_id_to_command[process_index]] == 1)
				{
					dependency_graph[i][map_process_id_to_command[process_index]] == 0;
					waiting[i]--;
				}
		}

		/* Clean up the dynamcially allocated memory */
		free(process_id);
		free(exit_status);
		free(map_process_id_to_command);
	}

	return current->command;
}

void
execute_command (command_t c, int time_travel)
{
	/* FIXME: Replace this with your implementation.  You may need to
	 add auxiliary functions and otherwise modify the source code.
	 You can also use external functions defined in the GNU C Library.  */
	if(!time_travel) 
		execute_compound_command(c);
}
