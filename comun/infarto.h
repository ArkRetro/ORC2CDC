#ifndef INFARTO_H
#define INFARTO_H
#include <stdint.h>
/* Modo INFARTO (HEART ATTACK, variante 2 de [780258]): las peticiones de la acompanante, los corazones, las notas y el
   resultado, como el original (ver infarto.c). Datos de INFARTO.DAT (tools/conv_infarto.py). */

/* lo que el gestor lee del coche del jugador en cada paso de logica (campos del original entre parentesis) */
typedef struct {
    int   seccion;          /* posicion de carretera en la etapa (car+0x64)                                    */
    int   en_union;         /* el coche esta en la union (car+0x5C != 0)                                       */
    int   pasados;          /* coches adelantados en la carrera (84DF0C + 84DF00, 4F2B10 + 4F2B00)              */
    int   choque_ini;       /* empieza un choque en este paso (+4 & 0x80000 / +2F0 & 1)                         */
    int   choque;           /* estado de choque ((+2F0 >> 2) & 31)                                              */
    int   impulso;          /* +4 & 0x4000000: otro coche le ha cambiado la velocidad                           */
    int   impulso_rapido;   /* ... y el otro va mas deprisa (+1C4 del otro > +1C4 del jugador)                  */
    int   roce;             /* +4 & 0x500000                                                                    */
    int   muro;             /* +4 & 0x20000                                                                     */
    uint32_t sup[4];        /* superficie de cada rueda (+24C..+258)                                            */
    int   d36;              /* car+0xD36: temporizador del inicio de derrape                                    */
    int   seccion_union;    /* car+0x260 en la union (posicion de carretera desde su principio)                  */
    int   derrapa;          /* car+0x2A8 & 0x10F407C (derrape)                                                  */
    float kmh;              /* car+0x1F8 (km/h del velocimetro)                                                 */
    float vel_mf;           /* car+0x1C4 (m por cuadro)                                                         */
    float hud;              /* parametro +0x2438 del coche (+0x2B4): factor del velocimetro                     */
    float ofs;              /* car+0x26C: distancia lateral al centro de la carretera (> 0 a la derecha)       */
    float d268, d264;       /* car+0x268 / +0x264 (4A4010): hasta el borde izquierdo (> 0 dentro) y el derecho (< 0) */
    float x, y, z;          /* car+0x14 (marco del mundo de la demo)                                            */
    int   modelo;           /* car+0x11                                                                         */
    const float *marco;     /* colocacion de la etapa (x, y, z, giro y) para los objetos de la mision (haobj.c) */
    int (*suelo)(float x, float z, float *y);   /* la consulta del suelo (43F2F0) */
    float fx, fz;           /* eje adelante del coche */
    int (*seccion_xz)(float x, float z);        /* 43F370: la seccion (car+0x64) de un punto del mundo */
    int (*carriles_xz)(float x, float z);       /* los carriles de la carretera en un punto (+0xA4 de los arcos) */
    void (*dir_xz)(float x, float z, float *fx, float *fz);   /* la direccion de la carretera en un punto */
    void (*punto_sec)(int sec, float *p, float *fx, float *fz);   /* 43E570: el centro de la seccion y su direccion */
    int   sig;              /* la clave de la etapa siguiente si ya se ha elegido la rama en la union (451350 != 2), si no -1 */
} infarto_coche_t;

/* coche que pide abrir el gestor (45D470): clase (+324), seccion de la etapa, carril, modelo del juego */
typedef struct { int clase, seccion, carril, modelo, id; } infarto_coche_nuevo_t;

int  infarto_inicia(void);                  /* lee INFARTO.DAT; 0 bien */
void infarto_preset(int preset);            /* el preset [0x44350c] de la carrera, antes de infarto_inicia */
void infarto_tramo_1a(int si);              /* 1: el jugador en la etapa del descriptor 0x1a (NEWY) */
void infarto_libera(void);
void infarto_carrera(int clave, int nivel); /* 45E0F0 al empezar la carrera en la etapa de esa clave */
void infarto_etapa(int clave, int nivel);   /* stage clear (7D39F0 bit 1): la etapa nueva */
void infarto_meta(void);                    /* 44FF10: GOAL */
/* 45ECA0 / 45E9B0: el modelo que la mision de la etapa dibuja sobre ella (HAMpqq.DAT de tools/conv_infarto.py metas:
   los carriles pintados de las peticiones 8 / 23): su nombre en nom (16) y 1, o 0 si no tiene */
int  infarto_metas(char *nom);
void infarto_paso(const infarto_coche_t *c);   /* 464D20, un paso de logica (modo 16) */
int  infarto_corazones(void);               /* los que ensena el contador (7F1C80 / 10000) */
int  infarto_total(void);                   /* 7F23C4 / 10000 */
int32_t infarto_disp(void);                /* 7F1C80 (45B820): 10000 por corazon */
int  infarto_grupo_voz(int clave);          /* banco HAM de la clave (sfx_voz_grupo) */
int  infarto_mueven(void);                  /* 45D820 (7F250C): los coches de la mision ya se mueven */
int  infarto_trafico(void);                 /* 64E08C: 0 = sin trafico normal (lo cierra y no abre) */
int  infarto_coche_pide(infarto_coche_nuevo_t *n);   /* 1 si hay un coche que abrir (la tabla de 45D710) */
void infarto_coche_abierto(int ok);         /* respuesta: 1 abierto (o gastado sin coche), 0 sin suceso libre */
int  infarto_sueltas(void);                 /* cuenta de 45D8E0: si cambia, los coches de la mision pasan a cerrarse */
/* estado para el HUD: tipo de la peticion (3 = ninguna), su resultado actual (corazones), ultima nota (0..7, -1) y
   cuadros desde que se puso; nota media de la ultima etapa */
typedef struct { int tipo, resultado, nota, t_nota, corazones, total, nota_etapa, t_etapa, anuncio, t_anuncio; } infarto_hud_t;
const infarto_hud_t *infarto_hud(void);
/* rotulo visible: texto (HA_Tnn de HAHUD.DAT), marco (HA_Mn) y cuadro de su escena; 0 sin rotulo */
int infarto_rotulo(int *texto, int *marco, int *cuadro);
/* el medidor (45FB80) y sus efectos (45B950): sprites de HAHUD.DAT a dibujar en este cuadro, {grupo (0..5 HA_C0..HA_C5 =
   2B000F..2B0014, 6 HA_CORAZON = 2B0015, 7 HA_ROTO = 2B000E), cuadro de su escena, x, y desde el centro de la pantalla
   de 640x480}; devuelve cuantos. infarto_tinte: color ABGR del marco del rotulo (7F8B68: rojizo con resultado < 0) */
typedef struct { int g, cuadro; float x, y; } infarto_spr_t;
int infarto_medidor(infarto_spr_t *v, int max);
uint32_t infarto_tinte(void);
/* lo 2D del modo que no es el medidor: el panel de la nota que entra (45BDF0) y la hoja de resultados de la etapa (4603F0):
   grupos de HAHUD.DAT (nombre), su cuadro y (x, y) de hudd_dibuja (el uso ya lleva el 4294C0 del original); cifras "%3d"
   en (x, y) de 4BC990 */
typedef struct { char g[12]; int cuadro; float x, y; } infarto_dib_t;
typedef struct { int x, y; int32_t v; } infarto_num_t;
int infarto_dibujos(infarto_dib_t *v, int max, infarto_num_t *nv, int *nn);
/* 4BCD50: filas de los niveles terminados (7F244C, hasta 4) y la de la etapa (7F2464) en v[n]; devuelve n */
int infarto_filas(int32_t *v);
#endif
