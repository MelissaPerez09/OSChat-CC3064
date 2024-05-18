/*
    * client.c
    * This file contains the client side of the chat application.
    * The client will be able to connect to the server, send messages, change status, view connected users, and see user information.
    * @autors: Melissa Pérez, Fernanda Esquivel
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "chat.pb-c.h"

#define PORT 8080
#define BUFFER_SIZE 1024

void register_user(int sockfd, const char *username) {
    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__REGISTER_USER;
    Chat__NewUserRequest new_user_request = CHAT__NEW_USER_REQUEST__INIT;
    new_user_request.username = (char *)username;
    request.register_user = &new_user_request;
    request.payload_case = CHAT__REQUEST__PAYLOAD_REGISTER_USER;

    size_t len = chat__request__get_packed_size(&request);
    uint8_t *buffer = malloc(len);
    chat__request__pack(&request, buffer);

    send(sockfd, buffer, len, 0);
    free(buffer);
    //printf("Registration request sent for username '%s'.\n", username);
}

int receive_server_response(int sockfd) {
    uint8_t buffer[BUFFER_SIZE];
    int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (len > 0) {
        Chat__Response *response = chat__response__unpack(NULL, len, buffer);
        if (response) {
            //printf("Received server response: %s\n", response->message);
            if (response->status_code == CHAT__STATUS_CODE__BAD_REQUEST) {
                fprintf(stderr, "Error: %s\n", response->message);
                chat__response__free_unpacked(response, NULL);
                return -1;
            }
            chat__response__free_unpacked(response, NULL);
            return 0;
        }
    } else if (len == 0) {
        printf("Server closed the connection.\n");
    } else {
        perror("recv failed");
    }
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <username>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    const char *username = argv[2];

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {0};

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        exit(1);
    }

    register_user(sockfd, username);

    if (receive_server_response(sockfd) == 0) {
        printf("Registered as %s. You can start sending messages.\n", username);

        char message[256];
        while (1) {
            printf("Type a message (or 'exit' to quit): ");
            fgets(message, sizeof(message), stdin);
            message[strcspn(message, "\n")] = '\0';

            if (strcmp(message, "exit") == 0) {
                close(sockfd);
                printf("Disconnecting...\n");
                break;
            }
        }
    } else {
        close(sockfd);  // Close the connection on failure
    }

    return 0;
}
