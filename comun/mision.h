#ifndef MISION_H
#define MISION_H
#include <stdint.h>
/* El gestor de las misiones de COAST 2 COAST en la carrera (496A30 = PS2 0x1f8d0c y lo que llama: 477B60 = 0x1f13c8,
   495C20 = 0x1f85d0, 495E90 = 0x1f8930, 4961F0 = 0x1f9ea8, 495A20 = 0x1f81c8): los rotulos del principio, las reglas
   de los tipos 1 (eliminacion por tiempo), 2 (derrape), 3 (rebufo) y 6 (pillar al rival), la nota de todos los tipos, el
   fin de la mision y las marcas de los puntos. Ver mision.c. Lo de los coches de carreras: comun/rival.c. */

/* lo que el gestor lee del jugador en cada paso de logica (campos del original entre parentesis) */
typedef struct {
    int   d36;          /* car+0xD36: el derrape (> 0)                                           */
    float kmh;          /* car+0x1C4 x 216,72                                                    */
    float tope_kmh;     /* (+0x134C del coche - 2,3) x -131,38678 + 318 (0x1f9550)               */
    int   fuera;        /* car+0x244 & 0x9C (fuera del asfalto o contra el muro)                 */
    int8_t *da4;        /* car+0xDA4: cuadros desde el ultimo coche tocado (el tipo 3 lo pone a 0x1E) */
    int   choque;       /* car+4: 0x10000 y 0x4000000 (otro coche le ha cambiado la velocidad)    */
    float rebufo;       /* car+0xE68                                                             */
    int   beta;         /* car+0x162                                                             */
    int   interior;     /* camara +0x34A <= 1: las marcas en (234, 108)                           */
    float mx[2], my[2]; /* 495D90: el punto de las marcas en la pantalla (desde el centro), [1] con x - 0,7 */
} mision_jug_t;

/* los elementos de la interfaz de 496A30 (465860 / 4659F0): 0 = 836590 (2B0020), 1 = 836450 (el rotulo), 2 = 8364F0
   (la marca), 3 = 8363B0 (la nota). nombre: el uso de C2CHUD.DAT / C2CH*.DAT */
typedef struct {
    int vivo, capa, modo, primero, ultimo, n;
    char nombre[12];
    float cuadro, vel, x, y, sx, sy;
    float x0, y0, x1, y1, sx0, sy0, sx1, sy1; int tm, nm, ts, ns;
} mision_el_t;
#define MISION_NEL 4

extern int  (*mision_cuadros)(const char *nombre);   /* cuadros del grupo de ese uso (0: no hay) */
extern void (*mision_voz)(int id);                   /* 45BD30 (la cola de la acompanante) */
extern void (*mision_se)(int id);                    /* 424940 (efecto) / 11C090 (0x518D) */

void mision_inicia(void);                   /* 495B90 + 4965A0 estado 0 con la mision de rival_mis() */
void mision_paso(const mision_jug_t *j);    /* 496A30 en el modo 16 (sin pausa; sale solo si la carrera acabo) */
void mision_ui_paso(void);                  /* 4659F0 de los cuatro elementos (en el modo 16) */
int  mision_fin(void);                      /* 7D38F0 que pone la mision: 0, 1 (ganada: la meta) o 2 (fallada) */
int  mision_suspende(void);                 /* 8363A4 (495820): acabo antes de la meta (sin camara de la meta) */
const char *mision_rotulo_fin(void);       /* 67E6B8 (495830): el rotulo de la meta (uso; NULL ninguno) */
int  mision_nota(void);                     /* 67E6B0: la nota (495C20) */
int  mision_puntos(void);                   /* 836388: puntos (2, 3) o cuadros que quedan (1, 6) */
float mision_escala(void);                  /* car+0xDBC del jugador: 83638C / 600 x 0,17 + 1 */
extern void (*mision_graba)(int clave, int sub, int nota);   /* 495A20: la nota a la licencia (si es mejor o sin jugar) */
const mision_el_t *mision_el(int i);
struct hud_dat;
/* el HUD (NAVI_PUB 0x1bd6f4): C2CHUD.DAT y el fichero del tipo (C2CH1 / C2CH23 / C2CH6.DAT, o NULL) */
void mision_hud(struct hud_dat *h, struct hud_dat *h2);
void mision_dibuja(int prof, int jug_pos, int puntos_vivos);   /* prof: etapas del curso; jug_pos: car+0x260 */
int  mision_dibuja_fin(int cuadro);         /* en la meta de los tipos 1 / 6 ganados: el rotulo en lugar de GOAL (1) */
#endif
