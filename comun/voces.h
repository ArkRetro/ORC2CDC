#ifndef VOCES_H
#define VOCES_H
/* Voces de la carrera del modo OutRun (la acompanante y el locutor) con la logica del original. Los datos (muestras y
   tablas de cuando habla) son de VOCES.DAT (tools/conv_sonido.py) y se tocan con sfx_voz (sfx.c). Ver voces.c. */

typedef struct {
    int   derrape;          /* 1 mientras el coche derrapa (car+0x248 & 0x10F407C del original)            */
    int   choque;           /* estado de choque del jugador (+2F0 >> 2 & 31)                                */
    float t_choque;         /* +2C8 tiempo que queda del choque (> 0: choque en curso)                      */
    float t_recup;          /* +2F8 recuperacion tras el choque (> 0: recuperandose)                        */
    int   contacto;         /* 1 si en este cuadro ha chocado con otro coche (car+4 & 0x4000000)            */
    float en_etapa;         /* posiciones de carretera recorridas en la etapa actual (car+0x260 - car+0x25E) */
    int   zona;             /* nivel de la etapa actual en el arbol del curso (0..4; 44C940)                */
} voces_coche_t;

int  voces_inicia(void);            /* despues de sfx_inicia: carga VOCES.DAT; 0 = bien */
void voces_carrera(void);           /* al empezar una carrera (488 5A0 + 4880C0: borra la cola y los contadores) */
void voces_salida(void);            /* modo GAME del original (empieza a contar el tiempo): la voz de salida */
void voces_checkpoint(void);        /* EXTEND TIME: efecto, voz y comentario de la zona nueva */
void voces_paso(const voces_coche_t *c);   /* un paso de logica de la carrera (60 por segundo) */
void voces_tiempo(int cuadros);     /* un paso de logica: aviso de poco tiempo (cuadros que quedan) */
void voces_meta(void);              /* GOAL */
void voces_tiempo_fuera(void);      /* TIME OVER */
void voces_para(void);
void voces_modo(int infarto);       /* 1: carrera del INFARTO (variante 2), 0: OutRun */              /* calla las voces (fin de carrera, menu) */

#endif
