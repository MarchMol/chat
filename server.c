#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <bits/pthreadtypes.h>

#define USER_LIMIT 5
#define STR_LEN 50
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chat_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct {
    char uname[STR_LEN];
    char uip[STR_LEN];
} active_users;

active_users ausers[USER_LIMIT];
int ausers_n = 0;
pthread_mutex_t ausers_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_CLIENTS 10  // Ajusta según lo necesites
int client_sockets[MAX_CLIENTS];  // Almacena los sockets de los clientes
typedef struct {
    int socket_fd;
    char username[50];
} Client;

Client *clients[MAX_CLIENTS] = {NULL};
int num_clients = 0;  // Contador de clientes conectados
#define STR_LEN 50
void raise_error(const char *msg){
    perror(msg);
    fflush(stdout);
    exit(1);
}

typedef struct {
    char username[50];
    char message[256];
} Message;

// Estructura que almacena el historial de mensajes por usuario
typedef struct {
    char chat_name[50];
    Message messages[100];  // Límite de 100 mensajes por chat
    int message_count;
} ChatHistory;

ChatHistory chat_histories[10];  // Soportamos hasta 10 chats diferentes
int chat_count = 0;

int find_user_socket(const char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < num_clients; i++) {
        if (strcmp(clients[i]->username, username) == 0) {
            int socket_fd = clients[i]->socket_fd;
            pthread_mutex_unlock(&clients_mutex);
            return socket_fd;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

void add_message_to_history(const char *chat_name, const char *username, const char *message) {
    pthread_mutex_lock(&chat_mutex);

    // Buscar si el historial del chat ya existe
    for (int i = 0; i < chat_count; i++) {
        if (strcmp(chat_histories[i].chat_name, chat_name) == 0) {
            if (chat_histories[i].message_count < 100) {
                strcpy(chat_histories[i].messages[chat_histories[i].message_count].username, username);
                strcpy(chat_histories[i].messages[chat_histories[i].message_count].message, message);
                chat_histories[i].message_count++;
            }
            pthread_mutex_unlock(&chat_mutex);
            return;
        }
    }

    // Si el historial no existe, crearlo
    if (chat_count < 10) {  // Límite de 10 chats diferentes
        strcpy(chat_histories[chat_count].chat_name, chat_name);
        chat_histories[chat_count].message_count = 0;

        strcpy(chat_histories[chat_count].messages[0].username, username);
        strcpy(chat_histories[chat_count].messages[0].message, message);
        chat_histories[chat_count].message_count++;

        chat_count++;
    }

    pthread_mutex_unlock(&chat_mutex);
}

void send_websocket_message(int client_fd, const char *message) {
    size_t message_length = strlen(message);
    uint8_t frame[2 + message_length];

    frame[0] = 0x81;  // FIN=1, tipo=Texto
    frame[1] = message_length;  // Longitud del mensaje (asumiendo que es < 126)

    memcpy(&frame[2], message, message_length);

    send(client_fd, frame, sizeof(frame), 0);
}


void handle_get_history(int client_fd, const char *chat_name) {
    printf("Solicitando historial para: %s\n", chat_name);
    printf("hay: %d chats\n", chat_count);
    pthread_mutex_lock(&chat_mutex);

    for (int i = 0; i < chat_count; i++) {
        if (strcmp(chat_histories[i].chat_name, chat_name) == 0) {
            char buffer[1024];
            int offset = 0;

            // Construir mensaje de historial en formato de texto
            for (int j = 0; j < chat_histories[i].message_count; j++) {
                offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                                   "%s: %s\n",
                                   chat_histories[i].messages[j].username,
                                   chat_histories[i].messages[j].message);
            }

            pthread_mutex_unlock(&chat_mutex);

            // Enviar como frame WebSocket
            send_websocket_message(client_fd, buffer);
            return;
        }
    }

    pthread_mutex_unlock(&chat_mutex);

    // Si no hay historial
    send_websocket_message(client_fd, "No se encontró historial para este chat.");
}


// Función para enviar un mensaje de estatus actualizado a todos los clientes
void broadcast_status_change(int *client_sockets, int num_clients, const char *username, int status) {
    char message[256];
    int username_len = strlen(username);

    // Construir mensaje tipo 54: Usuario cambió estatus
    message[0] = 54;  // Tipo de mensaje (54 para notificación de cambio de estatus)
    message[1] = username_len;  // Longitud del nombre de usuario
    memcpy(message + 2, username, username_len);  // Nombre del usuario
    message[2 + username_len] = status;  // Estatus

    // Enviar mensaje a todos los clientes
    for (int i = 0; i < num_clients; i++) {
        int n = write(client_sockets[i], message, 2 + username_len + 1);
        if (n < 0) {
            perror("Error enviando mensaje a los clientes");
        }
    }
}

// Función para enviar un mensaje a todos los clientes conectados
void send_message_to_all(int sender_fd, const char *sender, const char *message) {
    int sender_len = strlen(sender);
    int message_len = strlen(message);
    char buffer[256];
    
    buffer[0] = 55;  // Tipo de mensaje
    buffer[1] = sender_len;  // Longitud del nombre del usuario
    memcpy(buffer + 2, sender, sender_len);  // Nombre del usuario
    buffer[2 + sender_len] = message_len;  // Longitud del mensaje
    memcpy(buffer + 3 + sender_len, message, message_len);  // Contenido del mensaje

    // Enviar el mensaje a todos los clientes conectados
    for (int i = 0; i < num_clients; i++) {
        if (client_sockets[i] != sender_fd) {
            write(client_sockets[i], buffer, 3 + sender_len + message_len);
        }
    }    
}

// Función para enviar un mensaje a un cliente específico
void send_message_to_client(int recipient_fd, const char *sender, const char *message) {
    int sender_len = strlen(sender);
    int message_len = strlen(message);
    char buffer[256];

    buffer[0] = 55;  // Tipo de mensaje
    buffer[1] = sender_len;  // Longitud del nombre del usuario
    memcpy(buffer + 2, sender, sender_len);  // Nombre del usuario
    buffer[2 + sender_len] = message_len;  // Longitud del mensaje
    memcpy(buffer + 3 + sender_len, message, message_len);  // Contenido del mensaje

    write(recipient_fd, buffer, 3 + sender_len + message_len);
}
/*
int main(int argc, char *argv[]){
    // Argument Check
    if (argc<2){
        perror("Puerto de ejecucion faltante");
        exit(1);
    }
    
    int port_n, scoket_fd, accepted_sockfd, client_limit;
    client_limit = 5;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;

    // Abriendo socket
    scoket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (scoket_fd<0){
        perror("Error creando socket");
        exit(1);
    }

    // Serv_addr
    bzero((char *) &serv_addr, sizeof(serv_addr)); // Reinicia serv_addr
    port_n = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_n);
      
    // Binding
    if ( bind(scoket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        raise_error("binding failed");
    }

    // Listening
    listen(scoket_fd, client_limit);
    clilen =  sizeof(cli_addr);
    // Reading loop
    int n;
    char buffer[255];
    int no_request;
    no_request = 0;
    active_users ausers[USER_LIMIT];
    int ausers_n;
    ausers_n = 0;
    char response[512];
    while(1){
        // Socket reading
        accepted_sockfd = accept(scoket_fd, (struct sockaddr *) &cli_addr, &clilen);
        if (accepted_sockfd<0){
            printf("Error Aceptando");
            fflush(stdout);
        }
        getpeername(accepted_sockfd, (struct sockaddr *) &cli_addr, &clilen);
        char client_ip[INET_ADDRSTRLEN]; // Buffer for IP address
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        while(1){
        // Parsing of request
        bzero(buffer, 255);
        n = recv(accepted_sockfd, buffer, 255,0);
        printf("Buffer recibido: ");
        for (int i = 0; i < n; i++) {
            printf("%02X ", (unsigned char)buffer[i]);  // Muestra los bytes en formato hexadecimal
        }
        printf("\n");
        fflush(stdout);
        if (n<0){
            printf("Error leyendo");
            fflush(stdout);
            // raise_error("Error leyendo");
        }
        if (n==0){
            printf("User disconnected %s\n",client_ip);
            fflush(stdout);
            close(accepted_sockfd);
            no_request = 1;
            
        } 
        if (n>0){
            no_request = 0;
            
            char *get_line;
            get_line= strtok(buffer, "\r\n");
            char method[10], path[100], http_version[10];
            sscanf(get_line, "%s %s %s", method, path, http_version);

            char name_str[50];
            char *name_start = strstr(path, "?");
            if (name_start) {
                sscanf(name_start, "?name=%49s", name_str);
            }
            
            strcpy(ausers[ausers_n].uname, name_str);
            strcpy(ausers[ausers_n].uip, client_ip);
            ausers_n+=1;
            // Despues de que se asigno correctamente el usuario
            snprintf(response, sizeof(response),
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n");
            send(accepted_sockfd, response, strlen(response), 0);
        } 
        for(int i = 0; i<ausers_n; i++){
            printf("U%d: %s %s\n",i, ausers[i].uname, ausers[i].uip);
            fflush(stdout);
        }
        // // Lee si puede
        // bzero(buffer, 255);
        // n = read(accepted_sockfd, buffer, 255);
        // if (n<0){
        //     raise_error("Error leyendo");
        // }
        // printf("Client: %s\n", buffer);
        // // Escribe si puede
        // bzero(buffer, 255);
        // fgets(buffer, 255, stdin);
        // n = write(accepted_sockfd, buffer, strlen(buffer));
        // if (n<0){
        //     raise_error("Error escribiendo");
        // }
        // // Cerrar servidor desde cliente
        // int i = strncmp("close",buffer, 5);
        // if (i == 0){
        //     break;
        // }

        // Procesar el mensaje de cambio de estatus
        if (buffer[0] == 3) {  // Si el tipo de mensaje es 3 (cambio de estatus)
            printf("estoy en estado");
            int username_len = buffer[1];
            char username[50];
            memcpy(username, buffer + 2, username_len);
            int status = buffer[2 + username_len];

            // Validar el estatus
            if (status < 0 || status > 3) {
                printf("Estatus inválido recibido: %d\n", status);
                // Aquí puedes enviar un mensaje de error si lo deseas
                continue;
            }

            // Broadcast del cambio de estatus a todos los clientes
            broadcast_status_change(client_sockets, num_clients, username, status);
            printf("Cambio de estatus: %s ahora está en estatus %d\n", username, status);
        }
        // Procesar el mensaje de envío (tipo 4)
        else if (buffer[0] == 4) {
            int username_len = buffer[1];
            char recipient[50];
            memcpy(recipient, buffer + 2, username_len);
            recipient[username_len] = '\0';

            int message_len = buffer[2 + username_len];
            char message[256];
            memcpy(message, buffer + 3 + username_len, message_len);
            message[message_len] = '\0';

            if (strcmp(recipient, "~") == 0) {  // Enviar mensaje al chat general
                send_message_to_all(accepted_sockfd, recipient, message);

                add_message_to_history("general", recipient, message);
            } else {  // Enviar mensaje a un usuario específico
                int recipient_fd = find_user_socket(recipient);
                if (recipient_fd == -1) {
                    // El usuario no está conectado, responder con un error
                    char error_msg[] = "El usuario no existe.";
                    write(accepted_sockfd, error_msg, sizeof(error_msg));
                } else {
                    send_message_to_client(recipient_fd, recipient, message);

                    add_message_to_history(recipient, recipient, message);
                }
            }
        }
        else if (buffer[0] == 5) {
            int username_len = buffer[1];
            char username[50];
            memcpy(username, buffer + 2, username_len);
            username[username_len] = '\0';

            // Llamamos a la función para obtener el historial del chat solicitado
            handle_get_history(accepted_sockfd, username);
        }
        }
    }
    close(accepted_sockfd);
    close(scoket_fd);
    return 0;
}*/

void decode_websocket_message(uint8_t *buffer, int length, char *decoded_message) {
    if (length < 2) {
        printf("Mensaje WebSocket demasiado corto\n");
        return;
    }

    int payload_length = buffer[1] & 0x7F; // Obtener la longitud real
    int mask_offset = 2;

    if (payload_length == 126) {
        payload_length = (buffer[2] << 8) | buffer[3];
        mask_offset = 4;
    } else if (payload_length == 127) {
        printf("Payload demasiado grande\n");
        return;
    }

    uint8_t mask[4];
    memcpy(mask, buffer + mask_offset, 4); // Extraer la máscara
    int data_offset = mask_offset + 4;

    printf("Máscara: %02X %02X %02X %02X\n", mask[0], mask[1], mask[2], mask[3]);

    // Aplicar XOR con la máscara
    for (int i = 0; i < payload_length; i++) {
        decoded_message[i] = buffer[data_offset + i] ^ mask[i % 4];
    }
    decoded_message[payload_length] = '\0'; // Terminar string correctamente

    // Mostrar mensaje decodificado en hexadecimal
    printf("Mensaje decodificado en hexadecimal: ");
    for (int i = 0; i < payload_length; i++) {
        printf("%02X ", (unsigned char)decoded_message[i]);
    }
    printf("\n");
}

void *handle_client(void *arg) {
    int accepted_sockfd = *(int *)arg;
    free(arg);

    uint8_t buffer[4096];  // Buffer más grande para manejar fragmentación
    char decoded_message[4096];
    int n, total_received = 0;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    getpeername(accepted_sockfd, (struct sockaddr *)&cli_addr, &clilen);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));

    while (1) {
        total_received = 0;  // Reiniciar el contador para un nuevo mensaje
        bzero(buffer, sizeof(buffer));

        while (1) {  // Bucle para recibir todo el mensaje WebSocket
            n = recv(accepted_sockfd, buffer + total_received, sizeof(buffer) - total_received, 0);

            if (n < 0) {
                printf("Error leyendo\n");
                fflush(stdout);
                break;
            }
            if (n == 0) {
                printf("User disconnected %s\n", client_ip);
                fflush(stdout);
                close(accepted_sockfd);
                return NULL;
            }

            total_received += n;

            // Verificar si el mensaje completo llegó (Bit FIN)
            int fin_bit = buffer[0] & 0x80;  
            if (fin_bit) break;
        }

        printf("Buffer recibido en hexadecimal: ");
        for (int i = 0; i < total_received; i++) {
            printf("%02X ", (unsigned char)buffer[i]);
        }
        printf("\n");

        // Decodificar el mensaje WebSocket
        decode_websocket_message(buffer, total_received, decoded_message);

        printf("Mensaje decodificado: %s\n", decoded_message);

        // Manejo del mensaje
        if (decoded_message[0] == 3) {  // Cambio de estado
            printf("Cambio de estado detectado\n");
            int username_len = decoded_message[1];
            char username[50];
            memcpy(username, decoded_message + 2, username_len);
            username[username_len] = '\0';
            int status = decoded_message[2 + username_len];

            // Validar el estatus
            if (status < 0 || status > 3) {
                printf("Estatus inválido recibido: %d\n", status);
                continue;
            }

            // Broadcast del cambio de estatus
            broadcast_status_change(client_sockets, num_clients, username, status);
            printf("Cambio de estatus: %s ahora está en estatus %d\n", username, status);
        } else if (decoded_message[0] == 4) {  // Envío de mensaje
            printf("envio de mensaje detectado\n");
            int username_len = decoded_message[1];
            char recipient[50];
            memcpy(recipient, decoded_message + 2, username_len);
            recipient[username_len] = '\0';

            int message_len = decoded_message[2 + username_len];
            char message[256];
            memcpy(message, decoded_message + 3 + username_len, message_len);
            message[message_len] = '\0';
            printf("Mensaje de: '%s' para '%s': %s\n", client_ip, recipient, message);
            if (strcmp(recipient, "~") == 0) {
                send_message_to_all(accepted_sockfd, recipient, message);
                add_message_to_history(recipient, client_ip, message);
            } else {
                int recipient_fd = find_user_socket(recipient);
                if (recipient_fd == -1) {
                    char error_msg[] = "El usuario no existe.";
                    write(accepted_sockfd, error_msg, sizeof(error_msg));
                } else {
                    send_message_to_client(recipient_fd, recipient, message);
                    add_message_to_history(recipient, recipient, message);
                }
            }
        } else if (decoded_message[0] == 5) {  // Solicitud de historial
            int username_len = decoded_message[1];
            char username[50];
            memcpy(username, decoded_message + 2, username_len);
            username[username_len] = '\0';
            handle_get_history(accepted_sockfd, username);
        }
    }

    close(accepted_sockfd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        perror("Puerto de ejecución faltante");
        exit(1);
    }

    int port_n, socket_fd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    port_n = atoi(argv[1]);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Error creando socket");
        exit(1);
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_n);

    if (bind(socket_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        raise_error("binding failed");
    }

    listen(socket_fd, USER_LIMIT);
    clilen = sizeof(cli_addr);

    while (1) {
        int *accepted_sockfd = malloc(sizeof(int));
        if (!accepted_sockfd) {
            perror("Error asignando memoria");
            continue;
        }

        *accepted_sockfd = accept(socket_fd, (struct sockaddr *)&cli_addr, &clilen);
        if (*accepted_sockfd < 0) {
            perror("Error aceptando conexión");
            free(accepted_sockfd);
            continue;
        }

        // Obtener IP del cliente
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));

        // Leer solicitud inicial
        char buffer[255];
        int n = recv(*accepted_sockfd, buffer, 255, 0);
        if (n <= 0) {
            close(*accepted_sockfd);
            free(accepted_sockfd);
            continue;
        }

        char name_str[50] = {0};
        char *name_start = strstr(buffer, "?");
        if (name_start) {
            sscanf(name_start, "?name=%49s", name_str);
        }

        // Guardar usuario en la lista con mutex
        pthread_mutex_lock(&ausers_mutex);
        if (ausers_n < USER_LIMIT) {
            strcpy(ausers[ausers_n].uname, name_str);
            strcpy(ausers[ausers_n].uip, client_ip);
            ausers_n++;
        }
        pthread_mutex_unlock(&ausers_mutex);

        // Responder handshake
        char response[] =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n";
        send(*accepted_sockfd, response, strlen(response), 0);

        for (int i = 0; i < ausers_n; i++) {
            printf("U%d: %s %s\n", i, ausers[i].uname, ausers[i].uip);
            fflush(stdout);
        }

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, accepted_sockfd) != 0) {
            perror("Error creando hilo");
            free(accepted_sockfd);
        }
        pthread_detach(client_thread);
    }

    close(socket_fd);
    return 0;
}