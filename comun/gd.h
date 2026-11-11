#ifndef GD_H
#define GD_H
#include <stdint.h>
int gd_busca(const char *nombre, uint32_t *lba, uint32_t *tam);
void *gd_carga(const char *nombre, uint32_t *tam);
int gd_carga_en(const char *nombre, void *dst, uint32_t cap, uint32_t *tam);   /* en dst (cap bytes); 0 bien */
int gd_tam(const char *nombre, uint32_t *tam);   /* tamano del fichero (de una union empaquetada: recompuesta) */
int gd_lee(uint32_t lba, int n, void *dst);           /* sectores de 2048 desde lba (por DMA si dst va alineado a 32) */
/* El hilo de la musica se declara con gd_hilo_fondo(); sus lecturas son "de fondo". gd_ocupado() dice si alguien mas
   (una carga: etapa, menus, HUD) ha leido del GD hace menos de ms milisegundos: el hilo de fondo espera entonces, para
   que la cabeza del lector no salte entre dos ficheros en cada trozo (en un GD real cada salto cuesta ~100-200 ms) */
void gd_hilo_fondo(void);
int gd_ocupado(uint32_t ms);
/* El hilo del juego se declara con gd_hilo_juego(): gd_us_juego() son los us que lleva parado leyendo del GD (el reloj
   de la logica no los cuenta: una carga no es tiempo de juego) y gd_n_juego() cuantas lecturas ha hecho */
void gd_hilo_juego(void);
uint64_t gd_us_juego(void);
uint32_t gd_n_juego(void);
/* intentos de lectura fallidos desde el arranque y el codigo del ultimo (ERR_* de cdrom.h) */
void gd_errores(uint32_t *n, int *ult);
#endif
