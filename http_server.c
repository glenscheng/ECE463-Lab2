/* The code is subject to Purdue University copyright policies.
 * DO NOT SHARE, DISTRIBUTE, OR POST ONLINE
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>

#define LISTEN_QUEUE 50 /* Max outstanding connection requests; listen() param */

#define DBADDR "127.0.0.1"

#define CHUNK_SIZE 4096  // Chunk size for reading files

char *strremove(char *str, const char *sub) {
    size_t len = strlen(sub);
    if (len > 0) {
        char *p = str;
        while ((p = strstr(p, sub)) != NULL) {
            memmove(p, p + len, strlen(p + len) + 1);
        }
    }
    return str;
}

// Print to termianl
void print_terminal(struct sockaddr_in clientAddr, char *request_method, char *request_uri, char *http_version, char *status_code) {
  http_version = strremove(http_version, "\r");
  printf("%s \"%s %s %s\" %s\n", inet_ntoa(clientAddr.sin_addr), request_method, request_uri, http_version, status_code);
}

// Function to handle client requests
void handle_request(int clientSock, struct sockaddr_in clientAddr) {
  char buffer[BUFSIZ];
  ssize_t bytesReceived;
  char response[BUFSIZ];
  char *request_method, *request_uri, *http_version;

  // Receive request from client
  bytesReceived = recv(clientSock, buffer, BUFSIZ, 0);
  if (bytesReceived < 0) {
    perror("Error receiving request");
    close(clientSock);
    return;
  }

  buffer[bytesReceived] = '\0'; // Null-terminate the received data

  // Parse the received request (HTTP request format: METHOD URI HTTP_VERSION)
  request_method = strtok(buffer, " \t\n");
  request_uri = strtok(NULL, " \t");
  http_version = strtok(NULL, " \t\n");

  // Check if the request is valid
  if (request_method == NULL || request_uri == NULL || http_version == NULL) {
    sprintf(response, "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>");
    send(clientSock, response, strlen(response), 0);
    close(clientSock);
    char status_code[BUFSIZ] = "400 Bad Request";
    print_terminal(clientAddr, request_method, request_uri, http_version, status_code);
    return;
  }

  // Handle only GET requests
  if (strcmp(request_method, "GET") != 0) {
    sprintf(response, "HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>");
    send(clientSock, response, strlen(response), 0);
    close(clientSock);
    char status_code[BUFSIZ] = "501 Not Implemented";
    print_terminal(clientAddr, request_method, request_uri, http_version, status_code);
    return;
  }

  // Check if URI starts with '/'
  if (request_uri[0] != '/') {
    sprintf(response, "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>");
    send(clientSock, response, strlen(response), 0);
    close(clientSock);
    char status_code[BUFSIZ] = "400 Bad Request";
    print_terminal(clientAddr, request_method, request_uri, http_version, status_code);
    return;
  }

  // Check for security risks in the URI (e.g., '/../' or '/..')
  if (strstr(request_uri, "/../") != NULL || strcmp(&request_uri[strlen(request_uri) - 3], "/..") == 0) {
    sprintf(response, "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>");
    send(clientSock, response, strlen(response), 0);
    close(clientSock);
    char status_code[BUFSIZ] = "400 Bad Request";
    print_terminal(clientAddr, request_method, request_uri, http_version, status_code);
    return;
  }

  // Remove trailing '/' if present
  if (request_uri[strlen(request_uri) - 1] == '/' && strlen(request_uri) > 1) {
    request_uri[strlen(request_uri) - 1] = '\0';
  }

  // Append 'index.html' if the URI points to a directory without a '/'
  struct stat path_stat;
  if (stat(&request_uri[1], &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
    strcat(request_uri, "/index.html");
  }

  // Adjust the request URI to prepend the 'Webpage' directory if necessary
  char filepath[BUFSIZ] = "Webpage";
  strcat(filepath, request_uri);

  // Open and read the requested file
  FILE *file = fopen(filepath, "rb");
  if (file == NULL) {
    sprintf(response, "HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found<h1></body></html>");
    send(clientSock, response, strlen(response), 0);
    close(clientSock);
    char status_code[BUFSIZ] = "404 Not Found";
    print_terminal(clientAddr, request_method, request_uri, http_version, status_code);
    return;
  }

  // Send HTTP response headers
  sprintf(response, "HTTP/1.0 200 OK\r\n\r\n");
  send(clientSock, response, strlen(response), 0);

  // Read and send file contents in chunks
  size_t bytesRead;
  char fileChunk[CHUNK_SIZE];
  while ((bytesRead = fread(fileChunk, 1, CHUNK_SIZE, file)) > 0) {
    send(clientSock, fileChunk, bytesRead, 0);
  }

  fclose(file);

  // Log the request to terminal
  /*
  char print_string[BUFSIZ];
  strcat(print_string, inet_ntoa(clientAddr.sin_addr));
  strcat(print_string, " \"");
  strcat(print_string, request_method);
  strcat(print_string, " ");
  strcat(print_string, request_uri);
  strcat(print_string, " ");
  strcat(print_string, http_version);
  strcat(print_string, "\" 200 OK");
  printf("%s\n", print_string);
  //printf("%s\n", inet_ntoa(clientAddr.sin_addr));
  //printf("%s ", request_method);
  //printf("%s ", request_uri);
  //printf("%s ", http_version);
  //printf("200 OK\n");
  */
  //http_version = strremove(http_version, "\r");
  //printf("%s \"%s %s %s\" 200 OK\n", inet_ntoa(clientAddr.sin_addr), request_method, request_uri, http_version);
  char status_code[BUFSIZ] = "200 OK";
  print_terminal(clientAddr, request_method, request_uri, http_version, status_code);


  close(clientSock);
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(stderr, "usage: ./http_server [server port] [DB port]\n");
    exit(1);
  }

  int servPort = atoi(argv[1]);
  int dbPort = atoi(argv[2]);

  // Create a socket
  int servSock = socket(AF_INET, SOCK_STREAM, 0);
  if (servSock < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Prepare the server address structure
  struct sockaddr_in servAddr;
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(servPort);

  // Bind the socket
  if (bind(servSock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
    perror("Binding failed");
    exit(EXIT_FAILURE);
  }

  // Listen for incoming connections
  if (listen(servSock, LISTEN_QUEUE) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  // Accept incoming connections
  struct sockaddr_in clientAddr;
  socklen_t clientAddrLen = sizeof(clientAddr);
  int clientSock;

  while (1) {
    // Accept a client connection
    clientSock = accept(servSock, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientSock < 0) {
      perror("Accept failed");
      exit(EXIT_FAILURE);
    }

    // Handle client request
    handle_request(clientSock, clientAddr);
  }

  close(servSock); // Close server socket

  return 0;
}
