#ifndef SP_H
#define SP_H
/* Juego de OUTRUN2SP fuera del frontal de recreativa (ver demo/sp.c) */
void sp_carga_entra(int variante, int preset);      /* 0x1b8228: la pantalla de carga del preset (banco 0x2F) */
void sp_carga_cuadro(void);                         /* un cuadro de la escena entre dos pasos de la carga */
void sp_carga_sale(void);                           /* suelta CARGASP.DAT (antes de reservar la zona del curso) */
#endif
