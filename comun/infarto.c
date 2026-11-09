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

#define NIV 15
static uint8_t *dat;
static const uint8_t *mis, *pun, *anu, *res, *cor, *grp, *modelos;
static uint8_t modelos_c[56], coches_t[4096] __attribute__((aligned(32)));   /* 2 sectores con la tabla */
static uint32_t lba_dat, off_coches;
static uint8_t lv[NIV][0x44];                       /* 7F2548 + nivel * 0x44 (registro del nivel) */
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
/* coches de la mision */
static const uint8_t *ct_base; static int ct_v, ct_i, ct_n, ct_pend = -1, ct_sec;  /* 7F24B8, 7F24C0, 7F24BC, 7F24C4 */
static infarto_hud_t hud;
/* 45FB80 (PS2 0x1ac580): los corazones de la peticion en curso en un anillo de 19 (0x13) de radio 64 alrededor del
   rotulo (176, -96), a 0,33 rad cada uno (PS2 0x1acb84..0x1acc0c); cuantos: los corazones del resultado + 1 por 0,25
   (0x1acaac), con tope (6 o 7 - 2 golpe - 3 adelantado - 4 choque) x 2,7031 si ha habido sucesos (0x1acaf4..0x1acb60).
   Uno nuevo aparece (2B0015 cuadros 0x3D..0x69, luego late en 1..0x3B) con el efecto 0xD7; uno que sobra se rompe
   (2B000E a 2,5 cuadros por paso). El del centro (2B000F, bucle 0x1F..0x3C) en (234, 108) (0x1ac670: la vista sin
   la proyeccion del ancla 5E0AC8, PROBABLE para la vista por defecto) */
static infarto_corazon_t anillo[19];
static float roto_x[19], roto_y[19], roto_c[19];
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
static int medidor;                                 /* 7F19F8: 1 con una peticion en curso */
static float centro_c;
static void medidor_paso(void);

static int16_t rd16(const uint8_t *p) { return (int16_t)(p[0] | p[1] << 8); }
static int32_t rd32(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24); }

int infarto_inicia(void)
{
    uint32_t tam, o[8];
    if (dat) return 0;
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
    int b = grp[16 + k], i;
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
    rotulo_nota(nt);
    lv[nivel_act][0x38 + nt]++;
    if (n_req < 5) LV_U32(nivel_act, 8 + 4 * n_req) = nt;
    LV_U32(nivel_act, 0x1c) += nt;
    if ((int8_t)lv[nivel_act][0x40] < 5) LV_U32(nivel_act, 0x20 + 4 * (int8_t)lv[nivel_act][0x40]) = h;
    score += h * 10000; lv[nivel_act][0x40]++;
    hud.nota = nt; hud.t_nota = 0;
    dbglog(DBG_INFO, "INFARTO: fin de la peticion tipo %d: resultado %d, nota %d, +%d corazones, total %d\n", q,
           (int)(result / 10000), nt, h, (int)(score / 10000));
    medidor = 0;                                    /* 7F19F8 = 2: se cierra */
    empieza(req_ini, 3);
}
static void cierra(int nt, int32_t edi, int hs)    /* cola comun de las evaluaciones */
{
    int32_t h = corazones_res(edi, nt);
    contador = (hs + hs / 2) * 10 / 7;               /* 7F8B30 (PS2 0x35c950): efecto 0xD5 cada 10 cuadros */
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
static int portado(int k) { return k == 0 || k == 2 || k == 22 || k == 6; }
static void puesta(const uint8_t *r, const infarto_coche_t *c)
{
    int w0 = rd16(r), w2 = rd16(r + 2), w6 = rd16(r + 6), w8 = rd16(r + 8);
    switch (q) {
    case 0:                                         /* 45E5B0 */
        k0_fase = 1; k0_9c = k0_98 = k0_a0 = 0; k0_a = w0; k0_b = w2; k0_ab0 = 0; k0_aac = 0; reinicia_res(); break;
    case 2: case 22:                                /* 45F310 (el 22 instala ademas un registro de aparicion: SIN PORTAR) */
        k2_c = w6; k2_d = w8; k2_ini = k2_ult = c->pasados; reinicia_res(); break;
    case 6: k6_a = w0; k6_b = w2; k6_max = 0; reinicia_res(); break;
    }
    medidor = 1; centro_c = 0x1f;                    /* 7F19F8 = 1 */
}
static void control(const infarto_coche_t *c)
{
    int spd = (int)(c->vel_mf * 216.720001f * 0.61297071f);   /* PS2 0x1a8dc8..0x1a8de8 */
    switch (q) {
    case 0: {                                       /* 45E610 (PS2 0x1a8d88) */
        if (k0_fase == 2) {
            if (c->d36 > 0) { if (--k0_9c < 0) k0_9c = 0; }
            else if (k0_9c < 0x78) k0_9c += 2;
            else { k0_fase = 1; k0_aac = 0; }
        } else if (c->d36 > 0) {
            k0_fase = 2; k0_9c = 0; voz(0xc6, 0x3c, 0); k0_aac = c->seccion;
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
        }
        dx23dc++; break; }
    case 6: {                                       /* 45F540 (PS2 0x1aaf18) */
        int edi = (int)((float)(c->seccion - k6_a) / (float)(int16_t)(k6_b - k6_a) * 800000.0f);
        if (sucesos(pun + 72, c)) voz(0x14f, 0x3c, 6);
        if (edi > k6_max) k6_max = edi;
        resultado(k6_max); break; }
    default: {                                      /* sin peticion: PS2 0x1a8100..0x1a8250 */
        static const uint8_t T[14] = { 0, 0, 1, 1, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0 };
        if (c->choque_ini) {
            int k = c->choque - 1, t = k >= 0 && k <= 13 ? T[k] : 2;
            if (t == 0) { pierde(0x32); voz(0x150, 0x3c, 5); }
            else if (t == 1 && c->impulso_rapido) { pierde(0x14); voz(0x14c, 0x3c, 5); }
        }
        if (c->impulso && c->impulso_rapido) { pierde(0xa); voz(0x14b, 0x3c, 5); }
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
    case 2: mueven = 1; break;                      /* -17: 45D830, los de la mision a la velocidad de la carretera */
    case 1: mueven = 1; break;                      /* -18: 45B520 (escoltas: a la del jugador; SIN PORTAR) */
    case 17: trafico = 1; break;                    /* -2: 46E4A0(1) */
    case 13:                                        /* -6: anuncio de la peticion (tabla 0x3f0280) */
        if (w4 >= 0 && w4 < 26 && portado(w4)) {
            int id = rd16(r + 6) == 1 ? (anu[w4 * 4 + 2] | anu[w4 * 4 + 3] << 8) : (anu[w4 * 4] | anu[w4 * 4 + 1] << 8);
            if (id) { voz(id, 0x3c, 0xa); hud.anuncio = w4; hud.t_anuncio = 0; }
        }
        break;
    case 5:                                         /* -14: 45D710, tabla de coches */
        {   uint32_t o = off_coches + (w4 & 0x3f) * 0x320, s0 = o & ~2047u;
            ct_base = NULL;
            if (gd_lee(lba_dat + (s0 >> 11), 2, coches_t)) break;
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

static void gestor(const infarto_coche_t *c)        /* 464350 (PS2 0x1ae440) */
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

void infarto_carrera(int clave, int nivel)          /* 45E0F0 */
{
    int l;
    score = disp = 0; memset(vcola, 0, sizeof vcola); vc_cab = vc_fin = 0; voz_ult = 0; reloj = 0x10000;
    contador = 0; dx23dc = 0; medidor = 0; { int i; for (i = 0; i < 19; i++) { anillo[i].cuadro = -1; roto_c[i] = -1; } }
    result = 0; bonus = 0; meta_hecha = 0; vs_suma = 0; vs_n = 0; memset(vel_nivel, 0, sizeof vel_nivel); trafico = 1; ct_base = NULL; ct_pend = -1; mueven = 1; ban_on = 0;
    for (l = 0; l < NIV; l++) { memset(lv[l], 0, 0x44); LV_SEL(l) = -1; LV_U32(l, 4) = 0x29;
        LV_U32(l, 8) = LV_U32(l, 12) = LV_U32(l, 16) = LV_U32(l, 20) = LV_U32(l, 24) = -1; }
    memset(&hud, 0, sizeof hud); hud.nota = -1; hud.nota_etapa = -1; hud.anuncio = -1; hud.t_nota = hud.t_etapa = hud.t_anuncio = 9999;
    clave_act = clave; nivel_act = nivel; empieza(0, 3); n_req = 0; rec_i = 0; h_nivel = 0;
    cuenta_peticiones(mision(clave, nivel));
    if (dat) sfx_voz_grupo(grp[clave >= 0 && clave < 15 ? clave : 0]);
}
void infarto_etapa(int clave, int nivel)            /* 464350, stage clear */
{
    int prev = nivel_act, i; int32_t edi;
    edi = score / 10000 - h_nivel;
    if (prev >= 0 && prev < NIV) vel_nivel[prev] = vel_media();   /* 44FF20 */
    vs_suma = 0; vs_n = 0;
    {   int b = bono_nivel(prev, sec_union_ult);     /* 45D360 del nivel anterior */
        if (b > 0) { edi += b; score += b * 10000; }
        dbglog(DBG_INFO, "INFARTO: bono del nivel %d: %d (derrape %d, firme %d, en %d posiciones; velocidad media %.1f x %.4f)\n", prev, b,
               st_5c, st_60, sec_union_ult - st_5a, (double)vel_nivel[prev], (double)hud_coche);
    }
    st_54 = 0; st_5c = 0; st_60 = 0; st_5a = -1; st_5e = st_62 = -1;
    LV_U32(prev, 0x34) = edi;
    for (i = 0; i < (int8_t)lv[prev][0x40] && i < 5; i++) LV_U32(prev, 0x34) -= LV_U32(prev, 0x20 + 4 * i);
    if ((int8_t)lv[prev][0x40] > 0) { hud.nota_etapa = LV_U32(prev, 0x1c) / (int8_t)lv[prev][0x40]; hud.t_etapa = 0; }
    dbglog(DBG_INFO, "INFARTO: etapa %d terminada: %d corazones en ella, nota media %d, total %d\n", clave_act, (int)edi,
           hud.nota_etapa, (int)(score / 10000));
    disp = score; h_nivel = score / 10000;
    clave_act = clave; nivel_act = nivel; n_req = 0; rec_i = 0; trafico = 1;
    cuenta_peticiones(mision(clave, nivel));
    sfx_voz_grupo(grp[clave >= 0 && clave < 15 ? clave : 0]);
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
    disp = score; LV_U32(nivel_act, 0x34) = esi;
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
    hud_coche = c->hud;
    control(c);
    d = (score - disp) / 32;                         /* el contador alcanza a los corazones */
    if ((d < 0 ? -d : d) * 32 < 20000) disp = score; else disp += d;
    if (disp < 0) { score = disp = 0; }
    voz_bomba();
    if (c->en_union) sec_union_ult = c->seccion_union;
    estadisticas(c);
    medidor_paso();
    rotulo_paso();
    if (contador > 0) { if (contador % 10 == 0) sfx_voz_se(0xd5); contador--; }
    gestor(c);
    if (q != 3) { v_n++; v_acu += c->kmh; v_med = v_acu / v_n; if (v_min > c->kmh) v_min = c->kmh; }
    /* 45D7A0: los coches de la tabla, de 30 a 50 posiciones por delante */
    while (ct_base && ct_pend < 0 && !(ct_v & 0x80) && ct_i < 0x64) {
        const uint8_t *r = ct_base + ct_i * 8; int s = rd16(r + 4);
        if (s < 0 || (int8_t)r[6] < 0) break;
        if (s < c->seccion + 0x1e) { ct_i++; continue; }  /* ya atras: se salta */
        if (s <= c->seccion + 0x32) ct_pend = ct_i;
        break;
    }
    hud.tipo = q; hud.resultado = q == 3 ? 0 : (int)(res_pos() / 10000); hud.corazones = disp / 10000; hud.total = score / 10000;
    if (hud.t_nota < 9999) hud.t_nota++;
    if (hud.t_etapa < 9999) hud.t_etapa++;
    if (hud.t_anuncio < 9999) hud.t_anuncio++;
}
/* el coche que hay que abrir ahora (45D470): clase, seccion, carril + 1 del registro (el carril de la plaza), modelo */
int infarto_coche_pide(infarto_coche_nuevo_t *n)
{
    const uint8_t *r; int cl;
    if (!ct_base || ct_pend < 0 || ct_pend >= 0x64) return 0;
    r = ct_base + ct_pend * 8;
    if (rd16(r + 4) < 0 || (int8_t)r[6] < 0) { ct_base = NULL; return 0; }
    cl = rd32(r); n->clase = cl; n->seccion = rd16(r + 4); n->carril = (int8_t)r[6]; n->id = ct_n;
    if (cl <= 1) n->modelo = 0x2b;
    else if (cl == 2) n->modelo = 0x1f;
    else if (cl <= 4) n->modelo = rd32(modelos + 4 * (ct_n % 7));   /* 0x3f0850 (el tramo 0x1a, 0x3f0870, no esta) */
    else n->modelo = -1;
    return 1;
}
void infarto_coche_abierto(int ok)
{
    if (ct_pend < 0) return;
    if (!ok) return;                                /* sin plaza: se reintenta en el siguiente paso */
    ct_n++; ct_i = ct_pend + 1;
    if (ct_v & 0x80) { ct_pend = ct_i < 0x64 ? ct_i : -1; } else ct_pend = -1;
}
void infarto_libera(void) { free(dat); dat = NULL; }
static void medidor_paso(void)
{
    int i, h, lim, a, nuevo = 0;
    for (i = 0; i < 19; i++) if (roto_c[i] >= 0) { roto_c[i] += 2.5f; if (roto_c[i] >= 60) roto_c[i] = -1; }
    if (!medidor) { for (i = 0; i < 19; i++) if (anillo[i].cuadro >= 0) { roto_x[i] = anillo[i].x; roto_y[i] = anillo[i].y; roto_c[i] = 0; anillo[i].cuadro = -1; } return; }
    centro_c += 1; if (centro_c > 0x3c) centro_c = 0x1f;
    h = result / 10000 + 1; if (h < 0) h = 0;
    lim = (int)((float)h * 0.25f); if (lim > 0x13) lim = 0x13;
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
    for (i = 0; i < 19; i++) {
        infarto_corazon_t *c = anillo + i;
        float t = (float)i * 0.330078f;
        c->x = 176.0f + cosf(t) * 64.0f; c->y = sinf(t) * -64.0f - 96.0f;
        if (c->cuadro >= 0) {
            if (i + 1 > lim) { roto_x[i] = c->x; roto_y[i] = c->y; roto_c[i] = 0; c->cuadro = -1; }
            else { c->cuadro++; if (c->cuadro > 0x69 || c->cuadro == 0x3c) c->cuadro = 1; }   /* 0x3D..0x69, luego 1..0x3B */
        } else if (i + 1 <= lim) { c->cuadro = 0x3d; nuevo = 1; }
    }
    if (nuevo) sfx_voz_se(0xd7);
}
const infarto_corazon_t *infarto_anillo(float *rx, float *ry, float *rc, float *centro)
{
    int i;
    for (i = 0; i < 19; i++) { rx[i] = roto_x[i]; ry[i] = roto_y[i]; rc[i] = roto_c[i]; }
    *centro = medidor ? centro_c : -1;
    return anillo;
}
int infarto_corazones(void) { return disp / 10000; }
int infarto_total(void) { return score / 10000; }
int infarto_trafico(void) { return trafico; }
int infarto_mueven(void) { return mueven; }
int infarto_grupo_voz(int k) { return dat && k >= 0 && k < 15 ? grp[k] : -1; }
const infarto_hud_t *infarto_hud(void) { return &hud; }
