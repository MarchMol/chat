#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
void receive_message(int socket_fd);
void receive_history(int socket_fd);
// Realizar la generación de Sec-WebSocket-Key
const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(const uint8_t *in, size_t in_len, char *out) {
    int i, j;
    for (i = 0, j = 0; i < in_len;) {
        uint32_t octet_a = i < in_len ? in[i++] : 0;
        uint32_t octet_b = i < in_len ? in[i++] : 0;
        uint32_t octet_c = i < in_len ? in[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = base64_table[(triple >> 18) & 0x3F];
        out[j++] = base64_table[(triple >> 12) & 0x3F];
        out[j++] = base64_table[(triple >> 6) & 0x3F];
        out[j++] = base64_table[triple & 0x3F];
    }

    int mod = in_len % 3;
    if (mod > 0) out[j - 1] = '=';
    if (mod == 1) out[j - 2] = '=';
    out[j] = '\0';
}
// Función para generar 16 bytes aleatorios
void generate_websocket_key(char *output) {
    uint8_t random_key[16];
    srand(time(NULL));
    for (int i = 0; i < 16; ++i) {
        random_key[i] = rand() % 256;
    }
    base64_encode(random_key, 16, output);
}

// función para hacer el framing WebSocket básico (sin fragmentación)
int websocket_send(int socket_fd, const char *data, size_t data_len) {
    uint8_t frame[1024];
    size_t pos = 0;

    frame[pos++] = 0x81; // FIN + opcode 0x1 (text/binary según lo manejes)

    // Payload length
    if (data_len <= 125) {
        frame[pos++] = 0x80 | data_len; // Mask bit 1 + len
    } else if (data_len <= 65535) {
        frame[pos++] = 0x80 | 126;
        frame[pos++] = (data_len >> 8) & 0xFF;
        frame[pos++] = data_len & 0xFF;
    } else {
        // Not expected here, pero puedes extender para > 65535
        return -1;
    }

    // Masking key (cliente siempre enmascara)
    uint8_t mask[4];
    for (int i = 0; i < 4; ++i) {
        mask[i] = rand() % 256;
        frame[pos++] = mask[i];
    }

    // Enmascarar y copiar datos
    for (size_t i = 0; i < data_len; ++i) {
        frame[pos++] = data[i] ^ mask[i % 4];
    }

    return write(socket_fd, frame, pos);
}
int websocket_receive(int socket_fd, char *output, size_t max_len) {
    uint8_t header[2];
    if (read(socket_fd, header, 2) != 2) return -1;

    int fin = (header[0] >> 7) & 1;
    int opcode = header[0] & 0x0F;
    int masked = (header[1] >> 7) & 1;
    int payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2];
        if (read(socket_fd, ext, 2) != 2) return -1;
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (read(socket_fd, ext, 8) != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    if (payload_len >= max_len) return -1;

    uint8_t mask[4];
    if (masked) {
        if (read(socket_fd, mask, 4) != 4) return -1;
    }

    for (int i = 0; i < payload_len; i++) {
        uint8_t byte;
        if (read(socket_fd, &byte, 1) != 1) return -1;
        output[i] = masked ? (byte ^ mask[i % 4]) : byte;
    }

    output[payload_len] = '\0';  // 🔥 Asegurar fin de cadena si es texto
    return payload_len;
}


void raise_error(const char *msg){
    perror(msg);
    exit(1);
}
// Enviar solicitud de listado de usuarios (tipo 1)
void list_users(int socket_fd) {
    char buffer[1] = {1};  // Tipo de mensaje 1
    websocket_send(socket_fd, buffer, 1);
}

// Obtener info de un usuario específico (tipo 2)
void get_user_info(int socket_fd, const char *username) {
    int len = strlen(username);
    char buffer[256];
    buffer[0] = 2;           // Tipo de mensaje
    buffer[1] = len;         // Longitud del nombre
    memcpy(buffer + 2, username, len);
    websocket_send(socket_fd, buffer, 2 + len);
}

// Procesar respuesta general del servidor
void handle_server_response(int socket_fd) {
    char buffer[1024];
    int n = websocket_receive(socket_fd, buffer, sizeof(buffer));
    if (n <= 0) raise_error("Error al recibir datos del servidor.");


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
    int n = websocket_send(socket_fd, message, 2 + username_len + 1);
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
    int n = websocket_send(socket_fd, buffer, 3 + dest_len + message_len);
    if (n < 0) {
        raise_error("Error escribiendo el mensaje");
    }
    printf("Mensaje enviado a %s: %s\n", dest, message);
}

void receive_message(int socket_fd) {
    char buffer[256];
    int n = websocket_receive(socket_fd, buffer, sizeof(buffer));
    if (n < 0) raise_error("Error leyendo respuesta del servidor");

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
    int n = websocket_send(socket_fd, buffer, 2 + chat_name_len);
    if (n < 0) {
        raise_error("Error solicitando historial de mensajes");
    }

    // Esperar respuesta del servidor
    receive_history(socket_fd);
}

void receive_history(int socket_fd) {
    char buffer[1024];
    int n = websocket_receive(socket_fd, buffer, sizeof(buffer));
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
        fwrite(buffer + 1, 1, n - 1, stdout);
        printf("\n");// Mensaje de error
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

    server = gethostbyname(argv[1]);
    if (server == NULL){
        perror("Error host no existe");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(port_n);
    // Validar nombre antes del handshake
    if (strcmp(name, "~") == 0 || strlen(name) == 0) {
        raise_error("El nombre de usuario no puede ser '~' ni vacío.");
    }    
    // Conexión 
    if (connect(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        raise_error("Conexion fallo");
    }

    char buffer[255];
    char handshake[1024];
    int n;
    // generar Sec-WebSocket-Key
    char sec_key[32];
    generate_websocket_key(sec_key);
    snprintf(handshake, sizeof(handshake),
        "GET /?name=%s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Key: %s\r\n"
    "\r\n", name, argv[1], port_n, sec_key);

    n = write(socket_fd, handshake, strlen(handshake));
    if(n < 0){
        raise_error("Error escribiendo");
    }
    // Validar respuesta del servidor al handshake
    n = read(socket_fd, buffer, sizeof(buffer)-1);
    if (n < 0) raise_error("Error leyendo respuesta del servidor");
    buffer[n] = '\0';

    if (strstr(buffer, "HTTP/1.1 101") == NULL) {
        raise_error("El servidor no aceptó el upgrade a WebSocket.");
    }
    printf("Conexión establecida: %s\n", buffer);


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
    // Enviar código de cierre (opcional según protocolo real, no necesario si solo cierras TCP)
    char close_frame[2] = {0x88, 0x00};  // Opcode 8 (close), payload length 0
    websocket_send(socket_fd, close_frame, 2);

    close(socket_fd);
    return 0;
}
