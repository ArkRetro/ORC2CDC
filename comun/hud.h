#ifndef HUD_H
#define HUD_H
#include <stdint.h>
typedef struct {
    int tiempo, crono_ms, puntos, kmh, marcha, etapa, cuadro;
    float rpm;
    int rebufo, derrape, manual, seis, rev_on;
    int puntos_estilo;      /* cifras de la puntuacion (4BD480): 0 normal (estilo 1), 1 rebufo (0xC), 2 fuera (0xD) */
    int infarto;            /* NAVI_PUB 0x1bd544 (variante 2): sin los paneles 2C0028 / 2C0029 ni la puntuacion (1bf3e8) */
    int mision;             /* NAVI_PUB 0x1bd6f4 (variante 6): sin 2C0028 / 2C0029, el crono ni la puntuacion (C2CHUD.DAT
                               los pone la carrera) */
    int solo_puntos;        /* NAVI_PUB fuera del modo 16 (PS2 0x1bd4ac): solo el panel 2C0029 y la puntuacion */
} hud_estado_t;
int hud_carga(const char *fich, int modelo);            /* 0 bien; modelo = indice de tacometro (6 = Testarossa) */
void hud_dibuja(const hud_estado_t *e);                  /* dentro de la lista TR */
void hud_aviso(const char *nombre, int cuadro, float alfa);   /* despues de hud_dibuja */
/* cifras de la fuente de un uso (p. ej. "CRONO" = estilo 0xA) en (x, y) de la pantalla de 640x480, con el color rgb */
void hud_cifras(const char *uso, const char *txt, float x, float y, uint32_t rgb);
/* mensaje de puntos de la cola de 4B9770 ("%7d" bajo la puntuacion, 4BD480): estilo como puntos_estilo, dy pixeles
   por debajo de la puntuacion; despues de hud_dibuja */
void hud_puntos_msg(int puntos, int estilo, float dy, float alfa);
/* texturas del HUD de carrera fuera/dentro de la VRAM (los datos siguen en la RAM): los menus usan ese hueco */
void hud_baja_tex(void);
void hud_pon_modelo(int modelo);                        /* tacometro de otro modelo (con las texturas bajadas) */
int hud_sube_tex(void);                                  /* 0 bien */
uint32_t hud_suelta_quietas(uint32_t cuadros);           /* apuro de VRAM: echa las no dibujadas en `cuadros`; bytes */

/* ---- acceso generico a un fichero con el formato de HUD.DAT (ver tools/conv_hud.py), p.ej. MENU.DAT ---- */
typedef struct hud_dat hud_dat_t;
/* parcial = 1: en la RAM solo las tablas; las texturas se leen del GD al subirlas (ficheros grandes) */
hud_dat_t *hudd_abre(const char *fich, int modelo, int parcial);
hud_dat_t *hudd_abre_gd(const char *fich);               /* hudd_abre parcial con el GD para el (musica_pausa_lectura) */
int hudd_sube(hud_dat_t *h, int t0, int nt);             /* texturas [t0, t0+nt) a la VRAM (nt < 0: todas); 0 bien
                                                            (-2 sin VRAM, -3 sin RAM, -4 fallo del GD) */
/* bufer de RAM (alineado a 32) para lo temporal de hudd_sube (sectores leidos de un fichero parcial, indices de las
   paletas) o NULL; quien lo presta no lo toca mientras este puesto */
void hudd_bufer(void *p, uint32_t tam);
/* las tablas del siguiente hudd_abre parcial en p (cap bytes) si caben, en vez del monton (no se liberan al cerrar) */
void hudd_tablas_en(void *p, uint32_t cap);
/* lo mismo sin el monton: las tablas y las piezas de sus grupos en p o el hudd_abre falla (no se liberan al cerrar) */
void hudd_tablas_fijas(void *p, uint32_t cap);
uint32_t hudd_tablas_ocupa(void);                        /* bytes de esa RAM que ocupo el ultimo hudd_abre (0: ninguno) */
void hudd_baja(hud_dat_t *h, int t0, int nt);
void hudd_cierra(hud_dat_t *h);                          /* libera su VRAM y su RAM */
/* fichero parcial: suelta las piezas de los grupos que no usan las texturas [t0, t0 + nt) (bytes) / las vuelve a leer */
uint32_t hudd_grupos_suelta(hud_dat_t *h, int t0, int nt);
uint32_t hudd_grupos_suelta2(hud_dat_t *h, int t0, int nt, int t1, int n1);   /* lo mismo con dos rangos */
int hudd_grupos_carga(hud_dat_t *h);
/* reglas al dibujar los grupos (comun/menu.c: el carrusel del menu principal sin MULTIPLAYER): una pieza del sprite spr
   se dibuja con el sprite nuevo (-1: no se dibuja) si vale la condicion de su modo: 0 siempre; 1 con su x en [x0, x1];
   2 si en el mismo cuadro hay una pieza del sprite ancla con x - la suya en [x0, x1]. sitio = 1: en el sitio (x, y,
   ancla de giro) de la pieza del sprite nuevo mas opaca del grupo (rotulos quietos). Vale la primera regla que se
   cumpla; hudd_reglas(NULL, 0) las quita */
typedef struct { int16_t spr, nuevo, ancla; uint8_t modo, sitio; float x0, x1; } hudd_regla_t;
void hudd_reglas(const hudd_regla_t *r, int n);
/* piezas del cuadro de un uso (sprite, x, y, alfa del fichero; para sacar las reglas): cuantas */
typedef struct { int spr; float x, y, alfa; } hudd_pz_t;
int hudd_piezas(const hud_dat_t *h, int uso, int cuadro, hudd_pz_t *o, int max);
int hudd_uso(const hud_dat_t *h, const char *nombre);    /* indice del uso o -1 */
int hudd_cuadros(const hud_dat_t *h, int uso);           /* cuadros del grupo del uso */
/* dibuja el cuadro dado (se recorta al ultimo) del uso desplazado (dx, dy), con la capa z (z > 1: encima de todo;
   fondo = 1: z baja y comparacion de profundidad normal, queda detras de la escena 3D); dentro de la lista TR */
void hudd_dibuja(hud_dat_t *h, int uso, int cuadro, float dx, float dy, float alfa, float z, int fondo);
const uint8_t *hudd_extra(const hud_dat_t *h);
int hudd_tex_uso(const hud_dat_t *h, int uso, int *t0);   /* texturas del grupo del uso: primera (t0) y cuantas */
int hudd_sube_uso(hud_dat_t *h, int uso);                /* las de hudd_tex_uso a la VRAM: 0 bien o nada, si no hudd_sube */
int hudd_uso_residente(const hud_dat_t *h, int uso);      /* 1 si todas las texturas del uso estan en la VRAM */
void hudd_baja_uso(hud_dat_t *h, int uso);               /* y fuera (hudd_baja) */           /* lo que va detras de los 16 bytes de tacometros */
/* una vez por cuadro (despues de dibujar): libera la VRAM que se pidio liberar cuando ya no la lee la PowerVR (las
   texturas de hudd_baja / hud_baja_tex se liberan HUDD_ESPERA cuadros despues de su ultimo uso) */
void hudd_fin_cuadro(void);
int hudd_pendiente(void);                                /* liberaciones aun pendientes */
void hudd_vacia(void);                                   /* las libera ya (espera 40 ms): antes de una carga */
void hudd_color(uint32_t rgb);
/* escala (sx, sy) alrededor del centro de la pantalla de lo que dibuje hudd_dibuja despues (4656B0: la matriz del
   elemento de 465860, diag(sx, sy) y su traslacion; 1, 1 sin escala) */
void hudd_escala(float sx, float sy);
/* sprites sueltos (comun/sprani.c): ver hud.c */
void hudd_capa(hud_dat_t *h, float z, int fondo);
void hudd_sprite(int s, const float *m, const float *q, float alfa, int aditiva);
int hudd_sprite_tex(const hud_dat_t *h, int s);
float hudd_z(void);                                      /* z de la siguiente pieza */
void hudd_todas_usables(hud_dat_t *h);                   /* todas sus texturas se pueden subir (SPRANI con usos) */
int hudd_n_tex(const hud_dat_t *h);                       /* texturas del fichero */
int hudd_residente(const hud_dat_t *h, int t);                           /* color de las piezas que se dibujen despues (0xFFFFFF) */
#endif
