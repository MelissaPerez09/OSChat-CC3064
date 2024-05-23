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

void menu() {
    printf("\n----------------------------------\n1. Chat with everyone (broadcast)\n");
    printf("2. Send a direct message\n");
    printf("3. Change Status\n");
    printf("4. View connected users\n");
    printf("5. See user information\n");
    printf("6. Help\n");
    printf("7. Exit\n");
    printf("----------------------------------\nSelect an option: ");
}

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

int request_user_list(int sockfd) {
    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__GET_USERS;
    request.payload_case = CHAT__REQUEST__PAYLOAD_GET_USERS;

    size_t len = chat__request__get_packed_size(&request);
    uint8_t *buffer = malloc(len);
    chat__request__pack(&request, buffer);

    send(sockfd, buffer, len, 0);
    free(buffer);

    // Recibir y procesar la respuesta
    uint8_t response_buffer[BUFFER_SIZE];
    len = recv(sockfd, response_buffer, BUFFER_SIZE, 0);
    if (len > 0) {
        Chat__Response *response = chat__response__unpack(NULL, len, response_buffer);
        if (response) {
            if (response->status_code == CHAT__STATUS_CODE__BAD_REQUEST) {
                fprintf(stderr, "Error: %s\n", response->message);
                chat__response__free_unpacked(response, NULL);
                return -1;
            }
            if (response->result_case == CHAT__RESPONSE__RESULT_USER_LIST) {
                Chat__UserListResponse *user_list = response->user_list;
                printf("\nConnected Users:\n");
                for (size_t i = 0; i < user_list->n_users; i++) {
                    printf("User: %s, Status: %d\n", user_list->users[i]->username, user_list->users[i]->status);
                }
                chat__response__free_unpacked(response, NULL);
                return 0;  // Success
            }
            chat__response__free_unpacked(response, NULL);
        }
    } else if (len == 0) {
        printf("Server closed the connection.\n");
        return -1;
    } else {
        perror("recv failed");
        return -1;
    }
    return 0;  // No hubo errores, pero tampoco se recibió una lista de usuarios
}


void update_status(int sockfd, const char *username, Chat__UserStatus new_status) {
    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__UPDATE_STATUS;
    Chat__UpdateStatusRequest update_status_request = CHAT__UPDATE_STATUS_REQUEST__INIT;
    update_status_request.username = (char *)username;
    update_status_request.new_status = new_status;
    request.update_status = &update_status_request;
    request.payload_case = CHAT__REQUEST__PAYLOAD_UPDATE_STATUS;

    size_t len = chat__request__get_packed_size(&request);
    uint8_t *buffer = malloc(len);
    chat__request__pack(&request, buffer);
    send(sockfd, buffer, len, 0);
    free(buffer);
}

int receive_user_info_response(int sockfd) {
    uint8_t buffer[BUFFER_SIZE];
    int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (len > 0) {
        Chat__Response *response = chat__response__unpack(NULL, len, buffer);
        if (response && response->status_code == CHAT__STATUS_CODE__OK && response->result_case == CHAT__RESPONSE__RESULT_USER_LIST) {
            Chat__UserListResponse *user_list = response->user_list;
            if (user_list->n_users > 0) {
                printf("User: %s, Status: %d\n", user_list->users[0]->username, user_list->users[0]->status);
            } else {
                printf("No user found.\n");
                return -1;
            }
            chat__response__free_unpacked(response, NULL);
            return 0;
        } else {
            printf("Error: %s\n", response->message);
            chat__response__free_unpacked(response, NULL);
            return -1;
        }
    } else if (len == 0) {
        printf("Server closed the connection.\n");
        return -1;
    } else {
        perror("recv failed");
        return -1;
    }
}

int receive_server_response(int sockfd) {
    uint8_t buffer[BUFFER_SIZE];
    int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (len > 0) {
        Chat__Response *response = chat__response__unpack(NULL, len, buffer);
        if (response) {
            printf("Received server response: %s\n", response->message);
            if (response->status_code == CHAT__STATUS_CODE__BAD_REQUEST) {
                fprintf(stderr, "Error: %s\n", response->message);
                chat__response__free_unpacked(response, NULL);
                return -1;
            }

            if (response->result_case == CHAT__RESPONSE__RESULT_USER_LIST) {
                Chat__UserListResponse *user_list = response->user_list;
                printf("\nConnected Users:\n");
                for (size_t i = 0; i < user_list->n_users; i++) {
                    printf("User: %s, Status: %d\n", user_list->users[i]->username, user_list->users[i]->status);
                }
                chat__response__free_unpacked(response, NULL);
                return 0;  // Return 0 to indicate success
            }

            chat__response__free_unpacked(response, NULL);
            return 0;
        }
    } else if (len == 0) {
        printf("Server closed the connection.\n");
        return -1;
    } else {
        perror("recv failed");
        return -1;
    }
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
    if (receive_server_response(sockfd) != 0) {
        close(sockfd);
        return 1;
    }

    printf("\nRegistered as %s. Welcome to the chat!\n", username);

    while (1) {
        menu();

        int option;
        scanf("%d", &option);
        getchar();

        switch (option) {
            case 1:
                // TODO: Implement chat with everyone (broadcast)
                break;
            case 2:
                // TODO: Implement send a direct message
                break;
            case 3:
                // Change status
                printf("Choose new status (0: ONLINE, 1: BUSY, 2: OFFLINE): ");
                int new_status;
                scanf("%d", &new_status);
                char* status_names[] = {"ONLINE", "BUSY", "OFFLINE"};
                if (new_status >= 0 && new_status <= 2) {
                    update_status(sockfd, username, new_status);
                    if (receive_server_response(sockfd) == 0) {
                        printf("\nStatus updated to %s.\n", status_names[new_status]);
                    } else {
                        printf("\nFailed to update status.\n");
                    }
                } else {
                    printf("Invalid status. Please try again.\n");
                }
                break;
            case 4:
                // Solicitar la lista de usuarios conectados
                request_user_list(sockfd);
                break;
            case 5:
            {
                char username[32];
                printf("Enter the username to get information: ");
                fgets(username, sizeof(username), stdin);
                username[strcspn(username, "\n")] = 0; // Remove newline character

                Chat__UserListRequest user_info_request = CHAT__USER_LIST_REQUEST__INIT;
                user_info_request.username = username; // Specify the user

                Chat__Request request = CHAT__REQUEST__INIT;
                request.operation = CHAT__OPERATION__GET_USERS;
                request.payload_case = CHAT__REQUEST__PAYLOAD_GET_USERS;
                request.get_users = &user_info_request;

                size_t len = chat__request__get_packed_size(&request);
                uint8_t *buffer = malloc(len);
                chat__request__pack(&request, buffer);

                send(sockfd, buffer, len, 0);
                free(buffer);

                // Call the function to receive and process the response from the server
                if (receive_user_info_response(sockfd) != 0) {
                    printf("No user found with the username '%s'.\n", username);
                }
            }
            break;
            case 6:
                // Display help
                printf("\nHELP!: \n1 - Broadcast message to all\n2 - Send a direct message to a user\n3 - Change your status\n4 - View all connected users in the server\n5 - Get information about a specific user\n6 - Display this help\n7 - Exit the chat\n");
                break;
            case 7:
                // Exit the chat
                close(sockfd);
                printf("\nDisconnected from server.\n");
                exit(0);
            default:
                printf("Invalid option. Please try again.\n");
                break;
        }
    }

    return 0;
}
