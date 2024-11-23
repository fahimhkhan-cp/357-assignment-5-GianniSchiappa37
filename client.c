#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#define PORT 4000  // Match the new port in the server

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

void communicate_with_server(int fd)
{
    char *line = NULL;
    size_t size;
    ssize_t num;

    char buffer[1024];
    ssize_t bytes_received;

    while ((num = getline(&line, &size, stdin)) >= 0)
    {
        // Send the input to the server
        write(fd, line, num);

        // Read the echoed response from the server
        bytes_received = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';  // Null-terminate the received data
            printf("Server response: %s", buffer);
        }
        else
        {
            perror("read");
            break;
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
            communicate_with_server(fd);
            close(fd);
        }
    }

    return 0;
}
