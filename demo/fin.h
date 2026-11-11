#ifndef FIN_H
#define FIN_H
#include <stdint.h>
/* Fin de la carrera del original en la consola (43F860 = 1): modo 34 (OutRun Miles), 35 (continuar: REINTENTAR /
   ABANDONAR) y 27 (GAME OVER). Ver demo/fin.c. */

/* 450240 (7D38F0, PS2 0x1cf198): como acabo la carrera */
enum { FIN_TIEMPO = 1, FIN_META = 2, FIN_PAUSA_REINTENTA = 6, FIN_PAUSA_ABANDONA = 7 };
/* lo que pide fin_paso */
enum { FIN_SIGUE = 0, FIN_REINTENTA, FIN_MENU };

typedef struct {
    int variante;           /* [780258] */
    int puntos;             /* 4B99D0: la puntuacion del coche ([8447DC + id x 4]) */
    int32_t corazones;      /* 45B820: [7F1C80] del INFARTO (10000 por corazon) */
} fin_carrera_t;

void fin_carrera_empieza(void);                     /* coche nuevo: car+E78 = 0 (constructor, PS2 0x239e84) */
void fin_rebufo(int n);                             /* car+E78 += n (CheckSlipStream, PS2 0x186494) */
void fin_etapa(const int limpio[6], int segundos);  /* 4EF460 / 4EF440 (PS2 0x1f4118 / 0x1f40f8) desde 4F2B20 */
void fin_entra(int como, const fin_carrera_t *c);   /* 49B470 (modo 34) */
int  fin_paso(uint32_t nuevo);                      /* un paso de logica (botones nuevos de CONT_*); FIN_* */
void fin_dibuja(void);                              /* dentro de la lista TR, encima de todo */
void fin_game_over(int como);                        /* OUTRUN2SP: modo 27 (como: FIN_*) */
/* OUTRUN2SP tras la meta o el fin del tiempo: modo 25 (resultados: el mapa de la ruta, 49A790 / 49A800) y despues
   (sin el 26) el 27 */
typedef struct {
    int preset;             /* [78024C] */
    int modelo, color;      /* 48B140 / 48B160: modelo del coche (car+0x11) y color (0: ninguno) del frontal */
    int rama[4];            /* 451350(nivel 0..3): 0 izquierda, 1 derecha, 2 sin llegar */
    int nivel, lado;        /* 4AEEF0 al acabarse el tiempo: nivel de la etapa (car+0x68) y 0 etapa / 1 union (car+5C); */
    float razon;            /* posicion / filas del tramo (450AC0); -1, -1, -1 tras la meta (sin llamar) */
} fin_sp_t;
void fin_sp_resultados(int como, const fin_sp_t *d);
void fin_sale(void);                                /* suelta FIN.DAT / GOVER.DAT (y su VRAM) */
int  fin_activo(void);
#endif
