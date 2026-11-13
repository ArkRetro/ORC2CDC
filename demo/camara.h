/* Camara de carrera del original (evento CAMERA 385, funcion 0x0D; PC 485FE0 modo 16 -> 4858D0, SLES-53998
   0x179400..0x17b540). Tablas de PARAM.DAT ('CAM1', tools/conv_param.py). Convenciones del PC: coordenadas locales
   x derecha, y arriba, z atras; angulos en rad con giro y positivo hacia la izquierda (rumbo de la demo = -giro y).
   Un paso por cuadro de logica (60,2 Hz en el original), como el original. */
#ifndef CAMARA_H
#define CAMARA_H
#include <stdint.h>

#define CAM_NVISTAS 31
typedef struct {
    const float *vistas;     /* 31 x 13: ojo, mira, despl. x/y de la proyeccion, balanceo, FOV, +28, pivote z, matriz */
    const float *vistas_mod; /* 20 vistas (2..21) x 30 modelos x 9: ojo, mira, factor del cabeceo, tope alto, bajo  */
    const float *modelos;    /* 30 x 9: ojo de la vista interior, caja del coche (max x,y,z, min x,y,z)           */
} cam_tablas_t;

typedef struct {
    float pos[3];            /* car+14 origen del coche (mundo)                                     */
    float fw[3], up[3];      /* morro con cabeceo y arriba de la carroceria (mundo, unitarios)     */
    float normal[3];         /* work+628 normal del suelo                                           */
    float vel[3];            /* car+20 velocidad (m/cuadro, mundo)                                  */
    float vel_mf;            /* car+1C4 modulo de la velocidad (m/cuadro)                           */
    int   modelo;            /* car+11                                                              */
    int   choque_nuevo;      /* car+2F0 bit 0: el paso en que empieza un estado de choque           */
    int   choque_estado;     /* car+2F0 >> 2 & 31                                                   */
    float choque_dur;        /* 4F6580[estado]: duracion del estado (s)                             */
    int   choque_activo;     /* car+2C8 > 0                                                         */
    float pose_t[3], pose_r[3]; /* car+2D8 / +2E4..+2EC animacion del choque                        */
    int   grua;              /* car+D68 cuadros de grua                                             */
    int   boton_vista;       /* entrada 0x40000 (cambio de vista) pulsada en este paso              */
    int   salida;            /* 1 = camara de la salida (485250: temporizador > 180)                */
    int   agotado;           /* 1 = camara del fin del tiempo (modos 20..22: 485830 = PS2 0x178e58)   */
    const float *guion;      /* 486EF0 de la escena (82E7D0 == 1): ojo, mira (marco del coche), +130 y +A4; o NULL */
    float (*suelo)(const float *p);  /* 43EB60: altura del suelo bajo el punto p (mundo)            */
} cam_entrada_t;

typedef struct {
    int8_t  vista, vista_ant, elegida;   /* +34A / +34B / 7C24B8 (la que se elige con el boton)  */
    int16_t n33c, n368, n340, n342, n344, n346, n348;
    uint8_t k360;
    int     mat34c, mat350;              /* matriz de la vista: 1 = con la carroceria (+200)     */
    float   e0[3], ec[3], e110[3], e11c[3];   /* ojo y mira locales (actual y guardado)          */
    float   rol130, rol134, fov_a4, fov_a8, dx_c4, dy_c8, dx_cc, dy_d0, f358, f35c;
    float   giro_d8c;                    /* car+D8C giro de la camara al empezar el choque       */
    int16_t hist[30];                    /* +300: cabeceo de los 30 ultimos cuadros (unidades 2pi/65536) */
    /* salida */
    float   ojo[3], mira[3];             /* +F8 / +104 mundo                                     */
    float   balanceo;                    /* +130 rad                                             */
    float   fov;                         /* +A4 horizontal (rad) para 4:3                        */
    float   despl[2];                    /* +C4/+C8 desplazamiento del centro de la proyeccion   */
    int     dentro;                      /* 482E80: el ojo dentro de la caja del coche           */
} cam_t;

/* al empezar cada carrera (484EE0); conserva la vista elegida (poner c->elegida = -1 la primera vez) */
void cam_inicia(cam_t *c, const cam_tablas_t *t);
void cam_paso(cam_t *c, const cam_tablas_t *t, const cam_entrada_t *e);
/* el marco del mundo se ha trasladado (curso_recentra): resta (dx, dy, dz) a lo que esta en coordenadas del mundo.
   Solo lo son el ojo y la mira de salida; el resto del estado es local al coche (ojo y mira locales, mezcla de vistas)
   o angular (historia del cabeceo, giro guardado del choque), y se recalcula en cada paso desde el coche */
void cam_traslada(cam_t *c, float dx, float dy, float dz);
/* 4A2BA0: si se dibuja la carroceria por fuera con la vista actual */
int  cam_coche_visible(const cam_t *c);
#endif
