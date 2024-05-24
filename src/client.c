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
#include <pthread.h>
#include <sys/select.h>
#include <errno.h>


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

/*
Funcion que imprime el nombre de usuario y la dirección IP de un usuario.
Parametros:
    * char* full_username: nombre de usuario y dirección IP
*/
void print_user_info(char* full_username) {
    char* token = strtok(full_username, "@");
    if (token != NULL) {
        printf("User: %s, ", token);  // Imprime el nombre del usuario
        token = strtok(NULL, "@");
        if (token != NULL) {
            printf("IP: %s, ", token);  // Imprime la dirección IP
        }
    }
}

/*
Funcion que envía un mensaje de broadcast al servidor.
Parametros:
    * int sockfd: socket descriptor
    * const char *message: mensaje a enviar
*/
void send_broadcast_message(int sockfd, const char *message) {
    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__SEND_MESSAGE;
    Chat__SendMessageRequest send_message_request = CHAT__SEND_MESSAGE_REQUEST__INIT;
    send_message_request.recipient = ""; // Campo vacío para broadcast
    send_message_request.content = (char *)message;
    request.send_message = &send_message_request;
    request.payload_case = CHAT__REQUEST__PAYLOAD_SEND_MESSAGE;

    size_t len = chat__request__get_packed_size(&request);
    uint8_t *buffer = malloc(len);
    chat__request__pack(&request, buffer);

    send(sockfd, buffer, len, 0);
    free(buffer);
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

/*
Funcion que envìa una solicitud para obtener la lista de los usuarios conectados, la procesa e imprime la respuesta del servidor.
Parametros:
    * int sockfd: socket descriptor para comunicarse con el servidor
Retornos:
    * int: codigo de estado; 0 para exito y -1 para fallas
*/
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
                    //printf("User: %s, Status: %d\n", user_list->users[i]->username, user_list->users[i]->status);
                    print_user_info(user_list->users[i]->username);
                    printf("Status: %d\n", user_list->users[i]->status);  // Imprime el estado del usuario
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

/*
Funcion que envia una solicitud al servidor para actualizar el estado del usuario.
Parametros: 
    * int sockfd: socket descriptor
    * const char *username: usuario al que se le actualiza el estado
    * Chat__UserStatus new_status: nuevo estatu a aplicar
*/
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

/*
Funcion que limpia un buffer de bytes.
Parametros:
    * uint8_t *buffer: buffer a limpiar
    * size_t size: tamaño del buffer
*/
void clear_buffer(uint8_t *buffer, size_t size) {
    memset(buffer, 0, size);
}

/*
Funcion que maneja errores en la recepción de mensajes.
Parametros:
    * int len: longitud del mensaje recibido
Retornos:
    * int: codigo de estado; 0 para no errores y -1 para sí el server cerro la conexión o hubo un error
*/
int handle_recv_errors(int len) {
    if (len == 0) {
        printf("Server closed the connection.\n");
        return -1;
    } else if (len < 0) {
        perror("recv failed");
        return -1;
    }
    return 0;
}

/*
Funcion que recibe mensajes del servidor y los imprime en la consola.
Parametros:
    * void *sockfd_ptr: puntero al descriptor del socket
Retornos:
    * void: puntero nulo
*/
void *receive_messages(void *sockfd_ptr) {
    int sockfd = *(int *)sockfd_ptr;
    fd_set readfds;
    struct timeval timeout;
    char buffer[1024];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Configura un timeout de 0.5 segundos
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            printf("Select error.\n");
            break;
        }

        if (FD_ISSET(sockfd, &readfds)) {
            clear_buffer(buffer, sizeof(buffer));
            int len = recv(sockfd, buffer, sizeof(buffer), 0);
            if (len > 0) {
                printf("%s", buffer);
            } else if (len == 0) {
                printf("Server closed the connection.\n");
                break;
            } else {
                perror("recv failed");
                break;
            }
        }
    }
    return NULL;
}

/*
Funcion que envía un mensaje directo a un usuario específico.
Parametros:
    * int sockfd: socket descriptor
    * const char *recipient: destinatario del mensaje
    * const char *message: mensaje a enviar
*/
void send_direct_message(int sockfd, const char *recipient, const char *message) {
    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__SEND_MESSAGE;
    Chat__SendMessageRequest send_message_request = CHAT__SEND_MESSAGE_REQUEST__INIT;
    send_message_request.recipient = (char *)recipient;  // Especificar destinatario
    send_message_request.content = (char *)message;
    request.send_message = &send_message_request;
    request.payload_case = CHAT__REQUEST__PAYLOAD_SEND_MESSAGE;

    size_t len = chat__request__get_packed_size(&request);
    uint8_t *buffer = malloc(len);
    chat__request__pack(&request, buffer);

    send(sockfd, buffer, len, 0);
    free(buffer);
}

/*
Funcion que maneja la respuesta del servidor a una solicitud de información del usuario, imprimiendo detalles o un mensaje de error.
Parametros:
    * int sockfd: socket descriptor
Retornos:
    * int: codigo de estado; 0 usuario encontrado y -1 para lo contrario
*/
int receive_user_info_response(int sockfd) {
    uint8_t buffer[BUFFER_SIZE];
    clear_buffer(buffer, BUFFER_SIZE);
    int len = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (len > 0) {
        Chat__Response *response = chat__response__unpack(NULL, len, buffer);
        if (response && response->status_code == CHAT__STATUS_CODE__OK && response->result_case == CHAT__RESPONSE__RESULT_USER_LIST) {
            Chat__UserListResponse *user_list = response->user_list;
            if (user_list->n_users > 0) {
                print_user_info(user_list->users[0]->username);
                printf("Status: %d\n", user_list->users[0]->status);
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
    }
    return handle_recv_errors(len);
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
                    print_user_info(user_list->users[i]->username);
                    printf("Status: %d\n", user_list->users[i]->status);  // Imprime el estado del usuario
                }
                chat__response__free_unpacked(response, NULL);
                return 0;  // Éxito
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

    pthread_t recv_thread;
    if(pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd) == 0) {
        pthread_detach(recv_thread);
    } else {
        fprintf(stderr, "Failed to create thread for receiving messages.\n");
    }


    while (1) {
        menu();

        int option;
        scanf("%d", &option);
        getchar();

        switch (option) {
            case 1: {
                printf("Enter your message: ");
                char message[256];
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = 0; // Eliminar el carácter de nueva línea
                send_broadcast_message(sockfd, message);
                break;
            }
            case 2: {
                char recipient[32];
                printf("Enter the username to send message: ");
                fgets(recipient, sizeof(recipient), stdin);
                recipient[strcspn(recipient, "\n")] = 0;  // Remove newline character

                printf("Enter your message: ");
                char message[256];
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = 0;  // Remove newline character

                send_direct_message(sockfd, recipient, message);
                break;
            }

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
                Chat__UserListRequest user_list_request = CHAT__USER_LIST_REQUEST__INIT;
                user_list_request.username = NULL;  // Solicitar todos los usuarios
                request_user_list(sockfd);
                break;
            case 5:
            {
                // Obtener información de un usuario específico
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
