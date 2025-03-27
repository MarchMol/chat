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