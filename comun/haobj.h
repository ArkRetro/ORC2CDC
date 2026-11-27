#ifndef HAOBJ_H
#define HAOBJ_H
#include <stdint.h>
/* Objetos de las peticiones del INFARTO (conos, monedas, arcos, cajas): los ficheros oso_qt_* de la mision de la etapa
   (HAOSO.DAT) con los modelos del banco 0x57 (HAOBJ.DAT). Ver haobj.c y docs/PARIDAD.md (INFARTO: objetos). */

typedef struct {
    const float *marco;          /* colocacion de la etapa en el mundo de la demo (x, y, z, giro y; modelo_coloca) */
    int (*suelo)(float x, float z, float *y);   /* altura del suelo (y entra con la de referencia); < 0 sin suelo */
    float pos[3], fw[3];         /* coche del jugador: car+14 y su eje adelante */
    float v;                     /* m/s */
} haobj_coche_t;

int  haobj_inicia(void);                 /* la cabecera de HAOSO.DAT; 0 bien */
void haobj_libera(void);
void haobj_mision(int preset, int mision);   /* 45C470: los objetos de la mision de la etapa (fuera los de antes) */
void haobj_paso(const haobj_coche_t *c);     /* colocacion, toques y vuelo; un paso de logica */
/* toques del jugador en este paso (+3F8 / +3FC == 8 del original): devuelve cuantos y su clase (+5C) en cl[] */
int  haobj_toques(int *cl, int max);
/* cuantos objetos vivos de una clase hay (para contar los que pide la peticion) */
int  haobj_cuenta(int clase);
/* 45C8E0: los vivos de una clase en las secciones lo..hi (sec: la seccion de un punto, 43F370) */
int  haobj_cuenta_sec(int clase, int lo, int hi, int (*sec)(float x, float z));
/* lo que hay que dibujar: registro del banco 0x57 y matriz (filas: ejes y traslacion, en el mundo), los mas cercanos al
   ojo dentro de la distancia (HAOBJ_LEJOS), como mucho max */
#define HAOBJ_LEJOS 160.0f
typedef struct { int reg; float M[4][4]; } haobj_dib_t;
/* prueba: el objeto vivo de la clase cl (-1 cualquiera) mas cercano por delante del coche entre 4 y 70 m: 1 y su
   desvio lateral (m, > 0 a la derecha del eje adelante fw) */
int  haobj_cercano(const float pos[3], const float fw[3], int cl, float *lat);
int  haobj_lista(haobj_dib_t *v, int max, const float ojo[3]);
#endif
