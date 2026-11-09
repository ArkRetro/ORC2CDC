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
    float kmh;              /* car+0x1F8 (km/h del velocimetro)                                                 */
    float vel_mf;           /* car+0x1C4 (m por cuadro)                                                         */
} infarto_coche_t;

/* coche que pide abrir el gestor (45D470): clase (+324), seccion de la etapa, carril, modelo del juego */
typedef struct { int clase, seccion, carril, modelo, id; } infarto_coche_nuevo_t;

int  infarto_inicia(void);                  /* lee INFARTO.DAT; 0 bien */
void infarto_libera(void);
void infarto_carrera(int clave, int nivel); /* 45E0F0 al empezar la carrera en la etapa de esa clave */
void infarto_etapa(int clave, int nivel);   /* stage clear (7D39F0 bit 1): la etapa nueva */
void infarto_meta(void);                    /* 44FF10: GOAL */
void infarto_paso(const infarto_coche_t *c);   /* 464D20, un paso de logica (modo 16) */
int  infarto_corazones(void);               /* los que ensena el contador (7F1C80 / 10000) */
int  infarto_total(void);                   /* 7F23C4 / 10000 */
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
/* los corazones del medidor (45FB80): 19 del anillo {cuadro de HA_CORAZON (-1 ninguno), x, y desde el centro de la
   pantalla de 640x480}; rotos: x, y, cuadro de HA_ROTO (-1 ninguno); centro: cuadro de HA_CENTRO (-1 sin medidor) */
typedef struct { int cuadro; float x, y; } infarto_corazon_t;
const infarto_corazon_t *infarto_anillo(float *roto_x, float *roto_y, float *roto_c, float *centro);
#endif
