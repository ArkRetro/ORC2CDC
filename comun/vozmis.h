#ifndef VOZMIS_H
#define VOZMIS_H
#include <stdint.h>
/* (C2C, campeonatos) las voces de los bancos que el original carga por etapa (0x22ac00): COMMON.pak (lo que no esta en
   HC.PAK), RANKn (la fila de las voces de la nota) y el banco de cada tipo de peticion (VOCESM.DAT, tools/conv_camp.py).
   Se suben al AICA en un hilo y sfx.c las busca antes que las de VOCESH.DAT (sfx_voz_extra). */
int  vm_abre(void);                        /* lee el indice de VOCESM.DAT (al empezar la carrera del campeonato); 0 bien */
/* los bancos de la etapa: n peticiones con su tipo / subtipo, la fila (0..3) de RANK; rnd[k]: el azar del banco que lo
   lleva (13: FOOD 0..5, 16/2: SOUND -1 o 0..6, 16/6: CARS 0..2; si no -1) */
void vm_etapa(int n, const uint8_t *tipo, const uint8_t *sub, int fila, int *rnd);
void vm_cierra(void);                      /* suelta todo (fin del campeonato) */
#endif
