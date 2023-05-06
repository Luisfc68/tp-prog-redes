#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define OUT_BUFFER_SIZE 2048
#define IN_BUFFER_SIZE 256
#define MAX_INT_LENGTH 11 // max int es 2147483647

int main(int argc, const char** argv) {
    
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("./client <dns> <server-port> <message>\n");
        return 0;
    } else if (argc != 4 || strlen(argv[3]) > OUT_BUFFER_SIZE || atoi(argv[2]) <= 0) {
        printf("bad parameters\n");
        return -1;
    }
     
    struct addrinfo hints, *res;   
    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM;


    getaddrinfo(argv[1], argv[2], &hints, &res); 

    int server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    int error = connect(server_fd, res->ai_addr, res->ai_addrlen);
    if (error != 0) {
        printf("error connecting to server\n");
        return -1;
    }

    char out_buffer[OUT_BUFFER_SIZE];
    strcpy(out_buffer, argv[3]);
    printf("%d %s\n",server_fd, out_buffer);
    int sent = send(server_fd, out_buffer, OUT_BUFFER_SIZE, 0);
    if (sent == -1) {
        printf("Error sending message\n");
    } else {
        printf("Message sent (%db)\n", sent);
    }

    char in_buffer[IN_BUFFER_SIZE];
    memset(in_buffer,0, IN_BUFFER_SIZE);
    int received = recv(server_fd, &in_buffer, IN_BUFFER_SIZE, 0);
    if (received == -1) {
        printf("Error receiving response\n");
    } else {
        printf("Server response (%db): %s\n", received, in_buffer);
    }
    
    close(server_fd);
    return 0;
}