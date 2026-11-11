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
    int mat;                                 /* 1: los bits 4-7 de banderas son el codigo de superficie (seccion 4) */
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

/* Muros del original para la carroceria: CarBodyWallColiCheck (PC 504CE0, PS2 0x1987e8) y la geometria de
   CbwColiWall (PC 5041B0, PS2 0x198e98; pc_wall_controller.cpp / pc_wall_geometry.cpp): cada punto de la forma
   (work+690, n = work+68C: las cuatro esquinas de vehicle_body_corners 4A1FC0 = PS2 0x182718) en el mundo con la
   matriz del cuerpo y contra la COLI con el modo 0 (GetYPositionProg: el poligono que lo contiene en xz y el de
   altura mas cercana); con todos sin suelo PlWrecker(1); si no estan todos en la clase 2 y alguno pisa un muro
   (0x010F4060), por cada uno de esos la cara del muro (CopColiPoint 43D1D0, las caras especiales de los
   triangulos por el codigo de superficie y la tabla 0x3dede8 del PS2, o CalcColiWallFace 502E20 con el centro
   del coche), PushOutPosMatObsolete 502F80 (el coche sale lo que haya entrado el punto mas metido; las caras se
   suman) y ColiSetObsolete 5030F0 con la primera (punto de contacto: event+281). La respuesta (roce, rebote y
   choque fuerte) es de quien llama (comun/or2_fisica.c or2_choque_muro).
   M: matriz del cuerpo (work+10) con las filas del PC: ejes x (derecha), y (arriba) y z (atras) en el mundo y la
   traslacion (work+40, el centro de gravedad). suelo_n: la normal del suelo (work+628) */
typedef struct {
    int accion;                              /* 0 nada (work+670 = 0), 1 CbwColiWall (event+4 |= 0x20000), 2 PlWrecker(1) */
    uint32_t agregado;                       /* event+2A8: OR de las clases bajo los puntos */
    uint32_t mascara;                        /* work+670: bit 31 - k si el punto k ha entrado en la cara de un muro */
    float empuje[3];                         /* lo que se mueve work+40 (la suma de las caras) */
    float dist;                              /* max_distance: la mayor salida de una cara */
    int contacto;                            /* ColiSetObsolete: 0 sin punto de contacto (CbwColiWall acaba sin respuesta) */
    int cuadrante;                           /* event+281: 0..3 por el lado del punto de contacto */
    float normal[3];                         /* work+64C: la normal de la primera cara, cruzada con la del suelo */
} coli_muro_t;
void coli_muro(coli_t *const raices[2], coli_pred_t *pr, const float M[4][3], const float (*forma)[3], int n,
               const float suelo_n[3], coli_muro_t *o);
#endif
