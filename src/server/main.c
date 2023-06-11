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
#include <regex.h> 
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>

#define DEFAULT_PORT 8080
#define QUEUE_SIZE 1024
#define MAX_LONG_LENGTH 11 // max long es 2147483647
#define IN_BUFFER_SIZE 2048
#define OUT_BUFFER_SIZE 256
#define MAX_URL_PATH 2048

typedef struct client_context  {
    int fd;
    struct addrinfo* address;
} client;

typedef struct http_request_line {
    char method[16];
    char path[MAX_URL_PATH];
    char version[16];
} http_request_line;

char* int_to_string(int number);
int create_server(int port);
void serve_client(client* request);
void* serve_request(void* args);
void on_ping_request(pthread_t thread,client* request);
void on_other_request(pthread_t thread, client* request, char* content);
void read_request_message(client* request, pthread_t thread, char** request_content);
void merge_buffers(char** buffers, int number_of_buffers, char** result, int result_size);
int is_http(char *msg);
http_request_line* extract_request_line(char* content);
void on_http_request(pthread_t thread, client* request, char* content);
void handle_image_request(pthread_t thread, client* request);
void handle_other_request(pthread_t thread, client* request);



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
    char* string = (char*)malloc(MAX_LONG_LENGTH); 
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

    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) { 
	    printf("Error binding socket\n");
	    return -1; 
    } 

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

    int error = pthread_create(&request_thread, NULL, serve_request, request);
    if (error != 0) {
        printf("Error handling client request for client with file descriptor %d\n", request->fd);
    }
}

int is_http(char *msg) {
    char* msg_copy = malloc(strlen(msg));
    strcpy(msg_copy, msg);
    char* request_line = strtok(msg_copy, "\r\n");
    regex_t regex;
    int err;
    err  = regcomp(&regex, "^(GET|POST|PUT|DELETE) [^ ]+ HTTP/[0-9](\\.[0-9])?$", REG_EXTENDED);
    if (err != 0) {
        return err;
    }
    err = regexec(&regex, request_line, 0, NULL, 0);
    regfree(&regex);
    free(msg_copy);
    return err;
} 

void* serve_request(void* args) {
    client* request = (client*)args;
    pthread_t thread = pthread_self();
    char* request_content;

    read_request_message(request, thread, &request_content);

    if (strcmp("PING", request_content) == 0) {
        on_ping_request(thread, request);
    } else if (is_http(request_content) == 0) {
        on_http_request(thread, request, request_content);
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

void on_http_request(pthread_t thread, client* request, char* content) {
    http_request_line* rq_line = extract_request_line(content);
    printf("[Thread %lu] %s - %s\n", thread, rq_line->method, rq_line->path);

    if (strcmp(rq_line->method, "GET") == 0 && strcmp(rq_line->path, "/") == 0) {
        handle_image_request(thread, request);
    } else {
        handle_other_request(thread, request);
    }

    if (rq_line != NULL) {
        free(rq_line);
    }
}

void handle_image_request(pthread_t thread, client* request) {
    char *response, *response_headers;
    int headers_size;
    struct stat image_stats;
    int image = open(getenv("IMAGE_PATH"), O_RDONLY);
    fstat(image, &image_stats);
    response = "HTTP/1.1 200 Ok\r\nContent-Type: image/jpeg\r\nContent-Length: %ld\r\nConnection: Close\r\n\r\n";
    response_headers = malloc(strlen(response) + MAX_LONG_LENGTH);
    sprintf(response_headers, response, image_stats.st_size);
    headers_size = strlen(response_headers);
    int headers_sent = send(request->fd, response_headers, headers_size, 0);
    int image_sent = sendfile(request->fd, image, NULL, image_stats.st_size);
    if (headers_sent == -1 || image_sent == -1) {
        printf("[Thread %lu] Error sending image\n",thread); 
    }
    free(response_headers);
    close(image);
}

void handle_other_request(pthread_t thread, client* request) {
    char *response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\nContent-Length: 107\r\nConnection: Close\r\n\r\n<html><body><h1>Unsupported request</h1><p>This server only supports GET method on / path</p></body></html>";
    int response_size = strlen(response);
    int sent = send(request->fd, response, response_size, 0);
    if (sent == -1) {
        printf("[Thread %lu] Error sending response\n",thread); 
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

http_request_line* extract_request_line(char* content) {
    char* content_copy = malloc(strlen(content));
    strcpy(content_copy, content);
    char* token = strtok(content_copy, "\r\n");
    char* raw_line = malloc(strlen(token));
    strcpy(raw_line, token);
    http_request_line* request_line = malloc(sizeof(http_request_line));

    
    raw_line = strtok(raw_line, " ");
    strcpy(request_line->method, raw_line);
    raw_line = strtok(NULL, " ");
    strcpy(request_line->path, raw_line);
    raw_line = strtok(NULL, " ");
    strcpy(request_line->version, raw_line);

    free(content_copy);
    return request_line;
}