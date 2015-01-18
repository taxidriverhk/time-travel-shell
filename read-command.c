// UCLA CS 111 Lab 1 command reading

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

void 
initialize_command_stream_node(command_stream_t node)
{
	if(node == NULL)
		return;

	node->command = NULL;
	node->line_number = 0;
	node->read = 0;
	node->prev_command = NULL;
	node->next_command = NULL;
}

void
insert_node_back(command_stream_t node, command_stream_t dummy_node)
{
	command_stream_t temp = dummy_node->next_command;
	while(temp != dummy_node->prev_command)
		temp = temp->next_command;
	temp->next_command = node;
	node->next_command = dummy_node;
	node->prev_command = temp;
	dummy_node->prev_command = node;
}

/* Possible types of a token, I/O redirections are included 
   inside a simple command */
enum token_type
{
	AND, OR,
	OPEN_PARENTHESIS, CLOSE_PARENTHESIS,
	SIMPLE, // assume it is simple, but can be sequential command
	INPUT_AND_OUTPUT // I/O redirection for subshell command
};

/* Structure for decomposing a line of command */
struct command_token
{
	enum token_type type;
	char* command;
	struct command_token* next;
};

typedef struct command_token* command_token_t;

void 
insert_token_back(command_token_t node, command_token_t* top)
{
	if(*top == NULL)
		*top = node;
	else
	{
		command_token_t temp = *top;
		while(temp != NULL)
		{
			if(temp->next == NULL)
			{
				temp->next = node;
				node->next = NULL;
				return;
			}
			temp = temp->next;
		}
	}
}

void
delete_token_list(command_token_t* top)
{
	if(*top == NULL)
		return;

	command_token_t temp = *top;
	while(temp != NULL)
	{
		command_token_t target = temp;
		temp = temp->next;
		free(target);
	}
	*top = NULL;
}

/* Structure acting like C++ STL stack<struct command_token*> */
struct command_token_stack
{
	struct command_token* top;
	int size;
};

typedef struct command_token_stack* command_token_stack_t;

void
command_token_stack_initialize(command_token_stack_t stack)
{
	stack->top = NULL;
	stack->size = 0;
}

int
command_token_stack_push(command_token_stack_t stack, enum token_type t, char* str)
{
	command_token_t new_top = (command_token_t) 
								checked_malloc(sizeof(struct command_token));
	new_top->type = t;
	new_top->command = str;
	new_top->next = stack->top;
	stack->top = new_top;
	stack->size++;
	return 1; // Returns 0 if the node was pushed successfully
}

int
command_token_stack_pop(command_token_stack_t stack)
{
	if(stack->top == NULL)
		return 0;

	command_token_t new_top = stack->top->next;
	free(stack->top->command);
	free(stack->top);
	stack->top = new_top;
	stack->size--;
	return 1; // Returns 0 if the stack is empty, 1 if the top item was popped successfully
}

/* Structure acting like C++ STL stack<struct command*> */
struct command_stack_node
{
	struct command* c;
	struct command_stack_node* next;
};

typedef struct command_stack_node* command_stack_node_t;

struct command_stack
{
	struct command_stack_node* top;
	int size;
};

typedef struct command_stack* command_stack_t;

void
command_stack_initialize(command_stack_t stack)
{
	stack->top = NULL;
	stack->size = 0;
}

int
command_stack_push(command_stack_t stack, command_t command)
{
	command_stack_node_t new_top = (command_stack_node_t) 
								checked_malloc(sizeof(struct command_stack_node));
	new_top->c = command;
	new_top->next = stack->top;
	stack->top = new_top;
	stack->size++;
	return 1; // Returns 0 if the node was pushed successfully
}

int
command_stack_pop(command_stack_t stack)
{
	if(stack->top == NULL)
		return 0;

	command_stack_node_t new_top = stack->top->next;
	free(stack->top);
	stack->top = new_top;
	stack->size--;
	return 1; // Returns 0 if the stack is empty, 1 if the top item was popped successfully
}

/* Function declarations */
void initialize_command_node(command_t node);
int count_number_of_words(char** words);
void delete_one_command(command_t c);
void delete_one_node(command_stream_t node);
void delete_commands(command_stream_t s);
int /* bool */ is_word(char ch);
int /* bool */ is_special_token(char ch);
void trim_left(char* str);
command_t create_simple_command(char* command_line, int current_line_number);
command_t create_sequential_command(char* command_line, int current_line_number);
command_t create_compound_command(command_token_t postfix_top, int current_line_number);	// Similar to evaluating postfix expression
command_token_t decompose_command(char* command_line, int current_line_number);		// Similar to converting infix to postfix

void
initialize_command_node(command_t node)
{
	if(node == NULL)
		return;
	
	node->input = NULL; node->output = NULL;
	node->status = -1;
}

int 
count_number_of_words(char** words)
{
	int count = 0;
	char** w = words;
	while(*w != NULL)
	{
		w++;
		count++;
	}
	return count;
}

void
delete_one_command(command_t c)
{
	if(c == NULL)
		return;

	switch(c->type)
	{
		case SIMPLE_COMMAND:
		{
			/* Suppose to free the array of strings */
			free(c->u.word);
		}
		break;
		case AND_COMMAND:
		case OR_COMMAND:
		case PIPE_COMMAND:
		case SEQUENCE_COMMAND:
			delete_one_command(c->u.command[0]);
			delete_one_command(c->u.command[1]);
		break;
		case SUBSHELL_COMMAND:
			delete_one_command(c->u.subshell_command);
		break;
	}

	if(c->input != NULL)
		free(c->input);
	if(c->output != NULL)
		free(c->output);
}

void
delete_one_node(command_stream_t node)
{
	if(node == NULL)
		return;

	delete_one_command(node->command);
	free(node);
}

void
delete_commands(command_stream_t s)
{
	/* Delete all nodes except the dummy nodes first */
	command_stream_t current = s->next_command;
	while(current != s)
	{
		command_stream_t target = current;
		current = current->next_command;
		delete_one_node(target);
	}

	/* Finally delete the dummy node */
	delete_one_node(s);
}

int /* bool */
is_word(char ch)
{
	return isalnum(ch) || ch == '!' || ch == '%'
					   || ch == '+' || ch == ',' || ch == '-'
					   || ch == '.' || ch == '/' || ch == ':'
					   || ch == '@' || ch == '^' || ch == '_';
}

int /* bool */
is_special_token(char ch)
{
	return ch == ';' || ch == '|' || ch == '&' || ch == '('
					 || ch == ')' || ch == '<' || ch == '>';
}

void
trim_left(char* str)
{
	char* temp = str;
	while(*str == ' ' || *str == '\t')
		if(*str == ' ' || *str == '\t')
		{
			for(; *str != '\0'; str++)
				*str = *(str+1);
			str = temp;
		}
}

command_t 
create_simple_command(char* command_line, int current_line_number)
{
	if(command_line == NULL || *command_line == '\0')
		return NULL;

	command_t result_command = (command_t) checked_malloc(sizeof(struct command));
	initialize_command_node(result_command);

	int current_position = 0;
	int current_command = 0;

	/* Read each character from the Shell script and then determine
	   which combine as a command or which line does syntax error occur */
	char** words = (char**) checked_malloc(sizeof(char*));
	words[current_command] = (char*) checked_malloc(sizeof(char));
	words[current_command][0] = '\0';

	/* Buffers for I/O redirections */
	char* input = (char*) checked_malloc(sizeof(char));
	char* output = (char*) checked_malloc(sizeof(char));
	input[0] = '\0'; output[0] = '\0';

	char* current_char = command_line;
	while(*current_char != '\0')
	{
		/* Process I/O redirections */
		if(*current_char == '<' || *current_char == '>')
		{
			if(words[current_command][0] == '\0')
				error(1, 0, "%d: Syntax error, no command comes before I/O redirection(s).\n"
					, current_line_number);

			char redirection_char = *current_char;
			current_char++;
			/* Ignore whitespaces or indents */
			while(*current_char == ' ' || *current_char == '\t')
				current_char++;
			/* Either the filename was not specified or the filename contains invalid characters */
			if(!is_word(*current_char))
				error(1, 0, "%d: Syntax error, filename(s) for I/O redirection(s) was/were not specified.\n"
					, current_line_number);
				
			/* Input file redirection */
			if(redirection_char == '<')
			{
				/* Retrieve the filename until hitting newline, space or output redirection (if exists) */
				int input_filename_count = 0;
				while(*current_char != EOF && is_word(*current_char) 
						&& *current_char != ' ' && *current_char != '\t'
						&& *current_char != '>')
				{
					input[input_filename_count++] = *current_char;
					input = (char*) checked_realloc(input, (input_filename_count+1)*sizeof(char));
					current_char++;
				}
				input[input_filename_count] = '\0';
			}
			/* Input file redirection */
			else if(redirection_char == '>')
			{
				/* Retrieve the filename until hitting newline, space or input redirection (if exists) */
				int output_filename_count = 0;
				while(*current_char != EOF && is_word(*current_char) 
						&& *current_char != ' ' && *current_char != '\t' 
						&& *current_char != '<')
				{
					output[output_filename_count++] = *current_char;
					output = (char*) checked_realloc(output, (output_filename_count+1)*sizeof(char));
					current_char++;
				}
				output[output_filename_count] = '\0';
			}
		}
		/* When hitting space or indent, then a word would be created
		   , and begin to get another word */
		else if(*current_char == ' ' || *current_char == '\t')
		{	
			while(*current_char == ' ' || *current_char == '\t')
				current_char++;
			/* If any redirection character is hit, then there are probably no more
			   arguments */
			if(words[current_command][0] != '\0' && *current_char != '\0'
				&& *current_char != '<' && *current_char != '>')
			{
				words[current_command++][current_position] = '\0';
				words = (char**) checked_realloc(words, (current_command+1)*sizeof(char*));
				words[current_command] = (char*) checked_malloc(sizeof(char));
				words[current_command][0] = '\0';
				current_position = 0;
			}
		}
		/* Otherwise, the character is treated as part of an argument */
		else
		{
			words[current_command][current_position++] = *current_char;
			words[current_command] = (char*) checked_realloc(words[current_command]
									, (current_position+1)*sizeof(char));
			current_char++;
		}
	}

	result_command->type = SIMPLE_COMMAND;
	/* If I/O redirections exist */
	if(*input != '\0')
		result_command->input = input;				
	if(*output != '\0')
		result_command->output = output;
			
	words[current_command][current_position] = '\0';
	/* Append a NULL pointer to char to words */
	words = (char**) checked_realloc(words, (current_command+2)*sizeof(char*));
	current_command++;
	words[current_command] = NULL;
	result_command->u.word = words;

	return result_command;
}

command_t
create_sequential_command(char* command_line, int current_line_number)
{
	if(*command_line == '\0')
		return NULL;

	command_t result_command = (command_t) checked_malloc(sizeof(struct command));
	command_t left_command_buffer = (command_t) checked_malloc(sizeof(struct command));
	command_t right_command_buffer = (command_t) checked_malloc(sizeof(struct command));
	initialize_command_node(result_command);
	initialize_command_node(left_command_buffer);
	initialize_command_node(right_command_buffer);

	int current_buffer_position = 0;
	char* right_buffer = command_line;

	char* left_buffer = (char*) checked_malloc(sizeof(char));
	*left_buffer = '\0';

	while(*right_buffer != '\0')
	{
		if(is_word(*right_buffer) || *right_buffer == '<'  || *right_buffer == '>' 
			|| *right_buffer == ' ' || *right_buffer == '\t')
		{
			left_buffer[current_buffer_position++] = *right_buffer;
			left_buffer = (char*) checked_realloc(left_buffer
								, (current_buffer_position+1)*sizeof(char));
		}
		else if(*right_buffer == '|' || *right_buffer == ';')
		{
			if(*right_buffer == ';' && *(right_buffer+1) == ';')
				error(1, 0, "%d: Syntax error, format for SEQUENCE_COMMAND is wrong.\n"
						, current_line_number);

			char pipe_or_seq = *right_buffer;
			/* Check if there's a command before or after the special token */
			left_buffer[current_buffer_position] = '\0';
			trim_left(left_buffer);
			if(strlen(left_buffer) == 0)
				error(1, 0, "%d: Syntax error, no command comes before | or ; .\n"
						, current_line_number);
			right_buffer++;
			while((*right_buffer == ' ' || *right_buffer == '\t' || *right_buffer == '\n') 
				&& *right_buffer != '\0')
				right_buffer++;
			if(strlen(right_buffer) == 0)
				error(1, 0, "%d: Syntax error, no command comes after | or ; .\n"
						, current_line_number);

			if(pipe_or_seq == '|')
				result_command->type = PIPE_COMMAND;
			else if(pipe_or_seq == ';')
				result_command->type = SEQUENCE_COMMAND;

			/* Split the command line and create two other command lines recursively */
			left_command_buffer = create_simple_command(left_buffer, current_line_number);
			right_command_buffer = create_sequential_command(right_buffer, current_line_number);

			result_command->u.command[0] = left_command_buffer;
			result_command->u.command[1] = right_command_buffer;
			result_command->input = NULL; result_command->output = NULL;

			free(left_buffer);
			return result_command;
		}
		else
			error(1, 0, "%d: Syntax error, invalid characters are not allowed.\n"
				, current_line_number);

		right_buffer++;
	}
	
	/* From here, the command must be simple */
	left_buffer[current_buffer_position] = '\0';
	result_command = create_simple_command(left_buffer, current_line_number);
	free(left_buffer);
	return result_command;
}

command_t
create_compound_command(command_token_t postfix_top, int current_line_number)
{
	struct command_stack sequential_command_stack;
	command_stack_initialize(&sequential_command_stack);

	command_t result_command;
	command_t command_buffer;
	command_t left_buffer, right_buffer;

	/* Only one sequential command */
	if(postfix_top->type == SIMPLE && postfix_top->next == NULL)
	{
		result_command = (command_t) checked_malloc(sizeof(struct command));
		initialize_command_node(result_command);
		result_command = create_sequential_command(postfix_top->command, current_line_number);
		return result_command;
	}

	command_token_t current_token = postfix_top;
	while(current_token != NULL)
	{
		if(current_token->type == SIMPLE)
		{
			command_buffer = create_sequential_command(current_token->command, current_line_number);
			command_stack_push(&sequential_command_stack, command_buffer);
		}
		else if(current_token->type == OPEN_PARENTHESIS)
		{
			command_buffer = (command_t) checked_malloc(sizeof(struct command));
			initialize_command_node(command_buffer);
			command_buffer->type = SUBSHELL_COMMAND;
			command_buffer->input = NULL; command_buffer->output = NULL;
			if(current_token->next != NULL && current_token->next->type == INPUT_AND_OUTPUT)
			{
				char* input_filename = (char*) checked_malloc(sizeof(char));
				char* output_filename = (char*) checked_malloc(sizeof(char));
				int input_filename_count = 0, output_filename_count = 0;
				current_token = current_token->next;
				while(*(current_token->command) != '\0')
				{
					if(*(current_token->command) == '<')
					{
						current_token->command++;
						while(!is_word(*(current_token->command)))
							current_token->command++;
						while(*(current_token->command) != ' ' && *(current_token->command) != '\t'
							  && *(current_token->command) != '\0')
						{
							input_filename[input_filename_count++] = *(current_token->command);
							input_filename = (char*) checked_realloc(input_filename, input_filename_count+1);
							current_token->command++;
						}
						input_filename[input_filename_count] = '\0';
						command_buffer->input = input_filename;
						if(*(current_token->command) == '\0')
							current_token->command--;
					}
					else if(*(current_token->command) == '>')
					{
						current_token->command++;
						while(!is_word(*(current_token->command)))
							current_token->command++;
						while(*(current_token->command) != ' ' && *(current_token->command) != '\t'
							  && *(current_token->command) != '\0')
						{
							output_filename[output_filename_count++] = *(current_token->command);
							output_filename = (char*) checked_realloc(output_filename, output_filename_count+1);
							current_token->command++;
						}
						output_filename[output_filename_count] = '\0';
						command_buffer->output = output_filename;
						if(*(current_token->command) == '\0')
							current_token->command--;
					}
					else if(*(current_token->command) == ' ' || *(current_token->command) == '\t')
					{
						current_token->command++;	
						continue;
					}
					else
						error(1, 0, "%d: Syntax error, bad format of I/O redirection.\n", current_line_number);
					current_token->command++;
				}

			}
			command_buffer->u.subshell_command = sequential_command_stack.top->c;
			command_stack_pop(&sequential_command_stack);
			command_stack_push(&sequential_command_stack, command_buffer);
			result_command = command_buffer;
		}
		else
		{
			result_command = (command_t) checked_malloc(sizeof(struct command));
			initialize_command_node(result_command);
			right_buffer = sequential_command_stack.top->c;
			command_stack_pop(&sequential_command_stack);
			left_buffer = sequential_command_stack.top->c;
			command_stack_pop(&sequential_command_stack);

			if(current_token->type == AND)
				result_command->type = AND_COMMAND;
			else if(current_token->type == OR)
					result_command->type = OR_COMMAND;

			result_command->input = NULL; result_command->output = NULL;
			result_command->u.command[0] = left_buffer;
			result_command->u.command[1] = right_buffer;
			command_stack_push(&sequential_command_stack, result_command);
		}
		current_token = current_token->next;
	}

	delete_token_list(&postfix_top);
	return result_command;
}

/* Decompose the command line to a list of tokens and convert it to postfix
   , then return the first token of the postfix expression */
command_token_t
decompose_command(char* command_line, int current_line_number)
{
	/* 
	 *	Command decomposition 
	 */
	trim_left(command_line);
	command_token_t top_prefix_token = NULL;
	command_token_t buffer_token = NULL;
	char* command_buffer = (char*) checked_malloc(sizeof(char));
	int num_open = 0, num_close = 0;
	int current_buffer_position = 0;
	*command_buffer = '\0';

	char* current_char = command_line;
	while(*current_char != '\0')
	{
		if((*current_char == '&' && *(current_char+1) == '&') || (*current_char == '|' && *(current_char+1) == '|')
			|| *current_char == '(' || *current_char == ')')
		{
			command_buffer[current_buffer_position] = '\0';
			trim_left(command_buffer);
			if(*command_buffer != '\0')
			{
				enum token_type final_type;
				if(buffer_token != NULL && buffer_token->type == CLOSE_PARENTHESIS)
					final_type = INPUT_AND_OUTPUT;
				else
					final_type = SIMPLE;
				buffer_token = (command_token_t) checked_malloc(sizeof(struct command_token));
				buffer_token->type = final_type;
				buffer_token->command = command_buffer;
				buffer_token->next = NULL;
				insert_token_back(buffer_token, &top_prefix_token);
			}

			if(*current_char != '(' && *command_buffer == '\0' 
				&& (buffer_token == NULL || (buffer_token != NULL && buffer_token->type != CLOSE_PARENTHESIS)))
				error(1, 0, "%d: Syntax error, no command comes before &&, || or ( .\n"
					, current_line_number);

			buffer_token = (command_token_t) checked_malloc(sizeof(struct command_token));
			buffer_token->command = NULL;
			if(*current_char == '&' && *(current_char+1) == '&')
			{
				buffer_token->type = AND;
				current_char++;
			}
			else if(*current_char == '|' && *(current_char+1) == '|')
			{
				buffer_token->type = OR;
				current_char++;
			}
			else if(*current_char == '(')
			{
				buffer_token->type = OPEN_PARENTHESIS;
				num_open++;
			}
			else if(*current_char == ')')
			{
				buffer_token->type = CLOSE_PARENTHESIS;
				num_close++;
			}
			
			buffer_token->next = NULL;
			insert_token_back(buffer_token, &top_prefix_token);

			current_buffer_position = 0;
			command_buffer = (char*) checked_malloc(sizeof(char));
			*command_buffer = '\0';
		}
		else if(is_word(*current_char) || *current_char == ' ' || *current_char == '\t' 
				|| (*current_char == '|' && *(current_char+1) != '|') || *current_char == ';'
				|| *current_char == '<' || *current_char == '>')
		{
			command_buffer[current_buffer_position++] = *current_char;
			command_buffer = (char*) checked_realloc(command_buffer, (current_buffer_position+1)*sizeof(char));
		}
		else
			error(1, 0, "%d: Syntax error, invalid characters are not allowed.\n"
					, current_line_number);

		current_char++;
	}

	if(num_open != num_close)
		error(1, 0, "%d: Syntax error, parentheses are not balanced.\n", current_line_number);

	/* For last sequential command token */
	if(*command_buffer != '\0')
	{
		enum token_type final_type;
		if(buffer_token != NULL && buffer_token->type == CLOSE_PARENTHESIS)
			final_type = INPUT_AND_OUTPUT;
		else
			final_type = SIMPLE;
		command_buffer[current_buffer_position] = '\0';
		buffer_token = (command_token_t) checked_malloc(sizeof(struct command_token));
		buffer_token->type = final_type;
		buffer_token->command = command_buffer;
		buffer_token->next = NULL;
		insert_token_back(buffer_token, &top_prefix_token);
	}

	/* 
	 *	Infix to postfix conversion 
	 */
	struct command_token_stack special_token_stack;
	command_token_stack_initialize(&special_token_stack);
	command_token_t top_postfix_token = NULL;
	command_token_t postfix_token;

	command_token_t current_token = top_prefix_token;
	while(current_token != NULL)
	{
		switch(current_token->type)
		{
			case SIMPLE:
				postfix_token = (command_token_t) checked_malloc(sizeof(struct command_token));
				postfix_token->type = SIMPLE;
				postfix_token->command = current_token->command;
				postfix_token->next = NULL;
				insert_token_back(postfix_token, &top_postfix_token);
			break;
			case AND:
			case OR:
				while(special_token_stack.size != 0 
					&& special_token_stack.top->type != OPEN_PARENTHESIS)
				{
					postfix_token = (command_token_t) checked_malloc(sizeof(struct command_token));
					postfix_token->type = special_token_stack.top->type;
					postfix_token->command = NULL;
					postfix_token->next = NULL;
					insert_token_back(postfix_token, &top_postfix_token);
					command_token_stack_pop(&special_token_stack);
				}
			case OPEN_PARENTHESIS:
				command_token_stack_push(&special_token_stack, current_token->type, current_token->command);
			break;
			case CLOSE_PARENTHESIS:
				while(special_token_stack.top->type != OPEN_PARENTHESIS)
				{
					postfix_token = (command_token_t) checked_malloc(sizeof(struct command_token));
					postfix_token->type = special_token_stack.top->type;
					postfix_token->command = NULL;
					postfix_token->next = NULL;
					insert_token_back(postfix_token, &top_postfix_token);
					command_token_stack_pop(&special_token_stack);
				}
				postfix_token = (command_token_t) checked_malloc(sizeof(struct command_token));
				postfix_token->type = OPEN_PARENTHESIS;
				postfix_token->command = NULL;
				postfix_token->next = NULL;
				insert_token_back(postfix_token, &top_postfix_token);
				command_token_stack_pop(&special_token_stack);
			break;
			case INPUT_AND_OUTPUT:
				postfix_token = (command_token_t) checked_malloc(sizeof(struct command_token));
				postfix_token->type = INPUT_AND_OUTPUT;
				postfix_token->command = current_token->command;
				postfix_token->next = NULL;
				insert_token_back(postfix_token, &top_postfix_token);
			break;
			default:
			break;
		}

		/* Check for syntax errors */
		if(current_token->type == SIMPLE && current_token->next != NULL && 
			current_token->next->type == SIMPLE)
			error(1, 0, "%d: Syntax error, no special token between two commands.\n", current_line_number);
		if((current_token->type == OR || current_token->type == AND) 
			&& ((current_token->next != NULL && current_token->next->type != SIMPLE && current_token->next->type != OPEN_PARENTHESIS) 
			|| current_token->next == NULL))
			error(1, 0, "%d: Syntax error, no command comes after && or || .\n", current_line_number);
		if(current_token->type == OPEN_PARENTHESIS 
			&& (current_token->next == NULL || (current_token->next != NULL && current_token->next->type != SIMPLE)))
			error(1, 0, "%d: Syntax error, invalid token after ( .\n", current_line_number);

		current_token = current_token->next;
	}

	while(special_token_stack.size != 0)
	{
		postfix_token = (command_token_t) checked_malloc(sizeof(struct command_token));
		postfix_token->type = special_token_stack.top->type;
		postfix_token->command = NULL;
		postfix_token->next = NULL;
		insert_token_back(postfix_token, &top_postfix_token);
		command_token_stack_pop(&special_token_stack);
	}

	/* Clean up the prefix tokens */
	delete_token_list(&top_prefix_token);

	return top_postfix_token;
}

/* 
*	The purpose of this function is to create a doubly linked-list storing
*	the list of commands, which are split every line.
*	This fuction returns the head (the dummy node) of the linked-list.
*/
command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
	/* FIXME: Replace this with your implementation.  You may need to
		add auxiliary functions and otherwise modify the source code.
		You can also use external functions defined in the GNU C Library.  */

	/* Create a dummy node */
	command_stream_t command_stream_dummy 
		= (command_stream_t) checked_malloc(sizeof(struct command_stream));
	initialize_command_stream_node(command_stream_dummy);
	command_stream_dummy->prev_command = command_stream_dummy;
	command_stream_dummy->next_command = command_stream_dummy;

	int current_line_number = 1;
	int current_command_position = 0;

	int beginning_of_line = 0;
	
	command_token_t command_token_buffer;

	/* Buffer for command */
	command_t command_buffer = (command_t) checked_malloc(sizeof(struct command));

	/* Buffer for one line of command(s) */
	char* command_line = (char*) checked_malloc(sizeof(char));
	*command_line = '\0';

	char ch = get_next_byte(get_next_byte_argument);
	/* Take care of special cases */
	if(ch == ' ' || ch == '\t')
		while(ch == ' ' || ch == '\t')
			ch = get_next_byte(get_next_byte_argument);

	while(ch != EOF)
	{
		/* Insert a new line of command(s) into the stream */
		if(ch == '\n')
		{
			/* Check if the line is empty */
			if(*command_line != '\0')
			{
				/* Append the terminating character to the command line */
				command_line[current_command_position] = '\0';
				command_token_buffer = decompose_command(command_line, current_line_number);
				command_buffer = create_compound_command(command_token_buffer, current_line_number);

				command_stream_t new_command_stream
						= (command_stream_t) checked_malloc(sizeof(struct command_stream));
				initialize_command_stream_node(new_command_stream);
				new_command_stream->command = command_buffer;
				new_command_stream->line_number = current_line_number;
				insert_node_back(new_command_stream, command_stream_dummy);

				/* Reset the buffer, ready for the next command line */
				command_line = (char*) checked_malloc(sizeof(char));
				*command_line = '\0';
				current_command_position = 0;
			}
			current_line_number++;
			beginning_of_line = 1;
		}
		/* If the character is |, & or ; , then totally ignore newline character until a word is hit */
		else if(ch == '|' || ch == '&' || ch == ';')
		{
			while(!is_word(ch) && ch != EOF)
			{
				if(ch != '\n')
				{
					command_line[current_command_position++] = ch;
					command_line = (char*) checked_realloc(command_line, (current_command_position+1)*sizeof(char));
				}
				ch = get_next_byte(get_next_byte_argument);
			}
			if(ch == EOF)
				error(1, 0, "%d: Syntax error, no command comes after |, ||, && or ; .\n"
					, current_line_number);
			else
				continue;
		}
		/* Ignore all characters from comment line */
		else if(ch == '#')
		{
			while(ch != '\n' && ch != EOF)
				ch = get_next_byte(get_next_byte_argument);
			continue;
		}
		/* Insert the character into the command line buffer, regardless of correctness of the grammar */
		else
		{
			command_line[current_command_position++] = ch;
			command_line = (char*) checked_realloc(command_line, (current_command_position+1)*sizeof(char));
		}
		ch = get_next_byte(get_next_byte_argument);
		
		/* Ignore all spaces and tabs in the beginning of line */
		if(beginning_of_line && (ch == ' ' || ch == '\t'))
			while(ch == ' ' || ch == '\t')
				ch = get_next_byte(get_next_byte_argument);
		beginning_of_line = 0;
	}

	/* Process the last command line (if exists) */
	if(*command_line != '\0')
	{
		/* Append the terminating character to the command line */
		command_line[current_command_position] = '\0';
		command_token_buffer = decompose_command(command_line, current_line_number);
		command_buffer = create_compound_command(command_token_buffer, current_line_number);

		command_stream_t new_command_stream
				= (command_stream_t) checked_malloc(sizeof(struct command_stream));
		initialize_command_stream_node(new_command_stream);
		new_command_stream->command = command_buffer;
		new_command_stream->line_number = current_line_number;
		insert_node_back(new_command_stream, command_stream_dummy);
	}

	/* Return the dummy node of the command stream */
	//free(command_line);
	return command_stream_dummy;
}

/*
*	This function reads the linked list from the head, and deletes the previous node
*	that just has been read, that's why doubly linked-list is required.
*/
command_t
read_command_stream (command_stream_t s)
{
	/* FIXME: Replace this with your implementation too.  */

	/* Either the linked-list is empty or all commands have been read */
	if(s == NULL)
		return NULL;

	command_stream_t temp = s->next_command;
	while(temp != s)
	{
		if(temp->read == 0)
		{
			temp->read = 1;
			return temp->command;
		}
		temp = temp->next_command;
	}

	/* Clean up the dynamically-allocated memories after all commands are read */
	delete_commands(s);
	return NULL;
}
