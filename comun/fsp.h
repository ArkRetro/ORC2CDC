#ifndef FSP_H
#define FSP_H
#include <stdint.h>
/* Frontal de recreativa de OUTRUN2SP (modo 10 ATTRACT del original con el suceso 4 de arcade_attract.cpp; ver
   comun/fsp.c): "Single Play", coche (con el coche 3D), transmision, modo de juego (OutRun / Infarto / Contrarreloj con
   los cursos de OutRun 2 o de OutRun 2 SP), configuracion del coche (Contrarreloj) y musica. Graficos: SPFE.DAT. */
enum { FSP_SIGUE, FSP_CARRERA, FSP_MENU };
/* RAM para las tablas de SPFE.DAT (la pone demo/main.c: el final del hueco de la union; tam 0 la devuelve) o NULL */
extern void *(*fsp_hueco)(uint32_t tam);
int fsp_entra(void);                        /* abre SPFE.DAT y empieza (48B210 / 48AE90); 0 bien */
int fsp_paso(unsigned nuevos);              /* un paso de logica (60 Hz) */
void fsp_dibuja(int fondo_tras_coche);      /* lista TR; 1: las capas 0..7 detras del coche 3D (448FD0(0x100)) */
void fsp_sale(void);                        /* suelta SPFE.DAT y su VRAM */
int fsp_activo(void);
int fsp_coche3d(void);                      /* 84A318 == 2: el coche de la eleccion (49F4D0) */
/* lo elegido (4C05F0: 48B130 / 48B150 / 48B170 / 48B190 / 48B1B0 / 48B1D0, 43F940, 43F950) */
int fsp_modelo(void);                       /* modelo del original (car+0x11) */
int fsp_color(void);                        /* 1..8 (car+0x12) */
int fsp_manual(void);                       /* 0 AT, 1 MT */
int fsp_variante(void);                     /* 1 OutRun, 2 Infarto, 0 Contrarreloj */
int fsp_preset(void);                       /* +27 (43F950): lado 0 (rotulo "Recorrido OutRun2SP") 0, lado 1 ("OutRun2") 1 */
int fsp_musica(void);                       /* tema de carrera (MUSn.ADP) */
int fsp_config(void);                       /* Contrarreloj: 0 normal, 1 modificado (+23); -1 sin elegir */
int fsp_balance(void);                      /* +21: balance musica / motor (0..2) */
#endif
