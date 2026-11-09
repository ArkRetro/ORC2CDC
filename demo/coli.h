/* Consulta del suelo del original (colision COLI0200 de la etapa y de la union, convertida por tools/conv_coli.py a
   COL1): GetYPositionProg 43EB60 (pc_course_world.cpp get_y_position_prog), GetRoadCond (pc_course_query.cpp
   get_road_cond: rejilla de 24 m, lista de la celda, poligono que contiene el punto en xz y altura del plano (modo
   0x100) o de la superficie de Hermite con sus vecinos (modo 0x400, CalcYPosSpl de pc_course_spline.cpp), el mas
   cercano a la altura de referencia), la tabla de prediccion EasyLct (que raiz se prueba primero) y
   GetYPositionSplChk (pc_course_world.cpp: hasta cuatro intentos desplazando 0,001 en x / z mientras sale vacio). */
#ifndef COLI_H
#define COLI_H
#include <stdint.h>

typedef struct { uint16_t v[4]; uint8_t tipo, banderas; uint16_t vecinos, normal; } coli_pol_t;
typedef struct {
    const float *v, *nrm;                    /* vertices y normales (x, y, z) */
    const coli_pol_t *p;
    const uint16_t *rec, *rej, *li;          /* registros de normales (4), rejilla recortada, listas */
    uint32_t n, x0, z0, gw, gh, tipo;
    float marco[4];                          /* colocacion en el marco de la carrera (x, y, z, giro y; modelo_coloca) */
    int ok;
} coli_t;
typedef struct { uint32_t recent[16], cursor, easy; } coli_pred_t;   /* EasyLctPredictionState */

/* d: el fichero COL1 entero (alineado a 4); 0 bien */
int coli_abre(coli_t *c, const uint8_t *d, uint32_t tam, const float *marco);
/* GetYPositionProg(modo, punto): raices[0] la etapa (7D2DA0), raices[1] la union (7D3190), NULL si no esta.
   *y: altura de referencia al entrar y la del suelo al salir (-0,1 si no hay); pol: poligono o -1; clase: 1 << tipo
   (1 sin suelo). Devuelve la raiz del acierto o la segunda probada si no hay */
int coli_altura(coli_t *const raices[2], coli_pred_t *pr, uint32_t modo, float x, float *y, float z, int *pol, uint32_t *clase);
/* GetYPositionSplChk (modo 0x400 con reintentos) */
int coli_altura_spl(coli_t *const raices[2], coli_pred_t *pr, float x, float *y, float z, int *pol, uint32_t *clase);
#endif
