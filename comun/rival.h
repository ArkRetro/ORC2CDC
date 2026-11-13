#ifndef RIVAL_H
#define RIVAL_H
#include <stdint.h>
/* Misiones de carrera de COAST 2 COAST y sus coches de carreras (el rival: los Ferrari de delante), como el original
   (ver rival.c). Datos de MISION.DAT (tools/conv_mision.py: Races.bin y tablas del ELF de PS2). */

#define RIVAL_NMIS 94                           /* registros de Races.bin (el i corre el arbol 100 + i de CURSO.DAT) */
#define RIVAL_MAX 100                           /* coches de carreras de un registro (Races.bin: 98 como mucho) */
#define RIVAL_NODOS 16

/* registro de 0xA0 de la lista 80FB00 (47CF40); entre parentesis el campo del original */
typedef struct {
    float v_ref, v_curva, amp, periodo, fase, amp_c, periodo_c, fase_c;   /* +00..+1C -> coche +B78 +B88 +B7C +B80
                                                                              +B84 +B8C +B90 +B94 */
    float luz, f24, f28, f30, per, f38, pos, ritmo;  /* +20 (+BD0), +24, +28, +30, +34 segundos, +38 (+BCC), +40, +44 */
    int32_t pos_i;                              /* +3C: posicion de carretera entera */
    int32_t dif;                                /* +48: posicion - la del jugador */
    int32_t coche;                              /* +4C: plaza del trafico con su coche (-1 sin coche) */
    int32_t delante;                            /* +50: iba por delante del jugador al soltar el coche */
    int32_t vivo, d58, d5c, especial, d64, d68, d6c, d98, d9c;   /* +54, +58, +5C, +60, +64, +68, +6C, +98, +9C */
    uint8_t idx, fl71, modelo, color, puesto, b75, rival, b77;  /* +70..+77 */
} rival_t;

/* lo que la carrera lee de la mision elegida (registro de Races.bin) */
typedef struct {
    int clave, sub, b14, tipo; float segundos; int32_t umbral[3];
    int32_t cfg[13]; float c20;                 /* (cfg[8] es el float c20) */
    float perfil[16], c3c[2], luces[12];
    uint32_t nnodos; uint16_t largo_tramo[RIVAL_NODOS], largo_union[RIVAL_NODOS], largo;
} rival_mision_t;

/* lo que el montaje y el paso leen del jugador (campos del original entre parentesis) */
typedef struct {
    int pos;            /* car+0x260: posicion de carretera en el curso entero */
    float frac;         /* fraccion de la seccion (4A6CF0) */
    float v;            /* car+0x1C4 (m por cuadro) */
    int corre;          /* modo 16 y temporizador de la salida 49B2D0 < 0x3C */
    int cuadros;        /* 44FDF0: cuadros de la carrera */
} rival_jug_t;

int  rival_inicia(void);                        /* lee MISION.DAT (lista para el menu); 0 bien */
int  rival_lista(int i, int *clave, int *sub, int *tipo);   /* 0 si el registro i existe */
int  rival_carrera(int i);                      /* la mision i para la carrera siguiente (lee su registro); 0 bien */
void rival_fuera(void);                         /* sin mision (otros modos) */
const rival_mision_t *rival_mis(void);          /* la mision de la carrera (NULL fuera de C2C) */
int  rival_modelo_principal(void);              /* modelo del coche de carreras de la plaza mas adelantada (el rival) */
void rival_prepara(void);                       /* 47CF40: los registros de la mision (modelos, colores, ritmos) */
void rival_salida(const rival_jug_t *j);        /* 47DAC0 / 47D8B0: los que ya se ven abren su coche (en la parrilla) */
void rival_paso(const rival_jug_t *j);          /* la parte de los coches de carreras de 47EC00, un paso de logica */
int  rival_n(void);                             /* coches de carreras (80FB04) */
rival_t *rival_reg(int i);
const float *rival_acel(void);                  /* tabla de 472090 del coche de carreras (PS2 0x40bc98) */
const float *rival_curva(void);                 /* pares {radio, km/h del modelo} de 47A7C0 (PS2 0x40bb68) */
const float *rival_marcha(void);                /* aceleracion por marcha (+208 - 1) de 472090, x 0,002 (PS2 0x40bcb0) */
float rival_ritmo(int i);                       /* +9C / 600 x 0,17 + 1 */
float rival_escala_v(const rival_jug_t *j);     /* 47A7C0: x perfil +0x14 (8037B8) o +0x10 (jugador lento) o 1 */
void rival_sigue(int i, float pos, int pos_i);  /* 477090: la posicion del coche de carreras abierto */
void rival_suelta(int i, int jug_pos);          /* 4763D0: su coche se cierra */
int  rival_puesto(void);                        /* car+0xC36 del jugador (0 el primero) */
int  rival_total(void);                         /* 80FB2C: en la clasificacion (con el jugador) */
int  rival_47da80(void);                        /* el de referencia del HUD (lider o segundo); -1 sin coches */
int  rival_largo(void);                         /* 803710: largo del curso en posiciones */
int  rival_nota(void);
int  rival_millas(int nota);                    /* 84B900 + 4 nota (OutrunMiles.bin SETUP; 0 la 7) */
int  rival_siguiente(int i);                    /* 495A60: el registro de la subclave siguiente o -1 */                          /* 495C20 (tipo 0): 6 - puesto, 0 el ultimo */
/* la carrera (demo/main.c) abre el coche del registro i a ebx posiciones del jugador (delante: ebx > 0; parrilla:
   fuera del modo 16, al montar: en el carril 2 o 3 por +77 & 1). Devuelve la plaza del trafico o -1 si no puede (y el
   registro sigue sin coche: 476480 / 4764D0) */
extern int (*rival_abre)(int i, int ebx, int delante, int parrilla);
#endif
