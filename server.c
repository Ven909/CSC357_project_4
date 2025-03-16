#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT 2829 // Changed port number

// Signal handler for SIGCHLD
void sigchld_handler(int s)
{
   // Wait for all child processes that have terminated
   while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_request(int nfd)
{
   FILE *network = fdopen(nfd, "r+");
   char *line = NULL;
   size_t size;
   ssize_t num;

   if (network == NULL)
   {
      perror("fdopen");
      close(nfd);
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
               char file_buffer[1024];
               size_t file_num;
               while ((file_num = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0)
               {
                  write(nfd, file_buffer, file_num);
               }
               fclose(file);
            } 
            else
            {
               char *error_message = "HTTP 404 File Not Found";
               write(nfd, error_message, strlen(error_message));
               perror("fopen");
            }
         }
      } 
      else
      {
         char *error_message = "HTTP 400 Invalid Request";
         write(nfd, error_message, strlen(error_message));
      }
   }

   free(line);
   fclose(network);
   close(nfd); // Close the connection after handling the request
   exit(0); // Child process exits
}

void run_service(int fd)
{
   struct sigaction sa;

   // Zero-initialize the sigaction struct
   memset(&sa, 0, sizeof(sa));

   // Set up the signal handler
   sa.sa_handler = sigchld_handler;

   sa.sa_flags = SA_RESTART;  // Restart interrupted system calls
   
   // Register the signal handler
   if (sigaction(SIGCHLD, &sa, NULL) == -1)
   {
      perror("sigaction");  // Print error message
      exit(1);
   }

   // Loop to accept incoming connections
   while (1)
   {
      int nfd = accept_connection(fd);
      if (nfd != -1)
      {
         printf("Connection established\n");
         
         pid_t pid = fork();  
         // Fork a child process to handle the request
         if (pid == 0) 
         {
            close(fd); // Child doesn't need the listener
            handle_request(nfd);
            exit(0); // Ensure child process terminates
         } 
         else if (pid > 0)
         {
            close(nfd); // Parent doesn't need the connection
         } 
         else
         {
            perror("fork failed");  // Print error message if fork fails
         }
      }
   }
}

int main(void)
{
   int fd = create_service(PORT);

   if (fd == -1)
   {
      perror(0);
      exit(1);
   }

   printf("listening on port: %d\n", PORT);
   run_service(fd);
   close(fd);

   return 0;
}
