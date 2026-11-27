/* infarto.c: el modo INFARTO (HEART ATTACK, variante 2 de [780258] = PS2 0x443508) como el original.

   Fuente: el port de PC (outrun2-decomp, platform/race_traffic_heart*.inc, modulo 45AFB0..4650E3) contrastado con el
   ELF de PS2 (SLES-53998), que es el mismo codigo con otras direcciones; donde difieren manda el de PS2 (se cita).
   Bloque de estado: PC 7F1900..7F2A00 / 7F8A00..7F8C00 = PS2 0x589ac0 (+0x48 tipo de la peticion) y 0x58a530
   (corazones). Los nombres de abajo llevan la direccion del PC.
   - Gestor (464350 = PS2 0x1ae440), un paso por cuadro de logica (464D20 = PS2 0x1a7f28): en la etapa (no en la union)
     recorre la mision de la etapa (registros de 10 bytes de INFARTO.DAT) con la posicion de carretera del coche
     (car+0x64): las acciones previas (fin < 0, 463950 = PS2 0x1af060) se hacen al llegar a su seccion; un registro
     de peticion empieza al llegar a su seccion si no hay otra en curso (tipo 3 = ninguna) con la voz 0xE8 y su
     puesta en marcha; acaba al llegar a su fin (+2) con la evaluacion de su tipo.
   - Mision de la etapa (45C2C0 / 45C160 = PS2 0x1aeff0 / 0x1aec08 / 0x1aedb8): la base es el indice del descriptor
     de tramo de la clave (44DC50); a partir del nivel 2 las notas de los niveles anteriores eligen la variante
     especial (misiones 0x0F..0x28).
   - Corazones: 7F23C4 (10000 por corazon). Cada peticion acaba con una nota 0..7 (45C990 = PS2 0x1b0780: 12, 20,
     30, 45, 60 y mas de 79 corazones del resultado; tope 5 con golpes al trafico, -3 si le han adelantado, -4 con un
     choque) y suma los corazones de su resultado (45DCE0 = PS2 0x1b3708: con nota 5 o mas, la tabla del nivel y de
     la clave). Fuera de las peticiones un choque quita 50 corazones (voz 0x150), que le adelanten tras un choque 20
     (0x14C) y un golpe de un coche mas rapido 10 (0x14B) (PS2 0x1a8100..0x1a8250).
   - Puntos de la peticion en curso (45E2F0 = PS2 0x1a8988 con la tabla del tipo): choque, adelantado, golpe y
     trompo restan del resultado y de los corazones; muro y cada rueda fuera del asfalto restan del resultado por
     cuadro.
   - Tipos que se portan (los que no necesitan los objetos OSO de la mision): 0 (45E610 / 462FF0 = PS2 0x1a8d88 /
     0x1a90a8: derrapar desde que empieza el derrape hasta el final del tramo), 2 y 22 (45F3D0 / 4635C0 = PS2
     0x1aab18 / 0x1aad20: adelantar coches), 6 (45F540 / 4636F0 = PS2 0x1aaf18 / 0x1ab058: llegar lejos sin
     incidentes). Los demas (7, 8, 11, 15, 16, 17, 19, 20, 21, 23, 24, 25: conos, monedas, arcos, cajas, lineas del
     carril...) no empiezan: ni su anuncio ni su nota (PENDIENTE: los objetos de OSO.PS2 HAM_OSOnn y su logica).
   - Coches de la mision (45D710 / 45D7A0 / 45D470 = PS2 0x1b2e58 / 0x1b2b20): la accion -14 elige una tabla; sus
     coches se abren cuando el jugador esta entre 30 y 50 posiciones antes de su seccion (o todos a la vez con el bit
     0x80); acciones -1 / -2 (46E4A0): sin trafico normal / con el (el gestor de main.c cierra el de la etapa).
   - Voces: cola de 8 (45BD30 / 45BCD0 = PS2 0x1ac3d8) con el reloj de cuadros; las del banco HAM de la etapa.
   SIN PORTAR (PENDIENTE): la hoja de resultados de la etapa
   (4603F0) y los rotulos y el contador animado del HUD (45FB80 / 460AD0: banco SPRANI 0x2B), las reacciones de la
   acompanante a las peticiones (487D60 / 487D90). */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>
#include "gd.h"
#include "sfx.h"
#include "infarto.h"
#include "robot.h"
#include "haobj.h"

#define NIV 15
static uint8_t *dat;
static const uint8_t *mis, *pun, *anu, *res, *cor, *grp, *modelos;
static uint8_t modelos_c[56];
static uint32_t lba_dat, off_coches;
/* el preset [0x44350c] de la carrera (infarto_preset; 0x1760c0: 0 OutRun 2 SP, 1 OutRun 2): INFARTO.DAT lleva las
   tablas del preset 0, INFARTOP.DAT las del 1 (misiones 0x3f3ed8, coches 0x362718) y la mision base de las claves de
   los dos arboles (45C160); sin INFARTOP.DAT las de INFARTO.DAT (las bases del arbol 0) */
static int inf_preset = -1, inf_cargado = -1;
static uint32_t lba_coches;                         /* el fichero de la tabla de coches del preset */
static uint8_t base_k[15];                          /* 45C160: la mision base de la clave en el arbol del preset */
static uint8_t *mis1;                               /* misiones del preset 1 (INFARTOP.DAT) */
/* INFARTOX.DAT (tools/conv_infarto.py extra): secciones con etiqueta; PTS2 = 26 x 6 u32, la tabla de 45E2F0 por tipo */
static uint8_t *xdat; static uint32_t xtam;
static const uint8_t *xsec(const char *tag, uint32_t *n)
{
    uint32_t k, c;
    if (!xdat || xtam < 8) return NULL;
    memcpy(&c, xdat + 4, 4);
    for (k = 0; k < c && 8 + 12 * k + 12 <= xtam; k++) {
        const uint8_t *e = xdat + 8 + 12 * k; uint32_t o, l;
        memcpy(&o, e + 4, 4); memcpy(&l, e + 8, 4);
        if (!memcmp(e, tag, 4) && o + l <= xtam) { if (n) *n = l; return xdat + o; }
    }
    return NULL;
}
static const uint8_t *x_pts, *x_hoja, *x_clas, *x_rgir;   /* PTS2, HOJA, CLAS, RGIR */
static int lee_x(void)
{
    uint32_t lba, tam, n;
    if (xdat) return 0;
    if (gd_busca("INFARTOX.DAT", &lba, &tam) || tam < 8 || tam > 65536) { dbglog(DBG_ERROR, "INFARTO: sin INFARTOX.DAT\n"); return -1; }
    n = (tam + 2047) & ~2047u;
    if (!(xdat = memalign(32, n))) return -1;
    if (gd_lee(lba, (int)(n >> 11), xdat) || memcmp(xdat, "HRX1", 4)) { free(xdat); xdat = NULL; return -1; }
    xtam = tam;
    {   uint32_t l; x_pts = xsec("PTS2", &l); if (x_pts && l < 26 * 24) x_pts = NULL;
        x_hoja = xsec("HOJA", &l); if (x_hoja && l < 136) x_hoja = NULL;
        x_clas = xsec("CLAS", &l); if (x_clas && l < 26) x_clas = NULL;
        x_rgir = xsec("RGIR", &l); if (x_rgir && l < 64) x_rgir = NULL; }
    return 0;
}
static int ny_tramo;                                /* 1: la etapa del jugador es la del descriptor 0x1a (NEWY) */
/* (INFARTO 3) las tablas grandes del modo, en el monton solo mientras dura (infarto_inicia / infarto_libera): ~9 KB
   menos de bss, que son RAM del arranque */
struct ha_mem;
static struct ha_mem *hm;
#define coches_t (hm->coches_t_)                    /* 2 sectores con la tabla de coches */
#define lv (hm->lv_)                                /* 7F2548 + nivel * 0x44 (registro del nivel) */
#define m_rec (hm->m_rec_)
#define m_roto (hm->m_roto_)
#define m_efe (hm->m_efe_)
#define LV_SEL(l)   (*(int8_t *)&lv[l][0])
#define LV_U32(l, o) (*(int32_t *)&lv[l][o])
static int q = 3, rec_i, req_ini, n_req, n_mis, n_fin, quest_act, clave_act = -1, nivel_act;   /* 7F2428, 7F24E4, 7F24C8, 7F24CC */
static const uint8_t *req_r;                        /* 7F2430 */
static int32_t score, disp, h_nivel, bonus, result;  /* 7F23C4, 7F1C80, 7F2468, 7F24E8, 7F8AFC */
static int c_ev, c_golpe, c_adel, c_choque, c_rueda, c_trompo, c_muro;   /* 7F24EC, F4, F8, FC, 7F2500, 2504, 2508 */
static float v_ini, v_med, v_acu, v_min; static int v_n;               /* 7F2514, 7F2518, 7F251C, 7F24E0, 7F2520 */
static int meta_hecha, trafico = 1, contador, mueven = 1;   /* 7F250C: mueven */      /* 7F2478, 64E08C, 7F8B30 */
static uint32_t reloj, voz_ult;                     /* [842110] (cuadros), 7F8B40 */
static struct { int id, retardo, prio; } vcola[8]; static int vc_cab, vc_fin;   /* 7F1998, 7F1C64, 7F1994 */
static int dx23dc;                                  /* 7F23DC */
/* tipo 0 */
static int k0_fase, k0_9c, k0_98, k0_a0, k0_ab0, k0_aac, k0_a, k0_b;   /* 7F8A90, 7F8A9C, 7F8A98, 7F8AA0, 7F8AB0, 7F8AAC, 7F8AB4/6 */
/* tipos 2 y 22 */
static int k2_ini, k2_ult, k2_c, k2_d;              /* 7F2988, 7F8B38, 7F1C50, 7F1C52 */
/* tipo 6 */
static int k6_a, k6_b, k6_max;                      /* 7F23BC, 7F23BE, 7F23C0 */
/* tipos 7 y 24 (golpea el cono azul; 24: derrapando): 7F1C88 / 7F1C8A, 7F1C90 clase, 7F1C8C cuantos, 7F1C8E / 7F1C8F */
static int k7_cl, k7_need, k7_ok, k7_mal;
/* tipo 15 (recoge monedas): 7F1C9C / 7F1C9E secciones, 7F1CA0 cuantas, 7F1CA2 de la clase 0x0A en el tramo, 7F1CA3 / 7F1CA1 */
static int k15_need, k15_tramo, k15_a, k15_39;
/* tipos 16 y 25 (pasa por el arco; 25 derrapando): 7F1C74 clase, 7F1C70 cuantos, 7F1C71 cruzados; tramo de la accion -5
   (7F24D0 / 7F24D4) para la -11 */
static int k16_cl, k16_need, k16_n, a5_lo, a5_hi; static const infarto_coche_t *c_act;
/* tipo 19 (PS2 0x589d58..: +8 tope 7F8ADC, +A n 7F8ADE, +C c 7F8AE0, +E rotas 7F8AE2, +10 cogidas 7F8AE4, +1C activas
   7F8AF0) y la llamada [7F2528] = 0x1b11c8 (-15 / -16) */
static int k19_lim, k19_n, k19_c, k19_rot, k19_act, k19_cb; static int8_t k19_cog;
static float ant_pos[3]; static int ant_ok;
/* tipos 8 y 23 (circula por el azul / el rojo; 23: derrapando) */
static int k11_need, k11_c, k11_n, ani_on;         /* 0x5899bc.. (+8, +C, +A) y el anillo en marcha (11; el 9 en 0x5899e0..) */
/* tipo 14 (0x589c48..: +40 la cadena, +98 los que faltan, +AC escoltas, +AE / +B0 puesto y el anterior, +B6 cuadros que
   se guarda el puesto): las plazas de los escoltas (HA_ESC en su marca) */
static int k14_n, k14_pide, k14_vel, k14_ac, k14_ant, k14_hold, k14_sube; static int8_t k14_pl[24];
static struct { int16_t sec; int8_t carril, id; } esc_q[8]; static int esc_n, esc_i;   /* -12 (0x1ab3c8): por abrir */
/* tipo 20 (0x589da0..: +0 / +2 el tramo, +C cuadros, +10 golpeados, +12 / +14 p6 / p8; +B66 de cada coche) */
static int k20_lo, k20_hi, k20_need, k20_c, k20_n, k20_t, k20_rob; static int16_t k20_fr[24]; static uint8_t k20_tocado[24], k20_ver[24];
static float k20_p[24][3];
static int k9_lo, k9_hi, k9_cruces, par_on;        /* 0x5899e4 / 0x5899e6, los pares cruzados en infarto_autos */
static int k8_a, k8_b, k8_lado, k8_ult, k8_dist, k8_max;   /* 7F2530, 7F2532, 7F2538, 7F253C, 7F2540, 7F2534 */
/* reacciones de la acompanante que piden las peticiones (487D60 / 487D90 / 487DD0; robot 1: comun/robot.c robot_pide,
   con los movimientos de CHRH.DAT) */
static void rob_mov(uint32_t id) { robot_pide(1, id, 0, 0, 0); }        /* 487D60: +68 = id, +78 = 2 */
static void rob_cara(int v) { robot_pide(0, 0, 1, v, 0); }              /* 487D90: +70 = v, +78 = 0, +7C = 1 */
static void rob_para(void) { robot_pide(0, 0, 0, 0, 1); }               /* 487DD0: +7C = 0 */
static uint8_t *chrh;                               /* CHRH.DAT (tools/conv_chr.py --infarto) */
/* el animo de la acompanante (PS2 0x1aac60): el coche 9 siempre 0x6003D; si no, rand x 2^-31 < 0,5 -> otro, si no 0x6003E */
static uint32_t anima(int modelo, uint32_t otro)
{
    if (modelo == 9) return 0x6003d;
    return (float)rand() * 4.65661287e-10f < 0.5f ? otro : 0x6003e;
}
/* 45B380 (PS2 0x1aa3a0): 1 en los coches 2..24 cuyo byte de la tabla es 0 (los "callados": sin reacciones) */
static int modelo_callado(int m)
{
    static const uint8_t B[23] = { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0 };
    unsigned k = (unsigned)(m - 2);
    return k <= 0x16 && !B[k];
}
static int modelo_jug;                              /* car+0x11 del jugador (el ultimo paso) */
/* 45BDF0 (PS2 0x1ad6f4): el panel de la nota 7F8B2C que entra 7F8B28 cuadros (0x78 al cerrar la peticion) */
static int cara_t, cara_nota;                       /* 7F8B28, 7F8B2C */
/* 4603F0 (PS2 0x1ad004): la hoja de resultados mientras el jugador esta en la union (7F8B34: cuadros en ella) */
static uint32_t hoja_t; static int hoja_sig = -1, hoja_k, hoja_hb = -1, hoja_hb_t, hoja_hs = -1, hoja_hs_t;
/* coches de la mision */
static const uint8_t *ct_base; static int ct_v, ct_i, ct_n, ct_pend = -1, ct_sec;  /* 7F24B8, 7F24C0, 7F24BC, 7F24C4 */
static infarto_hud_t hud;
/* 45FB80 (PS2 0x1ac580): el medidor de la peticion en curso. Registros de 0x1C desde 7F1A08 (PS2 0x58a580): el 0 es el
   corazon del centro (ancla 7F19FC = (234, 108) con la vista por defecto; la proyeccion del ancla 5E0AC8 con la camara
   +34A > 1 no esta portada), del color de 0x3f0b20[corazones % 7] (2B000F..2B0015) en bucle 0x1F..0x3C; 1..19 el anillo
   de radio 64 alrededor del centro del rotulo (176, -96), a 0,33 rad cada uno (0x1acb84..0x1acc0c). Cuantos: los
   corazones del resultado + 1 por 0,25 (0x1acaac), con tope (7 o 6 - 2 golpe - 3 adelantado - 4 choque) x 2,7031 si ha
   habido sucesos (0x1acaf4..0x1acb60). Uno nuevo aparece (2B0015 0x3D..0x69, luego late en 1..0x3B) con el efecto 0xD7
   (el primer cuadro con la y al reves: 0x1acdd0 la guarda como -cy - a y 0x1acd34 como a - cy, igual en el PC); uno que
   sobra se rompe (2B000E a 2,5 cuadros por paso). Al ganar corazones (0x1ac91c): el del centro cambia de color y, si no
   es una peticion de los tipos 0, 6, 8 o 23 con 7F2510 > 0, sale un corazon (efecto 0x52, uno cada 10 pasos como mucho)
   que vuela del centro al anillo (45B860 = PS2 0x1abde8, tipo 2, 0x3f0b20[5]); al perderlos (0x1ac85c) el centro se
   rompe (2B000E, uno cada 20 pasos) y cambia de color. Al cerrar la peticion (7F19F8 = 2, 0x1ace3c), si se veian 7 o mas,
   cada corazon (el del centro el primero) vuela a (280, 200) (0x3efdd8) con el movimiento 1, 2, 3 (0x3f0b90[k % 3]) y
   velocidad (20 - k) x 0,3 + 0,5. Efectos (45B950 = PS2 0x1abee0, 16 de 0x38): esperan, abren 0x4C..0x5A moviendose
   (1/15 x velocidad x pasos del recorrido; 0: parabola x = (y - 100)^2 x 0,008 + 200; 1 y 3: un eje lineal y el otro
   acercandose 0,335 x velocidad por paso; 2: lineal), abren 0x5B..0x69 donde quedan y se cierran */
typedef struct { int usado, g, ini, fin, modo; float c, vel, x, y; int volteado; } spr_t;   /* modo 0 bucle, 1 una vez */
static int16_t m_f18[20];                          /* +0x18: color del centro / 7 en el anillo */
static spr_t m_alerta;                             /* [688B34]: 2B000E en (166, 182) mientras baja el contador (4BCD50) */
typedef struct { int usado, est, n, tipo, f, vida; float x0, y0, x1, y1, x, y, vel; spr_t s; } efe_t;
static int m_efe_n, m_efe_i;                        /* 7F8B20, 7F8B24 */
struct ha_mem {
    uint8_t coches_t_[4096];
    uint8_t lv_[NIV][0x44];
    spr_t m_rec_[20];                              /* 7F1A08 + k * 0x1C: centro y anillo */
    spr_t m_roto_[24];                             /* 2B000E sueltos (roturas) */
    efe_t m_efe_[16];                              /* 7F2038 (16 x 0x38) */
};
static int32_t m_prev;                              /* 7F1C3C */
static int m_t8c74, m_t8c78, m_t2510, m_visto;      /* 7F8C74, 7F8C78, 7F2510, 7F1CA8 */
static uint32_t m_tinte = 0xffffffff;               /* 7F8B68: color del marco del rotulo */
static float m_rx[19], m_ry[19]; static int m_tab;  /* 7F8BCC.. (cos x 64, sin x -64) */
/* rotulo de la peticion (460970 / 460AD0 / 45BEC0 = PS2 0x1ad998..): texto (tabla 0x3f0aa8: modo 0 empieza con el
   12, 0x2B003D, y a los `abre` cuadros pasa al de la peticion) y marco (0x3f0b40); `cierra` cuadros despues de entrar
   (o nunca, -1) se quita. En (176, 78). SIN la deformacion de entrada / salida ni el vaiven (7F8BB4) */
static int ban_on, ban_m4c, ban_txt, ban_mar, ban_n, ban_abre, ban_cierra, ban_ver;
static void rotulo_abre(int modo, int texto, int marco, int abre, int cierra)
{
    if (!texto) return;
    ban_on = 1; ban_m4c = modo; ban_txt = texto; ban_mar = marco; ban_n = 0; ban_abre = abre; ban_cierra = cierra;
    ban_ver = modo ? texto : 12;
}
static void rotulo_cierra(void) { ban_on = 0; }
static void rotulo_paso(void)
{
    if (!ban_on) return;
    if (ban_n <= 10) ban_n++;
    else if (!ban_m4c) {
        if (ban_abre < 0 || ban_n - 20 < ban_abre) ban_n++;
        else { ban_ver = ban_txt; ban_m4c = 1; ban_n = 0; }
    } else if (ban_cierra < 0 || ban_n - 10 < ban_cierra) ban_n++;
    else ban_on = 0;
}
int infarto_rotulo(int *texto, int *marco, int *cuadro)
{
    if (!ban_on) return 0;
    *texto = ban_ver; *marco = ban_mar; *cuadro = ban_n;
    return 1;
}
/* 462D80: el rotulo de la nota */
static void rotulo_nota(int r)
{
    static const int T[8] = { 0xb, 0xb, 0xa, 9, 8, 7, 6, 5 }, M[8] = { 1, 1, 1, 0, 0, 0, 0, 0 };
    if (r < 0) r = 0;
    if (r > 7) r = 7;
    rotulo_abre(1, T[r], M[r], -1, 0x78);
}
/* 4628D0 / 45D360 (PS2 0x1b2978): estadisticas de la union para el bono de nivel. En la union (+5C == 1): 7F1C5A la
   posicion (+260) al entrar, 7F1C5C posiciones con el temporizador de derrape (+D36 > 0), 7F1C60 posiciones con alguna
   rueda en las superficies (+24C.. >> 8) & 0x3B sin derrapar (+2A8 & 0x10F407C) */
static int16_t st_5a = -1, st_5c, st_5e = -1, st_60, st_62 = -1; static int32_t st_54;
static void estadisticas(const infarto_coche_t *c)
{
    uint32_t mm;
    if (!c->en_union) return;
    if (st_5a < 0) st_5a = (int16_t)c->seccion_union;
    if (c->d36 > 0) {
        if (st_5e > 0) { if (c->seccion_union > st_5e) { st_5c += c->seccion_union - st_5e; st_5e = (int16_t)c->seccion_union; } }
        else st_5e = (int16_t)c->seccion_union;
    } else st_5e = -1;
    if (!c->choque_ini) {
        mm = c->sup[0] | c->sup[1] | c->sup[2] | c->sup[3];
        if (((mm >> 8) & 0x3bu) && !c->derrapa) {
            if (st_62 > 0) { if (c->seccion_union > st_62) { st_60 += c->seccion_union - st_62; st_62 = (int16_t)c->seccion_union; } }
            else st_62 = (int16_t)c->seccion_union;
        } else st_62 = -1;
    }
}
/* 45D360 (PS2 0x1b2978): bono del nivel l (0 si alguna peticion suya saco nota 0..5): min(5, derrape / (t x 0,8 / 5))
   o min(5, firme / (t x 0,7 / 5)), el mayor, x 10, con t las posiciones en la union; mas (v - 250) x 2 con v la
   velocidad media del nivel x el parametro +2438 del coche si pasa de 250 */
/* 4513C0 / 44FF20 (race_manager.cpp: 7D3938 / 7D3930 / 7D3888): la suma de +1C4 del jugador en cada paso de logica
   desde la ultima etapa terminada (tope 0x57E3F pasos); la media x 216,72 se guarda en el nivel al terminar la etapa
   (44FF20, antes de poner la suma a 0) y en la meta (4513C0) */
static float vs_suma, vel_nivel[NIV], hud_coche; static int vs_n;
static float vel_media(void) { return vs_n ? vs_suma / (float)vs_n * 216.720001f : 0.0f; }
static int bono_nivel(int l, int sec_union)
{
    int k, a, b; float t;
    for (k = 0; k < 6; k++) if (lv[l][0x38 + k]) return 0;
    t = (float)(sec_union - st_5a);
    if (st_5a >= 0 && t > 0) {
        a = (int)((float)st_5c / (t * 0.8f / 5.0f)); if (a > 5) a = 5;
        b = (int)((float)st_60 / (t * 0.7f / 5.0f)); if (b > 5) b = 5;
        if (a > b) b = a;
        st_54 += b * 10;
    }
    /* velocidad media del nivel (4505F0: [7D3838 + nivel * 4]) x el factor del velocimetro del coche (+2B4 -> +2438):
       por encima de 250 (PS2 0x1b2ab0, 0x437a0000) suma (v - 250) x 2 */
    {   float v = vel_nivel[l] * hud_coche;
        if (v > 250.0f) st_54 += (int)(v - 250.0f) * 2;
    }
    return st_54;
}
static int sec_union_ult;
/* filas de 4BCD50 (PS2 0x1bda80): corazones de cada nivel terminado 7F2438 (con el bono) y lo que ensena 7F244C (empieza
   sin el bono), los de la etapa en curso 7F2460 y lo que ensena 7F2464; se acercan de 3 en 3 al final de 464350 */
static int32_t niv_tot[NIV], niv_ver[NIV], tot_obj, tot_ver; static int alerta;   /* + 7F1CAC */
static void filas_paso(void)
{
    int i;
    tot_obj = disp / 10000 - h_nivel;
    if (tot_obj != tot_ver) {
        int32_t v = tot_ver + (tot_obj >= 0 ? 1 : -1) * 3;
        tot_ver = (v < 0 ? -v : v) > (tot_obj < 0 ? -tot_obj : tot_obj) ? tot_obj : v;
    }
    for (i = 0; i < nivel_act && i < NIV; i++) {
        int32_t c = niv_tot[i], v;
        if (!c || niv_ver[i] == c) continue;
        v = niv_ver[i] + (c >= 0 ? 1 : -1) * 3;
        niv_ver[i] = (v < 0 ? -v : v) > (c < 0 ? -c : c) ? c : v;
    }
}
int infarto_filas(int32_t *v)
{
    int i, n = nivel_act < 0 ? 0 : nivel_act > 4 ? 4 : nivel_act;
    for (i = 0; i < n; i++) v[i] = niv_ver[i];
    v[n] = tot_ver;
    return n;
}
int infarto_alerta(void) { return alerta; }
static int medidor;                                 /* 7F19F8: 1 con una peticion en curso, 2 se cierra */
static void medidor_paso(void);
static void efectos_paso(void);
static void spr_abre(spr_t *s, int g, int ini, int fin, int modo, float x, float y);
static int spr_acabado(const spr_t *s);

static int16_t rd16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static int32_t rd32(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }

void infarto_preset(int preset) { inf_preset = preset; }
void infarto_tramo_1a(int si) { ny_tramo = si; }
/* INFARTOP.DAT (tools/conv_infarto.py): las tablas del preset 1 y las misiones base; 0 bien */
static int lee_preset(void)
{
    uint32_t lba, tam, o[4]; uint8_t *b;
    int p1 = inf_preset == 1, arb = p1 ? 0 : 1;     /* 0x1760c0: preset 1 -> csc_data_2 (arbol 0) */
    lba_coches = lba_dat;
    memcpy(base_k, grp + 16, 15);                   /* (las del arbol 0, en INFARTO.DAT) */
    if (gd_busca("INFARTOP.DAT", &lba, &tam) || tam < 20) {
        dbglog(DBG_ERROR, "INFARTO: sin INFARTOP.DAT: las tablas del preset 0 y las bases del arbol 0 (preset %d)\n", inf_preset);
        return -1;
    }
    if (!(b = memalign(32, 2048))) return -1;
    if (gd_lee(lba, 1, b) || memcmp(b, "HRP1", 4)) { free(b); return -1; }
    memcpy(o, b + 4, sizeof o);
    if (o[3] != tam || o[1] + 32 > o[2]) { free(b); return -1; }
    {   uint32_t s0 = o[1] & ~2047u; uint8_t *c = memalign(32, 4096);
        if (!c || gd_lee(lba + (s0 >> 11), 2, c)) { free(c); free(b); return -1; }
        memcpy(base_k, c + (o[1] - s0) + 16 * arb, 15); free(c);
    }
    free(b);
    if (p1) {
        uint32_t s0 = o[0] & ~2047u, n = (o[0] + 41 * 0x140 - s0 + 2047) & ~2047u;
        uint8_t *m = memalign(32, n);
        if (!m || gd_lee(lba + (s0 >> 11), (int)(n >> 11), m)) { free(m); return -1; }
        mis1 = m; mis = m + (o[0] - s0);
        lba_coches = lba; off_coches = o[2];
    }
    return 0;
}

int infarto_inicia(void)
{
    uint32_t tam, o[8];
    if (dat && inf_cargado == inf_preset) return 0;
    if (dat) infarto_libera();                      /* otro preset: otra vez */
    if (!hm && !(hm = memalign(32, sizeof *hm))) return -1;
    memset(hm, 0, sizeof *hm);
    /* en la RAM todo menos las tablas de coches (51 KB: se lee del GD la de cada accion -14, 0x320 bytes) */
    if (gd_busca("INFARTO.DAT", &lba_dat, &tam)) return -1;
    if (!(dat = memalign(32, 2048))) return -1;
    if (gd_lee(lba_dat, 1, dat)) { free(dat); dat = NULL; return -1; }
    memcpy(o, dat + 8, sizeof o);
    if (tam < 44 || memcmp(dat, "HRT1", 4) || rd32(dat + 40) != (int32_t)tam || o[6] > o[7]) { free(dat); dat = NULL; return -2; }
    off_coches = o[6];
    {   uint32_t n = (o[6] + 2047) & ~2047u, m = o[7] & ~2047u;   /* misiones..grupos; luego los modelos */
        uint8_t *d = memalign(32, n), *b = memalign(32, 4096);
        if (!d || !b || gd_lee(lba_dat, (int)(n >> 11), d) || gd_lee(lba_dat + (m >> 11), 2, b)) {
            free(d); free(b); free(dat); dat = NULL; return -3;
        }
        memcpy(modelos_c, b + (o[7] - m), sizeof modelos_c);
        free(b); free(dat); dat = d;
    }
    n_mis = rd32(dat + 4);
    mis = dat + o[0]; pun = dat + o[1]; anu = dat + o[2]; res = dat + o[3]; cor = dat + o[4]; grp = dat + o[5];
    modelos = modelos_c;
    lee_preset();
    lee_x();
    haobj_inicia();
    {   uint32_t lba, tam;                          /* CHRH.DAT: ~52 KB del monton mientras dure el modo */
        if (!chrh && !gd_busca("CHRH.DAT", &lba, &tam) && tam < 131072 && (chrh = memalign(32, (tam + 2047) & ~2047u))) {
            if (gd_lee(lba, (int)((tam + 2047) >> 11), chrh)) { free(chrh); chrh = NULL; }
            else robot_extra(chrh, tam);
        }
        if (!chrh) dbglog(DBG_ERROR, "INFARTO: sin CHRH.DAT (la acompanante no reacciona a las peticiones)\n");
    }
    inf_cargado = inf_preset;
    dbglog(DBG_INFO, "INFARTO: preset %d, misiones %s, coches %s, bases %d %d %d ...\n", inf_preset, mis1 ? "INFARTOP (0x3f3ed8)" : "INFARTO (0x3f0b98)",
           lba_coches != lba_dat ? "INFARTOP (0x362718)" : "INFARTO (0x35c958)", base_k[0], base_k[1], base_k[2]);
    return 0;
}

/* ---- voces: 45BD30 / 45BCD0 */
static void suena(int id) { if ((id & 0x1ff) && (id & 0x1ff) != 0x1ff) sfx_voz(id & 0x1ff); }
static int voz(int id, int retardo, int prio)
{
    int k;
    if (!(reloj - voz_ult <= (uint32_t)vcola[vc_cab].retardo)) {
        voz_ult = reloj; vcola[vc_cab].retardo = retardo; vcola[vc_cab].prio = prio; vcola[vc_cab].id = id;
        suena(id); return 1;
    }
    if (vcola[vc_fin].prio >= prio) return 0;
    vc_fin = (vc_fin + 1) & 7; k = vc_fin;
    vcola[k].prio = prio; vcola[k].id = id; vcola[k].retardo = retardo;
    return 1;
}
static void voz_bomba(void)
{
    int n;
    if (vc_cab == vc_fin) return;
    if (reloj - voz_ult <= (uint32_t)vcola[vc_cab].retardo) return;
    voz_ult = reloj; n = (vc_cab + 1) & 7; vc_cab = n;
    suena(vcola[n].id);
}

/* ---- mision de la etapa */
static int mis_tabla(int sel, int k)                /* 45C160 (PS2 0x1aec08) */
{
    int b = base_k[k >= 0 && k < 15 ? k : 0], i;
    if (sel == 1) { i = k - 3; if (i >= 0 && i <= 11) return i < 7 ? 0xf + i : 0x1a + (i - 7); }
    if (sel == 2) { i = k - 6; if (i >= 0 && i <= 8) return i < 4 ? 0x16 + i : 0x1f + (i - 4); }
    if (sel == 3) { i = k - 10; if (i >= 0 && i <= 4) return 0x24 + i; }
    return b;
}
static int mision(int k, int nivel)                 /* 45C2C0 (PS2 0x1aedb8) por la clave: el byte de nivel */
{
    int i, l = nivel < 0 ? 0 : nivel >= NIV ? NIV - 1 : nivel, e;
    if (LV_SEL(l) == -1) {
        LV_SEL(l) = 0;
        if (l == 2) {
            for (i = 0; i <= 4; i++) if (lv[0][0x38 + i] || lv[1][0x38 + i]) break;
            if (i > 4) LV_SEL(l) = 1;
        } else if (l == 3) {
            int dos = 1;
            if (LV_SEL(2) > 0) for (i = 0; i <= 4; i++) if (lv[2][0x38 + i]) { dos = 0; break; }
            if (LV_SEL(2) <= 0) dos = 0;
            if (dos) LV_SEL(l) = 2;
            else { LV_SEL(l) = 1; for (i = 0; i <= 3; i++) if (lv[2][0x38 + i]) { LV_SEL(l) = 0; break; } }
        } else if (l == 4) {
            int8_t st = LV_SEL(3); int s5 = 0, s6, dos;
            for (i = 0; i < 5; i++) s5 += (int8_t)lv[3][0x38 + i];
            s6 = s5 + (int8_t)lv[3][0x38 + 5];
            if (st == 2 && s6 == 0) LV_SEL(l) = 3;
            else {
                dos = st > 0 && s5 == 0;
                if (dos) LV_SEL(l) = 2;
                else { LV_SEL(l) = 1; for (i = 0; i <= 3; i++) if (lv[3][0x38 + i]) { LV_SEL(l) = 0; break; } }
            }
        }
    }
    e = mis_tabla(LV_SEL(l), k);
#ifdef PRUEBA_HA_MISION
    e = PRUEBA_HA_MISION;                           /* prueba: la mision de todas las etapas (con PRUEBA_NODO) */
#endif
    LV_U32(l, 4) = e;
    return e < n_mis ? e : 0;
}
static const uint8_t *mis_reg(int m, int i) { return mis + m * 0x140 + i * 10; }
static void cuenta_peticiones(int m)                /* 45E050: peticiones de la mision (7F24CC) */
{
    int i;
    n_fin = 0;
    for (i = 0; i < 32; i++) {
        const uint8_t *r = mis_reg(m, i);
        if (rd16(r) <= 0) break;
        if (rd16(r + 2) > 0 && rd16(r + 4) >= 0 && rd16(r + 4) < 0x1a) n_fin++;
    }
}

/* ---- corazones */
static void pierde(int n) { LV_U32(nivel_act, 0x34) -= n; score -= n * 10000; }   /* 45AFB0 */
static void quita(int v) { disp = score; score -= v; }
static void resultado(int v) { result = v - bonus; }
static int32_t res_pos(void) { return result < 0 ? 0 : result; }   /* 0x1ac4e0 */
static int nota(int h)                              /* 45C990 (PS2 0x1b0780) */
{
    int r = h < 0xc ? 1 : h < 0x14 ? 2 : h < 0x1e ? 3 : h < 0x2d ? 4 : h < 0x3c ? 5 : h > 0x4f ? 7 : 6;
    int c = c_golpe ? 5 : 7;
    if (c_adel) c -= 3;
    if (c_choque) c -= 4;
    if (r > c) r = c;
    return r < 0 ? 0 : r;
}
static float mantiene(float d)                      /* PS2 0x1b3b70: 1 hasta 10 km/h perdidos, 0 desde 30 */
{
    if (d < 10.0f) return 1.0f;
    if (!(d < 30.0f)) return 0.0f;
    d = (30.0f - d) / 20.0f;
    return d > 1.0f ? 1.0f : d;
}
static int32_t corazones_res(int32_t edi, int nt)  /* 45DCE0 (PS2 0x1b3708) */
{
    int32_t c = 0; int sel = LV_SEL(nivel_act);
    if (nt >= 5) c = (rd32(cor + 4 * (sel < 0 ? 0 : sel & 3)) + cor[16 + (clave_act >= 0 && clave_act < 15 ? clave_act : 0)]) * 30000;
    switch (n_fin) {
    case 1: return c + edi * 3;
    case 2: return edi * 3 / 2 + c / 3 * 2;
    case 3: return c / 3 + edi;
    default: return edi;
    }
}
static void empieza(int sec, int tipo)              /* 45C0E0 */
{
    c_ev = c_golpe = c_adel = c_choque = c_rueda = c_trompo = c_muro = 0;
    req_ini = sec; q = tipo; bonus = 0; v_min = 1.0e9f;
    if (tipo == 3) n_req++;
}
static void reinicia_res(void) { result = 0; }
static int sueltas;
static void suelta_coches(void) { ct_base = NULL; ct_i = 0; ct_pend = -1; sueltas++; }   /* 45D8E0 (los coches: main.c) */
int infarto_sueltas(void) { return sueltas; }
static void fin_peticion(int r, int h)              /* 462F10 (PS2 0x1b3a78) + 462EA0 */
{
    int nt = r > 7 ? 7 : r < 0 ? 0 : r, t = res[q < 26 ? q : 0];
    int tab = t == 0xff ? rd32(res + 28 + 5 * 32) : t;
    rotulo_cierra();
    voz(rd32(res + 28 + tab * 32 + nt * 4), 0x3c, 9);   /* 45DDC0 (PS2 0x1b3880) */
    if (x_rgir && !(modelo_jug == 9 && nt == 5)) {  /* 45DED0 (PS2 0x1b3988) */
        rob_para();
        if (nt > 0) rob_mov((uint32_t)rd32(x_rgir + (modelo_callado(modelo_jug) ? 32 : 0) + 4 * nt));
    }
    rotulo_nota(nt);
    lv[nivel_act][0x38 + nt]++;
    if (n_req < 5) LV_U32(nivel_act, 8 + 4 * n_req) = nt;
    LV_U32(nivel_act, 0x1c) += nt;
    if ((int8_t)lv[nivel_act][0x40] < 5) LV_U32(nivel_act, 0x20 + 4 * (int8_t)lv[nivel_act][0x40]) = h;
    score += h * 10000; lv[nivel_act][0x40]++;
    hud.nota = nt; hud.t_nota = 0; cara_nota = nt;  /* 462EA0: 7F8B2C */
    dbglog(DBG_INFO, "INFARTO: fin de la peticion tipo %d: resultado %d, nota %d, +%d corazones, total %d\n", q,
           (int)(result / 10000), nt, h, (int)(score / 10000));
    medidor = 2;                                    /* 7F19F8 = 2: se cierra (45FB80) */
    empieza(req_ini, 3);
}
static void cierra(int nt, int32_t edi, int hs)    /* cola comun de las evaluaciones */
{
    int32_t h = corazones_res(edi, nt);
    contador = (hs + hs / 2) * 10 / 7;               /* 7F8B30 (PS2 0x35c950): efecto 0xD5 cada 10 cuadros */
    cara_t = 0x78;                                  /* 7F8B28 (PS2 0x35c948) */
    fin_peticion(nt, h / 10000);
}

/* 45E2F0 (PS2 0x1a8988): puntos comunes de la peticion; t = 6 u32 de la tabla del tipo */
static const uint32_t MASCARA = 0x2f03f82u;
static int sucesos(const uint8_t *t, const infarto_coche_t *c)
{
    static const uint8_t B[14] = { 0, 0, 1, 1, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0 };
    int r = 0, k, i;
    if (!c->choque_ini) {
        if (c->impulso) { int v = rd32(t + 12); quita(v); bonus += v; c_ev++; c_golpe++; r = 1; }
        else if (c->roce) { int v = rd32(t + 20); quita(v); bonus += v; c_trompo++; r = 1; }
    } else {
        k = c->choque; i = k - 1 >= 0 && k - 1 <= 13 ? B[k - 1] : 2;
        if (i == 0) {
            int v = rd32(t); quita(v); vc_cab = vc_fin;
            voz(k == 1 ? 0x14d : 0x14e, 0x3c, 5); bonus += v; c_ev++; c_choque++; r = 1;
        } else if (i == 1) {
            if (c->impulso_rapido) { int v = rd32(t + 4); quita(v); bonus += v; vc_cab = vc_fin; voz(0x149, 0x3c, 5); c_adel++; }
            c_ev++; r = 1;
        }
    }
    for (k = 0; k < 4; k++) if (!(c->sup[k] & MASCARA)) { bonus += rd32(t + 16); c_rueda++; }
    if (c->muro) { bonus += rd32(t + 8); c_muro++; }
    return r;
}

/* ---- tipos de peticion: puesta en marcha (464350 / PS2 0x1ae768), control (464D20 / 0x1a7fe4), evaluacion
   (464350 / 0x1aea30) */
static int portado(int k) { return k == 20 || k == 14 || k == 9 || k == 11 || k == 0 || k == 2 || k == 22 || k == 6 || k == 8 || k == 23 || k == 7 || k == 24 || k == 15 || k == 16 || k == 25 || k == 19; }
static const uint8_t *pts(int k)                    /* la tabla de 45E2F0 del tipo k (INFARTOX.DAT PTS2) */
{
    static const uint8_t cero[24];
    return x_pts && k >= 0 && k < 26 ? x_pts + 24 * k : cero;
}
/* 45D040 (PS2 0x1b1150): la puesta del 19 (tambien desde la -15 con n 40 y c 50) */
static void est_puesta(int lo, int hi, int n, int cc, const infarto_coche_t *c)
{
    k19_n = n; k19_c = cc; k19_rot = 0; k19_act = 0;
    k19_lim = c ? haobj_cuenta_sec(0x12, lo, hi, c->seccion_xz) : 0;   /* 45C8E0 con la clase 0x12 */
#ifdef DIAG_HA
    dbglog(DBG_INFO, "INFARTO: estrellas %d..%d: %d en el tramo, n %d c %d\n", lo, hi, k19_lim, n, cc);
#endif
}
/* 461940 (PS2 0x1b11c8): las estrellas; el resultado 800000 / n x rotas */
static void est_control(const infarto_coche_t *c)
{
    haobj_est_t r; uint32_t l = 0; const uint8_t *pz = xsec("EST1", &l); int i;
    sucesos(pts(19), c);                            /* (0x3f0810) */
    haobj_est_paso(c->seccion, k19_lim, &k19_act, pz && l >= 140 ? (const float *)pz : NULL, &r);
    if (r.primera) rob_mov(0x220011);
    for (i = 0; i < r.rotas; i++) {                 /* +7C == 1 y +80 == 8: la estrella grande tocada */
        k19_rot++; rob_mov(0x260009); sfx_voz_se(0xd6);
#ifdef DIAG_HA
        dbglog(DBG_INFO, "INFARTO: estrella cogida (%d; trozos %d)\n", k19_rot, k19_cog);
#endif
    }
    for (i = 0; i < r.cogidas; i++) { k19_cog = (int8_t)(k19_cog + 1); sfx_voz_se(0xd6); }
    if (q == 19) resultado((int)(800000.0f / (float)(k19_n ? k19_n : 1) * (float)k19_rot));
}
static void puesta(const uint8_t *r, const infarto_coche_t *c)
{
    int w0 = rd16(r), w2 = rd16(r + 2), w6 = rd16(r + 6), w8 = rd16(r + 8);
    switch (q) {
    case 0:                                         /* 45E5B0 */
        k0_fase = 1; k0_9c = k0_98 = k0_a0 = 0; k0_a = w0; k0_b = w2; k0_ab0 = 0; k0_aac = 0; reinicia_res(); break;
    case 2: case 22:                                /* 45F310 (el 22 instala ademas un registro de aparicion: SIN PORTAR) */
        k2_c = w6; k2_d = w8; k2_ini = k2_ult = c->pasados; reinicia_res(); break;
    case 6: k6_a = w0; k6_b = w2; k6_max = 0; reinicia_res(); break;
    case 7: case 24:                                /* 464350 caso 3 (PS2 0x1ae7d0) */
        k7_cl = w6; k7_need = w8; k7_ok = k7_mal = 0; reinicia_res(); break;
    case 16: case 25:                               /* 464350 caso 9 (PS2 0x1ae8d4) */
        k16_cl = w6; k16_need = r[8]; k16_n = 0; reinicia_res(); break;
    case 15:                                        /* 464350 caso 8: 45C950 (PS2 0x1ae8a8) */
        k15_a = k15_39 = 0; k15_need = r[8];
        k15_tramo = haobj_cuenta_sec(0x0a, w0, w2, c->seccion_xz);
        reinicia_res(); break;
    case 19: est_puesta(w0, w2, w6, w8, c); reinicia_res(); break;   /* 464350 (PS2 0x1ae928) */
    case 20:                                        /* 464350 (PS2 0x1ae978): 0x1b1e10 */
        k20_lo = w0; k20_hi = w2; k20_need = w6; k20_c = w8; k20_n = 0; k20_t = 0; k20_rob = 0; reinicia_res();
        memset(k20_fr, 0, sizeof k20_fr); break;    /* (+B66 = 0 en los del tramo: aqui todos) */
    case 14: k14_pide = 1; k14_n = 0; k14_ac = k14_ant = 1; k14_hold = 0; reinicia_res(); break;   /* 0x1ab2d8 (en infarto_autos) */
    case 11:                                        /* 464350 (PS2 0x1ae854): 0x1aa358 y 0x1df060 (el anillo vacio) */
        k11_need = w6; k11_c = w8; k11_n = 0; reinicia_res(); haobj_anillo_vacia(); ani_on = 1; break;
    case 9:                                         /* 464350 (PS2 0x1ae828): 0x1a9e08 (SIN PORTAR: el registro de
                                                       aparicion que instala con 0x1f20e8) */
        k9_lo = w0; k9_hi = w2; k11_need = w6; k11_c = w8; k11_n = 0; k9_cruces = 0; reinicia_res(); par_on = 1; break;
    case 8: case 23:                                /* 464350 caso 4 (PS2 0x1ae800) */
        k8_a = w0; k8_b = w2; k8_lado = w6; k8_ult = -1; k8_dist = 0; k8_max = 0; reinicia_res(); break;
    }
    medidor = 1;                                    /* 7F19F8 = 1 */
}
static void control(const infarto_coche_t *c)
{
    int spd = (int)(c->vel_mf * 216.720001f * 0.61297071f);   /* PS2 0x1a8dc8..0x1a8de8 */
    switch (q) {
    case 0: {                                       /* 45E610 (PS2 0x1a8d88) */
        if (k0_fase == 2) {
            if (c->d36 > 0) { if (--k0_9c < 0) k0_9c = 0; }
            else if (k0_9c < 0x78) k0_9c += 2;
            else { k0_fase = 1; k0_aac = 0; rob_para(); }
        } else if (c->d36 > 0) {
            unsigned qf = (unsigned)((k0_a0 - bonus) / 80000);   /* PS2 0x1a8e50: tabla 0x3efb40 */
            k0_fase = 2; k0_9c = 0; voz(0xc6, 0x3c, 0); k0_aac = c->seccion;
            rob_cara(qf <= 2 ? 0 : qf <= 4 ? 5 : qf == 5 ? 3 : 4);
        }
        sucesos(pun, c);
        if (k0_fase == 2) {
            int span = (int16_t)(k0_b - k0_a) * 0x5a, e;
            float ratio = (float)k0_ab0 / (float)(span / 100);
            e = (int)(ratio * 800000.0f);           /* 0x1a8d18 */
            k0_98++;
            if (c->seccion > k0_aac) { k0_ab0 += c->seccion - k0_aac; k0_aac = c->seccion; }
            if (e > k0_a0) k0_a0 = e;
            if (spd > 0xf0 || (spd > 0xc8 && k0_98 % 0x3d == 0) || (spd > 0x96 && k0_98 % 0xbd == 0)) {
                static const int V[5] = { 0xcb, 0xc7, 0xc8, 0xc9, 0xca };   /* PS2 tabla 0x3efb60 */
                voz(V[k0_98 % 5], 0x3c, 0);
            }
        }
        resultado(k0_a0); break; }
    case 2: case 22: {                              /* 45F3D0 (PS2 0x1aab18) */
        int edi = c->pasados, e;
        sucesos(pun + (q == 22 ? 48 : 24), c);
        e = (int)((float)(edi - k2_ini) / (float)(k2_c ? k2_c : 1) * 800000.0f);
        resultado(e);
        if (k2_ult != edi) {
            static const int V[5] = { 0xc6, 0xc7, 0xc8, 0xc9, 0xca };   /* PS2 tabla 0x3efd90 */
            k2_ult = edi;
            if (q == 22) sfx_voz_se(0xd8);
            sfx_voz_se(0x50);
            contador = 10;
            voz(V[dx23dc % 5], 0x3c, 0);
            rob_mov(anima(c->modelo, q == 22 ? 0x6003d : 0x6003f));   /* PS2 0x1aac60 */
        }
        dx23dc++; break; }
    case 6: {                                       /* 45F540 (PS2 0x1aaf18) */
        int edi = (int)((float)(c->seccion - k6_a) / (float)(int16_t)(k6_b - k6_a) * 800000.0f);
        if (sucesos(pun + 72, c)) voz(0x14f, 0x3c, 6);
        else { rob_cara(0); rob_para(); }
        if (edi > k6_max) k6_max = edi;
        resultado(k6_max); break; }
    case 7: case 24: {                              /* 45E8A0 (PS2 0x1a9290): los objetos golpeados por el jugador */
        int tc[16], n = haobj_toques(tc, 16), i, got;
        for (i = 0; i < n; i++) {
            dbglog(DBG_INFO, "INFARTO: objeto de la clase %d golpeado (pide la %d)%s\n", tc[i], k7_cl, q == 24 && !(c->d36 > 0) ? " sin derrapar" : "");
            if (tc[i] == k7_cl) {
                if (q == 24 && !(c->d36 > 0)) continue;
                k7_ok = (int8_t)(k7_ok + 1); rob_cara(3); rob_para();
            } else if (tc[i] == 6) k7_mal = (int8_t)(k7_mal + 1);
        }
        got = k7_ok - k7_mal * 3;
        resultado(got >= k7_need ? 800000 : (int)((float)got * (800000.0f / (float)k7_need)));
        break; }
    case 16: case 25: {                             /* 461850 (PS2 0x1b0570): arcos cruzados, tabla 0x3f07d8 */
        int tc[8], n = haobj_cruces(tc, 8), i; float v;
        for (i = 0; i < n; i++) {
            if (tc[i] != k16_cl || (q == 25 && !(c->d36 > 0))) continue;
            k16_n = (int8_t)(k16_n + 1); rob_cara(4); rob_para();
            dbglog(DBG_INFO, "INFARTO: arco cruzado (%d de %d)\n", k16_n, k16_need);
        }
        v = k16_n == k16_need ? 800000.0f : 800000.0f / (float)(k16_need ? k16_need : 1) * (float)k16_n;
        sucesos(pts(16), c);
        resultado((int)v);
        break; }
    case 15: {                                      /* 4617A0 (PS2 0x1b0260): monedas tocadas, 0x3f07c0 */
        int tc[16], n = haobj_toques(tc, 16), i;
        sucesos(pts(15), c);
        for (i = 0; i < n; i++) {
            if (tc[i] == 0x0a) k15_a = (int8_t)(k15_a + 1); else if (tc[i] == 0x39) k15_39 = (int8_t)(k15_39 + 1); else continue;
            dbglog(DBG_INFO, "INFARTO: moneda de la clase %d cogida (%d + %d de %d; en el tramo %d)\n", tc[i], k15_a, k15_39, k15_need, k15_tramo);
            sfx_voz_se(0xd6); rob_cara(4); rob_para();
        }
        resultado((int)((float)(k15_a + k15_39) * (800000.0f / (float)(k15_need ? k15_need : 1))));
        break; }
    case 19: est_control(c); break;                 /* 0x1a80bc */
    case 20: {                                      /* (PS2 0x1b1f18): los fantasmas golpeados (infarto_autos) */
        int calla = modelo_callado(c->modelo);
        sucesos(pts(20), c);                        /* (0x3f0828) */
        resultado((int)((float)k20_n / (float)(k20_need ? k20_need : 1) * 800000.0f));
        if (k20_rob == 1) { if (!calla) rob_mov(0x260007); }
        else if (k20_rob == 2) { if (robot_fin(1) && !calla) rob_mov(0x60025); }
        else if (k20_rob == 3) { if (robot_fin(1)) rob_mov(0x250003); }
        k20_rob = 0; break; }
    case 14: {                                      /* 45F7F0 (PS2 0x1ab6a0): el puesto (infarto_autos) */
        uint32_t l = 0; const uint8_t *t = xsec("ESC1", &l); int k = k14_ac - 1 > 7 ? 7 : k14_ac - 1 < 0 ? 0 : k14_ac - 1;
        int calla = modelo_callado(c->modelo);
        sucesos(pts(14), c);                        /* (0x3efdc0) */
        if (k14_sube) { sfx_voz_se(0x5c); k14_hold = 0xf; }
        resultado(t && l >= 32 ? rd32(t + 4 * k) : 0);   /* tabla 0x3f7328 */
        if (k14_sube) { if (!calla && robot_fin(1)) rob_mov((float)rand() * 4.65661287e-10f >= 0.5f ? 0x220000 : 0x220003); }
        else if (!calla && robot_fin(1) && k14_ac != 1) rob_mov(0x220011);
        k14_sube = 0; break; }
    case 9:                                         /* 45EFB0 (PS2 0x1a9f68): los pares cruzados (infarto_autos) */
        sucesos(pts(9), c);                         /* (0x3efcd0) */
        for (; k9_cruces > 0; k9_cruces--) {
            rob_mov(anima(c->modelo, 0x6003f));     /* (0x1aa080..0x1aa0f4) */
            if (++k11_n > k11_need) sfx_voz_se(0x52);
            dbglog(DBG_INFO, "INFARTO: linea cruzada (%d de %d)\n", k11_n, k11_need);
        }
        resultado(k11_n > k11_need ? 800000 : (int)((float)k11_n / (float)(k11_need ? k11_need : 1) * 800000.0f));
        break;
    case 11: {                                      /* 45F180 (PS2 0x1aa3e8): los corazones del anillo a 4 m */
        float p[3] = { c->x, c->y, c->z }; int n, i;
        sucesos(pts(11), c);                        /* (0x3efce8) */
        for (n = haobj_anillo_toca(p), i = 0; i < n; i++) {
            if (!modelo_callado(c->modelo)) rob_mov(0x260003);
            if (++k11_n > k11_need) sfx_voz_se(0x52);
            dbglog(DBG_INFO, "INFARTO: corazon cogido (%d de %d)\n", k11_n, k11_need);
        }
        resultado(k11_n > k11_need ? 800000 : (int)((float)k11_n / (float)(k11_need ? k11_need : 1) * 800000.0f));
        break; }
    case 8: case 23: {                              /* 45EEB0 (PS2 0x1a9af8) */
        float a = c->ofs * (float)k8_lado, b = c->d264 * c->d268; int ok = !(q == 23 && !(c->d36 > 0)), e;
        sucesos(pts(8), c);                         /* (0x1a9a80: la tabla 0x3efcb8) */
        if (a > 0.0f && b < 0.0f && ok) {
            if (k8_ult < 0) k8_ult = c->seccion - 1;
            if (c->seccion > k8_ult) { k8_dist += c->seccion - k8_ult; k8_ult = c->seccion; }
            rob_cara(5); rob_para();
        } else k8_ult = -1;
        e = (int)((float)k8_dist / ((float)(int16_t)(k8_b - k8_a) * 0.9f) * 800000.0f);   /* 0x1a9aa0 */
        if (e > k8_max) k8_max = e;
        resultado(k8_max); break; }
    default: {                                      /* sin peticion: PS2 0x1a8100..0x1a8250 */
        static const uint8_t T[14] = { 0, 0, 1, 1, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0 };
        if (c->choque_ini) {
            int k = c->choque - 1, t = k >= 0 && k <= 13 ? T[k] : 2;
            if (t == 0) { pierde(0x32); voz(0x150, 0x3c, 5); }
            else if (t == 1 && c->impulso_rapido) { pierde(0x14); voz(0x14c, 0x3c, 5); }
        }
        if (c->impulso && c->impulso_rapido) { pierde(0xa); voz(0x14b, 0x3c, 5); }
        if (k19_cb) est_control(c);                 /* [7F2528] (0x1a81d8..) */
        break; }
    }
}
static void evalua(const infarto_coche_t *c)
{
    switch (q) {
    case 0: {                                       /* 462FF0 (PS2 0x1a90a8) */
        int32_t edi = res_pos(); int hs = edi / 10000, nt;
        float qq = (float)k0_ab0 / (float)(int16_t)(k0_b - k0_a);
        nt = nota(hs);
        if (nt == 7) {
            int v = qq >= 0.9f ? (int)((qq - 0.9f) / 0.1f * 300000.0f) : 0;
            edi = v + (int)(mantiene(v_ini - c->kmh) * 400000.0f) + 800000;
        }
        cierra(nt, edi, hs); break; }
    case 2: case 22: {                              /* 4635C0 (PS2 0x1aad20) */
        int hs = res_pos() / 10000, s4 = hs, s3 = c->pasados - k2_ini, nt = nota(hs), s2 = 0, s1 = hs;
        if (nt == 7) {
            s4 = 0x50;
            if (!c_rueda && !c_muro) s2 = 0x19;
            if (!c_trompo && !c_golpe) s2 += 0x19;
            if (s3 >= k2_d) s2 += 0x14 + (s3 - k2_d) * 5;
            else s2 += (int)((float)(s3 - k2_c) * 20.0f / (float)(k2_d - k2_c));
            s1 = s2 + 0x50;
        }
        cierra(nt, s1 * 10000, s4); break; }
    case 6: {                                       /* 4636F0 (PS2 0x1ab058) */
        int32_t edi = res_pos(); int hs = edi / 10000, nt = nota(hs);
        if (nt == 7) {
            int e = !c_trompo && !c_rueda && !c_muro ? 400000 : 0;
            edi = 800000 + e + (int)(300000.0f * mantiene(v_ini - v_med));
        } else {
            int cap = rd32(cor + 32 + 4 * nt);      /* 0x1b07f0 (tabla 0x3f07f0) */
            if (cap < hs) { hs = cap; edi = cap * 10000; }
        }
        cierra(nt, edi, hs); break; }
    case 7: case 24: case 16: case 25:              /* 463150 (PS2 0x1a9470) y 463F00 (0x1b0810, la misma cuenta) */
    {
        int32_t edi = res_pos(); int hs = edi / 10000, nt = nota(hs);
        if (nt == 7) { float k = mantiene(v_ini - c->kmh); edi += k == 1.0f ? 700000 : (int)(k * 700000.0f); }
        {   int32_t h = corazones_res(edi, nt);
            contador = hs * 20 / 7; cara_t = 0x78;  /* (meter_b) */
            fin_peticion(nt, h / 10000);
        }
        break; }
    case 15: {                                      /* 463DF0 (PS2 0x1b03e0) */
        int hs = res_pos() / 10000, nt = nota(hs), ebp; int32_t edi = 0;
        if (hs > 0x50) hs = 0x50;
        if (nt == 7) {
            edi = k15_a == k15_tramo ? 700000 : (int)((float)k15_a / (float)(k15_tramo ? k15_tramo : 1) * 700000.0f);
            ebp = k15_39 + k15_a;
            if (ebp > k15_need) edi += (int)((float)(ebp - k15_need) * 0.5f) * 10000;
        }
        {   int32_t h = corazones_res(edi + hs * 10000, nt);
            contador = hs * 20 / 7; cara_t = 0x78;
            fin_peticion(nt, h / 10000);
        }
        break; }
    case 19: {                                      /* 464040 (PS2 0x1b1778) */
        int32_t edi = res_pos(); int hs = edi / 10000, nt = nota(hs);
        if (nt == 7) {
            int a2 = k19_rot >= k19_c ? (k19_rot - k19_c) * 10000 + 0x46
                   : (int)(70.0f / (float)(k19_c - k19_n ? k19_c - k19_n : 1) * (float)(k19_rot - k19_n));
            hs = 0x50; edi = hs * 10000 + a2 + (int)((float)(k19_cog * 10000) * 0.1f);
        } else {
            int cap = rd32(cor + 32 + 4 * nt);      /* 0x1b07f0 */
            if (cap < hs) { hs = cap; edi = cap * 10000; }
        }
#ifdef DIAG_HA
        dbglog(DBG_INFO, "INFARTO: estrellas: %d grandes (n %d, c %d) y %d trozos\n", k19_rot, k19_n, k19_c, k19_cog);
#endif
        {   int32_t h = corazones_res(edi, nt);
            contador = hs * 20 / 7; cara_t = 0x78;
            fin_peticion(nt, h / 10000);
        }
        break; }
    case 20: {                                      /* (PS2 0x1b2348) */
        int hs = res_pos() / 10000, nt = nota(hs), a = 0;
        if (nt == 7 && k20_n > k20_need) {
            hs = 0x50;
            a = k20_n < k20_c ? (int)((float)(k20_n - k20_need) * 700000.0f / (float)(k20_c - k20_need)) : (k20_n - k20_c) * 50000 + 700000;
        }
        dbglog(DBG_INFO, "INFARTO: fantasmas: %d golpeados (pide %d, %d)\n", k20_n, k20_need, k20_c);
        {   int32_t h = corazones_res(hs * 10000 + a, nt);
            contador = hs * 20 / 7; cara_t = 0x78; fin_peticion(nt, h / 10000);
        }
        break; }
    case 14: {                                      /* 463840 (PS2 0x1abb30) */
        int hs = res_pos() / 10000, nt = nota(hs), a = 0;
        if (nt == 7) {
            hs = 0x50; a = c_ev ? 0 : 200000;
            if (c_trompo < 0x3c) a += (int)((float)(0x3c - c_trompo) / 60.0f * 200000.0f);
            if (c_rueda < 0x3c) a += (int)((float)(0x3c - c_rueda) / 60.0f * 300000.0f);
        }
        dbglog(DBG_INFO, "INFARTO: carrera: puesto %d de %d\n", k14_ac, k14_n + 1);
        cierra(nt, hs * 10000 + a, hs); break; }
    case 9: case 11: {                              /* 463470 (PS2 0x1aa780) y 0x1aa1e0 (el 9: la misma cuenta) */
        int hs = res_pos() / 10000, nt = nota(hs), s1 = 0;
        if (nt == 7) {
            hs = 0x50;
            if (!c_trompo && !c_ev) s1 = 400000;     /* (7F2504, 7F24EC) */
            if (k11_n > k11_need) s1 += k11_n >= k11_c ? 300000 + (k11_n - k11_c) * 50000
                                     : (int)((float)(k11_n - k11_need) / (float)(k11_c - k11_need) * 300000.0f);
        }
#ifdef DIAG_HA
        dbglog(DBG_INFO, "INFARTO: corazones: %d (pide %d, %d)\n", k11_n, k11_need, k11_c);
#endif
        cierra(nt, hs * 10000 + s1, hs); break; }
    case 8: case 23: {                              /* 463250 (PS2 0x1a9c70) */
        int hs = res_pos() / 10000, nt = nota(hs), ebp = 0;
        if (nt == 7) {
            float v = (float)k8_dist / (float)(int16_t)(k8_b - k8_a) / 0.1f;
            if (v > 1.0f) v = 1.0f;
            hs = 0x50;
            ebp = (int)(v * 300000.0f) + (int)(mantiene(v_ini - c->kmh) * 400000.0f);
        }
        cierra(nt, hs * 10000 + ebp, hs); break; }
    default: fin_peticion(0, 0); break;
    }
}

/* 463950 (PS2 0x1af060): accion previa; 1 si el registro lo era */
static int previa(const uint8_t *r, int sec)
{
    int s = rd16(r), code = rd16(r + 2), w4 = rd16(r + 4);
    if (!s || s > sec || code >= 0) return 0;
    switch (code + 0x13) {
    case 18: trafico = 0; break;                    /* -1: 46E4A0(0) */
    case 4: if (w4 == 0x13) { est_puesta(rd16(r + 6), rd16(r + 8), 0x28, 0x32, c_act); k19_cb = 1; } break;   /* -15: 462E20 (PS2 0x1b0030) */
    case 3: k19_cb = 0; break;                      /* -16: [7F2528] = [7F2524] = 0 (0x1b00b8) */
    case 9: if (w4 == 0x13) haobj_est_abre(1); break;   /* -10: 461080 (0x1af790): los 40 trozos (0x61) */
    case 6: if (w4 == 0x13) haobj_est_abre(0); break;   /* -13: 45C880 (0x1af990) */
    case 2: mueven = 1; break;                      /* -17: 45D830, los de la mision a la velocidad de la carretera */
    case 1: mueven = 1; k14_vel = 1; break;         /* -18: 0x1ab1d0 (45B520): los escoltas a la del jugador */
    case 7: {                                       /* -12: 0x1ab3c8 (45B5B0): hasta 8 escoltas de la tabla w4 */
        uint32_t l = 0; const uint8_t *t = xsec("ESC2", &l); int k;
        esc_n = esc_i = 0;
        if (!t || w4 < 0 || (uint32_t)(w4 + 1) * 0x78 > l) break;
        for (k = 0; k < 10 && esc_n < 8; k++) {
            const uint8_t *e = t + w4 * 0x78 + 12 * k;
            if (rd16(e) < 0) break;
            esc_q[esc_n].sec = (int16_t)(rd16(r + 6) + rd16(e)); esc_q[esc_n].carril = (int8_t)e[2]; esc_q[esc_n].id = (int8_t)e[9]; esc_n++;
        }
        dbglog(DBG_INFO, "INFARTO: %d escoltas de la tabla %d desde la seccion %d\n", esc_n, w4, rd16(r + 6));
        break; }
    case 17: trafico = 1; break;                    /* -2: 46E4A0(1) */
    case 14:                                        /* -5: las marcas de principio y fin (45B000 / 45B2A0) */
        a5_lo = w4; a5_hi = rd16(r + 6);
        {   uint32_t l; const uint8_t *m = xsec("MARC", &l);
            if (m && l >= 4 && c_act && c_act->punto_sec) {
                int reg[2] = { m[0] | m[1] << 8, m[2] | m[3] << 8 }; float p[2][3], fx[2], fz[2];
                c_act->punto_sec(a5_lo, p[0], fx, fz); c_act->punto_sec(a5_hi, p[1], fx + 1, fz + 1);
                haobj_marcas(2, reg, (const float (*)[3])p, fx, fz);
            }
        }
        break;
    case 8:                                         /* -11: 4611D0 (PS2 0x1afa48): los anchos de los arcos */
        if (w4 == 0x10 && c_act) {
            uint32_t l; const uint8_t *t = xsec("ARCO", &l); int tb = rd16(r + 6);
            if (t && (uint32_t)(tb + 1) * 16 <= l) haobj_arcos(a5_lo, a5_hi, (const float *)(t + 16 * tb), c_act->carriles_xz, c_act->seccion_xz, c_act->dir_xz);
        }
        break;
    case 13:                                        /* -6: anuncio de la peticion (tabla 0x3f0280) */
        if (w4 >= 0 && w4 < 26 && portado(w4)) {
            int id = rd16(r + 6) == 1 ? (anu[w4 * 4 + 2] | anu[w4 * 4 + 3] << 8) : (anu[w4 * 4] | anu[w4 * 4 + 1] << 8);
            if (id) { voz(id, 0x3c, 0xa); hud.anuncio = w4; hud.t_anuncio = 0; }
        }
        break;
    case 5:                                         /* -14: 45D710, tabla de coches */
        {   uint32_t o = off_coches + (w4 & 0x3f) * 0x320, s0 = o & ~2047u;
            ct_base = NULL;
            if (gd_lee(lba_coches + (s0 >> 11), 2, coches_t)) break;   /* (la del preset: 0x1b2e58) */
            ct_base = coches_t + (o - s0);
        }
        ct_v = w4; ct_i = 0; ct_n = 0; ct_sec = sec; mueven = 0;   /* 45D710: 7F250C = 0 */
        if (w4 & 0x80) ct_pend = 0;                 /* toda la tabla de una vez */
        break;
    case 16: {                                      /* -3: 460970(0, w4, 2, 30, -1), si la peticion que viene se porta */
        const uint8_t *x = r + 10;
        while (rd16(x) && rd16(x + 2) < 0) x += 10;
        if (rd16(x) && portado(rd16(x + 4))) rotulo_abre(0, w4, 2, 0x1e, -1);
        break; }
    case 15: rotulo_abre(1, w4, 2, -1, 0x78); break;   /* -4: 460970(1, w4, 2, -1, 120) */
    default: break;                                 /* objetos, escoltas: SIN PORTAR */
    }
    return 1;
}

/* 45C7E0 (PS2 0x1af710): la clase 1..3 de la mision especial de la clave k (0: no lo es) */
static int clase_mision(int k)
{
    int l = nivel_act + 1, q;
    if (l >= NIV || k < 0) return 0;
    q = mision(k, l);
    return q >= 0xf && q - 0xf < 26 && x_clas ? x_clas[q - 0xf] : 0;
}
/* 4603F0: lo que hace la hoja al pasar cada cuadro de su guion (43FA00: los pasos de este cuadro; aqui uno por paso) */
static void hoja_paso(const infarto_coche_t *c)
{
    uint32_t t = hoja_t; int lvl = nivel_act, i, n = n_req;
    if (lvl == 4 || lvl < 0 || lvl >= NIV || !x_hoja) return;
    if (t >= 0x21c) {                               /* la mision especial de la etapa siguiente (2B003F..41) */
        if (c->sig < 0) return;                     /* 451350 = 2: sin rama elegida */
        if (hoja_sig != c->sig) { hoja_sig = c->sig; hoja_k = clase_mision(c->sig); }
        if (t == 0x276 && hoja_k >= 1 && hoja_k <= 3) { sfx_voz_se(0xdb); hoja_hs = hoja_k; hoja_hs_t = 0; }
        if (t == 0x294) {                           /* (0x9cc.. con la bandera 0x800 del controlador) */
            if (hoja_k == 1) sfx_voz_se(0x1cc);
            else if (hoja_k == 2) { sfx_voz_se(0x1cd); sfx_voz_se(0xd9); }
            else if (hoja_k == 3) { sfx_voz_se(0x1ce); sfx_voz_se(0xda); }
        }
        return;
    }
    if (t == 0x78) {
        sfx_voz_se(0xa9);                           /* (0x8a9) */
        dbglog(DBG_INFO, "INFARTO: hoja de resultados del nivel %d: %d peticiones, nota media %d\n", lvl, n,
               (int8_t)lv[lvl][0x40] > 0 ? LV_U32(lvl, 0x1c) / (int8_t)lv[lvl][0x40] : -1);
    }
    if (t < 0x168) {
        for (i = 0; i < n && i < 5; i++)
            if (t == 0x78u + (uint32_t)i * 0x3c + 0x3c) sfx_voz_se(rd32(x_hoja + 4 * (LV_U32(lvl, 8 + 4 * i) & 7)));
    }
    if (t == 0x1a4 && (int8_t)lv[lvl][0x40] > 0) { /* la nota media: voz, rotulo 2B0007..0D y la acompanante */
        int g = LV_U32(lvl, 0x1c) / (int8_t)lv[lvl][0x40];
        g = g < 0 ? 0 : g > 7 ? 7 : g;
        sfx_voz_se(rd32(x_hoja + 4 * g));
        hoja_hb = g; hoja_hb_t = 0;
        rob_mov((uint32_t)rd32(x_hoja + 72 + (modelo_callado(c->modelo) ? 32 : 0) + 4 * (g < 1 ? 1 : g)));
    }
}
int infarto_dibujos(infarto_dib_t *v, int max, infarto_num_t *nv, int *nn)
{
    int n = 0, lvl = nivel_act, i, cn = 0;
    uint32_t t = hoja_t;
#define DIB(nom, cu, X, Y) do { if (n < max) { strncpy(v[n].g, nom, 11); v[n].g[11] = 0; v[n].cuadro = (cu); v[n].x = (X); v[n].y = (Y); n++; } } while (0)
    if (cara_t > 0 && dat) {                        /* 45BDF0 */
        char b[20]; int x = cara_t > 0x5a ? (int)((1.0f - (float)(cara_t - 0x5a) * 0.0333333f) * 384.0f) - 0x9c : 0xe4;
        sprintf(b, "HA_CARA%d", cara_nota & 7); DIB(b, 0, (float)x, 93.0f);
    }
    if (q == 14 && dat) { DIB("HA_RK0", 0, 0, 30.0f); DIB("HA_RK1", k14_ac - 1, 0, 30.0f); }   /* 45B7D0 (0x1abca0) */
    if (hoja_hb >= 0) {                             /* 428320(0x3effe8[g], 9, 3): una vez */
        char b[20]; sprintf(b, "HA_HB%d", hoja_hb); DIB(b, hoja_hb_t, 0, 0);
        if (++hoja_hb_t >= 90) hoja_hb = -1;
    }
    if (hoja_hs >= 1) {                             /* 428320(2B003F + k - 1, 7, 3) */
        char b[20]; sprintf(b, "HA_HS%d", hoja_hs); DIB(b, hoja_hs_t, 0, 0);
        if (++hoja_hs_t >= 180) hoja_hs = -1;
    }
    if (dat && x_hoja && lvl >= 0 && lvl < 4 && t >= 0x78 && t < 0x21c) {
        if (t < 0x168) {
            char b[20];
            sprintf(b, "HA_HN%d", lvl); DIB(b, 0, 0, 0);
            if (n_req >= 1 && n_req <= 3) { sprintf(b, "HA_HC%d", n_req); DIB(b, 0, 0, 0); }
            for (i = 0; i < n_req && i < 5; i++) {
                uint32_t lim = 0x78 + (uint32_t)i * 0x3c; float y; int g = -1;
                {   union { uint32_t u; float f; } q; q.u = (uint32_t)rd32(x_hoja + 32 + 4 * i); y = (float)(int)q.f; }
                if (t >= lim + 0x3c) g = LV_U32(lvl, 8 + 4 * i) & 7;
                else if (t >= lim) g = (int)(t >> 2) & 7;
                if (g >= 0) { sprintf(b, "HA_HG%d", g); DIB(b, 0, 276.0f, y + 224.0f); }   /* (96 x 32: 4294C0 = (48, 16)) */
                if (nv && cn < 5) { nv[cn].x = 0x19d; nv[cn].y = rd32(x_hoja + 52 + 4 * i); nv[cn].v = LV_U32(lvl, 0x20 + 4 * i); cn++; }
            }
        }
        if (t >= 0x1a4) DIB("HA_HE", 0, 0, -4.0f);
        else if (t >= 0x168) {
            char b[20]; DIB("HA_HE", 0, 0, -4.0f); sprintf(b, "HA_HA%d", (int)(t >> 2) & 7); DIB(b, 0, 0, -4.0f);
        }
    }
#undef DIB
    if (nn) *nn = cn;
    return n;
}
static void gestor_(const infarto_coche_t *c)       /* 464350 (PS2 0x1ae440) */
{
    if (!c->en_union) {
        int m = mision(clave_act, nivel_act);
        const uint8_t *r = mis_reg(m, rec_i);
        if (rec_i < 32 && previa(r, c->seccion)) rec_i++;
        else if (q == 3 && rec_i < 32) {
            int s0 = rd16(r);
            if (!s0 || s0 > c->seccion || rd16(r + 2) <= 0) return;
            if (!portado(rd16(r + 4))) { rec_i++; return; }   /* tipo sin portar: no empieza (PENDIENTE) */
            empieza(c->seccion, rd16(r + 4)); rec_i++; req_r = r; quest_act = m;
            v_n = 0; v_ini = c->kmh; v_acu = 0;
            voz(0xe8, 0x3c, 9);
            puesta(r, c);
            dbglog(DBG_INFO, "INFARTO: peticion tipo %d (mision %d, secciones %d..%d, p6 %d p8 %d) en la seccion %d\n",
                   q, m, rd16(r), rd16(r + 2), rd16(r + 6), rd16(r + 8), c->seccion);
            return;
        }
    }
    if (q == 3 || !req_r || rd16(req_r + 2) > c->seccion || c->en_union) return;
    suelta_coches();
    evalua(c);
}
static void gestor(const infarto_coche_t *c) { gestor_(c); filas_paso(); }   /* las filas al final de 464350 */

void infarto_carrera(int clave, int nivel)          /* 45E0F0 */
{
    int l;
    score = disp = 0; memset(vcola, 0, sizeof vcola); vc_cab = vc_fin = 0; voz_ult = 0; reloj = 0x10000;
    contador = 0; dx23dc = 0; medidor = 0; memset(&m_alerta, 0, sizeof m_alerta); memset(m_rec, 0, sizeof m_rec); memset(m_roto, 0, sizeof m_roto); memset(m_efe, 0, sizeof m_efe);
    memset(niv_tot, 0, sizeof niv_tot); memset(niv_ver, 0, sizeof niv_ver); tot_obj = tot_ver = 0; alerta = 0;
    m_efe_n = m_efe_i = 0; m_prev = 0; m_t8c74 = m_t8c78 = m_t2510 = m_visto = 0; m_tinte = 0xffffffff;
    result = 0; bonus = 0; meta_hecha = 0; vs_suma = 0; vs_n = 0; memset(vel_nivel, 0, sizeof vel_nivel); trafico = 1; ct_base = NULL; ct_pend = -1; mueven = 1; ban_on = 0;
    for (l = 0; l < NIV; l++) { memset(lv[l], 0, 0x44); LV_SEL(l) = -1; LV_U32(l, 4) = 0x29;
        LV_U32(l, 8) = LV_U32(l, 12) = LV_U32(l, 16) = LV_U32(l, 20) = LV_U32(l, 24) = -1; }
    memset(&hud, 0, sizeof hud); hud.nota = -1; hud.nota_etapa = -1; hud.anuncio = -1; hud.t_nota = hud.t_etapa = hud.t_anuncio = 9999;
    clave_act = clave; nivel_act = nivel; empieza(0, 3); n_req = 0; rec_i = 0; h_nivel = 0;
    hoja_t = 0; hoja_sig = -1; hoja_hb = hoja_hs = -1; cara_t = 0; cara_nota = 0;
    cuenta_peticiones(mision(clave, nivel)); k19_cb = 0; k19_cog = 0;
    haobj_mision(inf_preset, mision(clave, nivel)); /* 45C470: los objetos de la mision */
    if (dat) sfx_voz_grupo(grp[clave >= 0 && clave < 15 ? clave : 0]);
}
void infarto_etapa(int clave, int nivel)            /* 464350, stage clear */
{
    int prev = nivel_act, i; int32_t edi;
    edi = score / 10000 - h_nivel;
    if (prev >= 0 && prev < NIV) { vel_nivel[prev] = vel_media(); niv_ver[prev] = edi; }   /* 44FF20; 7F2448 + nivel x 4 */
    vs_suma = 0; vs_n = 0;
    {   int b = bono_nivel(prev, sec_union_ult);     /* 45D360 del nivel anterior */
        if (b > 0) { edi += b; score += b * 10000; }
        dbglog(DBG_INFO, "INFARTO: bono del nivel %d: %d (derrape %d, firme %d, en %d posiciones; velocidad media %.1f x %.4f)\n", prev, b,
               st_5c, st_60, sec_union_ult - st_5a, (double)vel_nivel[prev], (double)hud_coche);
    }
    st_54 = 0; st_5c = 0; st_60 = 0; st_5a = -1; st_5e = st_62 = -1;
    if (prev >= 0 && prev < NIV) niv_tot[prev] = edi;  /* 7F2434 + nivel x 4 */
    LV_U32(prev, 0x34) = edi;
    for (i = 0; i < (int8_t)lv[prev][0x40] && i < 5; i++) LV_U32(prev, 0x34) -= LV_U32(prev, 0x20 + 4 * i);
    if ((int8_t)lv[prev][0x40] > 0) { hud.nota_etapa = LV_U32(prev, 0x1c) / (int8_t)lv[prev][0x40]; hud.t_etapa = 0; }
    dbglog(DBG_INFO, "INFARTO: etapa %d terminada: %d corazones en ella, nota media %d, total %d\n", clave_act, (int)edi,
           hud.nota_etapa, (int)(score / 10000));
    disp = score; h_nivel = score / 10000; tot_ver = 0;   /* 7F2468, 7F2464 */
    hoja_t = 0; hoja_sig = -1;                       /* 7F8B34 = 0 (464350 en el stage clear) */
    clave_act = clave; nivel_act = nivel; n_req = 0; rec_i = 0; trafico = 1;
    cuenta_peticiones(mision(clave, nivel)); k19_cb = 0;
    haobj_mision(inf_preset, mision(clave, nivel));
    sfx_voz_grupo(grp[clave >= 0 && clave < 15 ? clave : 0]);
}
int infarto_metas(char *nom)
{
    uint32_t l; const uint8_t *t = xsec("META", &l); int m;
    nom[0] = 0;
    if (!dat || !t || l < 82) return 0;
    m = mision(clave_act, nivel_act);
    if (m < 0 || m > 40 || !t[(inf_preset == 0 ? 0 : 41) + m]) return 0;   /* [0x44350c] == 0: 0x1a9850, si no 0x1a95b0 */
    sprintf(nom, "HAM%d%02d.DAT", inf_preset == 0 ? 0 : 1, m);
    return 1;
}
void infarto_meta(void)                             /* 464350 con 44FF10 */
{
    int i; int32_t esi;
    if (meta_hecha) return;
    esi = score / 10000 - h_nivel;
    if (nivel_act >= 0 && nivel_act < NIV) vel_nivel[nivel_act] = vel_media();   /* 4513C0 */
    {   int b = bono_nivel(nivel_act, sec_union_ult);
        if (b > 0) { esi += b; score += b * 10000; }
    }
    disp = score; LV_U32(nivel_act, 0x34) = esi; tot_ver = esi;   /* 7F2464 */
    for (i = 0; i < (int8_t)lv[nivel_act][0x40] && i < 5; i++) LV_U32(nivel_act, 0x34) -= LV_U32(nivel_act, 0x20 + 4 * i);
    if ((int8_t)lv[nivel_act][0x40] > 0) { hud.nota_etapa = LV_U32(nivel_act, 0x1c) / (int8_t)lv[nivel_act][0x40]; hud.t_etapa = 0; }
    meta_hecha = 1;
    dbglog(DBG_INFO, "INFARTO: GOAL con %d corazones\n", (int)(score / 10000));
}
void infarto_paso(const infarto_coche_t *c)         /* 464D20 */
{
    int32_t d;
    reloj++;
    if (!dat) return;
    if (vs_n < 0x57e3f) { vs_suma += c->vel_mf; vs_n++; }
    hud_coche = c->hud; modelo_jug = c->modelo; { static infarto_coche_t ult; ult = *c; c_act = &ult; }   /* (copia: infarto_autos lo lee fuera del paso) */
    {   haobj_coche_t oc;                           /* los objetos de la mision: colocacion, toques, vuelo */
        oc.marco = c->marco; oc.suelo = c->suelo; oc.pos[0] = c->x; oc.pos[1] = c->y; oc.pos[2] = c->z;
        oc.fw[0] = c->fx; oc.fw[1] = 0; oc.fw[2] = c->fz; oc.v = c->kmh / 3.6f; oc.sec = c->seccion_xz; oc.dir = c->dir_xz;
        if (!ant_ok) { ant_pos[0] = c->x; ant_pos[1] = c->y; ant_pos[2] = c->z; ant_ok = 1; }
        memcpy(oc.ant, ant_pos, sizeof ant_pos);
        haobj_paso(&oc);
        { int k; for (k = haobj_postes(); k > 0; k--) sfx_voz_se(0xcd); }   /* 4AA140 con +78 = 0xCD (modo 16) */
        ant_pos[0] = c->x; ant_pos[1] = c->y; ant_pos[2] = c->z;
    }
    if (c->en_union) { hoja_t++; hoja_paso(c); }    /* 464D20: 7F8B34 + 1 en la union */
    control(c);
    alerta = score == disp ? 0 : score > disp ? 1 : -1;   /* 7F1CAC */
    d = (score - disp) / 32;                         /* el contador alcanza a los corazones */
    if ((d < 0 ? -d : d) * 32 < 20000) disp = score; else disp += d;
    if (disp < 0) { score = disp = 0; }
    voz_bomba();
    if (c->en_union) sec_union_ult = c->seccion_union;
    if (ani_on && q != 11) { haobj_anillo_vacia(); ani_on = 0; }   /* 1aa5b8 / 46DB60: solo con la peticion 11 */
    estadisticas(c);
    medidor_paso();
    cara_t--;                                       /* 45FB80 (PS2 0x1acf48): 7F8B28 - 1 */
    efectos_paso();
    rotulo_paso();
    if (contador > 0) { if (contador % 10 == 0) sfx_voz_se(0xd5); contador--; }
    gestor(c);
    /* 4BCD50 (PS2 0x1bdb70..0x1bdc4c): el aviso 2B000E (capa 6, una vez) mientras el contador baja, otra vez al acabar,
       con el efecto 0xD4 */
    if (alerta != -1) m_alerta.usado = 0;
    else if (!m_alerta.usado || spr_acabado(&m_alerta)) { spr_abre(&m_alerta, 7, 0, 59, 1, 166.0f, 182.0f); sfx_voz_se(0xd4); }
    if (q != 3) { v_n++; v_acu += c->kmh; v_med = v_acu / v_n; if (v_min > c->kmh) v_min = c->kmh; }
    /* 45D7A0: los coches de la tabla, de 30 a 50 posiciones por delante */
    while (ct_base && ct_pend < 0 && !(ct_v & 0x80) && ct_i < 0x64) {
        const uint8_t *r = ct_base + ct_i * 8; int s = rd16(r + 4);
        if (s < 0 || (int8_t)r[6] < 0) break;
        if (s < c->seccion + 0x1e) { ct_i++; continue; }  /* ya atras: se salta */
        if (s <= c->seccion + 0x32) ct_pend = ct_i;
        break;
    }
#ifdef DIAG_HA
    if (q != 3 && reloj % 60 == 0)
        dbglog(DBG_INFO, "DIAGHA tipo %d sec %d resultado %d bonus %d ofs %.2f izq %.2f der %.2f d36 %d k8 %d/%d\n", q, c->seccion,
               (int)(result / 10000), (int)(bonus / 10000), (double)c->ofs, (double)c->d268, (double)c->d264, c->d36, k8_dist, k8_b - k8_a);
#endif
    hud.tipo = q; hud.resultado = q == 3 ? 0 : (int)(res_pos() / 10000); hud.corazones = disp / 10000; hud.total = score / 10000;
    if (hud.t_nota < 9999) hud.t_nota++;
    if (hud.t_etapa < 9999) hud.t_etapa++;
    if (hud.t_anuncio < 9999) hud.t_anuncio++;
}
/* el coche que hay que abrir ahora (45D470): clase, seccion, carril + 1 del registro (el carril de la plaza), modelo */
int infarto_coche_pide(infarto_coche_nuevo_t *n)
{
    const uint8_t *r; int cl;
    if (esc_i < esc_n) {                            /* los escoltas de la -12: clase 6 (+324). DESVIACION: el modelo de la
                                                       tabla es un coche del jugador (2, 5, 8: sin cargar en el modo); va uno
                                                       del trafico de la lista de las clases 3 / 4 */
        n->fantasma = 0; n->clase = 6; n->seccion = esc_q[esc_i].sec; n->carril = esc_q[esc_i].carril; n->id = esc_q[esc_i].id;
        n->modelo = rd32(modelos + 4 * (esc_i % (inf_preset == 1 ? 6 : 7))); return 1;
    }
    if (!ct_base || ct_pend < 0 || ct_pend >= 0x64) return 0;
    r = ct_base + ct_pend * 8;
    if (rd16(r + 4) < 0 || (int8_t)r[6] < 0) { ct_base = NULL; return 0; }
    cl = rd32(r); n->clase = cl; n->seccion = rd16(r + 4); n->carril = (int8_t)r[6]; n->id = ct_n;
    n->fantasma = (ct_v & 0x40) != 0;               /* 0x1b2e04: [+E0] & 0x40 -> +C5C 0x80000 */
    if (cl <= 1) n->modelo = 0x2b;
    else if (cl == 2) n->modelo = 0x1f;
    /* 0x1b2ca4: en la etapa del descriptor 0x1a la lista de 0x3f0870 (% 7); si no la de 0x3f0850, % 6 con el preset
       1 y % 7 con los demas */
    else if (cl <= 4) n->modelo = ny_tramo ? rd32(modelos + 28 + 4 * (ct_n % 7)) : rd32(modelos + 4 * (ct_n % (inf_preset == 1 ? 6 : 7)));
    else n->modelo = -1;
    return 1;
}
void infarto_coche_abierto(int ok)
{
    if (esc_i < esc_n) { if (ok) esc_i++; return; }
    if (ct_pend < 0) return;
    if (!ok) return;                                /* sin plaza: se reintenta en el siguiente paso */
    ct_n++; ct_i = ct_pend + 1;
    if (ct_v & 0x80) { ct_pend = ct_i < 0x64 ? ct_i : -1; } else ct_pend = -1;
}
void infarto_autos(struct haobj_auto_s *v, int n)
{
    uint32_t l = 0; const uint8_t *a;
    if (!dat) return;
    if (q == 11 && (a = xsec("ANI1", &l)) && l >= 24) haobj_anillo(v, n, (const float *)a);   /* 46DB60 */
    if ((q == 14 || k14_vel) && c_act) {                         /* 0x1ab2d8 (la lista y 0x1ab1d0) y el puesto de 0x1ab6a0 */
        int i, k, delante = 0, faltan = 0, p;
        if (k14_pide || k14_vel) {
            float vj = c_act->kmh / 3.6f, vm = 1.38427f * 216.720001f / 3.6f;   /* +1C4 del jugador, como poco 0x3fb12fea */
            int lista = k14_pide;
            if (lista) k14_n = 0;
            k14_pide = k14_vel = 0;
            for (i = 0; i < n; i++) {
                if (!v[i].pos || v[i].clase != 6) continue;
                if (lista && k14_n < 24) { v[i].marca |= HA_ESC; k14_pl[k14_n++] = (int8_t)i; }
                if (v[i].v) *v[i].v = vj < vm ? vm : vj;
                /* (PROBABLE: despues la velocidad la lleva su IA, sin portar: aqui la de la carretera, v_des) */
            }
            if (lista) { k14_ac = k14_ant = k14_n + 1; dbglog(DBG_INFO, "INFARTO: carrera con %d escoltas\n", k14_n); }
        }
        if (q == 14) {
            for (k = 0; k < k14_n; k++) {           /* los que faltan (+98) y los que van delante (+260) */
                haobj_auto_t *e = v + k14_pl[k];
                if (!e->pos || !(e->marca & HA_ESC)) faltan++;
                else if (e->sec > c_act->seccion) delante++;
            }
            k14_ant = k14_ac; p = faltan + delante + 1;
            if (k14_hold > 0) { if (p > k14_ant) p = k14_ant; k14_hold--; }
            k14_ac = p; if (p < k14_ant) k14_sube = 1;
#ifdef DIAG_HA
            if (reloj % 60 == 0) {
                char b[96]; int o = 0;
                for (k = 0; k < k14_n && o < 80; k++) { haobj_auto_t *e = v + k14_pl[k]; o += sprintf(b + o, " %d", e->pos && (e->marca & HA_ESC) ? e->sec : -1); }
                dbglog(DBG_INFO, "DIAGHA escoltas: jugador %d, faltan %d delante %d:%s\n", c_act->seccion, faltan, delante, b);
            }
#endif
        }
    }
    if (q == 20 && c_act) {                         /* 0x1b1f18: los fantasmas (+C5C 0x80000) y los corazones de 0x1b2190 */
        int i, s4 = 0, s5 = 0, s6 = 0;
        for (i = 0; i < n && i < 24; i++) {
            k20_ver[i] = 0;
            if (!v[i].pos) continue;
            if (!(v[i].marca & HA_VISTO)) { v[i].marca |= HA_VISTO; k20_fr[i] = 0; k20_tocado[i] = 0; }
            if (v[i].marca & HA_FANT) {
                if (v[i].sec - 7 == c_act->seccion) {   /* a 7 secciones: un efecto cada 60 cuadros */
                    uint32_t l = 0; const uint8_t *t = xsec("FAN1", &l);
                    s5 = 1;
                    if (k20_t >= 0x3c) { if (t && l >= 12) sfx_voz_se(rd32(t + 4 * (rand() % 3))); k20_t = 0; }
                }
                if (v[i].golpe && !k20_tocado[i]) {     /* (PROBABLE: una vez por coche; el original cuenta los pasos) */
                    k20_tocado[i] = 1; s6 = 1; k20_n++;
                    dbglog(DBG_INFO, "INFARTO: fantasma golpeado (%d de %d)\n", k20_n, k20_need);
                } else if (v[i].sec < c_act->seccion) s4 = 1;
            }
            if (!v[i].golpe && v[i].sec - c_act->seccion < 10) {   /* 0x1b2190: 2C0006 sobre el coche */
                k20_ver[i] = 1; memcpy(k20_p[i], v[i].pos, sizeof k20_p[i]);
            }
        }
        k20_t++;
        k20_rob = s6 ? 1 : s5 ? 2 : s4 ? 3 : 0;
    }
    if (q == 9 && c_act) {                          /* 46D250 y la linea de 45EFB0 */
        float p[3] = { c_act->x, c_act->y, c_act->z };
        haobj_pares(v, n, k9_lo, k9_hi, p);
        k9_cruces += haobj_pares_cruza(v, n, p, c_act->fx, c_act->fz);
    } else if (par_on) {                            /* fuera de la peticion 9: los pares se sueltan (46D250) */
        int i; haobj_pares_vacia(); par_on = 0;
        for (i = 0; i < n; i++) v[i].marca &= ~(HA_PAR | HA_SUELTO);
    }
}
int infarto_spr3d(infarto_s3d_t *v, int max, int (*cuadros)(const char *g))
{
    int i, n = 0;
    if (q == 20) {                                  /* 0x1b2190: escala 1,68 hasta el cuadro 25, luego - 0,2 por cuadro */
        int nc = cuadros ? cuadros("HA_S06") : 1;
        for (i = 0; i < 24 && n < max; i++) {
            if (!k20_ver[i]) continue;
            strcpy(v[n].g, "HA_S06"); v[n].cuadro = k20_fr[i]; memcpy(v[n].p, k20_p[i], sizeof v[n].p);
            v[n].esc = k20_fr[i] <= 0x19 ? 1.68f : 1.68f - (float)(k20_fr[i] - 0x19) * 0.2f; n++;
            if (++k20_fr[i] >= nc) k20_fr[i] = (int16_t)(nc - 1);   /* (+B66 + 1, tope [+8] - 1) */
        }
    }
    {   uint32_t l = 0; const uint8_t *a = xsec("ARC2", &l);   /* los de los objetos (arcos y estrellas) */
        if (cuadros) n += haobj_spr3d(v + n, max - n, cuadros, a && l >= 60 ? (const float *)a : NULL);
    }
    return n;
}
void infarto_libera(void) { free(dat); dat = NULL; free(mis1); mis1 = NULL; free(xdat); xdat = NULL; x_pts = NULL;
                          robot_extra(NULL, 0); free(chrh); chrh = NULL; haobj_libera(); inf_cargado = -1;
                          free(hm); hm = NULL; }
/* sprites de la animacion (428460: cuadros ini..fin, modo 0 en bucle, 1 una vez; estado 3 = acabado) */
static void spr_abre(spr_t *s, int g, int ini, int fin, int modo, float x, float y)
{
    s->usado = 1; s->g = g; s->ini = ini; s->fin = fin; s->modo = modo; s->c = (float)ini; s->vel = 1.0f; s->x = x; s->y = y;
    s->volteado = 0;
}
static int spr_acabado(const spr_t *s) { return s->modo && s->c > (float)s->fin; }   /* 428880 == 3 */
static void spr_avanza(spr_t *s)
{
    if (!s->usado) return;
    s->c += s->vel;
    if (!s->modo && s->c > (float)s->fin + 0.999f) s->c = (float)s->ini;
}
static void roto_abre(float x, float y)             /* 2B000E (modo 3, una vez) a 2,5 cuadros por paso */
{
    int i;
    for (i = 0; i < 24 && m_roto[i].usado; i++) ;
    if (i == 24) return;
    spr_abre(m_roto + i, 7, 0, 59, 1, x, y); m_roto[i].vel = 2.5f;
}
/* 45B860 (PS2 0x1abde8): un efecto desde (x0, y0) hacia (x1, y1); tipo 0 empieza en (280, 0) */
static void efecto(float x0, float y0, float x1, float y1, int tipo, int f, float vel, int vida)
{
    int n, c = m_efe_i;
    for (n = 0; n < 16; n++) {
        efe_t *e = m_efe + c;
        if (++c == 16) c = 0;
        if (e->usado) continue;
        m_efe_i = c; m_efe_n++;
        memset(e, 0, sizeof *e); e->usado = 1; e->x0 = x0; e->y0 = y0;
        if (!tipo) { e->x = 280.0f; e->y = 0.0f; } else { e->x = x0; e->y = y0; }
        e->x1 = x1; e->y1 = y1; e->tipo = tipo; e->vel = vel; e->f = f < 7 ? f : 6; e->vida = vida;
        return;
    }
    m_efe_i = c;
}
static void efectos_paso(void)                     /* 45B950 (PS2 0x1abee0) */
{
    int k, hechos = 0;
    for (k = 0; k < 16 && hechos < m_efe_n; k++) {
        efe_t *e = m_efe + k; float sv;
        if (!e->usado) continue;
        hechos++;
        switch (e->est) {
        case 0: if (--e->vida < 0) e->est++; break;
        case 1: e->est++; break;
        case 2: spr_abre(&e->s, e->f, 0x4c, 0x5a, 1, e->x, e->y); e->s.vel = e->vel; e->est++; break;
        case 3:
            if (spr_acabado(&e->s)) { e->est++; spr_abre(&e->s, e->f, 0x5b, 0x69, 1, e->x, e->y); e->s.vel = e->vel; e->est++; break; }
            sv = e->vel * 0.0666667f * (float)e->n;
            switch (e->tipo) {
            case 0: e->y = (float)(int)((e->y1 - e->y0) * sv) + e->y0;
                    e->x = (e->y - 100.0f) * 0.008f * (e->y - 100.0f) + 200.0f; break;
            case 1: e->x = (float)(int)((e->x1 - e->x0) * sv) + e->x0; e->y += (e->y1 - e->y) * (e->vel * 0.335f); break;
            case 2: e->x = (float)(int)((e->x1 - e->x0) * sv) + e->x0; e->y = (float)(int)((e->y1 - e->y0) * sv) + e->y0; break;
            case 3: e->x += (e->x1 - e->x) * (e->vel * 0.335f); e->y = (float)(int)((e->y1 - e->y0) * sv) + e->y0; break;
            }
            e->s.x = e->x; e->s.y = e->y; e->n++;
            break;
        case 4: spr_abre(&e->s, e->f, 0x5b, 0x69, 1, e->x, e->y); e->s.vel = e->vel; e->est++; break;
        case 5: if (spr_acabado(&e->s)) { m_efe_i = k; e->s.usado = 0; e->usado = 0; m_efe_n--; } break;
        }
    }
}
static int tipo_sin_vuelo(int t) { return t == 0 || t == 6 || t == 8 || t == 23; }   /* 0x1ac944..0x1ac98c */
static void medidor_paso(void)
{
    int i, k, h, lim, a, nuevo = 0, ganan, delta, mostrados;
    const float ax = 234.0f, ay = 108.0f, cx = 176.0f, cy = 96.0f;   /* ancla y centro del rotulo (7F8BBC / 7F8BC0) */
    for (i = 0; i < 24; i++) { spr_avanza(m_roto + i); if (m_roto[i].usado && spr_acabado(m_roto + i)) m_roto[i].usado = 0; }
    for (i = 0; i < 20; i++) { spr_avanza(m_rec + i); m_rec[i].volteado = 0; }
    for (i = 0; i < 16; i++) if (m_efe[i].usado) spr_avanza(&m_efe[i].s);
    spr_avanza(&m_alerta);
    if (medidor == 2) {                             /* 0x1ace3c: se cierra; vuelan si se veian 7 o mas */
        for (k = 0; k < 20; k++) {
            spr_t *r = m_rec + k;
            if (!r->usado) continue;
            r->usado = 0;
            if (m_visto >= 7) efecto(r->x, r->y, 280.0f, 200.0f, 1 + k % 3, m_f18[k], (20.0f - (float)k) * 0.3f + 0.5f, 0);
            m_f18[k] = -1;
        }
        m_prev = 0; m_visto = 0; medidor = 0;
    } else if (medidor != 1) {
        m_visto = 0; m_prev = 0; m_t8c78 = 0;
    } else {
        m_t8c74--;
        if (m_t2510 > 0) m_t2510--;
        h = result / 10000 + 1; delta = result - m_prev; ganan = h - m_prev / 10000 - 1;
        if (result < 0) {                           /* 0x1ac718: el marco del rotulo hacia el rojo (28 corazones: rojo) */
            float t = -(float)result, f = 280000.0f <= t ? 0.0f : 1.0f - t / 280000.0f; int b = (int)(f * 255.0f) & 0xff;
            m_tinte = 0xff0000ffu | (uint32_t)b << 8 | (uint32_t)b << 16;   /* ABGR del GS */
        } else m_tinte = 0xffffffff;
        m_prev = result;
        if (!m_rec[0].usado) { spr_abre(m_rec, 0, 0x1f, 0x3c, 0, ax, ay); m_f18[0] = 0; }
        m_rec[0].x = ax; m_rec[0].y = ay;
        if (ganan < 0 || delta < 0) {               /* 0x1ac85c */
            if (!m_t8c78) { roto_abre(ax, ay); m_t8c78 = 0x14; }
            m_f18[0] = (h < 0 ? 0 : h) % 7;
            spr_abre(m_rec, m_f18[0], 0x1f, 0x3c, 0, ax, ay);
        } else if (ganan > 0) {                     /* 0x1ac91c */
            m_f18[0] = (h < 0 ? 0 : h) % 7;
            if (!(tipo_sin_vuelo(q) && m_t2510 > 0)) {
                m_t2510 = 0x14;
                spr_abre(m_rec, m_f18[0], 0x1f, 0x3c, 0, ax, ay);
                if (m_t8c74 <= 0) { sfx_voz_se(0x52); m_t8c74 = 10; efecto(ax, ay, cx, -cy, 2, h < 0 ? 1 : 5, 1.0f, 0); }
            }
        }
        mostrados = h < 0 ? 0 : h;
        lim = (int)((float)mostrados * 0.25f); if (lim > 0x13) lim = 0x13;
        if (c_ev > 0) {
            a = 7;                                      /* 6 con 7F24F0 (no lo cuenta ningun tipo portado) */
            if (c_golpe) a -= 2;
            if (c_adel) a -= 3;
            if (c_choque) a -= 4;
            if (a < 0) a = 0;
            a = (int)((float)a * 2.70312f);
            if (lim > a) lim = a;
            if (lim > 0x13) lim = 0x13;
        }
        if (!m_tab) {
            for (i = 0; i < 19; i++) { float t = (float)i * 0.330078f; m_rx[i] = cosf(t) * 64.0f; m_ry[i] = sinf(t) * -64.0f; }
            m_tab = 1;
        }
        for (k = 1; k < 20; k++) {
            spr_t *r = m_rec + k;
            if (r->usado) {
                if (k > lim) { r->usado = 0; roto_abre(m_rx[k - 1] + cx, -(cy - m_ry[k - 1])); }
                else {
                    if (spr_acabado(r)) spr_abre(r, 6, 1, 0x3b, 0, r->x, r->y);
                    r->x = m_rx[k - 1] + cx; r->y = m_ry[k - 1] - cy;
                }
            } else {
                if (k > lim) break;
                m_f18[k] = 7;
                spr_abre(r, 6, 0x3d, 0x69, 1, m_rx[k - 1] + cx, -cy - m_ry[k - 1]); r->volteado = 1;
                nuevo = 1;
            }
        }
        if (nuevo) sfx_voz_se(0xd7);
        m_visto = mostrados;
    }
    if (m_t8c78 > 0) m_t8c78--;
}
/* lo que hay que dibujar del medidor: corazones del centro y del anillo, roturas y efectos */
int infarto_medidor(infarto_spr_t *v, int max)
{
    int i, n = 0;
#define PON(s) do { if ((s)->usado && n < max) { v[n].g = (s)->g; v[n].cuadro = (int)((s)->c > (float)(s)->fin ? (s)->fin : (s)->c); \
                     v[n].x = (s)->x; v[n].y = (s)->y; n++; } } while (0)
    for (i = 0; i < 20; i++) PON(m_rec + i);
    for (i = 0; i < 24; i++) PON(m_roto + i);
    for (i = 0; i < 16; i++) if (m_efe[i].usado) PON(&m_efe[i].s);
    PON(&m_alerta);
#undef PON
    return n;
}
uint32_t infarto_tinte(void) { return m_tinte; }
int infarto_corazones(void) { return disp / 10000; }
int infarto_total(void) { return score / 10000; }
int32_t infarto_disp(void) { return disp; }
int infarto_trafico(void) { return trafico; }
int infarto_mueven(void) { return mueven; }
int infarto_grupo_voz(int k) { return dat && k >= 0 && k < 15 ? grp[k] : -1; }
const infarto_hud_t *infarto_hud(void) { return &hud; }
