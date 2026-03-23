#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mensaje.h"

#define PORT 9090
#define MAX_SUBSCRIBERS 100

typedef struct {
    struct sockaddr_in addr;
    char tema[32];
    int activo;
} SuscriptorUDP;

SuscriptorUDP suscriptores[MAX_SUBSCRIBERS];

int misma_direccion(struct sockaddr_in a, struct sockaddr_in b) {
    return (a.sin_addr.s_addr == b.sin_addr.s_addr) && (a.sin_port == b.sin_port);
}

int ya_suscrito(struct sockaddr_in cliente, const char *tema) {
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (suscriptores[i].activo &&
            misma_direccion(suscriptores[i].addr, cliente) &&
            strcmp(suscriptores[i].tema, tema) == 0) {
            return 1;
        }
    }
    return 0;
}

void agregar_suscriptor(struct sockaddr_in cliente, const char *tema) {
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (!suscriptores[i].activo) {
            suscriptores[i].addr = cliente;
            strcpy(suscriptores[i].tema, tema);
            suscriptores[i].activo = 1;
            printf("Nuevo suscriptor al tema %s desde %s:%d\n",
                   tema,
                   inet_ntoa(cliente.sin_addr),
                   ntohs(cliente.sin_port));
            return;
        }
    }
    printf("No hay espacio para mas suscriptores\n");
}

void reenviar_a_suscriptores(int sockfd, Mensaje *msg) {
    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        if (suscriptores[i].activo && strcmp(suscriptores[i].tema, msg->tema) == 0) {
            sendto(sockfd, msg, sizeof(Mensaje), 0,
                   (struct sockaddr *)&suscriptores[i].addr,
                   sizeof(suscriptores[i].addr));
        }
    }
}

int main() {
    int sockfd;
    struct sockaddr_in servidor, cliente;
    socklen_t len;
    Mensaje msg;

    for (int i = 0; i < MAX_SUBSCRIBERS; i++) {
        suscriptores[i].activo = 0;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error al crear socket UDP");
        exit(EXIT_FAILURE);
    }

    memset(&servidor, 0, sizeof(servidor));
    memset(&cliente, 0, sizeof(cliente));

    servidor.sin_family = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        perror("Error en bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Broker UDP escuchando en el puerto %d...\n", PORT);

    while (1) {
        len = sizeof(cliente);
        int n = recvfrom(sockfd, &msg, sizeof(Mensaje), 0,
                         (struct sockaddr *)&cliente, &len);

        if (n < 0) {
            perror("Error en recvfrom");
            continue;
        }

        if (strcmp(msg.tipo, "SUBSCRIBE") == 0) {
            printf("SUBSCRIBE recibido: tema=%s\n", msg.tema);

            if (!ya_suscrito(cliente, msg.tema)) {
                agregar_suscriptor(cliente, msg.tema);
            }

        } else if (strcmp(msg.tipo, "PUBLISH") == 0) {
            printf("PUBLISH recibido: tema=%s, seq=%u, contenido=%s\n",
                   msg.tema, msg.seq_num, msg.contenido);

            reenviar_a_suscriptores(sockfd, &msg);

            Mensaje ack;
            memset(&ack, 0, sizeof(Mensaje));
            strcpy(ack.tipo, "ACK");
            strcpy(ack.tema, msg.tema);
            snprintf(ack.contenido, sizeof(ack.contenido),
                     "Broker recibio seq %u", msg.seq_num);
            ack.seq_num = msg.seq_num;

            sendto(sockfd, &ack, sizeof(Mensaje), 0,
                   (struct sockaddr *)&cliente, len);
        }
    }

    close(sockfd);
    return 0;
}
