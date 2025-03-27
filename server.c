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
    }
    close(accepted_sockfd);
    close(scoket_fd);
    return 0;
}