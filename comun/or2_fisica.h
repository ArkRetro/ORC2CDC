/*
 * or2_fisica.h - Reimplementacion C99 (solo float) del manejo de los coches
 * de OutRun 2006 Coast 2 Coast, a partir de la decompilacion C++ del PC
 * (src/driving) y de la tabla de parametros por coche del ELF de PS2.
 *
 * Modelo: plano de guinada (2D). Cuatro ruedas con el modelo de neumatico
 * del juego (fuerza lateral "deadbeat", circulo de friccion suavizado),
 * transmision automatica real (embrague, cortes de cambio, limitador),
 * y las capas arcade del original: RearGripCtrl, SlipAngleCtrl,
 * CorneringCtrl (bicicleta cinematica), CalcPlBody_2nd (mezcla, limitador
 * de fuerza, ass_compulsive_move) y la supresion de arrastre en derrape.
 *
 * Convenciones del API (exterior):
 *   - angulos en radianes, positivo = giro a la DERECHA (horario visto
 *     desde arriba). Internamente se usa la convencion del PC (+ = izquierda).
 *   - velocidad en m/s "del mundo del juego". OJO: el mundo de OR2 esta
 *     escalado; el velocimetro muestra v*3.6*P[0x2438] (aprox 0.63), ver
 *     campo velocidad_hud.
 *   - El paso interno es fijo a 60.2 Hz (como el original, cuyo modelo de
 *     neumatico es "deadbeat" para ese paso). or2_paso acumula dt.
 *
 * No contiene datos del juego: la tabla se carga en tiempo de ejecucion.
 */
#ifndef OR2_FISICA_H
#define OR2_FISICA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OR2_COLUMNAS     19      /* columnas (coches) de la tabla           */
#define OR2_FILA_BYTES   0x4c    /* 19 floats por fila                       */
#define OR2_FILAS_MIN    130     /* filas validas: 0x0000..0x264c            */
#define OR2_PAR_MUESTRAS 160     /* muestras de la curva de par              */
#define OR2_FRENO_MUESTRAS 256   /* muestras de la curva de presion de freno */
#define OR2_HZ           60.2f

typedef struct {
    /* --- carroceria / geometria --- */
    float masa;            /* 0x0000 kg                                     */
    float k_inercia[3];    /* 0x004c,0x0098,0x00e4 inercia = masa*k (x,y,z) */
    float batalla;         /* 0x0130 m                                      */
    float via[2];          /* 0x017c,0x01c8 via delantera/trasera m         */
    float reparto;         /* 0x0214 d: carga del = d/(1+d)                 */
    float alt_cg;          /* 0x0260 (aprox. altura CG para transferencia)  */
    float transferencia;   /* no es de la tabla: 0 = sin transferencia de   */
                           /* carga longitudinal (defecto), 1 = con ella    */
    float toe[2];          /* 0x07b8,0x0804 rad                             */
    float comp_base[2];    /* 0x0850,0x089c complianza fija rad/kgf         */
    float comp_trac[2];    /* 0x08e8,0x0934 * fuerza motriz +               */
    float comp_fren[2];    /* 0x0980,0x09cc * fuerza motriz -               */
    float comp_dir[2];     /* 0x0a18,0x0a64 * |volante|                     */
    float comp_lev[2];     /* 0x0ab0,0x0afc * (1-acelerador)                */
    float radio[2];        /* 0x0b48,0x0b94 radio rueda m                   */
    float inercia_rueda[2];/* 0x0c78,0x0cc4 kgf m s^2                       */
    float rod_fija[2];     /* 0x0d10,0x0d5c coef. rodadura * carga          */
    float rod_cuad[2];     /* 0x0da8,0x0df4 perdida = F^2 / este            */
    float mu_max[2];       /* 0x0e40,0x0e8c                                 */
    float mu_min[2];       /* 0x0ed8,0x0f24                                 */
    float rigidez[2];      /* 0x0f70,0x0fbc rigidez de deriva kgf/rad       */
    float curva_der[2];    /* 0x1008,0x1054 agarre objetivo en derrape      */
    /* --- motor / transmision --- */
    int   marchas;         /* 0x10a0 (entero)                               */
    float rel_extra;       /* 0x10ec                                        */
    float rel[8];          /* 0x1184.. rel[1..6] (rel[0]=0 punto muerto)    */
    float rel_final;       /* 0x134c                                        */
    float w_baja2;         /* 0x1398 rad/s bajar 2->1                       */
    float w_baja;          /* 0x13e4 rad/s bajar marchas intermedias        */
    float w_baja_top;      /* 0x1430 rad/s bajar desde la ultima            */
    float w_baja_freno;    /* 0x147c rad/s bajar frenando                   */
    float w_sube;          /* 0x14c8 rad/s subir marcha                     */
    float cilindros;       /* 0x1514                                        */
    float diametro;        /* 0x1560 m                                      */
    float carrera;         /* 0x15ac m                                      */
    float w_ralenti;       /* 0x15f8 rad/s                                  */
    float w_corte;         /* 0x1644 rad/s limitador                        */
    float w_max;           /* 0x1690 rad/s                                  */
    float inercia_motor;   /* 0x16dc kgf m s^2                              */
    float par_mult;        /* 0x1728                                        */
    float embrague_cap;    /* 0x1774 kgf m                                  */
    float freno_g;         /* 0x17c0 freno total = freno_g*masa kgf         */
    float freno_rep[2];    /* 0x180c,0x1858 reparto                         */
    float empuje[8];       /* 0x24d0.. (fila 123+marcha) multiplicador      */
    /* --- direccion --- */
    float dir_k1;          /* 0x18a4                                        */
    float dir_k2;          /* 0x18f0                                        */
    float radio_giro;      /* 0x193c m                                      */
    /* --- aerodinamica --- */
    float area;            /* 0x1988                                        */
    float cx;              /* 0x19d4                                        */
    /* --- RearGripCtrl --- */
    float grip_baja;       /* 0x1c34                                        */
    float grip_media;      /* 0x1c80                                        */
    float grip_sube;       /* 0x1ccc                                        */
    float grip_amin;       /* 0x1d18                                        */
    float grip_amax;       /* 0x1d64                                        */
    /* --- SlipAngleCtrl --- */
    float c0_der[2];       /* 0x1db0,0x1e48                                 */
    float c1_der[2];       /* 0x1dfc,0x1e94                                 */
    float c0_nor[2];       /* 0x1ee0,0x1f78                                 */
    float c1_nor[2];       /* 0x1f2c,0x1fc4                                 */
    float k_guinada;       /* 0x2010                                        */
    float max_dslip;       /* 0x205c                                        */
    float arrastre_lat;    /* 0x20a8 (1 = arrastre lateral fuera de derrape)*/
    float ass_base;        /* 0x20f4                                        */
    float ass_fuera;       /* 0x2140                                        */
    float ass_dentro;      /* 0x218c                                        */
    float ass_borde;       /* 0x21d8                                        */
    float adj_fuera;       /* 0x22bc                                        */
    float ass_sin;         /* 0x2224                                        */
    float ass_tiempo;      /* 0x2270                                        */
    float adj_pend;        /* 0x2308                                        */
    float adj_opu;         /* 0x2354                                        */
    float adj_opu_k;       /* 0x23a0                                        */
    float adj_dentro;      /* 0x23ec                                        */
    float hud;             /* 0x2438 factor del velocimetro                 */
    /* --- curvas (fuera de la tabla de parametros) --- */
    float par_idx;         /* muestras por rad/s                            */
    float par_gan;         /* ganancia                                      */
    float par[OR2_PAR_MUESTRAS];
    float freno[OR2_FRENO_MUESTRAS];
    int   curvas_reales;   /* 1 si se cargaron del ejecutable               */
    /* --- derivados --- */
    float dir_g;           /* angulo geometrico de direccion (unid. 16 bit) */
    float cg_del, cg_tras; /* distancia CG-eje m                            */
    float carga_est[2];    /* carga estatica por rueda kgf                  */
    float sube[8], baja[8], baja_fr[8]; /* umbrales m/cuadro               */
} or2_param_t;

typedef struct {
    float stick;            /* -1..1, + = derecha                           */
    float acelerador;       /* 0..1                                          */
    float freno;            /* 0..1                                          */
    float rumbo_carretera;  /* rad, rumbo de la carretera junto al coche     */
    float rumbo_adelante;   /* rad, rumbo de la cuerda hacia un punto ~40 m  */
                            /* por delante (equivale a d4e del original)     */
    float dist_izq;         /* m hasta el borde izq. (>0 dentro), 0 = n/d    */
    float dist_der;         /* m hasta el borde der.                         */
    float mu;               /* adherencia del firme (1.0 asfalto, 0.7 tierra)*/
                            /* 0 se interpreta como 1.0                      */
    int   en_suelo;         /* 0 = en el aire: sin fuerzas de neumatico      */
} or2_entrada_t;

typedef struct {
    float carga_ref, carga; /* kgf                                           */
    float vel;              /* m/s modulo de la velocidad en la rueda        */
    float omega, omega_t;   /* rad/s                                         */
    float freno, agarre, comp, toe_ang, toe_comp;
    float f_lat, f_lon, resist;
    int16_t ang, ang_vel, desliz; /* unidades 65536 = 2pi (+ izquierda)    */
    float fx, fy;           /* fuerza total en el cuerpo (kgf), x adel y izq */
    float vdx, vdy;         /* direccion de la velocidad de la rueda (cuerpo)*/
} or2_rueda_t;

typedef struct {
    /* --- salidas (convencion exterior) --- */
    float velocidad;        /* m/s                                           */
    float velocidad_hud;    /* km/h como el velocimetro del juego            */
    float rumbo_vel;        /* rad, rumbo de la velocidad (+ derecha)        */
    float rumbo_cuerpo;     /* rad, rumbo del morro                          */
    float beta;             /* rad, rumbo_cuerpo - rumbo_vel                 */
    float guinada;          /* rad/s (+ derecha)                             */
    float angulo_rueda;     /* rad, angulo de las ruedas delanteras (+ der.) */
    int   derrape;          /* 1 mientras el temporizador de derrape >= 12   */
    int   temporizador;     /* event+0xd36 (cuadros)                         */
    float desliz_grados;    /* deriva trasera (event+0x4e) en grados (+der.) */
    int   marcha;           /* 0 = punto muerto                              */
    float rpm;              /* rpm del motor                                 */
    int   rpm_suave;        /* rpm filtradas para HUD/sonido (event+0x20c)   */
    int   cambio;           /* estado de cambio event+0x296 (1 sube, 3 baja) */
    /* --- estado interno (convencion PC: + = izquierda) --- */
    float vx, vy;           /* velocidad mundo m/s (x der., y adelante)      */
    float psi;              /* rumbo del cuerpo                              */
    float w;                /* guinada rad/s                                 */
    float acum;             /* acumulador de tiempo                          */
    float a_lon;            /* aceleracion longitudinal filtrada (m/s^2)     */
    float vel_mf;           /* event+0x1c4 m/cuadro                          */
    int   kmh;              /* event+0x1f4                                   */
    int16_t rumbo_v16;      /* event+0x160                                   */
    int16_t s32, s4c, s4e, s168, s16a, d44, d46, d48, d4a, d4c, d4e;
    int8_t  d36;
    float d38, d3c, d40, d54, d58;
    int   pedal_ac, pedal_fr, pedal_ac_ant, pedal_fr_ant;
    int   stick16;          /* event+0x202                                   */
    /* motor / transmision */
    float w_motor;          /* event+0x21c rad/s                             */
    float par_neto;         /* event+0x214                                   */
    float limitador;        /* event+0x2a0                                   */
    float emb_frac;         /* event+0x22c                                   */
    float emb_par;          /* event+0x228                                   */
    int   embrague;         /* event+0x3c 0..255                             */
    int   emb_estado;       /* event+0x297                                   */
    int   ac_motor;         /* event+0x34 (acelerador efectivo)              */
    int   ac_guardado;      /* event+0x29c                                   */
    int   rpm_i, rpm_ant;   /* event+0x48, 0x210                             */
    or2_rueda_t r[4];       /* 0 del-izq, 1 del-der, 2 tras-izq, 3 tras-der  */
} or2_coche_t;

/* Copia los campos de una columna. tabla = floats LE tal cual (filas*19).
 * Devuelve 0 si va bien, <0 si error. Deja curvas aproximadas: llamar
 * despues a or2_carga_curvas con las reales si se tienen. */
int  or2_carga_param(const float *tabla, int filas, int columna, or2_param_t *p);

/* Curvas reales (PS2 SLES_539.98): par = 162 floats (indice, ganancia,
 * 160 muestras), freno = 256 floats. Cualquiera puede ser NULL. */
void or2_carga_curvas(or2_param_t *p, const float *par162, const float *freno256);

void or2_inicia(or2_coche_t *c, const or2_param_t *p);
/* Coloca el coche con un rumbo (rad, + derecha) y velocidad (m/s). */
void or2_coloca(or2_coche_t *c, const or2_param_t *p, float rumbo, float velocidad);
/* Avanza dt segundos (pasos internos de 1/60.2 s). dt<=0: un paso exacto. */
void or2_paso(or2_coche_t *c, const or2_param_t *p, const or2_entrada_t *e, float dt);

#ifdef __cplusplus
}
#endif
#endif
