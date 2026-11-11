#ifndef RKSP_H
#define RKSP_H
#include <stdint.h>
/* Rankings de OUTRUN2SP (las tablas del PS2 0x5d7548..0x5da108, PC 84DF40..850B00 de sp_rankings.cpp): ver comun/rksp.c.
   Registros de 16 bytes {+0 puntos 24 bits | ruta << 24 | coche << 28, +4 tiempo (ms), +8 cuenta 12 bits | coche de
   la licencia << 12 | cambio << 17 | configuracion << 18 (24 bits), +0xB nombre (4)}, 10 por tabla. */
#define RKSP_TAM 0x2bc0
uint8_t *rksp_tablas(void);                 /* las tablas (RKSP_TAM bytes) */
int rksp_hay(void);                         /* 1: leidas de la VMU o puestas por defecto */
void rksp_defecto(const uint8_t *def);      /* 0x1fa3b0: sin tablas, las de por defecto (RANKSP.DAT, NOM1) */
void rksp_pon(const uint8_t *t);            /* las leidas de la VMU */
/* desplazamiento de la tabla de 10 registros (4F3010 / 0x1fae90): variante 1 puntos, 2 corazones, 0 (y las demas)
   tiempo; preset 0..3, cuenta = meta (0..4), a = cambio, b = configuracion (variante 0) */
uint32_t rksp_tabla(int variante, int preset, int cuenta, int a, int b);
/* 0x1fae90 con un registro: lo inserta en su tabla y devuelve su puesto (0..9) o -1 */
int rksp_inserta(const uint8_t rec[16], int variante, int preset, int cuenta, int a, int b);
#endif
