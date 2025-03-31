#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
void receive_message(int socket_fd);
void receive_history(int socket_fd);


void raise_error(const char *msg){
    perror(msg);
    exit(1);
}
// Validar nombre antes del handshake
int is_valid_username(const char *name) {
    return (strlen(name) > 0 && strcmp(name, "~") != 0);
}

// Enviar solicitud de listado de usuarios (tipo 1)
void list_users(int socket_fd) {
    char buffer[1] = {1};  // Tipo de mensaje 1
    write(socket_fd, buffer, 1);
}

// Obtener info de un usuario específico (tipo 2)
void get_user_info(int socket_fd, const char *username) {
    int len = strlen(username);
    char buffer[256];
    buffer[0] = 2;           // Tipo de mensaje
    buffer[1] = len;         // Longitud del nombre
    memcpy(buffer + 2, username, len);
    write(socket_fd, buffer, 2 + len);
}

// Procesar respuesta general del servidor
void handle_server_response(int socket_fd) {
    char buffer[1024];
    int n = read(socket_fd, buffer, sizeof(buffer));
    if (n < 0) {
        raise_error("Error leyendo respuesta del servidor");
    }

    int tipo = buffer[0];
    switch (tipo) {
        case 50: {  // Error
            int code = buffer[1];
            printf("Error del servidor (%d): ", code);
            switch (code) {
                case 1: printf("Usuario no existe.\n"); break;
                case 2: printf("Estatus inválido.\n"); break;
                case 3: printf("Mensaje vacío.\n"); break;
                case 4: printf("Usuario desconectado.\n"); break;
                default: printf("Desconocido.\n");
            }
            break;
        }
        case 51: {  // Lista de usuarios
            int count = buffer[1];
            int offset = 2;
            printf("Usuarios conectados (%d):\n", count);
            for (int i = 0; i < count; i++) {
                int len = buffer[offset++];
                char username[256];
                memcpy(username, buffer + offset, len);
                username[len] = '\0';
                offset += len;
                int status = buffer[offset++];
                const char *status_str = (status == 1) ? "ACTIVO" :
                                         (status == 2) ? "OCUPADO" :
                                         (status == 3) ? "INACTIVO" : "DESCONECTADO";
                printf("  - %s [%s]\n", username, status_str);
            }
            break;
        }
        case 52: {  // Info de usuario
            int len = buffer[1];
            char username[256];
            memcpy(username, buffer + 2, len);
            username[len] = '\0';
            int status = buffer[2 + len];
            const char *status_str = (status == 1) ? "ACTIVO" :
                                     (status == 2) ? "OCUPADO" :
                                     (status == 3) ? "INACTIVO" : "DESCONECTADO";
            printf("Información del usuario:\n");
            printf("  - Nombre: %s\n", username);
            printf("  - Estado: %s\n", status_str);
            break;
        }
        case 55:
            receive_message(socket_fd);
            break;
        case 56:
            receive_history(socket_fd);
            break;
        default:
            printf("Respuesta desconocida del servidor (%d)\n", tipo);
    }
}

// Función para enviar un mensaje para cambiar el estatus
void change_status(int socket_fd, const char *username, int status) {
    char message[256];
    int username_len = strlen(username);

    // Validamos el estatus
    if (status < 0 || status > 3) {
        printf("Estatus inválido. Debe ser entre 0 y 3.\n");
        return;
    }

    // Construimos el mensaje
    message[0] = 3;  // Tipo de mensaje (3 para cambiar estatus)
    message[1] = username_len;  // Longitud del nombre de usuario
    memcpy(message + 2, username, username_len);  // Nombre del usuario
    message[2 + username_len] = status;  // Estatus (0 a 3)

    // Enviamos el mensaje al servidor
    int n = write(socket_fd, message, 2 + username_len + 1);
    if (n < 0) {
        raise_error("Error escribiendo el mensaje");
    }
    printf("Estatus de %s cambiado a %d.\n", username, status);
}

void send_message(int socket_fd, const char *username, const char *dest, const char *message) {
    int dest_len = strlen(dest);
    int message_len = strlen(message);
    char buffer[256];

    // Tipo de mensaje = 4 (enviar mensaje)
    buffer[0] = 4; 
    buffer[1] = dest_len;  // Longitud del nombre del destinatario
    memcpy(buffer + 2, dest, dest_len);  // Nombre del destinatario
    buffer[2 + dest_len] = message_len;  // Longitud del mensaje
    memcpy(buffer + 3 + dest_len, message, message_len);  // Mensaje

    // Enviar el mensaje al servidor
    int n = write(socket_fd, buffer, 3 + dest_len + message_len);
    if (n < 0) {
        raise_error("Error escribiendo el mensaje");
    }
    printf("Mensaje enviado a %s: %s\n", dest, message);
}

void receive_message(int socket_fd) {
    char buffer[256];
    int n = read(socket_fd, buffer, 256);
    if (n < 0) {
        raise_error("Error leyendo la respuesta del servidor");
    }

    // Analizar el tipo de mensaje recibido
    int response_type = buffer[0];
    if (response_type == 55) {  // Tipo 55: Recibió mensaje
        int username_len = buffer[1];
        char origin[256];
        memcpy(origin, buffer + 2, username_len);
        origin[username_len] = '\0';  // Asegurar el fin de la cadena

        int message_len = buffer[2 + username_len];
        char message[256];
        memcpy(message, buffer + 3 + username_len, message_len);
        message[message_len] = '\0';  // Asegurar el fin de la cadena

        // Mostrar el mensaje recibido
        printf("Mensaje recibido de %s: %s\n", origin, message);
    }
}

void request_history(int socket_fd, const char *chat_name) {
    int chat_name_len = strlen(chat_name);
    char buffer[256];

    // Tipo de mensaje = 5 (solicitar historial)
    buffer[0] = 5;
    buffer[1] = chat_name_len;
    memcpy(buffer + 2, chat_name, chat_name_len);

    // Enviar la solicitud al servidor
    int n = write(socket_fd, buffer, 2 + chat_name_len);
    if (n < 0) {
        raise_error("Error solicitando historial de mensajes");
    }

    // Esperar respuesta del servidor
    receive_history(socket_fd);
}

void receive_history(int socket_fd) {
    char buffer[1024];
    int n = read(socket_fd, buffer, sizeof(buffer));
    if (n < 0) {
        raise_error("Error leyendo respuesta del servidor");
    }

    // Analizar el tipo de respuesta
    if (buffer[0] == 56) {
        int num_messages = buffer[1];
        printf("Número de mensajes en el historial: %d\n", num_messages);

        int offset = 2;
        for (int i = 0; i < num_messages; i++) {
            int username_len = buffer[offset++];
            char username[256];
            memcpy(username, buffer + offset, username_len);
            username[username_len] = '\0';
            offset += username_len;

            int message_len = buffer[offset++];
            char message[256];
            memcpy(message, buffer + offset, message_len);
            message[message_len] = '\0';
            offset += message_len;

            printf("Mensaje de %s: %s\n", username, message);
        }
    } else {
        printf("Error: %s\n", buffer + 1);  // Mensaje de error
    }
}

int main(int argc, char *argv[]){
    int socket_fd, port_n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 4) {
        raise_error("Uso: ./client <hostname> <puerto> <nombre>");
    }

    port_n = atoi(argv[2]);
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0){
        raise_error("Error abriendo socket");
    }

    char name[255];
    strcpy(name, argv[3]);
    if (!is_valid_username(name)) {
        printf("Nombre de usuario inválido. No puede ser vacío ni '~'.\n");
        exit(1);
    }

    server = gethostbyname(argv[1]);
    if (server == NULL){
        perror("Error host no existe");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port_n);

    if (connect(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        raise_error("Conexion fallo");
    }

    char buffer[255];
    char handshake[1024];
    int n;

    snprintf(handshake, sizeof(handshake),
        "GET ?name=%s HTTP/1.1\r\n"
        "Host: localhost:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n\r\n", name, port_n);
    n = write(socket_fd, handshake, strlen(handshake));
    if(n < 0){
        raise_error("Error escribiendo");
    }
    n = read(socket_fd, buffer, 255);
    if(n < 0){
        raise_error("Error leyendo");
    }
    printf("Server: %s\n", buffer);

    // Escritura
        // bzero(buffer, 255);
        // fgets(buffer, 255, stdin);
        // n = write(socket_fd, buffer, strlen(buffer));
        // if(n<0){
        //     raise_error("Error escribiendo");
        // }
        // // Lectura
        // bzero(buffer, 255);
        // n = read(socket_fd, buffer, 255);
        // if(n<0){
        //     raise_error("Error leyendo");
        // }
        // printf("Server: %s\n", buffer);

        // // Condicion de serrado
        // int i = strncmp("close",buffer, 5);
        // if (i == 0){
        //     break;
        // }
    // Menú interactivo
    int opcion;
    char input[256];
    do {
        printf("\nMenú de opciones:\n");
        printf("1. Listar usuarios conectados\n");
        printf("2. Obtener información de un usuario\n");
        printf("3. Cambiar estatus\n");
        printf("4. Enviar mensaje\n");
        printf("5. Ver historial de chat\n");
        printf("6. Esperar mensaje del servidor\n");
        printf("0. Salir\n");
        printf("Selecciona una opción: ");
        scanf("%d", &opcion);
        getchar();

        switch (opcion) {
            case 1:
                list_users(socket_fd);
                handle_server_response(socket_fd);
                break;
            case 2:
                printf("Nombre del usuario: ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                get_user_info(socket_fd, input);
                handle_server_response(socket_fd);
                break;
            case 3: {
                printf("Nuevo estatus (1 = ACTIVO, 2 = OCUPADO, 3 = INACTIVO): ");
                int status;
                scanf("%d", &status);
                getchar();
                change_status(socket_fd, name, status);
                break;
            }
            case 4:
                printf("Enviar a (~ para general o nombre del usuario): ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                char destino[256];
                strcpy(destino, input);
                printf("Mensaje: ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                send_message(socket_fd, name, destino, input);
                break;
            case 5:
                printf("Historial con (~ para general o nombre del usuario): ");
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0';
                request_history(socket_fd, input);
                break;
            case 6:
                printf("Esperando mensaje...\n");
                handle_server_response(socket_fd);
                break;
            case 0:
                printf("Cerrando sesión...\n");
                break;
            default:
                printf("Opción inválida.\n");
        }
    } while (opcion != 0);

    close(socket_fd);
    return 0;
}