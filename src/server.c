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
#include <time.h>
#include "chat.pb-c.h"

#define MAX_CLIENTS 100
#define INACTIVITY_TIMEOUT 300

typedef enum {
    ACTIVO = 0,   // En línea y disponible para recibir mensajes
    OCUPADO = 1,  // En línea pero marcado como ocupado, puede no responder de inmediato
    INACTIVO = 2  // Desconectado y no puede recibir mensajes
} ClientStatus;

const char* get_status_name(ClientStatus status) {
    switch (status) {
        case ACTIVO: return "ONLINE";
        case OCUPADO: return "BUSY";
        case INACTIVO: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    time_t last_active;
    ClientStatus status;
} client_t;

client_t *clients[MAX_CLIENTS];
int uid = 10;
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
    response.message = strdup(message);
    size_t len = chat__response__get_packed_size(&response);
    uint8_t *buffer = malloc(len);
    chat__response__pack(&response, buffer);
    send(sockfd, buffer, len, 0);
    free(buffer);
    free(response.message);
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
            printf("\n(*) Client disconnected: %s (IP: %s)\n", clients[i]->name, inet_ntoa(clients[i]->address.sin_addr));
            clients[i] = NULL;
            break;
        } 
    }
    pthread_mutex_unlock(&clients_mutex);
}

/*
Función para enviar la lista de usuarios conectados a un cliente específico.
Si se proporciona un nombre de usuario, se envía solo la información de ese usuario.
De lo contrario, se envía la lista completa de usuarios.
Parametros:
    * int sockfd: socket descriptor
    * Chat__UserListRequest *request: detalles de la solicitud, puede incluir un username específico
*/
void send_user_list(int sockfd, Chat__UserListRequest *request) {
    pthread_mutex_lock(&clients_mutex);
    size_t num_users = 0;
    Chat__User **users = NULL;

    if (request != NULL && request->username != NULL && strlen(request->username) > 0) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] && strcmp(clients[i]->name, request->username) == 0) {
                users = malloc(sizeof(Chat__User*));
                users[0] = malloc(sizeof(Chat__User));
                chat__user__init(users[0]);
                char full_name[64]; // Asumiendo que el nombre y la IP caben en este buffer
                sprintf(full_name, "%s@%s", clients[i]->name, inet_ntoa(clients[i]->address.sin_addr));
                users[0]->username = strdup(full_name);
                users[0]->status = clients[i]->status;
                num_users = 1;
                break;
            }
        }
    } else {
        users = malloc(MAX_CLIENTS * sizeof(Chat__User*));
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i]) {
                users[num_users] = malloc(sizeof(Chat__User));
                chat__user__init(users[num_users]);
                char full_name[64];
                sprintf(full_name, "%s@%s", clients[i]->name, inet_ntoa(clients[i]->address.sin_addr));
                users[num_users]->username = strdup(full_name);
                users[num_users]->status = clients[i]->status;
                num_users++;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    // Empaquetar y enviar la respuesta
    Chat__UserListResponse user_list_response = CHAT__USER_LIST_RESPONSE__INIT;
    user_list_response.n_users = num_users;
    user_list_response.users = users;
    user_list_response.type = request ? CHAT__USER_LIST_TYPE__SINGLE : CHAT__USER_LIST_TYPE__ALL;

    Chat__Response response = CHAT__RESPONSE__INIT;
    response.operation = CHAT__OPERATION__GET_USERS;
    response.status_code = CHAT__STATUS_CODE__OK;
    response.result_case = CHAT__RESPONSE__RESULT_USER_LIST;
    response.user_list = &user_list_response;

    size_t len = chat__response__get_packed_size(&response);
    uint8_t *buffer = malloc(len);
    chat__response__pack(&response, buffer);
    send(sockfd, buffer, len, 0);
    free(buffer);

    // Liberar recursos
    for (int i = 0; i < num_users; i++) {
        free(users[i]->username);
        free(users[i]);
    }
    free(users);
}

void broadcast_message(char *sender_name, char *message) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->name, sender_name) != 0) {
            printf("Sending to %s: %s\n", clients[i]->name, message);  // Debug print
            send(clients[i]->sockfd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}


void* check_inactivity(void* arg) {
    while (1) {
        sleep(1);
        time_t now = time(NULL);
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] && difftime(now, clients[i]->last_active) > INACTIVITY_TIMEOUT) {
                if (clients[i]->status != INACTIVO) {
                    clients[i]->status = INACTIVO;
                    printf("%s has been set OFFLINE due to inactivity.\n", clients[i]->name);
                    char message[256];
                    sprintf(message, "Your status has been changed to OFFLINE due to inactivity.");
                    send_response(clients[i]->sockfd, CHAT__STATUS_CODE__OK, message);
                }
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
    return NULL;
}

void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    cli->last_active = time(NULL);
    uint8_t buffer[1024];
    int len;

    while ((len = recv(cli->sockfd, buffer, sizeof(buffer), 0)) > 0) {
        cli->last_active = time(NULL);
        Chat__Request *req = chat__request__unpack(NULL, len, buffer);
        if (req == NULL) {
            fprintf(stderr, "Error unpacking incoming message\n");
            continue;
        }

        switch (req->operation) {
            case CHAT__OPERATION__GET_USERS:
                if (req->payload_case == CHAT__REQUEST__PAYLOAD_GET_USERS) {
                    // Se envía la solicitud completa que incluye la posibilidad de un nombre de usuario específico
                    send_user_list(cli->sockfd, req->get_users);
                } else {
                    // En caso de que no haya detalles (lo cual es poco probable dado cómo está configurada tu aplicación), se envía NULL
                    send_user_list(cli->sockfd, NULL);
                }
                break;
            
            // Cambiar el estado de un usuario
            case CHAT__OPERATION__UPDATE_STATUS: {
                if (req->update_status && username_exists(req->update_status->username)) {
                    for (int i = 0; i < MAX_CLIENTS; ++i) {
                        if (clients[i] && strcmp(clients[i]->name, req->update_status->username) == 0) {
                            ClientStatus old_status = clients[i]->status; // Guarda el estado antiguo
                            clients[i]->status = req->update_status->new_status; // Actualiza al nuevo estado
                            send_response(cli->sockfd, CHAT__STATUS_CODE__OK, "\nStatus updated successfully!");
                            printf("\nUpdated status for %s from %s to %s\n", clients[i]->name, get_status_name(old_status), get_status_name(clients[i]->status));
                            break;
                        }
                    }
                } else {
                    send_response(cli->sockfd, CHAT__STATUS_CODE__BAD_REQUEST, "User not found");
                }
                break;
            }
            case CHAT__OPERATION__SEND_MESSAGE:
                if (req->send_message) {
                    if (strlen(req->send_message->recipient) > 0) {
                        // Enviar a un usuario específico
                        char dm_msg[1024];
                        snprintf(dm_msg, sizeof(dm_msg), "\n> DM from %s: %s\n", cli->name, req->send_message->content);
                        bool sent = false;
                        pthread_mutex_lock(&clients_mutex);
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (clients[i] && strcmp(clients[i]->name, req->send_message->recipient) == 0) {
                                send(clients[i]->sockfd, dm_msg, strlen(dm_msg), 0);
                                sent = true;
                                break;
                            }
                        }
                        pthread_mutex_unlock(&clients_mutex);
                        if (!sent) {
                            char error_msg[256];
                            snprintf(error_msg, sizeof(error_msg), "User %s not found.\n", req->send_message->recipient);
                            send(cli->sockfd, error_msg, strlen(error_msg), 0);
                        }
                    } else {
                        // Broadcast message
                        char broadcast_msg[1024];
                        snprintf(broadcast_msg, sizeof(broadcast_msg), "\n> Broadcast Message from %s: %s\n", cli->name, req->send_message->content);
                        broadcast_message(cli->name, broadcast_msg);
                    }
                }
                break;

        }

        chat__request__free_unpacked(req, NULL);
    }

    if (len <= 0) {
        remove_client(cli->uid);
        close(cli->sockfd);
        free(cli);
        pthread_detach(pthread_self());
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    int opt = 1;
    // Configuración para reutilizar la dirección IP y puerto
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    // Agregamos la llamada a listen()
    if (listen(listenfd, 10) < 0) {
        perror("Server: can't listen on port");
        exit(1);
    }

    printf("Server started on port %d\n", port); 
    pthread_t tid_inactivity;
    pthread_create(&tid_inactivity, NULL, &check_inactivity, NULL); 

    while (1) {
        client_t *cli = malloc(sizeof(client_t));
        socklen_t clilen = sizeof(cli->address);
        cli->sockfd = accept(listenfd, (struct sockaddr*)&cli->address, &clilen);

        if (cli->sockfd < 0) {
            perror("Accept failed");
            free(cli);
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
                    printf("\n(*) New connection: %s (IP: %s)\n", cli->name, inet_ntoa(cli->address.sin_addr));
                    add_client(cli);
                    send_response(cli->sockfd, CHAT__STATUS_CODE__OK, "Registration successful");
                    pthread_t tid;
                    pthread_create(&tid, NULL, &handle_client, (void*)cli);
                } else {
                    send_response(cli->sockfd, CHAT__STATUS_CODE__BAD_REQUEST, "(!)User is already connected");
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
