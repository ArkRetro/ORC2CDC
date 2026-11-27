#ifndef CAMPEONATO_H
#define CAMPEONATO_H
#include <stdint.h>
/* Campeonatos de COAST 2 COAST (variante 5: las peticiones de la acompanante de los grupos 4..6 de las categorias),
   como el gestor del original (suceso 0x190 de SLES-53998, ver campeonato.c). Datos de CAMP.DAT (tools/conv_camp.py),
   en el monton solo mientras dura la carrera. */

/* lo que el gestor lee del coche del jugador en cada paso de logica (campos del original entre parentesis) */
typedef struct {
    int   seccion;          /* posicion de carretera en la etapa (car+0x64)                                  */
    float pos;              /* la misma con decimales (0x182b40, [+0x3e78]): el avance del paso              */
    int   en_union;         /* car+0x5C != 0                                                                 */
    float kmh;              /* car+0x1F8                                                                     */
    int   pasados;          /* coches adelantados (car+0x32A)                                                */
    int   choque_ini;       /* empieza un choque en este paso (+4 & 0x80000 / +2F0 & 1)                       */
    int   choque;           /* estado de choque ((+2F0 >> 2) & 31)                                            */
    int   impulso;          /* +4 & 0x4000000: otro coche le cambia la velocidad                             */
    int   impulso_rapido;   /* ... y el otro va mas deprisa                                                   */
    int   roce;             /* +4 & 0x500000                                                                  */
    int   muro;             /* +4 & 0x20000                                                                   */
    uint32_t sup[4];        /* superficie de cada rueda (+24C..+258)                                          */
    int   derrapa;          /* derrape (0x22bf08)                                                             */
    float rebufo;           /* car+0xE68                                                                      */
    int   rebufo_ang;       /* |car+0x162| en unidades de 65536 por vuelta                                    */
    const int *toq; int ntoq;   /* clases de los objetos tocados en este paso (comun/haobj.c, +3F8 / +3FC == 8)   */
    const int *cru; int ncru;   /* clases de los arcos cruzados en este paso (+7C == 3)                           */
} camp_coche_t;

/* al empezar la carrera: fila (46C520: 1 OutRun2 SP, 0 OutRun 2, 2 mixto), forzada (cfg1: se sale de una etapa del
   mapa con su lista), lista (cfg3), dificultad (0..4); 0 bien */
int  camp_inicia(int grupo, int fila, int forzada, int lista, int dificultad);
void camp_libera(void);
int  camp_activo(void);
int  camp_tipo(int grupo, int nodo);        /* tipo de la etapa del registro nodo del arbol del grupo (0 SP, 1 OR2, 2 MIX) */
void camp_etapa(int tipo);                  /* 0x22ac00: la etapa nueva (tipo de su descriptor, 0..59) */
void camp_fin_etapa(void);                  /* 0x1cec50 en 0x22c000: etapa terminada (su nota a la licencia) */
void camp_paso(const camp_coche_t *c);      /* 0x22a048, un paso de logica */
/* graba la nota de la etapa: tipo de etapa, lista y nota (0..6); 1 si mejora (0x22edd8) */
extern int (*camp_graba)(int tipo, int lista, int nota);
extern void (*camp_voz)(int id);           /* voces (VOCESH.DAT) */
extern void (*camp_se)(int id);            /* efectos del banco de voces */
extern void (*camp_millas)(int m);         /* millas a la licencia */
/* los objetos de la peticion (0x2938d0 / 0x293968 / 0x297e20): uno de la clase cl (banco 0x57) y tipo de control tipo
   en la seccion sec de la etapa y la fraccion lat del ancho de la carretera, con la escala esc (el arco); cl = -1 quita
   los que haya, -2 ademas suelta lo de los objetos (fin del campeonato) */
extern void (*camp_obj)(int cl, int tipo, float sec, float lat, float esc);
/* HUD (0x22a5b8: el panel 2C002A, las filas 0x1bd920 por nivel y el contador; 0x230f70 / 0x2314e8: los rotulos) */
int  camp_rotulo(int *texto, int *marco, int *cuadro);
uint32_t camp_tinte(void);
int  camp_filas(int32_t *v);                /* corazones de cada nivel hasta el actual; devuelve el ultimo indice */
int  camp_corazones(void);                  /* el contador (+0x1fe8) */
int  camp_total(void);                      /* +0x1fe4 */
#endif
