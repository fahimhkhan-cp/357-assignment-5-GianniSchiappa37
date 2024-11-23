#define _GNU_SOURCE
#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define PORT 4000  // Use a different port

void handle_request(int nfd)
{
    char buffer[1024];
    ssize_t bytes_read;

    while ((bytes_read = read(nfd, buffer, sizeof(buffer))) > 0)
    {
        // Echo the data back to the client
        write(nfd, buffer, bytes_read);
    }

    if (bytes_read == -1)
    {
        perror("read");
    }

    close(nfd);  // Close the connection
}

void run_service(int fd)
{
    while (1)
    {
        int nfd = accept_connection(fd);
        if (nfd != -1)
        {
            printf("Connection established\n");
            handle_request(nfd);
            printf("Connection closed\n");
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

    printf("Listening on port: %d\n", PORT);
    run_service(fd);
    close(fd);

    return 0;
}
