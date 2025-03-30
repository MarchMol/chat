#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void raise_error(const char *msg){
    perror(msg);
    exit(1);
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
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != NULL && clients[i]->socket_fd != sender_fd) {
            write(clients[i]->socket_fd, buffer, 3 + sender_len + message_len);
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
    accepted_sockfd = accept(scoket_fd, (struct sockaddr *) &cli_addr, &clilen);
    if (accepted_sockfd<0){
        raise_error("Error aceptando");
    }
    getpeername(accepted_sockfd, (struct sockaddr *) &cli_addr, &clilen);
    char client_ip[INET_ADDRSTRLEN]; // Buffer for IP address
    inet_ntop(AF_INET, &cli_addr.sin_addr, client_ip, sizeof(client_ip));

    // Reading loop
    int n;
    char buffer[255];
    while(1){
        n = read(accepted_sockfd, buffer, 255);
        if (n<0){
            raise_error("Error leyendo");
        }
        printf("Request\n%s",buffer);
        char *get_line;
        get_line= strtok(buffer, "\r\n");
        char method[10], path[100], http_version[10];
        sscanf(get_line, "%s %s %s", method, path, http_version);

        char name_str[50];
        char *name_start = strstr(path, "?");
        if (name_start) {
            sscanf(name_start, "?name=%49s", name_str);
        }
        printf("Name: %s IP: %s",name_str, client_ip);
        fflush(stdout);




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
                send_message_to_all(client->socket_fd, client->username, message);
            } else {  // Enviar mensaje a un usuario específico
                int recipient_fd = find_user_socket(recipient);
                if (recipient_fd == -1) {
                    // El usuario no está conectado, responder con un error
                    char error_msg[] = "El usuario no existe.";
                    write(client->socket_fd, error_msg, sizeof(error_msg));
                } else {
                    send_message_to_client(recipient_fd, client->username, message);
                }
            }
        }
    
    }
    close(accepted_sockfd);
    close(scoket_fd);
    return 0;
}