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
    float ant[3];                /* car+14 del paso anterior (+16C / +174: el cruce de los arcos) */
    int (*sec)(float x, float z);                       /* 43F370: la seccion de un punto (las estrellas del 19) */
    void (*dir)(float x, float z, float *fx, float *fz);   /* la direccion de la carretera (4AADC0) */
} haobj_coche_t;

int  haobj_inicia(void);                 /* la cabecera de HAOSO.DAT; 0 bien */
void haobj_libera(void);
void haobj_mision(int preset, int mision);   /* 45C470: los objetos de la mision de la etapa (fuera los de antes) */
void haobj_paso(const haobj_coche_t *c);     /* colocacion, toques y vuelo; un paso de logica */
/* toques del jugador en este paso (+3F8 / +3FC == 8 del original): devuelve cuantos y su clase (+5C) en cl[] */
int  haobj_toques(int *cl, int max);
/* cruces de arcos en este paso (+7C == 3 del original, 0x1d42f0): cuantos y su clase */
int  haobj_cruces(int *cl, int max);
int  haobj_postes(void);                 /* postes de los arcos tocados en este paso (0x1d4434: +7C 1 / 2, efecto 0xCD) */
/* 4611D0 con la clase 0x10 (accion -11): el ancho de los arcos (clase 0x0B) del tramo lo..hi con la fila t {inicio,
   medio, final, reparto} de 0x3f7218 y los carriles del primero (carriles(x, z), sec: la seccion de un punto) */
void haobj_arcos(int lo, int hi, const float *t, int (*carriles)(float x, float z), int (*sec)(float x, float z),
                 void (*dir)(float x, float z, float *fx, float *fz));   /* dir: la direccion de la carretera */
/* cuantos objetos vivos de una clase hay (para contar los que pide la peticion) */
int  haobj_cuenta(int clase);
/* 45C8E0: los vivos de una clase en las secciones lo..hi (sec: la seccion de un punto, 43F370) */
int  haobj_cuenta_sec(int clase, int lo, int hi, int (*sec)(float x, float z));
/* lo que hay que dibujar: registro del banco 0x57 y matriz (filas: ejes y traslacion, en el mundo), los mas cercanos al
   ojo dentro de la distancia (HAOBJ_LEJOS), como mucho max */
#define HAOBJ_LEJOS 160.0f
#define HAOBJ_ANI_LEJOS 100.0f           /* (DESVIACION, poligonos) los corazones del tipo 11, solo hasta aqui */
typedef struct { int reg; float M[4][4]; } haobj_dib_t;
/* prueba: el objeto vivo de la clase cl (-1 cualquiera) mas cercano por delante del coche entre 4 y 70 m: 1 y su
   desvio lateral (m, > 0 a la derecha del eje adelante fw) */
int  haobj_cercano(const float pos[3], const float fw[3], int cl, float *lat);
/* tipo 19 (461940 = PS2 0x1b11c8): las estrellas (clase 0x12, tipo 8 de oso_qt, control 0x1d4d80) que llegan volando,
   botan y sueltan 5 mas (los objetos 0x134.. de la tabla 0x3f7348 = piezas: 5 x {f32 desvio[3], angulos[3], escala}).
   lim: 7F8ADC; *activas: 7F8AF0; devuelve en r lo de este paso */
typedef struct { int rotas, cogidas, primera; } haobj_est_t;
void haobj_est_paso(int sec_jug, int lim, int *activas, const float *piezas, haobj_est_t *r);
void haobj_est_abre(int si);             /* 461080 / 45C880 con la clase 0x13 (-10 / -13): los 40 trozos */
/* 45B000 / 45B2A0 (accion -5): las marcas de principio y fin (registros del banco 0x57) en el centro de la seccion + 4 m
   con su direccion; n = 0 las quita */
void haobj_marcas(int n, const int *reg, const float (*p)[3], const float *fx, const float *fz);
int  haobj_lista(haobj_dib_t *v, int max, const float ojo[3]);
/* tipo 11, "Atrapa el corazon" (46DB60 = PS2 0x1dcaf8, el anillo 0x5d51f8 de 32 registros de 0x5C): dos corazones
   (registro 0x2a del banco 0x57, 1aa5b8) en los puntos l[2][3] (INFARTOX.DAT ANI1) del marco de cada coche vivo.
   Por plaza de trafico: pos / fw / up del coche (pos NULL: plaza libre) y marca (+C5C 0x20000: 0 en un coche nuevo; la
   pone el modulo y la plaza la guarda). Un paso de logica mientras la peticion es del tipo 11 */
typedef struct haobj_auto_s { const float *pos, *fw, *up; int marca; } haobj_auto_t;
void haobj_anillo(haobj_auto_t *v, int n, const float *l);
void haobj_anillo_vacia(void);           /* 0x1df060 (la puesta del 11) y al acabar la peticion: sin corazones */
int  haobj_anillo_toca(const float pos[3]);   /* 0x1aa3e8: los corazones a 4 m (xz) del jugador se cogen; cuantos */
#endif
