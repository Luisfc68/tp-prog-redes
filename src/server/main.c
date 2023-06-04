#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define DEFAULT_PORT 8080
#define QUEUE_SIZE 1024
#define MAX_INT_LENGTH 11 // max int es 2147483647
#define IN_BUFFER_SIZE 2048
#define OUT_BUFFER_SIZE 256

typedef struct client_context  {
    int fd;
    struct addrinfo* address;
} client;

char* int_to_string(int number);
int create_server(int port);
void serve_client(client* request);
void* handle_request(void* args);
void on_ping_request(pthread_t thread,client* request);
void on_other_request(pthread_t thread, client* request, char* content);
void read_request_message(client* request, pthread_t thread, char** request_content);
void merge_buffers(char** buffers, int number_of_buffers, char** result, int result_size);


int main(int argc, const char** argv) {
    
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("./server <port>\n");
        return 0;
    }

    int port;
    if (argc > 1 && (port = atoi(argv[1])) != 0) {
        printf("Using port :%d\n", port);
    } else {
        printf("Using default port :%d\n", DEFAULT_PORT);
        port = DEFAULT_PORT;
    }
     
    
    int server_fd = create_server(port);
    if (server_fd == -1) {
        return -1;
    }
    
    listen(server_fd, QUEUE_SIZE);
    
    client* client_info;
    socklen_t size = sizeof(struct sockaddr);
   
    while (1) {
        client_info = malloc(sizeof(client_info));
        client_info->address = malloc(size);
        client_info->fd = accept(
            server_fd, 
            (struct sockaddr*)client_info->address, 
            (socklen_t *__restrict__)&size // el __restrict__ es para sacar el warning
        );
        serve_client(client_info);
    }
    

    return 0;
}

char* int_to_string(int number) {
    char* string = (char*)malloc(MAX_INT_LENGTH); 
    sprintf(string, "%d", number);
    return string;
}

int create_server(int port) {
    struct addrinfo* config;
    config = malloc(sizeof(struct addrinfo));
    memset(config, 0, sizeof(struct addrinfo));

    config->ai_family   = AF_UNSPEC;
    config->ai_socktype = SOCK_STREAM;
    config->ai_flags    = AI_PASSIVE;

    getaddrinfo(NULL, int_to_string(port), config, &config);

    int server_fd = socket(config->ai_family, config->ai_socktype, config->ai_protocol);
    int error = bind(server_fd, config->ai_addr, config->ai_addrlen);
    freeaddrinfo(config);
    if (error != 0 || server_fd == -1) {
        printf("Error binding socket\n");
        return -1;
    }
    return server_fd;
}


void serve_client(client* request) {
    pthread_t request_thread;
    pthread_attr_t 	attributes;
    if (request->fd == -1) {
        printf("Error accepting client with file descriptor %d\n", request->fd);
    }
    pthread_attr_init (&attributes);
	pthread_attr_setdetachstate (&attributes, PTHREAD_CREATE_DETACHED);

    int error = pthread_create(&request_thread, NULL, handle_request, request);
    if (error != 0) {
        printf("Error handling client request for client with file descriptor %d\n", request->fd);
    }
}

void* handle_request(void* args) {
    client* request = (client*)args;
    pthread_t thread = pthread_self();
    char* request_content;

    read_request_message(request, thread, &request_content);

    if (strcmp("PING", request_content) == 0) {
        on_ping_request(thread, request);
    } else {
        printf("Waiting 5 seconds to respond unknown request\n");
        sleep(5);
        on_other_request(thread, request, request_content);
    }

    free(request->address);
    free(request);
    free(request_content);
    close(request->fd);
    return NULL;
}

void on_ping_request(pthread_t thread, client *request) {
    printf("[Thread %lu] Ping request received\n", thread);
    char *response = "PONG";
    int sent = send(request->fd, response, OUT_BUFFER_SIZE, 0);
    if (sent == -1) {
        printf("[Thread %lu] Error sending response\n", thread);
    }
}

void on_other_request(pthread_t thread, client *request, char *content) {
    printf("[Thread %lu] Unknown request received. Content: %s\n", thread, content);
    int sent = send(request->fd, "?", OUT_BUFFER_SIZE, 0);
    if (sent == -1) {
        printf("[Thread %lu] Error sending response\n", thread);
    }
}

void read_request_message(client* request, pthread_t thread, char** request_content) {
    ssize_t received_bytes;
    char* buffers[IN_BUFFER_SIZE];
    char buffer[IN_BUFFER_SIZE];
    int buffers_received = 0, total_received = 0, real_data_length;

    memset(buffer,0, IN_BUFFER_SIZE);
    received_bytes = recv(request->fd, &buffer, IN_BUFFER_SIZE, 0);
    real_data_length = strlen(buffer);

    do {
        if (received_bytes != 0 && received_bytes != -1) {
            buffers[buffers_received] = malloc(received_bytes);
            memcpy(buffers[buffers_received], buffer, received_bytes);
            buffers_received++;
            total_received += real_data_length;
        }
        if (real_data_length == IN_BUFFER_SIZE) {
            received_bytes = recv(request->fd, &buffer, IN_BUFFER_SIZE, 0);
            real_data_length = strlen(buffer);
        }
    } while(real_data_length == IN_BUFFER_SIZE);
    
    if (received_bytes == -1) {
       printf("[Thread %lu] Error receiving bytes\n",thread); 
    } else {
        merge_buffers(buffers, buffers_received, request_content, total_received);
    }
}

void merge_buffers(char** buffers, int number_of_buffers, char** result, int result_size) {
    *result = malloc(result_size);
    memset(*result, 0, result_size);
    for (int i = 0; i < number_of_buffers; i++) {
        strcat(*result, buffers[i]);
        free(buffers[i]); 
    }
}