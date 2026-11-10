#ifndef ROBOT_H
#define ROBOT_H
/* El piloto y la acompanante en el coche del jugador (los "robots" ROB01 / ROB02 del original: sucesos 362 y 363 de
   la carrera, PS2 0x201b10 / 0x201c40 y 0x202c58 / 0x202de0; PC race_robots.cpp 4884D0 / 488DD0 y 4885A0 / 489A90).
   Datos: el bloque RBT1 de CHR.DAT (tools/conv_chr.py): esqueletos, movimientos, piezas con sus pesos y las tablas
   de la logica del ELF de PS2. Ver robot.c. */
#include <stdint.h>

#define ROBOT_N 3                 /* 0 el piloto, 1 la acompanante, 2 el banderero de la salida (si CHR.DAT lo trae) */
#define ROBOT_HUESOS 56                /* el esqueleto mayor (bone.bin 3) tiene 53 */

/* pieza del personaje (CHR_*.bin): rigida (un hueso), con pesos (hasta 4 matrices inv(enlace) M(hueso)) o una mano
   (puno o abierta, en su hueso) */
enum { RP_RIGIDA, RP_PESOS, RP_MANO_GU, RP_MANO_PA };
typedef struct { uint8_t per, tipo, n, r; uint16_t hueso[4], inv[4]; } rpieza_t;
typedef struct { uint16_t pieza, mat; } rmalla_t;                                 /* pieza y material (robot_material) */
typedef struct { int8_t nx, ny, nz, r; uint8_t w[4]; } rvert_t;                  /* normal x 127, pesos x 255 (la posicion: CHR.DAT) */

typedef struct {
    int   modelo;                 /* car+0x11 */
    int   s204;                   /* car+0x204: media del volante (4A4710; demo/main.c ani.s204) */
    int   fase;                   /* 0 cuenta atras (estado 8 del piloto), 1 carrera, 2 meta, 3 tiempo agotado */
    int   choque;                 /* car+2F0 >> 2 & 31 (estado de choque) */
    float t_choque, t_recup;      /* car+2C8 y car+2F8 */
    uint32_t sup;                 /* car+0x248: banderas del material bajo el coche (fis.sup_centro) */
    int   kmh;                    /* car+1F4 */
    int   s4e;                    /* car+0x4E (deriva) */
    int   clase_meta;             /* 44DC50 de la clave de la etapa (450380(8)): 0x1B, 0x1A / 0x1C eligen otra tabla de meta */
} robot_coche_t;

int   robot_inicia(const void *rbt, uint32_t bytes);       /* 0 = bien */
int   robot_listo(void);
void  robot_carrera(int modelo);                           /* init de los dos (salida) */
void  robot_paso(const robot_coche_t *c);                  /* un paso de logica (60 por segundo) */
const float (*robot_huesos(int p))[4][4];                  /* matrices de hueso en el marco del personaje */
const float (*robot_inversa(int p, int i))[4];             /* matriz inversa de enlace i del personaje p */
int   robot_mano_gu(int p);                                /* 1: el puno (513A30), 0: la mano abierta (513BA0) */
const float *robot_asiento(int modelo, int p);             /* +108 / +118 de la disposicion del coche */
int   robot_npiezas(void);
const rpieza_t *robot_pieza(int i);
const rmalla_t *robot_malla(int i);                        /* por malla del modelo de CHR.DAT */
const rvert_t  *robot_vert(int i);                         /* por vertice del modelo de CHR.DAT */
int   robot_nmat(void);
const float *robot_material(int i);                        /* difuso, ambiente y emision (9 floats, 1 = el 128 del GS) */
uint32_t robot_movimiento(int p);                          /* id del movimiento puesto (diagnostico) */
float robot_cuadro(int p);
int   robot_estado(int p);
/* el banderero de la salida (ROB03): personaje 2 de CHR.DAT; el mundo es el de la demo */
typedef struct {
    float ojo[3], mira[3];        /* camara +F8 / +104 (las del paso anterior: el suceso va antes de la camara) */
    float coche[3];               /* car+14 del jugador */
    float B0[4][4];               /* car+B0 (filas: derecha, arriba, atras; traslacion) */
    int   t8367bc;                /* temporizador de la salida */
} robot_band_t;
int  robot_band_hay(void);                                 /* CHR.DAT lo trae */
void robot_band_inicia(int preset, const float *marco);    /* 0x2046f0; marco de la etapa de salida {x, y, z, giro} */
void robot_band_quita(void);
void robot_band_paso(const robot_band_t *e);               /* 0x204888 */
int  robot_band_activo(void);
int  robot_band_visible(void);                             /* work+4 & 0x8000: la tela se dibuja */
const float (*robot_band_mundo(void))[4];                  /* work+10 */
uint32_t robot_band_textura(void);                         /* la textura de la tela en CHR.DAT */
void robot_band_tela(float (*pos)[3], float (*nor)[3]);    /* 35 vertices (fila x 5 + columna) */
#endif
