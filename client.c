#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void raise_error(const char *msg){
    perror(msg);
    exit(1);
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

int main(int argc, char *argv[]){
    int socket_fd, port_n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc<3){
        raise_error("Se requiere <hostname> <puerto>");
    }
    port_n = atoi(argv[2]);
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd<0){
        raise_error("Error abriendo socket");
    }
   
    char name[255];
    strcpy(name, argv[3]);
    server = gethostbyname(argv[1]);
    if (server==NULL){
        perror("Error host no existe");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy(
        (char *) server ->h_addr, 
        (char *) &serv_addr.sin_addr.s_addr, 
        server->h_length
    );
    serv_addr.sin_port = htons(port_n);

    // Connect
    if (
        connect(
            socket_fd, 
            (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)
        ) < 0
    ){
        raise_error("Conexion fallo");
    }

    // Ciclo de comunicacion
    char buffer[255];
    char handshake[1024];
    int n;

    snprintf(handshake, sizeof(handshake),
    "GET ?name=%s HTTP/1.1\r\n"
    "Host: localhost:%d\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "\r\n", name, port_n);
    n = write(socket_fd, handshake, strlen(handshake));
    if(n<0){
        raise_error("Error escribiendo");
    }
    n = read(socket_fd, buffer, 255);
        if(n<0){
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
    
    close(socket_fd);
    return 0;
}