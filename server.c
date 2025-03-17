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
#define BUFFER_SIZE 1024

// Signal handler for SIGCHLD
void sigchld_handler(int s)
{
   // Wait for all child processes that have terminated
   while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Function to handle client requests
void handle_request(int client_fd)
{
   FILE *network = fdopen(client_fd, "r+");
   char *line = NULL;
   size_t size;
   ssize_t num;

   if (network == NULL)
   {
      perror("fdopen");
      close(client_fd);
      return;
   }

   if ((num = getline(&line, &size, network)) >= 0)
   {
      // Process the request
      char *token = strtok(line, " ");
      if (token != NULL && strcmp(token, "GET") == 0)
      {
         token = strtok(NULL, " ");
         if (token != NULL)
         {
            // Remove trailing newline
            size_t len = strlen(token);
            if (len > 0 && token[len - 1] == '\n')
            {
               token[len - 1] = '\0';
            }

            FILE *file = fopen(token, "r");
            if (file != NULL)
            {
               // Send "Server returns: " prefix first
               char prefix[] = "Server returns: ";
               write(client_fd, prefix, strlen(prefix));

               // Send file contents
               char file_buffer[BUFFER_SIZE];
               size_t file_num;
               while ((file_num = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
               {
                  write(client_fd, file_buffer, file_num);
               }
               fclose(file);
            } 
            else
            {
               char *error_message = "ERROR: File Not Found\n";
               write(client_fd, error_message, strlen(error_message));
               perror("fopen");
            }
         }
      } 
      else
      {
         char *error_message = "ERROR: Invalid Request\n";
         write(client_fd, error_message, strlen(error_message));
      }
   }

   free(line);
   fclose(network);
   close(client_fd); // Close connection after handling request
   exit(0); // Ensure the child process exits
}

// Function to run the server and accept clients
void run_service(int fd)
{
   struct sigaction sa;
   memset(&sa, 0, sizeof(sa)); // Zero out the sigaction struct

   sa.sa_handler = sigchld_handler;
   sa.sa_flags = SA_RESTART; // Restart interrupted system calls

   // Register SIGCHLD handler
   if (sigaction(SIGCHLD, &sa, NULL) == -1)
   {
      perror("sigaction");  // Print error message
      exit(1);
   }

   while (1)
   {
      int client_fd = accept_connection(fd);
      if (client_fd != -1) {
         printf("Connection established\n");

         // Fork a child process to handle the client request
         pid_t pid = fork();
         if (pid == 0)
         {
            // Child process
            close(fd); // Close server socket in child
            handle_request(client_fd);
            exit(0); // Ensure child terminates
         }
         else if (pid > 0)
         {
            // Parent process
            close(client_fd); // Parent does not need this socket
         } 
         else
         {
            perror("fork failed");
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

