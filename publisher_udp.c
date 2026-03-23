#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mensaje.h"

#define PORT 9090
#define NUM_MENSAJES 10

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
    Mensaje msg, ack;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket UDP");
        return 1;
    }

    memset(&broker, 0, sizeof(broker));
    broker.sin_family = AF_INET;
    broker.sin_port = htons(PORT);
    broker.sin_addr.s_addr = inet_addr("127.0.0.1");

    for (uint32_t i = 1; i <= NUM_MENSAJES; i++) {
        memset(&msg, 0, sizeof(Mensaje));
        strcpy(msg.tipo, "PUBLISH");
        strcpy(msg.tema, tema);
        msg.seq_num = i;

        if (strcmp(tema, "PartidoA") == 0) {
            snprintf(msg.contenido, sizeof(msg.contenido),
                     "PartidoA - evento %u: jugada importante", i);
        } else {
            snprintf(msg.contenido, sizeof(msg.contenido),
                     "PartidoB - evento %u: actualizacion del partido", i);
        }

        sendto(sockfd, &msg, sizeof(Mensaje), 0,
               (struct sockaddr *)&broker, sizeof(broker));

        printf("Enviado -> tipo=%s tema=%s seq=%u contenido=%s\n",
               msg.tipo, msg.tema, msg.seq_num, msg.contenido);

        int n = recvfrom(sockfd, &ack, sizeof(Mensaje), 0,
                         (struct sockaddr *)&broker, &len);

        if (n > 0 && strcmp(ack.tipo, "ACK") == 0) {
            printf("ACK recibido -> tema=%s seq=%u mensaje=%s\n",
                   ack.tema, ack.seq_num, ack.contenido);
        }

        sleep(1);
    }

    close(sockfd);
    return 0;
}
