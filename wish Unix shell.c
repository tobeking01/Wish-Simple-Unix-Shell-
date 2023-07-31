#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_COMMANDS 10
#define MAX_COMMAND_LENGTH 100

char history[MAX_COMMANDS][MAX_COMMAND_LENGTH];
int history_count = 0;
char search_path[MAX_COMMAND_LENGTH] = "/bin:/usr/bin"; // Default search path
const char error_message[] = "An error has occurred\n";

// Display the command history
void display_history()
{
    for (int i = 0; i < history_count; i++)
        printf("%d %s\n", i + 1, history[i]);
}

// Execute the most recent command from history
void execute_recent_command()
{
    if (history_count == 0)
        printf("No commands in history\n");
    else
    {
        char command[MAX_COMMAND_LENGTH];
        strcpy(command, history[history_count - 1]);
        printf("%s\n", command);
        system(command);
    }
}

// Execute a command from history by its index
void execute_indexed_command(int index)
{
    if (index <= 0 || index > history_count)
        printf("Invalid history index\n");
    else
    {
        char command[MAX_COMMAND_LENGTH];
        strcpy(command, history[index - 1]);
        printf("%s\n", command);
        system(command);
    }
}

// Print error message
void print_error_message()
{
    write(STDERR_FILENO, error_message, sizeof(error_message) - 1);
}

// Execute a command
void execute_command(char *buffer)
{
    int background = 0;
    if (buffer[strlen(buffer) - 1] == '&')
    {
        background = 1;
        buffer[strlen(buffer) - 1] = '\0';
    }

    // Check if the command is 'cd'
    if (strncmp(buffer, "cd ", 3) == 0)
    {
        // Extract the directory path from the command
        char *directory = buffer + 3;

        // Change directory to the specified path
        if (chdir(directory) != 0)
        {
            print_error_message();
            return;
        }
        return;
    }
    // Check if the command is 'path'
    if (strncmp(buffer, "path ", 5) == 0)
    {
        // Extract the new path from the command
        char *new_path = buffer + 5;

        // Update the search path
        strcpy(search_path, new_path);
        return;
    }

    // Check if the command includes redirection
    char *redirect_output = strchr(buffer, '>');
    if (redirect_output != NULL)
    {
        // Extract the filename after the redirection symbol
        char *filename = strtok(redirect_output + 1, " ");
        if (filename == NULL)
        {
            print_error_message();
            return;
        }

        // Open the file for writing, truncating it if it already exists
        FILE *file = fopen(filename, "w");
        if (file == NULL)
        {
            print_error_message();
            return;
        }

        // Redirect standard output and standard error to the file
        int stdout_copy = dup(fileno(stdout));
        int stderr_copy = dup(fileno(stderr));
        if (dup2(fileno(file), fileno(stdout)) == -1 || dup2(fileno(file), fileno(stderr)) == -1)
        {
            print_error_message();
            fclose(file);
            dup2(stdout_copy, fileno(stdout)); // Restore the original stdout
            dup2(stderr_copy, fileno(stderr)); // Restore the original stderr
            return;
        }

        fclose(file);
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        // Child process
        char *args[MAX_COMMAND_LENGTH];
        int arg_count = 0;

        char *token = strtok(buffer, " ");
        while (token != NULL)
        {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;

        execvp(args[0], args);
        print_error_message();
        exit(1);
    }
    else if (pid > 0)
    {
        // Parent process
        if (!background)
            waitpid(pid, NULL, 0);
    }
    else
    {
        print_error_message();
        exit(1); // Exit the shell when there is an error
    }
}

int main(int argc, char *argv[])
{
    char *buffer = NULL;
    size_t bufsize = 0;
    ssize_t characters;

    if (argc > 1)
    {
        FILE *batch_file = fopen(argv[1], "r");
        if (batch_file == NULL)
        {
            perror("Error opening batch file");
            return 1;
        }

        // Read commands from the batch file instead of stdin
        buffer = NULL;
        bufsize = 0;
        while ((characters = getline(&buffer, &bufsize, batch_file)) != -1)
        {
            buffer[strcspn(buffer, "\n")] = '\0'; // Remove the newline character

            // Execute the command
            if (strcmp(buffer, "exit") == 0)
                break;
            else if (strcmp(buffer, "history") == 0)
                display_history();
            else if (strcmp(buffer, "!!") == 0)
                execute_recent_command();
            else if (buffer[0] == '!')
                execute_indexed_command(atoi(&buffer[1]));
            else
            {
                // Store the command in history
                if (history_count < MAX_COMMANDS)
                {
                    strcpy(history[history_count], buffer);
                    history_count++;
                }
                else
                {
                    // Shift the history array to make space for the new command
                    for (int i = 0; i < MAX_COMMANDS - 1; i++)
                        strcpy(history[i], history[i + 1]);

                    strcpy(history[MAX_COMMANDS - 1], buffer);
                }

                execute_command(buffer);
            }
        }

        fclose(batch_file); // Close the batch file
    }
    else
    {
        // Interactive mode
        while (1)
        {
            printf("wish> ");
            fflush(stdout);

            characters = getline(&buffer, &bufsize, stdin);

            if (characters == -1)
            {
                // Check for end-of-file (EOF) marker
                printf("\n");
                break;
            }

            buffer[strcspn(buffer, "\n")] = '\0'; // Remove the newline character

            if (strcmp(buffer, "exit") == 0)
                break;
            else if (strcmp(buffer, "history") == 0)
                display_history();
            else if (strcmp(buffer, "!!") == 0)
                execute_recent_command();
            else if (buffer[0] == '!')
                execute_indexed_command(atoi(&buffer[1]));
            else
            {
                // Store the command in history
                if (history_count < MAX_COMMANDS)
                {
                    strcpy(history[history_count], buffer);
                    history_count++;
                }
                else
                {
                    // Shift the history array to make space for the new command
                    for (int i = 0; i < MAX_COMMANDS - 1; i++)
                        strcpy(history[i], history[i + 1]);

                    strcpy(history[MAX_COMMANDS - 1], buffer);
                }

                execute_command(buffer);
            }
        }

        free(buffer); // Free the allocated memory for the buffer
        return 0;
    }
}
