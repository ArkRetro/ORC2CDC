#ifndef SP_H
#define SP_H
#include <stdint.h>
/* Juego de OUTRUN2SP fuera del frontal de recreativa (ver demo/sp.c) */
/* 0x1b8228: la pantalla de carga del preset (banco 0x2F); ram, cap: RAM prestada para sus tablas y la lectura (o NULL) */
#define SP_TABLAS (64 * 1024)                       /* RAM para las tablas de CARGASPn.DAT (y sus piezas: 28..45 KB) */
int sp_carga_entra(int variante, int preset, void *ram, uint32_t cap);   /* 0 bien */
void sp_carga_cuadro(void);                         /* un cuadro de la escena entre dos pasos de la carga */
void sp_carga_dibuja(void);                         /* la escena, dentro de la lista TR (demo/pcarga.c) */
void sp_carga_sale(void);
void sp_carga_baja(void);                           /* sus texturas fuera de la VRAM (sigue abierto) */
int sp_carga_sube(void);                            /* y de vuelta; 0 bien */                           /* suelta CARGASP.DAT (antes de reservar la zona del curso) */
#endif
