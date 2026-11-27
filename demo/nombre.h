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
    int desc4p;             /* Rev 2.9.9.c: 1 + el descriptor (44DC50) de la etapa del nivel 4 en los 15 recorridos (presets
                               2 / 3: los arboles 290 / 291 no estan en las tablas de RANKSP.DAT); 0 ninguno */
    uint8_t *ram; uint32_t ram_cap;   /* RAM fija para las tablas de sus bancos (fin_sp_t): sin ella no se entra */
    int consulta;           /* Rev 2.9.3: 0 la entrada de nombre; 1 consulta de las tablas de OUTRUN2SP, 2 las de UN JUGADOR
                               (sin registro ni fila del jugador; cuenta = la meta 0..4); 3 (Rev 2.9.4) la de tabla16 */
    int cuenta;
    const uint8_t *tabla16; /* Rev 2.9.4: consulta 3, una tabla de 10 registros de 16 bytes (rkta_fila16: los records de
                               la CONTRARRELOJ por codigo), leida como la variante 0 (tiempos) */
    char rotulo3[16];       /* Rev 2.9.4: consulta 3, el rotulo de la posicion (etapa / meta / curso y el sentido) en la
                               banda de abajo (DESVIACION: el original no tiene esta pantalla) */
} nombre_t;
int  nombre_entra(const nombre_t *d);       /* 49A960 + 0x1fc560; 0 bien */
int  nombre_paso(uint32_t nuevo);           /* 49AA60 + 0x1fcd30 (botones nuevos CONT_*); 1: pide el modo 27 */
void nombre_dibuja(float z);                /* 0x1fd1e8 (el tablero 0x200398) dentro de la lista TR */
void nombre_sale(void);
void nombre_pagina(int p);                  /* consulta: 0 puestos 1..5, 1 puestos 6..10 */
int  nombre_activo(void);                     /* 49AAE0 + 0x1fd528: suelta los bancos y su VRAM */
#endif
