#ifndef SPRANI_H
#define SPRANI_H
#include <stdint.h>
/* Reproductor de escenas SPRANI (animaciones 2D del original) para los bancos convertidos con tools/conv_sprani.py:
   la animacion se evalua en cada cuadro como el original (429460 / 429350 / 48B970 del PC) y las escenas se crean en el
   grupo de instancias del original (427F70 / 4282B0 / 428170: 21 capas x 64), con sus modos y su estado. Ver
   comun/sprani.c. Los tokens son los del original: banco << 16 | escena. */
typedef struct spr_banco spr_banco_t;
spr_banco_t *spr_abre(const char *fich);                /* abre un banco (solo tablas en la RAM); NULL si falla */
spr_banco_t *spr_abre_en(const char *fich, void *ram, uint32_t cap);   /* sus tablas en ram (cap bytes) si caben */
void spr_cierra(spr_banco_t *b);                        /* suelta sus instancias, su VRAM y su RAM */
const uint8_t *spr_extra(const spr_banco_t *b, uint32_t *tam);   /* tablas del conversor que lo uso (o NULL) */
int spr_sube_escena(uint32_t token);                    /* texturas de la escena a la VRAM; 0 bien */
void spr_baja_texturas(spr_banco_t *b);                 /* su VRAM fuera (las instancias vivas la vuelven a pedir) */
/* grupo de instancias */
enum { SPR_CAPAS = 21 };
int spr_crea(uint32_t token, int capa, int modo, int primero, int ultimo);   /* 428460 (ultimo < 0: hasta el final) */
int spr_crea_entera(uint32_t token, int capa, int modo);                     /* 428320 */
void spr_suelta(int h);                                 /* 4285A0 (h < 0: nada) */
void spr_velocidad(int h, float v);                     /* 428800 */
void spr_pon_cuadro(int h, int cuadro);                 /* 4288C0 */
void spr_traslada(int h, float x, float y);             /* 4287B0 con D3DXMatrixTranslation(x, y, 0) */
int spr_estado(int h);                                  /* 428880: 1 en curso, 2 dio la vuelta, 3 / 4 acabada (0 si h < 0) */
float spr_cuadro(int h);
void spr_suelta_todas(void);                            /* 428600 */
void spr_paso(void);                                    /* 427F70: un paso de 60 Hz */
void spr_inmediata(uint32_t token, float x, float y, int capa, int cuadro);  /* 429530: un cuadro en (x, y) */
/* dibuja las capas [c0, c1) en la lista TR; z: capa de z de hudd (fondo = 1: detras de la escena 3D). Las inmediatas
   de esas capas se dibujan y se olvidan */
void spr_dibuja(int c0, int c1, float z, int fondo);
#endif
