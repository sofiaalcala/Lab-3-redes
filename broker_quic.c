#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "mensaje.h"
 
#define PORT        7070
#define MAX_SUB     100
#define MAX_TEMAS   10
#define BUF_SIZE    50   
 
typedef struct {
    struct sockaddr_in addr;
    char tema[32];
    int  activo;
} SuscriptorQUIC;
 
static SuscriptorQUIC suscriptores[MAX_SUB];
 
typedef struct {
    char     tema[32];
    uint32_t esperado;           
    Mensaje  buf[BUF_SIZE];      /* mensajes recibidos fuera de orden */
    int      ocupado[BUF_SIZE];
} TemaEstado;
 
static TemaEstado temas[MAX_TEMAS];
 
static int misma_dir(struct sockaddr_in a, struct sockaddr_in b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr &&
           a.sin_port         == b.sin_port;
}
 
static int ya_suscrito(struct sockaddr_in cli, const char *tema) {
    for (int i = 0; i < MAX_SUB; i++)
        if (suscriptores[i].activo &&
            misma_dir(suscriptores[i].addr, cli) &&
            strcmp(suscriptores[i].tema, tema) == 0)
            return 1;
    return 0;
}
 
static void agregar_suscriptor(struct sockaddr_in cli, const char *tema) {
    for (int i = 0; i < MAX_SUB; i++) {
        if (!suscriptores[i].activo) {
            suscriptores[i].addr  = cli;
            strcpy(suscriptores[i].tema, tema);
            suscriptores[i].activo = 1;
            printf("[QUIC-Broker] Nuevo suscriptor tema=%s desde %s:%d\n",
                   tema, inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
            return;
        }
    }
    printf("[QUIC-Broker] Sin espacio para mas suscriptores\n");
}
 
/* Busca o crea el estado de un tema */
static TemaEstado *obtener_tema(const char *tema) {
    for (int i = 0; i < MAX_TEMAS; i++) {
        if (temas[i].tema[0] != '\0' && strcmp(temas[i].tema, tema) == 0)
            return &temas[i];
    }
    /* crear nuevo */
    for (int i = 0; i < MAX_TEMAS; i++) {
        if (temas[i].tema[0] == '\0') {
            strcpy(temas[i].tema, tema);
            temas[i].esperado = 1;
            memset(temas[i].ocupado, 0, sizeof(temas[i].ocupado));
            return &temas[i];
        }
    }
    return NULL;
}
 
static void entregar(int fd, Mensaje *msg) {
    for (int i = 0; i < MAX_SUB; i++) {
        if (suscriptores[i].activo &&
            strcmp(suscriptores[i].tema, msg->tema) == 0) {
            sendto(fd, msg, sizeof(Mensaje), 0,
                   (struct sockaddr *)&suscriptores[i].addr,
                   sizeof(suscriptores[i].addr));
        }
    }
}
 

static void vaciar_buffer(int fd, TemaEstado *te) {
    int encontrado = 1;
    while (encontrado) {
        encontrado = 0;
        for (int i = 0; i < BUF_SIZE; i++) {
            if (te->ocupado[i] && te->buf[i].seq_num == te->esperado) {
                printf("[QUIC-Broker] Entregando (buf) seq=%u tema=%s\n",
                       te->esperado, te->tema);
                entregar(fd, &te->buf[i]);
                te->ocupado[i] = 0;
                te->esperado++;
                encontrado = 1;
                break;
            }
        }
    }
}
 
static void guardar_en_buffer(TemaEstado *te, Mensaje *msg) {
    for (int i = 0; i < BUF_SIZE; i++) {
        if (!te->ocupado[i]) {
            te->buf[i]    = *msg;
            te->ocupado[i] = 1;
            printf("[QUIC-Broker] Guardado en buffer seq=%u tema=%s "
                   "(esperado=%u)\n",
                   msg->seq_num, msg->tema, te->esperado);
            return;
        }
    }
    printf("[QUIC-Broker] Buffer lleno, descartando seq=%u\n", msg->seq_num);
}
 
int main(void) {
    int sockfd;
    struct sockaddr_in servidor, cliente;
    socklen_t len;
    Mensaje msg;
 
    memset(suscriptores, 0, sizeof(suscriptores));
    memset(temas,        0, sizeof(temas));
 
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); exit(EXIT_FAILURE); }
 
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin_family      = AF_INET;
    servidor.sin_addr.s_addr = INADDR_ANY;
    servidor.sin_port        = htons(PORT);
 
    if (bind(sockfd, (struct sockaddr *)&servidor, sizeof(servidor)) < 0) {
        perror("bind"); close(sockfd); exit(EXIT_FAILURE);
    }
 
    printf("[QUIC-Broker] Escuchando en puerto %d...\n", PORT);
 
    while (1) {
        len = sizeof(cliente);
        int n = recvfrom(sockfd, &msg, sizeof(Mensaje), 0,
                         (struct sockaddr *)&cliente, &len);
        if (n < 0) { perror("recvfrom"); continue; }
 
        if (strcmp(msg.tipo, "SUBSCRIBE") == 0) {
            printf("[QUIC-Broker] SUBSCRIBE tema=%s\n", msg.tema);
            if (!ya_suscrito(cliente, msg.tema))
                agregar_suscriptor(cliente, msg.tema);
 
        } else if (strcmp(msg.tipo, "PUBLISH") == 0) {
            printf("[QUIC-Broker] PUBLISH tema=%s seq=%u contenido=%s\n",
                   msg.tema, msg.seq_num, msg.contenido);
 
            Mensaje ack;
            memset(&ack, 0, sizeof(Mensaje));
            strcpy(ack.tipo, "ACK");
            strcpy(ack.tema, msg.tema);
            snprintf(ack.contenido, sizeof(ack.contenido),
                     "Broker recibio seq %u", msg.seq_num);
            ack.seq_num = msg.seq_num;
            sendto(sockfd, &ack, sizeof(Mensaje), 0,
                   (struct sockaddr *)&cliente, len);
 
            TemaEstado *te = obtener_tema(msg.tema);
            if (!te) {
                printf("[QUIC-Broker] Sin espacio para tema %s\n", msg.tema);
                continue;
            }
 
            if (msg.seq_num == te->esperado) {
                printf("[QUIC-Broker] Entregando seq=%u tema=%s\n",
                       msg.seq_num, msg.tema);
                entregar(sockfd, &msg);
                te->esperado++;
                vaciar_buffer(sockfd, te);
 
            } else if (msg.seq_num > te->esperado) {
                guardar_en_buffer(te, &msg);
 
            } else {
                printf("[QUIC-Broker] Duplicado ignorado seq=%u\n",
                       msg.seq_num);
            }
        }
    }
 
    close(sockfd);
    return 0;
}
 