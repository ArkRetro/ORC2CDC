#ifndef SP_H
#define SP_H
#include <stdint.h>
/* Juego de OUTRUN2SP fuera del frontal de recreativa (ver demo/sp.c) */
/* 0x1b8228: la pantalla de carga del preset (banco 0x2F); ram, cap: RAM prestada para sus tablas y la lectura (o NULL) */
void sp_carga_entra(int variante, int preset, void *ram, uint32_t cap);
void sp_carga_cuadro(void);                         /* un cuadro de la escena entre dos pasos de la carga */
void sp_carga_sale(void);                           /* suelta CARGASP.DAT (antes de reservar la zona del curso) */
#endif
