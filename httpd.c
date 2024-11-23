#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_REQUEST_SIZE 1024
#define MAX_RESPONSE_SIZE 2048

// Function to send an error response
void send_error_response(int client_socket, int error_code) {
    char *error_message;
    switch (error_code) {
        case 404: error_message = "Not Found"; break;
        case 400: error_message = "Bad Request"; break;
        default: error_message = "Internal Server Error"; break;
    }

    char response[MAX_RESPONSE_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.0 %d %s\r\n"
             "Content-Type: text/plain\r\n"
             "Content-Length: %zu\r\n\r\n"
             "%s",
             error_code, error_message, strlen(error_message), error_message);
    write(client_socket, response, strlen(response));
}

// Function to serve static files
void serve_file(int client_socket, const char *filename) {
    char file_path[512];
    snprintf(file_path, sizeof(file_path), ".%s", filename);

    FILE *file = fopen(file_path, "r");
    if (!file) {
        send_error_response(client_socket, 404);
        return;
    }

    fseek(file, 0, SEEK_END);
    long content_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char response[MAX_RESPONSE_SIZE];
    snprintf(response, sizeof(response),
             "HTTP/1.0 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n\r\n", content_length);
    write(client_socket, response, strlen(response));

    char buffer[MAX_REQUEST_SIZE];
    while (fgets(buffer, sizeof(buffer), file)) {
        write(client_socket, buffer, strlen(buffer));
    }

    fclose(file);
}

// Function to execute commands and return their output
void execute_command(int client_socket, const char *url, const char *query) {
    char command_path[512];
    snprintf(command_path, sizeof(command_path), ".%s", url);

    // Check if the file exists and is executable
    if (access(command_path, X_OK) != 0) {
        send_error_response(client_socket, 404);
        return;
    }

    // Fork a process to execute the command
    pid_t pid = fork();
    if (pid < 0) {
        send_error_response(client_socket, 500);
        return;
    } else if (pid == 0) {
        // Child process: execute the command
        // Redirect output to a temporary file
        char temp_file[256];
        snprintf(temp_file, sizeof(temp_file), "/tmp/cgi_output_%d.txt", getpid());

        int temp_fd = open(temp_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (temp_fd < 0) {
            perror("Error opening temp file");
            exit(1);
        }
        dup2(temp_fd, STDOUT_FILENO); // Redirect stdout
        dup2(temp_fd, STDERR_FILENO); // Redirect stderr
        close(temp_fd);

        // Build arguments for execv
        char *args[64];
        args[0] = "/bin/bash";  // Specify bash as the interpreter
        args[1] = "-c";         // Use the -c option to pass a command to bash
        args[2] = "cd /Users/giannischiappa/cpe357/357-assignment-5-GianniSchiappa37/cgi-like && ls"; // Full path to cgi-like directory
        args[3] = NULL;         // End of arguments

        execv("/bin/bash", args);  // Execute using bash
        perror("execv failed");    // Log error if execv fails
        exit(1);
    } else {
        // Parent process: wait for the child to finish
        int status;
        waitpid(pid, &status, 0);

        // Read the output from the temporary file
        char temp_file[256];
        snprintf(temp_file, sizeof(temp_file), "/tmp/cgi_output_%d.txt", pid);

        FILE *temp_fp = fopen(temp_file, "r");
        if (!temp_fp) {
            send_error_response(client_socket, 500);
            return;
        }

        // Determine file size
        fseek(temp_fp, 0, SEEK_END);
        long content_length = ftell(temp_fp);
        fseek(temp_fp, 0, SEEK_SET);

        // Send response header
        char response_header[MAX_RESPONSE_SIZE];
        snprintf(response_header, sizeof(response_header),
                 "HTTP/1.0 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: %ld\r\n\r\n",
                 content_length);
        write(client_socket, response_header, strlen(response_header));

        // Send the file contents
        char buffer[MAX_REQUEST_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), temp_fp)) > 0) {
            write(client_socket, buffer, bytes_read);
        }

        fclose(temp_fp);
        remove(temp_file); // Clean up temporary file
    }
}




// Main function to set up the server
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    short port = atoi(argv[1]);

    // Create server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error opening socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(1);
    }

    listen(server_socket, 5);
    printf("Server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }

        char request[MAX_REQUEST_SIZE];
        int received = read(client_socket, request, sizeof(request) - 1);
        if (received <= 0) {
            close(client_socket);
            continue;
        }

        request[received] = '\0';

        // Parse the request
        char *method = strtok(request, " ");
        char *url = strtok(NULL, " ");
        if (url == NULL) {
            send_error_response(client_socket, 400);
            close(client_socket);
            continue;
        }

        // Check for query string in the URL
        char *query = strchr(url, '?');
        if (query) {
            *query = '\0';  // Null-terminate the URL
            query++;        // Move to the query part
        }

        // Determine if it's a static file or command
        if (strncmp(url, "/cgi-like/", 10) == 0) {
            if (query) {
                execute_command(client_socket, url, query);
            } else {
                serve_file(client_socket, url);
            }
        } else if (strncmp(url, "/", 1) == 0) {
            serve_file(client_socket, url);
        } else {
            send_error_response(client_socket, 400);
        }

        close(client_socket);
    }

    close(server_socket);
    return 0;
}