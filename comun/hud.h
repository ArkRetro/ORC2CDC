#ifndef HUD_H
#define HUD_H
#include <stdint.h>
typedef struct {
    int tiempo, crono_ms, puntos, kmh, marcha, etapa, cuadro;
    float rpm;
    int rebufo, derrape, manual, seis, rev_on;
    int puntos_estilo;      /* cifras de la puntuacion (4BD480): 0 normal (estilo 1), 1 rebufo (0xC), 2 fuera (0xD) */
    int infarto;            /* NAVI_PUB 0x1bd544 (variante 2): sin los paneles 2C0028 / 2C0029 ni la puntuacion (1bf3e8) */
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
int hudd_sube(hud_dat_t *h, int t0, int nt);             /* texturas [t0, t0+nt) a la VRAM (nt < 0: todas); 0 bien
                                                            (-2 sin VRAM, -3 sin RAM, -4 fallo del GD) */
/* bufer de RAM (alineado a 32) para lo temporal de hudd_sube (sectores leidos de un fichero parcial, indices de las
   paletas) o NULL; quien lo presta no lo toca mientras este puesto */
void hudd_bufer(void *p, uint32_t tam);
void hudd_baja(hud_dat_t *h, int t0, int nt);
void hudd_cierra(hud_dat_t *h);                          /* libera su VRAM y su RAM */
int hudd_uso(const hud_dat_t *h, const char *nombre);    /* indice del uso o -1 */
int hudd_cuadros(const hud_dat_t *h, int uso);           /* cuadros del grupo del uso */
/* dibuja el cuadro dado (se recorta al ultimo) del uso desplazado (dx, dy), con la capa z (z > 1: encima de todo;
   fondo = 1: z baja y comparacion de profundidad normal, queda detras de la escena 3D); dentro de la lista TR */
void hudd_dibuja(hud_dat_t *h, int uso, int cuadro, float dx, float dy, float alfa, float z, int fondo);
const uint8_t *hudd_extra(const hud_dat_t *h);
int hudd_tex_uso(const hud_dat_t *h, int uso, int *t0);   /* texturas del grupo del uso: primera (t0) y cuantas */           /* lo que va detras de los 16 bytes de tacometros */
/* una vez por cuadro (despues de dibujar): libera la VRAM que se pidio liberar cuando ya no la lee la PowerVR (las
   texturas de hudd_baja / hud_baja_tex se liberan HUDD_ESPERA cuadros despues de su ultimo uso) */
void hudd_fin_cuadro(void);
int hudd_pendiente(void);                                /* liberaciones aun pendientes */
void hudd_vacia(void);                                   /* las libera ya (espera 40 ms): antes de una carga */
void hudd_color(uint32_t rgb);                           /* color de las piezas que se dibujen despues (0xFFFFFF) */
#endif
