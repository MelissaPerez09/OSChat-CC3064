/* 
    * server.c
    * Implementation of the server side of the chat application.
    * The server is responsible for handling the connections with the clients, managing the users, and broadcasting the messages.
    * The server is implemented using the protocol buffers library.
    * @autors: Melissa Pérez, Fernanda Esquivel
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "chat.pb-c.h"

#define PORT 8080
#define MAX_CLIENTS 100

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];
int uid = 10;  // Unique identifier for each client
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

bool username_exists(const char* username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && strcmp(clients[i]->name, username) == 0) {
            pthread_mutex_unlock(&clients_mutex);
            return true;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return false;
}

void send_response(int sockfd, Chat__StatusCode status_code, const char *message) {
    Chat__Response response = CHAT__RESPONSE__INIT;
    response.status_code = status_code;
    response.message = strdup(message); // Ensure the message is copied
    size_t len = chat__response__get_packed_size(&response);
    uint8_t *buffer = malloc(len);
    chat__response__pack(&response, buffer);
    send(sockfd, buffer, len, 0);
    free(buffer);
    free(response.message); // Free the duplicated message
}

void add_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->uid == uid) {
            printf("Client disconnected: %s (IP: %s)\n", clients[i]->name, inet_ntoa(clients[i]->address.sin_addr));
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    uint8_t buffer[1024];
    int len; // Declare len outside the loop

    while ((len = recv(cli->sockfd, buffer, sizeof(buffer), 0)) > 0) {
        // Process incoming messages...
    }

    if (len == 0) {
        remove_client(cli->uid);
        close(cli->sockfd);
        free(cli);
        pthread_detach(pthread_self());
    } else {
        perror("recv failed");
    }
    return NULL;
}

int main() {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);
    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(listenfd, 10);

    printf("Server started on port %d\n", PORT);

    while (1) {
        client_t *cli = malloc(sizeof(client_t));
        socklen_t clilen = sizeof(cli->address);
        cli->sockfd = accept(listenfd, (struct sockaddr*)&cli->address, &clilen);

        if (cli->sockfd < 0) {
            perror("Accept failed");
            free(cli); // Free memory if accept fails
            continue;
        }

        uint8_t buffer[1024];
        int len = recv(cli->sockfd, buffer, sizeof(buffer), 0);
        if (len > 0) {
            Chat__Request *req = chat__request__unpack(NULL, len, buffer);
            if (req && req->payload_case == CHAT__REQUEST__PAYLOAD_REGISTER_USER) {
                if (!username_exists(req->register_user->username)) {
                    strcpy(cli->name, req->register_user->username);
                    cli->uid = uid++;
                    printf("New connection: %s (IP: %s)\n", cli->name, inet_ntoa(cli->address.sin_addr));
                    add_client(cli);
                    send_response(cli->sockfd, CHAT__STATUS_CODE__OK, "Registration successful");
                    pthread_t tid;
                    pthread_create(&tid, NULL, &handle_client, (void*)cli);
                } else {
                    send_response(cli->sockfd, CHAT__STATUS_CODE__BAD_REQUEST, "User is already connected");
                    close(cli->sockfd);
                    free(cli);
                }
            }
            chat__request__free_unpacked(req, NULL);
        } else {
            close(cli->sockfd);
            free(cli);
        }
    }

    return 0;
}
