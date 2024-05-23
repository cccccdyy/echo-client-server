#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define BUF_MAX 4096
pthread_mutex_t socket_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct _client {
    int socket;
    char buffer[BUF_MAX];
}client, *pclient; 

void usage() {
    printf("syntax : echo-client <ip> <port>\n");
    printf("sample : echo-client 192.168.10.2 1234\n");
}

void* recv_message (void* arg) {
    pclient clientptr = (pclient)arg;
    while (true) {
        int value = read(clientptr->socket, clientptr->buffer, BUF_MAX - 1);
        if (value < 0) {
            perror("[ERROR] read() Failed\n");
            exit(EXIT_FAILURE);
        }
        else if (value == 0) {
            printf("[ERROR] Server Disconnected\n");
            exit(EXIT_FAILURE);
        }
        else printf("%s", clientptr->buffer);
    }
}

int main (int argc, char* argv[]) {

    if (argc != 3) {
        usage();
        return 0;
    }
    int port = atoi(argv[2]);
    int client_socket = 0;
    struct sockaddr_in server_addr;
    char message[BUF_MAX] = {0};

    // socket create
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[ERROR] socket() Failed\n");
        exit(EXIT_FAILURE);
    }

    // set server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        printf("[ERROR] Invalid address/ Address not supported \n");
        exit(EXIT_FAILURE);
    }

    // connect
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[ERROR] connect() Failed \n");
        exit(EXIT_FAILURE);
    }

    // receive from server
    pthread_t recv_thread;
    pclient clientptr = (pclient)malloc(sizeof(client));
    clientptr->socket = client_socket;
    if (pthread_create(&recv_thread, NULL, recv_message, (void*)clientptr)) {
        perror("[ERROR] pthread_create() Failed\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // send message 
    while (true) {
        memset(message, 0, BUF_MAX);
        read(0, message, BUF_MAX - 1);
        pthread_mutex_lock(&socket_mutex);
        send(client_socket, message, strlen(message), 0);
        pthread_mutex_unlock(&socket_mutex);
    }

    // socket close
    close(client_socket);
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL); 

    return 0;
}