#ifndef MENSAJE_H
#define MENSAJE_H

#include <stdint.h>
typedef struct {
    char tipo[16];        // "PUBLISH", "SUBSCRIBE", "ACK"
    char tema[32];        // "PartidoA", "PartidoB"
    char contenido[256];  // "Gol de Equipo A al minuto 32"
    uint32_t seq_num;     // numero de secuencia
} Mensaje;
#endif
