#ifndef MINIJUEGO_H
#define MINIJUEGO_H
#include <stdint.h>
#include "campeonato.h"
/* Los minijuegos de los campeonatos de COAST 2 COAST: el tipo de peticion 16 (la clase 0x29f520 de SLES-53998, vtable
   0x423a80, 7 subtipos), ver minijuego.c y docs/PARIDAD.md (campeonatos, minijuegos). Datos: el bloque 'M161' de CAMP.DAT
   y los sprites MJ_kk de MJHUD.DAT (tools/conv_camp.py). El estado va al monton solo durante el campeonato. */
struct haobj_s3d_s;

int  mj_inicia(const uint8_t *d, uint32_t tam);   /* (camp_inicia) busca 'M161' en CAMP.DAT; 0 si esta */
void mj_libera(void);
/* un paso de la peticion pet (indice de CAMP.DAT) del tipo 16 con su subtipo, la fase de la peticion (1..7), el azar del
   banco de voces (+8 de los parametros, -1 ninguno) y el coche; voz: la cola de voces del gestor (0x1ac3d8). Devuelve lo
   que sube el medidor (+0x28) en este paso */
float mj_paso(int pet, int sub, int fase, int rnd, const camp_coche_t *c, void (*voz)(int id, int retardo, int prio));
/* los sprites 3D de la peticion en curso (0x2a0138 -> 0x115870): grupo MJ_kk de MJHUD.DAT (o HA_S02 de HAHUD.DAT),
   cuadro, punto y escala; cuadros(g): los cuadros de un grupo */
int  mj_spr3d(struct haobj_s3d_s *v, int max, int (*cuadros)(const char *g));
/* (main.c) el punto del mundo de la seccion sec de la etapa en la fraccion lat del ancho del asfalto (la de camp_obj) y dx m
   mas hacia el mismo lado */
extern void (*mj_mundo)(float sec, float lat, float dx, float p[3]);
#endif
