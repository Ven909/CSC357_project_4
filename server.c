#define _GNU_SOURCE
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT 2829 // Updated port number
#define BUFFER_SIZE 1024   // Buffer size for reading files

// Signal handler for SIGCHLD
void sigchld_handler(int s)
{
   // Wait for all child processes that have terminated
   while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Function to handle client requests
void handle_request(int client_fd)
{
   FILE *network = fdopen(client_fd, "r"); // Open file stream for reading  
   char *line = NULL;
   size_t size;
   ssize_t num;

   // Check if file stream was created successfully
   if (network == NULL)
   {
      perror("fdopen"); // Print error message
      close(client_fd); // Close connection
      return;
   }

   // Read request from client
   if ((num = getline(&line, &size, network)) >= 0)
   {
      // Process the request
      char *token = strtok(line, " ");
      
      // Check if request is a GET request
      if (token != NULL && strcmp(token, "GET") == 0)
      {
         token = strtok(NULL, " "); // Get the file name
         
         // Check if file name is not NULL
         if (token != NULL)
         {
            char *extra_token = strtok(NULL, " "); // Check for extra argument
            if (extra_token != NULL)   // Extra argument detected
            {
               char error[] = "ERROR: Invalid Request\n";   // Error message
               write(client_fd, error, strlen(error));   // Send error message to client
               free(line);
               fclose(network);
               close(client_fd);
               printf("Connection closed\n");
               exit(0);
            }  
            
            // Remove trailing newline
            size_t len = strlen(token);
            if (len > 0 && token[len - 1] == '\n')
            {
               token[len - 1] = '\0';  // Replace newline with null terminator
            }

            FILE *file = fopen(token, "r");  // Open file for reading
            
            // Check if file was opened successfully   
            if (file != NULL)
            {
               // Send "Server returns: " prefix first
               char prefix[] = "Server returns: ";
               write(client_fd, prefix, strlen(prefix)); // Send prefix to client

               // Send file contents
               char file_buffer[BUFFER_SIZE];   // Buffer to read file contents
               size_t file_num;  // Number of bytes read from file    
               
               // Read file contents and send to client   
               while ((file_num = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
               {
                  write(client_fd, file_buffer, file_num);  // Send file contents to client
                  write(STDOUT_FILENO, file_buffer, file_num); // Print to stdout
               }
               fclose(file);  // Close file after reading
            }
            else  // File not found
            {
               char *error_message = "ERROR: File Not Found\n";   // Error message
               write(client_fd, error_message, strlen(error_message));  // Send error message to client
               perror("fopen");  // Print error message
            }
         }
      } 
      else  // Invalid request
      {
         char *error_message = "ERROR: Invalid Request\n";  // Error message
         write(client_fd, error_message, strlen(error_message));  // Send error message to client    
      }
   }

   free(line); // Free memory allocated by getline
   fclose(network);  // Close file stream
   close(client_fd); // Close connection after handling request
   printf("Connection closed\n"); // Print connection closed message
   exit(0); // Ensure the child process exits
}

// Function to run the server and accept clients
void run_service(int fd)
{
   struct sigaction sa; // Signal action struct
   memset(&sa, 0, sizeof(sa)); // Zero out the sigaction struct

   sa.sa_handler = sigchld_handler;    // Set signal handler
   sa.sa_flags = SA_RESTART; // Restart interrupted system calls

   // Register SIGCHLD handler
   if (sigaction(SIGCHLD, &sa, NULL) == -1)
   {
      perror("sigaction");  // Print error message
      exit(1);
   }

   // Accept client connections
   while (1)
   {
      int client_fd = accept_connection(fd); // Accept client connection
      
      // Check if connection was established
      if (client_fd != -1)
      {
         printf("Connection established\n"); // Print success message

         // Fork a child process to handle the client request
         pid_t pid = fork();
         
         // Child process
         if (pid == 0)
         {
            close(fd);                 // Close server socket in child
            handle_request(client_fd); // Handle client request
            exit(0);                   // Ensure child terminates
         }
         else if (pid > 0) // Parent process
         {
            close(client_fd); // Parent does not need this socket
         } 
         else  // Fork failed
         {
            perror("fork failed");  // Print error message
         }
      }
   }
}

int main(void)
{
   int fd = create_service(PORT);

   if (fd == -1)
   {
      perror("Server setup failed");
      exit(1);
   }

   printf("Listening on port: %d\n", PORT);
   run_service(fd);
   close(fd);

   return 0;
}

