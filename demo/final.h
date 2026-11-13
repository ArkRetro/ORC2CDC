#ifndef FINAL_H
#define FINAL_H
#include <stdint.h>
/* El final de recreativa de OUTRUN2SP (modo 24 del original: PS2 0x1ba4d8 / 0x1ba5f0 / 0x1ba6a0, PC 49A660 / 49A710 /
   4527A0; el guion AUTOSCENE 4B5F60.. = PS2 0x28d878..). Datos: FINAL.DAT, FINEc.DAT, FINPROP.DAT y STAFF.DAT de
   tools/conv_final.py. Ver final.c. */

#define FINAL_PROPS 32
typedef struct {
    float ojo[3], mira[3];          /* camara +E0 / +EC (483E10 = PS2 0x17b540) en el mundo de la escena */
    float balanceo, fov;            /* +130 y +A4 (FOV horizontal de 4:3, rad) */
    int   coche;                    /* el coche del final (suceso 8 funcion 0x2D: fila +0x48 != 0) */
    float B0[4][4];                 /* car+B0: la carroceria (BD0000; identidad si el guion no la mueve) */
    int16_t s32, giro[4];           /* car+32 (BD0007) y car+40 / 42 / 44 / 46 (BD0008 / C / 10 / 14) */
    int   nprop;
    struct { uint16_t reg; float M[4][4]; } prop[FINAL_PROPS];   /* objetos 0xD0nnnn (405360) del cuadro */
    struct {                        /* la pareja del final (registros de tipo 2; 0 CHR_MAL, 1 CHR_FAL) */
        int   hay, vis;             /* robot creado (4B65B0) y work+4 & 0x8000 (se dibuja) */
        float M[4][4];              /* work+10 (487D10): su matriz en el mundo */
        uint32_t cara_a, cara_b;    /* rob_disp +10 / +14: la cara y la de antes (405450 a, b) */
        float w;                    /* 406800: 1 - (+8 / +4) (1 = solo a) */
        uint32_t ojo[2], mano[2];   /* +18 / +1C y +58 / +5C */
        float ojo_t[2][3], ojo_r[2][3];   /* +20 / +2C y +38 / +44 (514880) */
    } rob[2];
} final_vista_t;

/* 0x19bae0 (4527F0) para la variante 1: la columna (meta 0..4 = 450380(8) - 0xA, + 5 con los presets 0 / 2); -1 si
   no hay final para esa variante */
int  final_columna(int variante, int preset, int meta);
/* modo 24: ram / cap del hueco de la etapa (FINAL.DAT entero; el guion se usa en su sitio), staff / scap del hueco de
   la union prestado (tablas de STAFF.DAT). 0 bien */
int  final_entra(int columna, uint8_t *ram, uint32_t cap, uint8_t *staff, uint32_t scap);
int  final_paso(uint32_t nuevo);    /* un paso de 60 Hz (botones nuevos CONT_*); 0 = acabado (pasa al 25) */
const final_vista_t *final_vista(void);
void final_dibuja2d(void);          /* la capa 7 (los creditos) en la lista TR */
void final_sale(void);              /* 0x19ba90 (4527A0): suelta STAFF.DAT */
int  final_activo(void);
int  final_grupo(void);             /* el grupo de movimientos de la fila (+0x44: FINCHRgg.DAT) */
uint32_t final_usado(void);
int  final_fondo(float *p);         /* 4522D0: x, y, z, alfa del fondo (columnas 5..9); 0 sin fondo */         /* bytes de ram que ocupa FINAL.DAT (alineados a 32) */
#endif
