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
    float alt_cg;          /* 0x0260 altura del CG sobre el origen (w+224)   */
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
    /* segunda curva de par (la del cambio manual: PS2 0x3db7a8, tabla de  */
    /* punteros 0x3db118 [automatico, manual], la elige event+0x13)        */
    float par_m_idx, par_m_gan;
    float par_m[OR2_PAR_MUESTRAS];
    int   par_m_ok;
    int   modelo;          /* car+0x11 (0..29; >= 15 preparados): lo pone  */
                           /* quien llama (0 por defecto)                  */
    /* --- suspension (CalcSuspensionForce 4A1B70, ColiCar 519830) --- */
    float sus_anclaje[2];  /* 0x0390,0x03dc altura del anclaje de la rueda */
    float sus_centro[2];   /* 0x0428,0x0474 extension de reposo del muelle */
    float sus_inf[2];      /* 0x04c0,0x050c tope de compresion             */
    float sus_sup[2];      /* 0x0558,0x05a4 extension maxima               */
    float sus_muelle[2];   /* 0x0688,0x06d4 rigidez N/m                    */
    float sus_estab[2];    /* 0x0720,0x076c barra estabilizadora           */
    /* --- derivados --- */
    float dir_g;           /* angulo geometrico de direccion (unid. 16 bit) */
    float cg_del, cg_tras; /* distancia CG-eje m                            */
    float carga_est[2];    /* carga estatica por rueda kgf                  */
    float sube[8], baja[8], baja_fr[8]; /* umbrales m/cuadro               */
    float v_tope;          /* e+0xE64: punta al corte (0x1644) con la fila  */
                           /* 0x1300 (6a relacion), m/cuadro; la usa el     */
                           /* choque fuerte contra el muro (CwCrushStatus)  */
} or2_param_t;

/* --- choques (PARAM.DAT, bloques MAP1 / CHQ1 / POS1 de tools/conv_param.py) --- */
#define OR2_ESTADOS_CHOQUE 20
#define OR2_CLASES         7
typedef struct { int n; const float *k; } or2_canal_t;   /* n claves de 4 floats: t, valor, tang. sal., tang. ent. */
typedef struct {
    int      ok;
    uint32_t mapa[4][16];        /* columna de la tabla por modelo (5051D0); [1] = el normal                */
    float    umbral[2];          /* velocidad de cierre (m/cuadro): 80 y 280 km/h del modelo / 216.72        */
    float    rest[3];            /* restitucion por banda de cierre (bajo, medio, alto)                      */
    uint32_t estado_traf[8];     /* estados de choque del trafico (4F76E0)                                   */
    float    masa[3];            /* factor de masa: jugador, coche, pesado (clases 0..2)                     */
    float    reparto[OR2_CLASES * OR2_CLASES];  /* minimo del reparto de la separacion [clase A * 7 + B] */
    float    dur[OR2_ESTADOS_CHOQUE], recup[OR2_ESTADOS_CHOQUE];
    or2_canal_t pose[OR2_ESTADOS_CHOQUE][6];  /* desplazamiento x,y,z y giro x,y,z (unidades del PC)       */
} or2_choques_t;

/* Lo que el juego sabe del otro coche en un contacto (convencion exterior: mundo x a la derecha del rumbo 0,
   y hacia delante; velocidades en m/s). */
typedef struct {
    float dx, dy;           /* posicion del otro menos la mia (m)                                    */
    float vx, vy;           /* velocidad del otro (m/s); a la salida, la nueva                       */
    int   clase;            /* clase del otro (+324: 0..2 pesados, 3..5 coches, 6 rivales)            */
    int   frontal;          /* 1 = el contacto es por delante o por detras (no un roce lateral)      */
    /* salidas */
    float reparto;          /* fraccion de la separacion que se lleva el jugador (el resto, el otro) */
    int   golpe;            /* estado de choque del otro (16..19) o -1                               */
    int   cambia_vel;       /* 1 si se ha cambiado la velocidad del otro                             */
    int   reaccion;         /* 1 si el jugador ha entrado en el estado 7 (salto y derrape)           */
} or2_otro_t;

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
    int   manual;           /* 1 = cambio manual (cambio_pide decide)        */
    int   cambio_pide;      /* +1 subir, -1 bajar (solo en el paso en que se */
                            /* pulsa)                                        */
    /* grua (PlWrecker) mientras c->grua > 0: punto de la carretera donde se  */
    /* recoloca el coche, relativo al coche (mundo: x der., y adelante, m) y  */
    /* rumbo de la carretera alli (rad, + derecha). grua_ok = 0: sin punto.  */
    int   grua_ok;
    float grua_dx, grua_dy, grua_rumbo;
    /* superficie (atributo de colision del poligono bajo cada rueda y bajo el centro, como wheel+0x14 y
       event+0x248 del original): bandera 1 << indice de material de la COLI. 0 = sin dato (se usa mu para las
       cuatro ruedas y no hay resistencia de tierra). Ver or2_mu_superficie y OR2_SUP_TIERRA. */
    uint32_t sup[4];
    uint32_t sup_centro;
    /* volumen del volante ya procesado (volume0, -128..127, de or2_volante); si volumen_ok = 0 se usa  */
    /* stick * 127                                                                                     */
    int   volumen, volumen_ok;
    int   nivel;            /* nivel de la etapa (0..4): CheckWanderer acepta el material 7 en el 0  */
    int   sin_ayuda_tierra; /* 1 = sin CheckWanderer (la variante de juego 0)                        */
} or2_entrada_t;

/* materiales de tierra/arena/hierba (2, 3, 4 y 15): mu 0,7 y resistencia de rodadura (SetRunningResistance) */
#define OR2_SUP_TIERRA 0x801cu
/* materiales de muro (CbwColiWall): 5, 6, 14, 16..19 y 24 */
#define OR2_SUP_MURO   0x010f4060u

typedef struct {
    float carga_ref, carga; /* kgf                                           */
    float vel;              /* m/s modulo de la velocidad en la rueda        */
    float omega, omega_t;   /* rad/s                                         */
    float freno, agarre, comp, toe_ang, toe_comp;
    float esc;              /* +E4 escala del circulo de friccion (1 = sin saturar)                 */
    /* suspension: +28 posicion de la rueda (cuerpo), +18/+1C extension actual/anterior, +20 velocidad, */
    /* +24 fuerza del muelle (kgf, x 6 por el apriete contra el suelo); aire = bandera +0 bit 0          */
    float y28, ext, ext_ant, ext_vel, f_sus;
    int   aire;
    float f_lat, f_lon, resist;
    float mu;               /* +E8 adherencia del firme bajo la rueda (GetRoadMu + assMuControl)    */
    float r_tierra;         /* +94 resistencia fuera del asfalto (kgf, contra la velocidad de rueda)*/
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
    float vel_mf;           /* event+0x1c4 m/cuadro                          */
    int   kmh;              /* event+0x1f4                                   */
    int16_t rumbo_v16;      /* event+0x160                                   */
    int16_t s32, s4c, s4e, s168, s16a, d44, d46, d48, d4a, d4c, d4e;
    int8_t  d36;
    float d38, d3c, d40, d54, d58;
    int   pedal_ac, pedal_fr, pedal_ac_ant, pedal_fr_ant;   /* volumenes del mando 1 y 2 (0..255)  */
    int   freno_op;         /* event+0x38: el freno de OperationInput (255 mientras +2F8 > 0)        */
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
    /* cambio manual y ayudas (RearGripCtrl / CheckShiftWarning / CheckWanderer) */
    int   d90;              /* +D90 cuadros de rigidez de deriva x 0,01 por debajo de 20 km/h        */
    int   d94, d98, d9c;    /* +D94.. temporizadores de la reduccion con el motor al corte (manual)  */
    int   manual;           /* +13 cambio manual (copia de or2_entrada_t.manual)                     */
    int   e7c, e80, e84;    /* +E7C cuenta del aviso de cambio, +E80 aviso, +E84 cambio automatico   */
    int   e88, e8c, e90;    /* +E88/+E8C ayuda de vuelta a la carretera, +E90 fuera del asfalto     */
    float e94, e98;         /* +E94 lado (+26C) al salir, +E98 cuadros hasta el borde               */
    /* cuerpo vertical (ActionForce2 en 3D reducido a altura, cabeceo y balanceo sobre un suelo plano) */
    float alt, v_alt;       /* altura del CG sobre el suelo (m) y su velocidad (m/s)                 */
    float cabeceo, w_cab;   /* giro alrededor del eje x del cuerpo (rad, + morro arriba) y velocidad  */
    float balanceo, w_bal;  /* giro alrededor del eje z del cuerpo (rad, + lado derecho arriba)      */
    float rueda_y[4];       /* +134 + i*0xC: altura de cada rueda en el modelo (m), para el dibujo   */
    int   vol_ant;          /* volumen del volante del cuadro anterior (filtro 4537C0)               */
    float vol_dig;          /* +7D6884 rampa de la cruceta (-1..1)                                   */
    /* choques (crash state +2C8.., rebote +282..+290, paso derrapando +1030) */
    int   choque;           /* estado de choque (+2F0 >> 2 & 31), valido si t_choque > 0             */
    float t_choque;         /* +2C8 tiempo que queda (s)                                             */
    float t_pose;           /* +2D0 tiempo de la animacion                                           */
    float t_recup;          /* +2F8 recuperacion                                                     */
    int   lado;             /* +2F0 >> 7: 1 = animacion reflejada                                    */
    float pose_t[3];        /* +2D8 desplazamiento del cuerpo (m, x der., y arriba, z atras)         */
    float pose_r[3];        /* +2E4 giro del cuerpo (rad: cabeceo, guinada, balanceo; PC)            */
    int   c282, c283, c284; /* frenada tras golpe, cuadros de rebote, espera de rebote              */
    int   rebote;           /* +290 tipo de rebote (0 simple, 1 siguiendo la carretera)              */
    int16_t h286, h288;     /* rumbo objetivo de la velocidad y giro pendiente del cuerpo            */
    float v28c;             /* velocidad al empezar el rebote (m/cuadro)                             */
    int   roce;             /* cuadros desde el ultimo roce con el muro (work+0x244 bit 3)           */
    int   paso;             /* +1030 cuadros en que se atraviesa el trafico derrapando               */
    int   golpe_sonido;     /* se pone a 1 en cada golpe (para el sonido), lo borra quien lo lee     */
    float ofs;              /* +26C desplazamiento lateral del ultimo paso (m, <0 izquierda)          */
    float rebufo;           /* +E68 mejor puntuacion de rebufo (0..1; >0.18 = rebufo en el HUD)       */
    float rebufo_k;         /* +E6C fraccion de resistencia del aire que se quita                     */
    int   rebufo_n;         /* +E70 rampa (0..180 cuadros)                                            */
    uint32_t rebufo_marca;  /* salida de or2_rebufo: bit i = el otro i ha sido el mejor hasta ese momento con  */
                            /* mas de 0,18 y |+162| < 1500 (el original le pone +B72 = 180: rebufo reciente) */
    /* choque fuerte contra el muro (CwCrushStatus 5038D0) y grua (PlWrecker 504900 / WrecPlBody 4A0C70) */
    int   grua;             /* +D68 cuadros que le quedan a la grua (0 = inactiva)                    */
    int   grua_ini;         /* +D6A cuadros con que empezo                                            */
    int   recolocar;        /* 1 en el paso en que la grua deja el coche en la carretera: el juego    */
                            /* lo pone en el punto de la grua (lo borra quien lo lee)                 */
    float corr_lat;         /* RebPlBody_Sub2 4A0320: desplazamiento lateral pedido durante el rebote */
                            /* (m, + derecha de la carretera); lo suma y lo borra el juego            */
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
/* Volante del mando (pc_input_analog_453860 / PS2 0x1b3fb8 + 0x1b42e0): eje analogico -128..127 (0 =
   centro), cruceta izquierda/derecha, opcion de velocidad de la direccion (0..7, 4 por defecto; 8 fuera de
   la carrera) y mando con volante (1: zona muerta 1/128 y sin filtro). Devuelve el volumen -127..127 que va
   a or2_entrada_t.volumen. Una llamada por cuadro de logica. */
int  or2_volante(or2_coche_t *c, int eje, int izq, int der, int opcion, int en_carrera, int es_volante);
/* Segunda curva de par (cambio manual), 162 floats como en or2_carga_curvas. */
void or2_carga_par_manual(or2_param_t *p, const float *par162);
/* mu de una bandera de superficie (GetRoadMu 500B30 sin el servicio de red: 1 -> 0, tierra -> 0,7, resto 1) */
float or2_mu_superficie(uint32_t sup);

/* Bloques de choques detras de las curvas en PARAM.DAT (datos = todo el fichero). 0 si va bien. Los punteros
   de las animaciones apuntan dentro de datos, que debe seguir vivo. */
int  or2_carga_choques(const void *datos, uint32_t tam, or2_choques_t *q);
/* Columna de la tabla de un modelo (byte car+0x11) en la partida normal; -1 si no hay datos. */
int  or2_columna_modelo(const or2_choques_t *q, int modelo);

/* Contacto con otro coche (4F9450: 4F8490 reparto, 4F6FE0 reaccion, 4F7BF0 intercambio de velocidad,
   4F6820 tope de velocidad). Cambia la velocidad y el estado de choque del jugador y devuelve la reaccion del
   otro en o. Llamar una vez por cuadro de contacto. */
void or2_choque_coche(or2_coche_t *c, const or2_param_t *p, const or2_choques_t *q, or2_otro_t *o);

/* Contacto con el muro (CbwColiWall 5041B0: CalcFrictionStatus / CwReboundStatus): nx, ny = normal del muro
   hacia la calzada (mundo, unitaria), pen = penetracion (m). Llamar cada cuadro de contacto, despues de
   or2_paso. Devuelve 1 si ha sido un golpe (rebote), 0 si un roce y 2 si ademas es un choque fuerte (estados 1,
   2 o 5: animacion, perdida de velocidad y grua; ver c->grua y or2_entrada_t.grua_*). */
int  or2_choque_muro(or2_coche_t *c, const or2_param_t *p, float nx, float ny, float pen);

/* Rebufo (CheckSlipStream 4A4D20): los n coches cercanos (dx, dy, vx, vy de or2_otro_t). Llamar una vez por
   cuadro antes de or2_paso. Quita resistencia del aire y sube el corte en la ultima marcha. */
void or2_rebufo(or2_coche_t *c, const or2_otro_t *otros, int n);

/* Estado de choque de otro coche (el trafico): avanza su animacion. t = tiempo transcurrido del estado.
   Devuelve 0 cuando ha acabado; tr y gr = desplazamiento y giro (rad) del cuerpo. lado refleja la animacion. */
int  or2_pose(const or2_choques_t *q, int estado, float t, int lado, float tr[3], float gr[3]);

#ifdef __cplusplus
}
#endif
#endif
