#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "mensaje.h"

#define PORT 8080
#define MAX_CLIENTES 100

typedef struct {
    int socket;
    char tema[32];
    int activo;
} SuscriptorTCP;

SuscriptorTCP suscriptores[MAX_CLIENTES];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void agregar_suscriptor(int client_sock, const char *tema) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (!suscriptores[i].activo) {
            suscriptores[i].socket = client_sock;
            strcpy(suscriptores[i].tema, tema);
            suscriptores[i].activo = 1;
            printf("Nuevo suscriptor en socket %d al tema %s\n", client_sock, tema);
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

void eliminar_suscriptor(int client_sock) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (suscriptores[i].activo && suscriptores[i].socket == client_sock) {
            suscriptores[i].activo = 0;
        }
    }
    pthread_mutex_unlock(&lock);
}

void reenviar_a_suscriptores(Mensaje *msg) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (suscriptores[i].activo && strcmp(suscriptores[i].tema, msg->tema) == 0) {
            send(suscriptores[i].socket, msg, sizeof(Mensaje), 0);
        }
    }
    pthread_mutex_unlock(&lock);
}

void *manejar_cliente(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);

    Mensaje msg;
    int n;

    while ((n = recv(client_sock, &msg, sizeof(Mensaje), 0)) > 0) {
        if (strcmp(msg.tipo, "SUBSCRIBE") == 0) {
            printf("SUBSCRIBE recibido: tema=%s\n", msg.tema);
            agregar_suscriptor(client_sock, msg.tema);
        }
        else if (strcmp(msg.tipo, "PUBLISH") == 0) {
            printf("PUBLISH recibido: tema=%s seq=%u contenido=%s\n",
                   msg.tema, msg.seq_num, msg.contenido);

            reenviar_a_suscriptores(&msg);

            Mensaje ack;
            memset(&ack, 0, sizeof(Mensaje));
            strcpy(ack.tipo, "ACK");
            strcpy(ack.tema, msg.tema);
            snprintf(ack.contenido, sizeof(ack.contenido),
                     "Broker recibio seq %u", msg.seq_num);
            ack.seq_num = msg.seq_num;

            send(client_sock, &ack, sizeof(Mensaje), 0);
        }
    }

    printf("Cliente desconectado: socket %d\n", client_sock);
    eliminar_suscriptor(client_sock);
    close(client_sock);
    return NULL;
}

int main() {
    int server_fd, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    for (int i = 0; i < MAX_CLIENTES; i++) {
        suscriptores[i].activo = 0;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error al crear socket TCP");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Broker TCP escuchando en puerto %d...\n", PORT);

    while (1) {
        client_sock = malloc(sizeof(int));
        if (!client_sock) {
            perror("malloc");
            continue;
        }

        *client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_sock < 0) {
            perror("Error en accept");
            free(client_sock);
            continue;
        }

        printf("Nueva conexion desde %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        pthread_t tid;
        pthread_create(&tid, NULL, manejar_cliente, client_sock);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
