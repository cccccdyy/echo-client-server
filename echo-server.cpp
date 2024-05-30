#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>

#define BUF_MAX 4096
#define MAX_CLIENT 10
bool echo;
bool broadcast;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct _client {
    pthread_t thread;
    int socket;
    char buffer[BUF_MAX];
}client, *pclient; 

bool flag[MAX_CLIENT] = {0};
pclient clients[MAX_CLIENT];

void usage () {
    printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
}

void bcast (char* buffer, size_t size) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENT; i++) {
        if (flag[i] == false) continue;
        send(clients[i]->socket, buffer, size, 0);
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client (void* arg) {
    pclient clientptr = (pclient)arg;
    while (true) {
        memset(clientptr->buffer, 0, BUF_MAX);
        int value = read(clientptr->socket, clientptr->buffer, BUF_MAX - 1);
        if (value < 0) {
            perror("[ERROR] read() Failed");
            break;
        }
        else if (value == 0) break;
        printf("%s", clientptr->buffer);
        if (broadcast) bcast(clientptr->buffer, BUF_MAX - 1);
        else if (echo) {
            pthread_mutex_lock(&clients_mutex);
            send(clientptr->socket, clientptr->buffer, BUF_MAX - 1, 0);
            pthread_mutex_unlock(&clients_mutex);
        }
        else {
            pthread_mutex_lock(&clients_mutex);
            send(clientptr->socket, " ", 1, 0);
            pthread_mutex_unlock(&clients_mutex);
        } 
    }
    pthread_mutex_lock(&clients_mutex);
    close(clientptr->socket);
    memset(clientptr, 0, sizeof(client));
    free(clientptr);
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

int main (int argc, char* argv[]) {

    if (argc < 2 || argc > 4) {
        usage();
        return 0;
    }
    if (argc == 3 && !strcmp(argv[2], "-e")) {
        echo = true;
        broadcast = false;
    }
    else if (argc == 3 && !strcmp(argv[2], "-b")) {
        echo = false;
        broadcast = true;
    }
    else if (argc == 4 && ((!strcmp(argv[2], "-b") && !strcmp(argv[3], "-e")) || (!strcmp(argv[2], "-e") && !strcmp(argv[3], "-b")))) {
        echo = true;
        broadcast = true;
    }
    else {
        echo = false;
        broadcast = false;
    }
    
    int port = atoi(argv[1]);
    int server_socket, connection_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // socket create
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[ERROR] socket() Failed\n");
        exit(EXIT_FAILURE);
    }

    // socket bind 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("[ERROR] bind() Failed\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // listening
    if (listen(server_socket, MAX_CLIENT) < 0) {
        perror("[ERROR] listen() Failed\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while (true) {
        // accept
        if ((connection_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("[ERROR] accept() Failed\n");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        // echo
        pthread_mutex_lock(&clients_mutex);
        int idx;
        for (idx = 0; flag[idx] == true && idx < MAX_CLIENT; idx++)
            ;
        if (idx == 10) {
            printf("[ERROR] MAX %d Clients Available\n", MAX_CLIENT);
            close(connection_socket);
            continue;
        }
        flag[idx] = true;
        clients[idx] = (pclient)malloc(sizeof(client));
        memset(clients[idx], 0, sizeof(client));
        clients[idx]->socket = connection_socket;
        pthread_mutex_unlock(&clients_mutex);
        if (pthread_create(&clients[idx]->thread, NULL, handle_client, (void *)clients[idx])) {
            perror("[ERROR] pthread_create() Failed\n");
            close(server_socket);
            close(clients[idx]->socket);
            exit(EXIT_FAILURE);
        }
        pthread_detach(clients[idx]->thread);
    }

    // socket close
    close(server_socket);

    return 0;
}