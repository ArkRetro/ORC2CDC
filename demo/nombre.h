#ifndef NOMBRE_H
#define NOMBRE_H
#include <stdint.h>
/* OUTRUN2SP: entrada de nombre y rankings (modo 26 del original, suceso 0x189 funcion 0x16). Ver demo/nombre.c. */
typedef struct {
    int variante;           /* [780258]: 1 OutRun (puntos), 2 Infarto (corazones), 0 Contrarreloj (tiempo) */
    int preset;             /* [78024C] */
    int puntos;             /* 4B99D0 (0x1bf0e8) */
    uint32_t tiempo;        /* 451180(0) (0x1cfe98): ms del tiempo de la meta (0 sin acabar) */
    int corazones;          /* 45B830 / 45B840 (7F23C4 / 10000) */
    int modelo, manual, config;   /* 48B140 / 48B180 / 48B1A0 (0x1bcfb0 / 0x1bcff0 / 0x1bd010) */
    int ruta;               /* 450320 (0x1ff718): bit n = derecha en el nivel n */
    int registro4;          /* indice del registro del arbol de la etapa del nivel 4 (450560(4)); -1 sin llegar */
} nombre_t;
int  nombre_entra(const nombre_t *d);       /* 49A960 + 0x1fc560; 0 bien */
int  nombre_paso(uint32_t nuevo);           /* 49AA60 + 0x1fcd30 (botones nuevos CONT_*); 1: pide el modo 27 */
void nombre_dibuja(float z);                /* 0x1fd1e8 (el tablero 0x200398) dentro de la lista TR */
void nombre_sale(void);                     /* 49AAE0 + 0x1fd528: suelta los bancos y su VRAM */
#endif
