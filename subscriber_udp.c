#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mensaje.h"

#define PORT 9090

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <tema>\n", argv[0]);
        printf("Temas validos: PartidoA o PartidoB\n");
        return 1;
    }

    char *tema = argv[1];

    if (strcmp(tema, "PartidoA") != 0 && strcmp(tema, "PartidoB") != 0) {
        printf("Tema invalido. Use PartidoA o PartidoB\n");
        return 1;
    }

    int sockfd;
    struct sockaddr_in broker;
    socklen_t len = sizeof(broker);
    Mensaje msg;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket UDP");
        return 1;
    }

    memset(&broker, 0, sizeof(broker));
    broker.sin_family = AF_INET;
    broker.sin_port = htons(PORT);
    broker.sin_addr.s_addr = inet_addr("127.0.0.1");

    memset(&msg, 0, sizeof(Mensaje));
    strcpy(msg.tipo, "SUBSCRIBE");
    strcpy(msg.tema, tema);
    strcpy(msg.contenido, "Solicitud de suscripcion");
    msg.seq_num = 0;

    sendto(sockfd, &msg, sizeof(Mensaje), 0,
           (struct sockaddr *)&broker, sizeof(broker));

    printf("Suscrito al tema %s en broker UDP puerto %d\n", tema, PORT);

    while (1) {
        int n = recvfrom(sockfd, &msg, sizeof(Mensaje), 0,
                         (struct sockaddr *)&broker, &len);

        if (n > 0) {
            printf("Recibido -> tipo=%s tema=%s seq=%u contenido=%s\n",
                   msg.tipo, msg.tema, msg.seq_num, msg.contenido);
        }
    }

    close(sockfd);
    return 0;
}
