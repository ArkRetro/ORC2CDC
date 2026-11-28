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
/* Rev 2.9.3: los records de UN JUGADOR (mismo formato y orden; vacias hasta el primer registro: nombre vacio) */
uint8_t *rkuj_tablas(void);
int rkuj_hay(void);                         /* 1: leidas de la VMU o con algun registro */
void rkuj_pon(const uint8_t *t);
int rkuj_inserta(const uint8_t rec[16], int variante, int preset, int cuenta, int a, int b);
/* Rev 2.9.4: RKU2, los records de UN JUGADOR en la VMU solo con las 42 tablas que se usan (RKUJ copiaba las 0x2bc0
   enteras): rkuj_compacta escribe RKUJ2_TAM bytes; rkuj_expande los lee (en la RAM sigue el formato de rksp) */
#define RKUJ2_N 42
#define RKUJ2_TAM (RKUJ2_N * 160)
void rkuj_compacta(uint8_t *out);
int rkuj_expande(const uint8_t *in, uint32_t tam);     /* 0 bien */
/* Rev 2.9.4: RKTA, los records de la CONTRARRELOJ por codigo que no son las metas 0x3c..0x45 (esas van en rkuj):
   etapas 0x00..0x3b, metas del reves 0x46..0x4f y cursos seguidos 0x50..0x53 (RKTA_N tablas de RKTA_FILAS filas de 8
   bytes: u32 tiempo en ms (bits 0..22; 0x7FFFFF vacia) | modelo << 23 (4 bits) | bandera de la licencia << 27 (5 bits),
   char nombre[4] con el cambio en el bit 7 del primero). Ver comun/rksp.c */
#define RKTA_N 0x4a
#define RKTA_FILAS 10
#define RKTA_TAM (RKTA_N * RKTA_FILAS * 8)
int rkta_indice(int codigo);                /* la tabla del codigo, o -1 (las metas 0x3c..0x45 y lo demas) */
uint8_t *rkta_tablas(void);
int rkta_hay(void);                         /* 1: leidas de la VMU o con algun registro */
int rkta_pon(const uint8_t *t, uint32_t tam);   /* el bloque de la VMU (sin la marca ni el tam); 0 bien */
int rkta_bloque(uint8_t *out);              /* el bloque para la VMU (u16 n, u16 filas, tablas): sus bytes */
#define RKTA_BLOQUE (4 + RKTA_TAM)
/* inserta (con < estricto, los de detras bajan) y devuelve el puesto 0..9 o -1 */
int rkta_inserta(int codigo, uint32_t ms, int modelo, int cambio, int bandera, const char *nombre);
/* la tabla del codigo en el formato de 16 bytes de rksp (tiempo en +4, 0xFFFFFFFF vacia; modelo << 28; cambio << 17,
   bandera << 12; nombre en +0xB) para los tableros y la consulta; 0 bien, -1 sin tabla */
int rkta_fila16(int codigo, uint8_t out[160]);
#endif
