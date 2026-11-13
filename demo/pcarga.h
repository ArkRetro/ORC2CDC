#ifndef PCARGA_H
#define PCARGA_H
#include <stdint.h>
/* Pantalla de carga al empezar un modo de un jugador (demo/pcarga.c): se ve desde que se sale del menu hasta que la
   carrera esta lista (etapa, texturas, coche, pareja, voces, HUD), con un minimo, y se anima entre trozo y trozo de las
   lecturas del GD (gd_latido). Al principio del modo, no entre etapas. */
enum { PCARGA_NADA, PCARGA_MAPA_SP, PCARGA_PROPIA };
/* GANCHO para los modos: la pantalla de un modo (variante de [780258]: 1 OutRun, 2 Heart Attack, 7 Time Attack, 6 Coast
   2 Coast (misiones), 5 sus campeonatos; juego_sp: OUTRUN2SP). Un modo que empiece por la salida de demo/main.c
   (MENU_CARRERA) la tiene ya; si lleva su propia carga, pcarga_entra(PCARGA_PROPIA, ...), pcarga_avance entre sus pasos,
   pcarga_pausa / pcarga_sigue alrededor de lo que suba a la VRAM para toda la carrera y pcarga_suelta antes de reservar
   la zona del curso */
int pcarga_tipo(int variante, int juego_sp);
/* empieza. PCARGA_MAPA_SP: la escena del preset de CARGASPn.DAT con sus tablas en ram (cap >= SP_TABLAS, RAM prestada);
   minimo: tiempo minimo en pantalla (ms) */
void pcarga_entra(int tipo, int variante, int preset, void *ram, uint32_t cap, uint32_t minimo);
void pcarga_avance(float f);                /* lo cargado (0..1: la barra de la propia) y un cuadro si toca */
void pcarga_cuadro(void);                   /* un cuadro si toca (cada 1/60 s como mucho; lo llama gd_latido) */
void pcarga_pausa(void);                    /* sus texturas fuera (se ve su ultimo cuadro) mientras se sube a la VRAM */
void pcarga_sigue(void);                    /* lo que se queda en la carrera; y de vuelta */
void pcarga_suelta(void);                   /* espera al minimo, ultimo cuadro y fuera de la VRAM (antes de la zona) */
int pcarga_activa(void);
#define PCARGA_MINIMO 3000                  /* ms (DESVIACION pedida por el usuario: docs/PARIDAD.md) */
#endif
