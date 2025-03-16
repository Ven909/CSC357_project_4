#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#define PORT 2829 // Changed port number
#define MIN_ARGS 2
#define MAX_ARGS 2
#define SERVER_ARG_IDX 1

#define USAGE_STRING "usage: %s <server address>\n"

void validate_arguments(int argc, char *argv[])
{
    if (argc == 0)
    {
        fprintf(stderr, USAGE_STRING, "client");
        exit(EXIT_FAILURE);
    }
    else if (argc < MIN_ARGS || argc > MAX_ARGS)
    {
        fprintf(stderr, USAGE_STRING, argv[0]);
        exit(EXIT_FAILURE);
    }
}

void send_request(int fd)
{
   char *line = NULL;
   size_t size;
   ssize_t num;

   while ((num = getline(&line, &size, stdin)) >= 0)
   {
      // Remove trailing newline
      size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n')
      {
         line[len - 1] = '\0';
      }

      // Construct the HTTP GET request
      char request[1024]; // Increased buffer size
      snprintf(request, sizeof(request), "GET %s\n", line); // Added GET and newline

      write(fd, request, strlen(request));

      // Read and print the response from the server
      char buffer[1024];
      ssize_t bytes_read;
      while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
      {
         // null termination necessary or not?
         write(STDOUT_FILENO, buffer, bytes_read);
      }
   }

   free(line);
}

int connect_to_server(struct hostent *host_entry)
{
   int fd;
   struct sockaddr_in their_addr;

   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      return -1;
   }

   their_addr.sin_family = AF_INET;
   their_addr.sin_port = htons(PORT);
   their_addr.sin_addr = *((struct in_addr *)host_entry->h_addr);

   if (connect(fd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
   {
      close(fd);
      perror(0);
      return -1;
   }

   return fd;
}

struct hostent *gethost(char *hostname)
{
   struct hostent *he;

   if ((he = gethostbyname(hostname)) == NULL)
   {
      herror(hostname);
   }

   return he;
}

int main(int argc, char *argv[])
{
   validate_arguments(argc, argv);
   struct hostent *host_entry = gethost(argv[SERVER_ARG_IDX]);

   if (host_entry)
   {
      int fd = connect_to_server(host_entry);
      if (fd != -1)
      {
         send_request(fd);
         close(fd);
      }
   }

   return 0;
}
