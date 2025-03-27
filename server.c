#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

void raise_error(const char *msg){
    perror(msg);
    exit(1);
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

    // Constant read/write
    int n;
    char buffer[255];
    while(1){
        // Lee si puede
        bzero(buffer, 255);
        n = read(accepted_sockfd, buffer, 255);
        if (n<0){
            raise_error("Error leyendo");
        }
        printf("Client: %s\n", buffer);
        // Escribe si puede
        bzero(buffer, 255);
        fgets(buffer, 255, stdin);
        n = write(accepted_sockfd, buffer, strlen(buffer));
        if (n<0){
            raise_error("Error escribiendo");
        }
        // Cerrar servidor desde cliente
        int i = strncmp("close",buffer, 5);
        if (i == 0){
            break;
        }
    }
    close(accepted_sockfd);
    close(scoket_fd);
    return 0;
}