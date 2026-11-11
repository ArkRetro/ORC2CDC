/* Frontal de recreativa de OUTRUN2SP (ver fsp.h). Port linea a linea del suceso 4 de arcade_attract.cpp (PC 4BEE60..
   4C4880, funciones 1..12 de la tabla 59BE78 = PS2 0x3ee7b4) y del modo 10 ATTRACT (race_end_modes.cpp 48B210 /
   48AE90), con las tablas del ELF de SLES-53998 (bloque FSP1 de SPFE.DAT, tools/conv_sp.py tablas_fe) y las escenas
   del banco 0x2E SELECTOR_CVT con el reproductor de comun/sprani.c (428460 / 4285A0 / 428800 / 428880 / 4288C0 /
   429530).

   Recorrido de un jugador sin red (secuencia 5C8BE0 = PS2 0x4126a0 {0xB, 5, 6, -1, 4, 2, 8, 9, 0xA, 0}: el codigo c
   arranca su pantalla con el paso c (4C2AC0) y el modo 10 pone la funcion c + 1):
     0xB (4C0DE0) -> funcion 12 "Single Play." (2E004E, 0x78 pasos) -> 5 (4C2570) funcion 6: coche (lista de 10 de
     2E0019 / 2E0017 / 2E001A y el coche 3D; Y sostenido: el color) -> 6 (4BF3B0) funcion 7: transmision (2E004F) ->
     +2 (un jugador: salta el -1) 4 (4C0110) funcion 5: modo de juego (OutRun / Infarto / Contrarreloj; R cambia los
     cursos: rotulos "Recorrido OutRun2SP" / "OutRun2", preset 0 / 1) -> 2 (4BFFC0) funcion 3 solo en la Contrarreloj: configuracion del coche
     (normal / modificado) -> 8 (4C2610) funcion 9: musica (Y sostenido: balance musica / motor) -> +2 0xA funcion 11:
     la carrera (4C05F0). B vuelve a la pantalla anterior; B en el coche: 4C2C60 lleva al 8 -> funcion 11 con las
     banderas 3 -> 4BF510: modo 32, el menu de la consola.
   Memoria: las variables del PC 0x844900..0x84AB00 en mm[] con sus direcciones (como el port de PC) y la del suceso
   (780440) en wk[]. Los mandos del frontal del PC (4536F0 / 4536C0) en la Dreamcast: 8 = B (cancelar), 1 / 4 = START / A
   (4BFB20), 0x10 sostenido = Y (el color y el balance, como el Y del menu de la consola), 0x400 / 0x800 = arriba /
   abajo, 0x40000 = gatillo R (el bit 0x2000 del menu de la consola); el eje 453720(0) = la palanca X (la cruceta
   izquierda / derecha cuenta como la palanca a tope: PROBABLE, el PS2 la filtra en 0x210a00) y 453750(0) = la X del paso
   anterior (PROBABLE).
   SIN PORTAR (ver docs/PARIDAD.md, OUTRUN2SP): el mundo y la camara del coche (4BF2E0 / 4BF340: suceso 0x181 funcion
   0xD), los textos de ayuda de 84A320 (446D90), las voces 6..0x3F (4BEE60: solo con el idioma 0, en la PAL espanola no
   suenan), el fundido de volumen de la musica (42E020), la red, varios jugadores y la pagina 84A218. */
#include <kos.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "sprani.h"
#include "musica.h"
#include "sfx.h"
#include "fsp.h"

typedef struct {
    char m[4];
    uint32_t fila[564][3];                  /* 5C7170 */
    uint32_t sec[10];                       /* 5C8BE0 */
    uint32_t son[64];                       /* [6898D8] */
    uint32_t mus[16][6];                    /* 5C8D00 */
    uint32_t coche[10][10];                 /* 5C91DC */
    uint32_t col[10][16];                   /* 5C9204: n e indices */
    uint32_t rotulo[10];                    /* 5C922C */
    uint32_t col_ini[10];                   /* 5C70A8 */
    uint32_t voz_coche[10];                 /* 5C9408 */
    uint32_t ta[2][10][8];                  /* 689B28 / 689C68 */
    uint32_t modo[2][21];                   /* 689DA8 */
    uint32_t m50[2], m58[2], m60[4];        /* 689E50, 689E58, 689E60 */
    float f70[4];                           /* 689E70..7C */
    uint32_t fondo[13];                     /* 689F58 */
    uint32_t panel_y[11];                   /* 5C9334 */
    uint32_t tok19[4], tok1a[4];            /* 689AAC, 689ABC */
    float lx, ly;                           /* 689ACC, 689AD0 */
    uint32_t modelos[10], ranura[10];       /* 689AD4, 689AFC */
    float aa0, aa4, aa8, fc4, fbc, fa0;
    uint32_t fc0, a9fc, fa4, fb8, fb4;
    float barra[4];                         /* 213540: 4,37 / 563 / 406 / 12 */
} tab_t;

static spr_banco_t *bk;
static const tab_t *T;
static int activo, salida;                  /* salida: FSP_CARRERA / FSP_MENU al acabar */
static unsigned bot_nuevos, bot_pulsados;
static int eje_x, eje_x_ant;

/* ---- memoria del PC ---- */
static uint8_t mm[0x6200];
#define A_(a) (mm + ((a) - 0x844900u))
static uint32_t U32(uint32_t a) { uint32_t v; memcpy(&v, A_(a), 4); return v; }
static int32_t I32(uint32_t a) { return (int32_t)U32(a); }
static void P32(uint32_t a, uint32_t v) { memcpy(A_(a), &v, 4); }
static uint16_t U16(uint32_t a) { uint16_t v; memcpy(&v, A_(a), 2); return v; }
static int16_t I16(uint32_t a) { return (int16_t)U16(a); }
static void P16(uint32_t a, uint16_t v) { memcpy(A_(a), &v, 2); }
static uint8_t U8(uint32_t a) { return *A_(a); }
static int8_t I8(uint32_t a) { return (int8_t)*A_(a); }
static void P8(uint32_t a, uint8_t v) { *A_(a) = v; }
static float F32(uint32_t a) { float v; memcpy(&v, A_(a), 4); return v; }
static void PF(uint32_t a, float v) { memcpy(A_(a), &v, 4); }
/* el suceso 4 (780440) */
static uint8_t wk[0x70];
#define WU8(o) (wk[o])
#define WI8(o) ((int8_t)wk[o])
#define WP8(o, v) (wk[o] = (uint8_t)(v))
static uint16_t WU16(int o) { uint16_t v; memcpy(&v, wk + o, 2); return v; }
static int16_t WI16(int o) { return (int16_t)WU16(o); }
static void WP16(int o, uint16_t v) { memcpy(wk + o, &v, 2); }
static uint32_t WU32(int o) { uint32_t v; memcpy(&v, wk + o, 4); return v; }
static void WP32(int o, uint32_t v) { memcpy(wk + o, &v, 4); }
static int seq_k;                           /* +0: la secuencia (siempre 5C8BE0) */
static uint32_t seq_(int i) { return i >= 0 && i < 10 ? T->sec[i] : 0; }
static uint32_t fb_ = 0x689fb4, umbral_mas, umbral_menos;   /* 689FB4 / 689FB8 (se cambian) */
static float suave_84a314;
static int fn_actual;                       /* funcion del suceso 4 en curso (1..12; 0 ninguna) */
static int estado_ev;                       /* 79FB4C & 3: 2 en marcha */
static int modo10;                          /* 830368 */
static int colores_84a208 = -1;             /* 84A208: tabla de colores del coche (o -1: la 5C8EA8, sin portar) */
static int ta_84a240 = -1;                  /* 84A240: registro de 689B28 del coche */
static int pista_sonando = -1;

/* ---- servicios ---- */
#define H(a) ((int)U32(a))
static int crea(uint32_t tok, int capa, int modo, int a, int b) { return spr_crea(tok, capa, modo, a, b); }
static int crea_fila(int r, int capa, int modo) { return spr_crea(T->fila[r][0], capa, modo, T->fila[r][1], T->fila[r][2]); }
static void suelta(uint32_t a) { spr_suelta(H(a)); }
static int estado_h(int h) { return h < 0 ? 0 : spr_estado(h); }
static void inm(uint32_t tok, float x, float y, int capa, int cuadro) { spr_inmediata(tok, x, y, capa, cuadro); }
static int idioma0(void) { const char *s = sfx_voces_idioma(); return s && (s[0] == 'E' || s[0] == 'e'); }
static void sonido(int id) { sfx_menu(id); }                          /* 424940 */
static void sonido_4bee60(uint32_t id) { if (!idioma0() && id >= 6 && id <= 0x3f) return; sfx_menu((int)id); }
static void musica_401000(int pista) { if (pista != pista_sonando) { pista_sonando = pista; musica_pon(pista); } }
static void musica_401030(void) { pista_sonando = -1; musica_para(); }
static int pulsa(unsigned m)                /* 4536F0 */
{
    unsigned b = bot_nuevos;
    switch (m) {
    case 1: return !!(b & CONT_START);
    case 4: return !!(b & CONT_A);
    case 8: return !!(b & CONT_B);
    case 0x400: return !!(b & CONT_DPAD_UP);
    case 0x800: return !!(b & CONT_DPAD_DOWN);
    case 0x40000: return !!(b & (1u << 31));            /* gatillo R (bit propio, ver fsp_paso) */
    }
    return 0;
}
static int sostiene(unsigned m) { return m == 0x10 ? !!(bot_pulsados & CONT_Y) : 0; }   /* 4536C0 */
static int r_4bfb20(void) { return pulsa(1) || pulsa(4); }
static int eje(int k) { return k == 0 ? eje_x : 0; }                  /* 453720 */
static int cvtt(float f) { if (!(f > -2147483904.0f && f < 2147483648.0f)) return (int)0x80000000; return (int)f; }

/* ---- 4BEE90 .. 4BF8D0 ---- */
static void lista_4bee90(uint32_t tok, int8_t cl, float x2, float x4, float s0, uint32_t s1)
{
    float t = T->aa4 + T->aa0, x1, x0;
    PF(0x84a26c, 1.0f);
    P32(0x84a298, tok); P32(0x84a29c, tok); P8(0x84a291, (uint8_t)cl);
    t = (t + x2) * 2.0f;                                /* 6280B0 */
    P32(0x84a2a0, s1); PF(0x84a278, t);
    x1 = (float)cl * t; x0 = t * 0.5f + x2;             /* 628064 */
    P8(0x84a260, 2); P8(0x84a261, 2);
    PF(0x84a27c, x1); PF(0x84a264, x0); PF(0x84a270, x0);
    x1 = x1 + x4;
    PF(0x84a268, 0);
    P8(0x84a28c, 1); P8(0x84a28d, 0); P8(0x84a28e, 0); P8(0x84a28f, 0); P8(0x84a290, 0); P8(0x84a2a4, 0);
    PF(0x84a274, x2); PF(0x84a280, x4); PF(0x84a284, x1); PF(0x84a288, 0);
    P8(0x84a293, 0); PF(0x84a294, s0); P8(0x84a292, 0xa);
    WP8(0x19, 0); WP8(0x1a, 0);
}
static float fila_4bef90(int r)
{
    float x1, x0;
    if (r < 0) r = 0; else { int n = I8(0x84a291); if (!(r < n)) r = n - 1; }
    x1 = F32(0x84a274) * 2.0f; x0 = (float)r * F32(0x84a278);
    x0 = x0 + x1; x0 = x0 + T->aa4; x0 = x0 + T->aa0; x0 = x0 - F32(0x84a264);
    return x0;
}
static int casi_cero(float v) { return 1e-6f > v && v > -1e-6f; }   /* 5A29E0 > v > 5C9404 (SUPUESTO: +-1e-6) */
static void f_4befe0(float lim, float reset)
{
    float v = F32(0x84a268), t;
    if (!casi_cero(v)) return;
    t = F32(0x84a270) + 1.0f; PF(0x84a270, t);
    if (!(t >= lim)) return;
    PF(0x84a270, reset);
}
static void f_4bf030(void)
{
    int k = WI8(0x19);
    if (I32(0x8449cc) >= 0 && estado_h(H(0x8449cc)) == 3) {
        int r = T->coche[k][2];
        suelta(0x8449cc);
        if (U32(0x844958)) {
            int y, x;
            suelta(0x8449c4);
            y = cvtt(F32(0x84a284)); x = cvtt(F32(0x84a280));
            P32(0x8449c4, crea(T->tok19[0], 7, 2, x, y));
        }
        P32(0x8449cc, crea_fila(r, 3, 0));
        P32(0x844958, 0);
    }
    if (I32(0x84497c) >= 0 && estado_h(H(0x84497c)) == 3) {
        uint32_t kk;
        suelta(0x84497c);
        kk = U32(0x84a2b8);
        P32(0x84497c, 0xffffffffu);
        if (kk != 0xffffffffu) {
            int h;
            if (F32(0x84a20c) > 0.5f) h = crea(T->fila[kk][0], 4, 1, T->fila[kk][1], T->fila[kk][2]);
            else h = crea(T->fila[kk][0], 4, 1, T->fila[kk][2], T->fila[kk][1]);
            P32(0x844980, h);
            spr_velocidad(h, F32(0x84a20c));
            return;
        }
        P32(0x844984, crea_fila(T->coche[k][7], 4, 0));
        P32(0x84a2b8, 0xffffffffu); PF(0x84a20c, 1.0f);
    }
    if (I32(0x844980) >= 0 && estado_h(H(0x844980)) == 3) {
        suelta(0x844980);
        P32(0x844980, 0xffffffffu);
        P32(0x844984, crea_fila(T->coche[k][7], 4, 0));
        P32(0x84a2b8, 0xffffffffu); PF(0x84a20c, 1.0f);
    }
}
/* 4BF260: la Dreamcast lee el coche entero al elegirlo (demo/main.c, menu_coche): las 10 ranuras quedan listas */
static void modelos_4bf260(void) { uint32_t i; for (i = 0; i < 10; i++) P32(0x84a2e8 + i * 4, 2); }
static int index_4bf370(uint32_t key, uint8_t kind) { (void)key; (void)kind; return 1; }   /* (pagina 84A218: sin portar) */
static void paso6_4bf3b0(void)
{
    P32(0x844990, crea(0x2e004f, 8, 1, 0, 0xe));
    sonido(0);
    P32(0x844958, 1);
}
static void f_4bf400(void)
{
    int k = WI8(0x3e), sel = WI8(0x19), r;
    static const int TT[5] = { 1, 4, 7, 10, 13 };       /* 689DAC.. = 689DA8 + 4 + sel x 0xC (palabras) */
    suelta(0x8449cc); suelta(0x8449d8);
    r = T->modo[k][TT[sel <= 3 ? (sel < 0 ? 4 : sel) : 4]];
    P32(0x8449cc, crea(U32(0x84a23c), 4, 0, T->fila[r][1], T->fila[r][2]));
    P32(0x8449d8, crea(0x2e0024, 9, 1, T->fila[r][1], T->fila[r][2]));
}
static void fn11_ctrl_4bf500(void) { WP8(0xa, WU8(0xa) | 2); }
static void fn11_dest_4bf510(void)
{
    musica_401030();
    if ((WU8(8) & 3) != 3) return;
    salida = FSP_MENU;                                  /* 43F8C0(0x20): SUMO_FE, el menu de la consola */
    WP8(8, WU8(8) & 0xfc);
}
static void pista_4bf970(void)
{
    uint32_t want = U32(0x844a00);
    if (want == 0xffffffffu) {
        if (I32(0x844a04) >= 0x14) return;
        P32(0x8449fc, U32(0x844a00)); P32(0x844a04, U32(0x844a04) + 1);
        return;
    }
    if (U32(0x8449fc) == want) return;
    if (I32(0x844a04) < 0x14) { P32(0x844a04, U32(0x844a04) + 1); return; }   /* 42E020: el fundido (sin portar) */
    musica_401000((int)want);
    P32(0x8449fc, U32(0x844a00)); P32(0x844a04, 0);
}
static int repite_4bfac0(void)
{
    int v = eje(4) ? eje(4) : eje_x, a, q;            /* 453720(4): (en la Dreamcast la palanca X) */
    P32(0x84a9ac, U32(0x84a9ac) + 1);
    a = v < 0 ? -v : v;
    if (a < 0x40) { P32(0x84a9ac, 0x12); return 0; }
    q = I32(0x84a9ac) % 0x14;
    if (q != 0x13) return 0;
    return v > 0 ? 1 : -1;
}
static void fn12_init_4bfb90(void)
{
    WP8(8, WU8(8) & 0xfc); WP8(0xa, WU8(0xa) & 0xfc);
    WP16(0x3c, 0); WP16(0x3e, 0); WP32(0x40, 0xffffffffu);
}
static uint32_t elige_4bfbd0(void)
{
    int8_t bl; int k, r, e;
    if (!pulsa(8) && !r_4bfb20() && (WU8(8) & 3) != 1) return 0;
    bl = WI8(0x19); k = bl;
    r = T->coche[k][4];
    if (!pulsa(8)) sonido_4bee60(T->son[T->voz_coche[k] & 63]);
    suelta(0x8449c4); suelta(0x8449cc); suelta(0x84497c); suelta(0x844980); suelta(0x844984);
    P32(0x8449c4, 0xffffffffu); P32(0x8449cc, 0xffffffffu); P32(0x84497c, 0xffffffffu); P32(0x844980, 0xffffffffu);
    P32(0x8449c4, crea(T->tok1a[0], 3, 1, T->fila[r][1], T->fila[r][2]));
    e = k + U32(0x84a218) * 10;
    WP8(0x1d, bl);
    WP8(0x1e, T->col_ini[e < 10 ? e : 0] & 0xff);
    if (pulsa(8)) { WP8(8, WU8(8) | 3); WP16(0x10, 0); return 1; }
    WP16(8, (uint16_t)((WU16(8) & 0xfffe) | 2));
    sonido(0x40);
    WP16(0x10, 0);
    return 1;
}
static void fn6_dest_4bfdb0(void) { P32(0x84a318, 0); }
/* 4BFA20: 84A318 = 2 con la funcion 5 (codigo de la secuencia; la 6 del suceso) y su coche cargado */
static void capa8_4bfa20(void)
{
    int on = modo10 == 7 && estado_ev == 2 && WU32(4) == 5 && U32(0x84a2e8 + T->ranura[(WI8(0x19) & 0x7f) % 10] * 4) == 2;
    P32(0x84a318, on ? 2 : 0);
}
static void fondo_4c0b70(void)
{
    uint32_t i = U32(0x84a214) ? 0xc : 0;
    P32(0x844968, crea_fila(T->fondo[i], 2, 1));
    P32(0x84a2c4, 0); P16(0x84a2c0, 0); P32(0x8449dc, 0);
}
static void fondo_4c2430(void)
{
    int h = H(0x844968); uint32_t t;
    if (h >= 0 && estado_h(h) == 3) {
        uint32_t k = U32(0x8449dc), i;
        if (k == 0 || k == 2 || k == 4) i = 9; else if (k == 1) i = 0xa; else i = (k != 3 ? 1 : 0) + 0xb;
        suelta(0x844964); suelta(0x844968);
        P32(0x844968, 0xffffffffu);
        P32(0x844964, crea_fila(T->fondo[i], 2, 0));
    }
    t = U32(0x84a214);
    if (t) {
        suelta(0x844964); suelta(0x844968);
        P32(0x844964, 0xffffffffu);
        P32(0x844968, crea_fila(T->fondo[t], 2, 1));
    }
    /* (2E0049 de varios jugadores: sin portar) */
}
static void paso5_4c2570(void)
{
    P32(0x844958, 1); P32(0x84a2d8, 0); P32(0x84a2dc, 0);
    P32(0x8449c4, crea(T->tok19[0], 7, 1, 0, 0x59));
    P32(0x8449cc, crea(0x2e0017, 3, 1, 0, 0x59));
    P32(0x84a214, 0xc);
    fondo_4c2430();
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
}
static void paso8_4c2610(void)
{
    int k;
    P32(0x8449c4, crea(0x2e0028, 7, 1, 0, 0x3b));
    P32(0x8449cc, crea(0x2e0027, 4, 1, 0, 0x3b));
    P32(0x84a214, 0xc);
    fondo_4c2430();
    for (k = 0; k < 5; k++) { P32(0x84a220 + k * 4, 0); P32(0x84492c + k * 4, 0); }
    P32(0x84a234, 0);
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    P32(0x844958, 1);
    P8(0x84a220, 1); P8(0x84a221, 1);
    P32(0x84a2ac, 1);
}
static void paso2_4bffc0(void);
static void paso4_4c0110(void);
static void paso11_4c0de0(void);
static void secuencia_4c2ac0(int index)
{
    uint32_t v = seq_(index & 0xff) - 1;
    if (v > 10) return;
    for (;;) {
        switch (v) {
        case 0: {
            uint8_t i;
            WP8(0xa, WU8(0xa) | 1);
            i = (uint8_t)(WU8(0x15) + 1); WP8(0x15, i);
            v = seq_((int8_t)i) - 1;
            if (v <= 10) continue;
            return; }
        case 1: paso2_4bffc0(); return;
        case 3: paso4_4c0110(); return;
        case 4: paso5_4c2570(); return;
        case 5: paso6_4bf3b0(); return;
        case 7: paso8_4c2610(); return;
        case 8:
            P32(0x8449a8, crea(0x2e0050, 9, 1, 0, 0xd));
            P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
            return;
        case 10: paso11_4c0de0(); return;
        default: return;
        }
    }
}
static void fn1_init_4bfe00(void)
{
    static const int O[7] = { 0x1c, 0x1d, 0x22, 0x20, 0x1f, 0x23, 0x29 };
    int i; uint8_t a; uint32_t a2;
    for (i = 0; i < 7; i++) WP8(O[i], 0xff);
    WP32(0x58, 0xffffffffu);
    WP32(0x2a, 0); WP8(0x2e, 0);
    a = WU8(0xa);
    WP8(8, WU8(8) & 0xfc);
    P32(0x844950, 0);
    WP8(0x1e, 1); WP8(0x21, 1); WP8(0x24, 0xff);
    seq_k = 0;
    WP8(0xa, a & 0xfc);
    WP32(0xc, 0);
    WP16(0x10, 0); WP16(0x12, 0); WP8(0x14, 0); WP8(0x15, 0);
    WP32(4, seq_(0));
    for (a2 = 0x844960; a2 <= 0x8449d8; a2 += 4) P32(a2, 0xffffffffu);
    P32(0x8449fc, 0x1e); P32(0x844a00, 0x1e); P32(0x844a04, 0);
    musica_401000(0x1e);
    P32(0x8449f4, 3);
}
static void fn1_ctrl_4c3510(void) { WP8(0xa, WU8(0xa) | 1); secuencia_4c2ac0(WU8(0x15)); }
static void paso2_4bffc0(void)
{
    if (WU8(0x1f)) {
        uint8_t a = (uint8_t)(WU8(0xa) | 1), i = (uint8_t)(WU8(0x15) + 1);
        WP8(0x15, i); WP8(0xa, a);
        secuencia_4c2ac0(i);
        return;
    }
    ta_84a240 = WI8(0x1d);                              /* 689B28 (689C68 con [7C24BC]: sin portar) */
    P32(0x8449a0, crea_fila(T->ta[0][ta_84a240][0], 0xc, 1));
    sonido(0);
    P32(0x844958, 1);
}
static void fn12_ctrl_4c2b70(void)
{
    uint16_t st = WU16(0x3c);
    if (st == 0) {
        WP16(0x3c, 1);                                  /* 455B40: un jugador, al momento */
        P32(0x84a218, 0);
        seq_k = 0;
        P32(0x84a244, 0);
        WP32(0x40, (uint32_t)spr_crea_entera(0x2e004e, 7, 1));
        sonido_4bee60(T->son[0xf0 / 4]);
        return;
    }
    if (st != 1) return;
    P32(0x84a244, 0);
    {   uint16_t f = WU16(0x3e); WP16(0x3e, (uint16_t)(f + 1)); if (f != 0x78) return; }
    spr_suelta((int)WU32(0x40));
    {   uint8_t a = (uint8_t)(WU8(0xa) | 1), i = (uint8_t)(WU8(0x15) + 1);
        WP8(0x15, i); WP16(0x3c, 3); WP8(0xa, a); WP8(0x18, 0);
        secuencia_4c2ac0(i); }
}
static void paso11_4c0de0(void)
{
    uint8_t a, i;
    P32(0x84a210, 0); P32(0x84a244, 0);
    P32(0x84a214, 0xc);
    fondo_4c0b70();
    WP32(0x50, 1); WP32(0x54, 0); WP8(0x28, 1); WP8(0x27, 0xff);
    a = (uint8_t)(WU8(0xa) | 1);
    P32(0x84a218, 0);
    i = (uint8_t)(WU8(0x15) + 1);
    WP8(0xa, a); seq_k = 0; WP8(0x15, i);
    secuencia_4c2ac0(i);
}
static void paso4_4c0110(void)
{
    int r = 0;                                          /* 453320 = [0x54c108] (PS2 0x178478): 0 (ver PARIDAD) */
    P32(0x8449e0, (uint32_t)r);
    if (r == 0) P32(0x844944, 0); else if (r > 0 && r <= 2) P32(0x844944, 1);
    P32(0x84a24c, T->m60[0]); P32(0x84a23c, 0x2e0022);
    P32(0x84a250, T->m60[1]); P32(0x84a254, T->m60[2]); P32(0x84a258, T->m60[3]);
    P32(0x8449cc, crea(0x2e0022, 4, 1, 0, 0x3b));
    P32(0x8449d8, crea(0x2e0024, 9, 1, 0, 0x3b));
    P32(0x8449c4, crea(T->m50[U32(0x844944) & 1], 7, 1, 0, 0x3b));
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    P32(0x844958, 1);
}
static void fn5_init_4c01e0(void)
{
    int r; static const int O[7] = { 0x3e, 0x3f, 0x40, 0x41, 0x19, 0x1a, 0x27 }; int i;
    WP8(8, WU8(8) & 0xfc); WP8(0xa, WU8(0xa) & 0xfc);
    WP16(0x10, 0x384); WP16(0x12, 0x3c); WP16(0x3c, 0); WP8(0x1f, 0xff);
    r = I32(0x8449e0);
    P32(0x844950, 0);
    if (r >= 0 && r <= 2) lista_4bee90(T->m50[U32(0x844944) & 1], (int8_t)(r == 0 ? 3 : 5), T->f70[1], T->f70[0], 1080.0f, 7);
    for (i = 0; i < 7; i++) WP8(O[i], 0);
    sonido(4);
}

/* ---- 4C0A10 / 4C0BD0 / 4C0EB0 ---- */
static void pista_4bf8d0(void)
{
    uint32_t v = seq_(WI8(0x15)), t;
    if (I16(0x8449f8) != (int32_t)v) { P16(0x8449f8, (uint16_t)v); P16(0x84a2b4, 0); P16(0x8449e4, 0xf); P16(0x84494c, 0); }
    switch (v) { case 4: case 5: case 8: t = 0xf; break; case 6: t = 1; break; default: return; }
    if (WU8(0x18)) P16(0x84a2b4, (uint16_t)(t - 1));
    if (U16(0x84a2b4)) P16(0x84a2b4, (uint16_t)(U16(0x84a2b4) - 1));
    if (U16(0x8449e4)) P16(0x8449e4, (uint16_t)(U16(0x8449e4) - 1));
}
static void panel_4c0a10(void)
{
    uint32_t ebx = 0, esi = 0; uint8_t a;
#define Y_(i) ((float)(int32_t)T->panel_y[(i) < 11 ? (i) : 10])
    pista_4bf8d0();
    if (WU8(0x22) != 0xff) { inm(0x2e0011, 580.0f, 90.0f, 5, WI8(0x1d)); esi = 1; }
    if (WU8(0x22) != 0xff) { inm(0x2e0016, 580.0f, Y_(esi), 5, WI8(0x22)); ++esi; }
    a = WU8(0x1f);
    if (a != 0xff && a != 3 && a != 4) {
        switch ((int8_t)a) { case 0: ebx = 2; break; case 1: ebx = 0; break; case 2: ebx = 1; break; default: break; }
        if (WU8(0x27) == 1) ebx += 3;
        inm(0x2e0012, 580.0f, Y_(esi), 5, ebx); ++esi;
    }
    if (WU8(0x23) != 0xff) { inm(0x2e0015, 580.0f, Y_(esi), 5, WI8(0x23)); ++esi; }
    if (WU8(0x20) != 0xff) inm(0x2e0013, 580.0f, Y_(esi), 5, WI8(0x20));
#undef Y_
}
static int palanca_4c0bd0(int arg)
{
    int ebp = 0, esi = eje_x, other = eje_x_ant, ebx = (int)umbral_menos, ecx0 = esi - other, eax = (int)umbral_mas, edi;
    int edx = (int)T->fa4; float x0; int step = 0;
    if (esi > ebx && esi < eax) {
        int n = I32(0x84a9f8) + 1;
        P32(0x84a9f8, (uint32_t)n);
        if (n > 0x32) { ebx = -40; eax = 40; umbral_menos = (uint32_t)-40; umbral_mas = 40; P32(0x84a9f8, 0); }
        edi = 0xffff; P32(0x84a9f4, (uint32_t)edi);
    } else {
        edi = I32(0x84a9f4);
        if (edi < 0xffff) { ++edi; P32(0x84a9f4, (uint32_t)edi); }
    }
    if (esi > edx) {
        if (ecx0 > 0) x0 = (float)(esi - edx);
        else { float x1 = suave_84a314; x0 = (float)(esi - edx); x0 = x0 - x1; x0 = x0 * T->fa0; x0 = x0 + x1; }
        suave_84a314 = x0;
        if (0.0f > x0) { x0 = 0.0f; suave_84a314 = x0; }          /* 5B4354 (SUPUESTO: 0) */
    } else if (esi >= -edx) {
        x0 = 0.0f; suave_84a314 = 0.0f;
    } else {
        x0 = (float)(edx + esi);
        if (ecx0 >= 0) { float x1 = suave_84a314; x0 = x0 - x1; x0 = x0 * T->fa0; x0 = x0 + x1; }
        suave_84a314 = x0;
        if (x0 > 0.0f) { x0 = 0.0f; suave_84a314 = x0; }          /* 5B4350 (SUPUESTO: 0) */
    }
    if (arg) {
        if (!(esi < 0x50) && edi > 0x28) step = 1;
        else if (!(esi > -0x50) && edi > 0x28) step = 1;
        else if (!(esi < eax) && edi > 0x50) step = 1;
        else if (esi > ebx) step = 0;
        else step = edi > 0x50;
    } else {
        int go = 0;
        if (esi > 0) { if ((float)esi > x0) go = 1; }
        else if (esi < 0) { if (x0 > (float)esi) go = 1; }
        if (go) {
            if (!(esi < 0x50) && edi > 0) step = 1;
            else if (!(esi > -0x50) && edi > 0) step = 1;
            else if (!(esi < eax) && edi > 0) step = 1;
            else if (esi > ebx) step = 0;
            else step = edi > 0;
        }
    }
    if (step) {
        P32(0x84a9f8, 0); P32(0x84a9f4, 0);
        if (esi > 0) { P32(0x84a2b0, 1); umbral_mas = 0x1e; umbral_menos = (uint32_t)-0x5a; return 1; }
        umbral_menos = (uint32_t)-0x1e; umbral_mas = 0x5a;
        ebp = -1;
    }
    P32(0x84a2b0, (uint32_t)ebp);
    return ebp;
}
static void lista_4c0eb0(uint32_t a1, uint32_t a2)
{
    int r0 = eje_x, esi = r0 < 0 ? -r0 : r0, edi, ebp, eax;
    uint8_t ral = U8(0x84a292), cl, al, bl, dl, B15;
    float x0, x1, x2, x3, x4, x5, x6, x7 = 0.0f, Lm4, Lm8, Lmc, Lm10, Lm14;
    WP8(0x18, 0);
    if (ral) { P8(0x84a292, (uint8_t)(ral - 1)); edi = 0; }
    else {
        edi = palanca_4c0bd0(0);
        if (edi == 0) {
            int skip = 0;
            if (a2 && eje(2) > 0x80) skip = 1;
            if (!skip && U8(0x84a2a4)) P8(0x84a2a4, 0);
        }
    }
    ebp = WI8(0x19);
    x5 = F32(0x84a274); x2 = F32(0x84a278); x1 = T->aa4; x3 = (float)ebp;
    x0 = x3 + 0.5f; x0 = x0 * x2; x1 = x1 + x5; x4 = x0 - x1; x1 = x1 + x0; Lm8 = x1; x1 = x0; x0 = x0 + x5; x6 = x0;
    x0 = F32(0x84a268); Lm4 = x4; x4 = T->aa0; x1 = x1 - x5; Lmc = x1; Lm14 = x6;
    if (edi > 0) {
        x7 = 0.0f;
        if (U8(0x84a28c)) {
            if (x7 >= x0) PF(0x84a26c, T->fc4);
            al = U8(0x84a290);
            if (al == 2) { x2 = F32(0x84a278); x1 = x4 * 2.0f; x2 = x2 - x1; x2 = x2 + x0; x0 = x2; PF(0x84a268, x0); }
            else if (al == 3 || al == 1) {
                if (casi_cero(x0)) { x1 = F32(0x84a270) + 1.0f; PF(0x84a270, x1); if (x1 >= x6) PF(0x84a270, Lmc); }
            } else { x0 = fila_4bef90(ebp + 1); PF(0x84a268, x0); }
            P8(0x84a28e, 0);
            if (x0 > x7) P8(0x84a28c, 0);
        }
        x2 = T->aa8; x1 = (float)esi;
        if (x2 > x1) PF(0x84a288, (float)(int8_t)T->fc0); else PF(0x84a288, (float)(int8_t)T->a9fc);
        x2 = F32(0x84a264);
    } else if (edi < 0) {
        x7 = 0.0f;
        if (U8(0x84a28c)) {
            if (x0 >= x7) PF(0x84a26c, T->fc4);
            al = U8(0x84a28f);
            if (al == 2) { x1 = x4 * 2.0f; x0 = x2; x0 = x0 - x1; x0 = x0 * -1.0f; PF(0x84a268, x0); }   /* 6280C4 = -1 (PS2 neg.s) */
            else if (al == 3 || al == 1) { f_4befe0(Lm14, Lmc); x0 = F32(0x84a268); x5 = F32(0x84a274); }
            else { x0 = fila_4bef90(ebp - 1); PF(0x84a268, x0); }
            P8(0x84a28d, 0);
            if (x7 > x0) P8(0x84a28c, 0);
        }
        x2 = T->aa8; x1 = (float)esi;
        if (x2 > x1) PF(0x84a288, (float)(int8_t)T->fc0); else PF(0x84a288, (float)(int8_t)T->a9fc);
        x2 = F32(0x84a264);
    } else {
        x7 = 1.0f; PF(0x84a26c, x7);
        if (casi_cero(x0)) { x2 = F32(0x84a270) + x7; PF(0x84a270, x2); if (x2 >= x6) PF(0x84a270, x1); }
        x7 = 0.0f;
        if (U8(0x84a28d)) { x2 = x6; x0 = x7; PF(0x84a264, x2); PF(0x84a270, x6); PF(0x84a268, x0); P8(0x84a28d, 0); }
        else x2 = F32(0x84a264);
        if (U8(0x84a28e)) { x2 = x6; x0 = x7; PF(0x84a264, x2); PF(0x84a270, x6); PF(0x84a268, x0); P8(0x84a28e, 0); }
    }
    al = U8(0x84a260);
    if (al == 0 || al == 4) x1 = F32(0x84a26c); else if (al == 1 || al == 3) x1 = T->fbc; else x1 = 1.0f;
    x3 = x3 * F32(0x84a278);
    Lm10 = x1; x1 = x2; x1 = x1 - x3;
    P8(0x84a261, al);
#define ZONA(out) do { \
        out = 2; \
        if (x4 > x1) { out = 0; break; } \
        x3 = x4 + T->aa4; \
        if (x3 > x1) { out = 1; x7 = 0.0f; break; } \
        x6 = T->aa4; x3 = x5 * 2.0f; x7 = x3 + x4; x7 = x7 + x6; \
        if (x7 >= x1) { out = 2; x6 = Lm14; x7 = 0.0f; break; } \
        x6 = x6 * 2.0f; x6 = x6 + x3; x6 = x6 + x4; \
        if (x6 >= x1) { out = 3; x6 = Lm14; x7 = 0.0f; break; } \
        x6 = x4 + T->aa4; x6 = x6 * 2.0f; x6 = x6 + x3; \
        if (x6 >= x1) out = 4; \
        x6 = Lm14; x7 = 0.0f; \
    } while (0)
    ZONA(cl);
    P8(0x84a260, cl);
    if (x0 > x7) x1 = x1 + Lm10; else x1 = x1 - Lm10;
    ZONA(B15);
#undef ZONA
    dl = U8(0x84a291);
    bl = WU8(0x19);
    eax = (int8_t)dl - 1;
    P8(0x84a290, ebp == eax ? 1 : 0);
    bl = bl == 0 ? 1 : 0;
    P8(0x84a28f, bl);
    if (x0 > x7) {
        int store = 0;
        bl = U8(0x84a28d);
        if (bl == 0) {
            x1 = Lm10; x0 = x0 - x1; PF(0x84a268, x0);
            if (x7 >= x0) { x0 = x0 + x1; x0 = x0 + x2; x2 = x0; x0 = x7; PF(0x84a268, x0); P8(0x84a28c, 1); }
            else x2 = x2 + x1;
            PF(0x84a264, x2);
        }
        x5 = Lmc;
        if (x2 >= x5 && x6 > x2) {
            int below;
            x1 = x6; x1 = x1 - x5; x0 = x0 - x1;
            below = !(x7 >= x0);
            x2 = x2 - x5; x2 = x2 + x6; PF(0x84a264, x2); PF(0x84a268, x0);
            if (!below) { x0 = x7; PF(0x84a268, x0); P8(0x84a28c, 1); }
        }
        if (U8(0x84a290) == 3) {
            x1 = F32(0x84a27c); x1 = x1 - x4;
            if (x2 > x1) { x2 = x2 - x1; x2 = x2 + x4; store = 1; }
        } else if (ebp != eax) {
            int go = 0;
            if (x0 >= F32(0x84a278) && cl == 1 && U8(0x84a261) == 0) go = 1;
            if (!go && I8(0x84a28d) > 0) go = 1;
            if (go) {
                x3 = Lm4;
                if (x2 >= x3) {
                    x1 = Lm8;
                    if (x1 > x2) {
                        al = (uint8_t)(U8(0x84a28d) + 1); P8(0x84a28d, al);
                        x5 = (float)(int8_t)al;
                        if (x5 > F32(0x84a288)) {
                            x2 = x2 - x3; x2 = x2 + x1; PF(0x84a264, x2);
                            x0 = fila_4bef90(ebp + 1); PF(0x84a268, x0); P8(0x84a28d, 0);
                        }
                        x5 = Lmc;
                    }
                }
            }
            x4 = x4 + T->aa4;
            x1 = F32(0x84a27c); x1 = x1 - x4;
            if (x2 >= x1) { x2 = x1; store = 1; }
        }
        if (store) PF(0x84a264, x2);
        al = U8(0x84a28d); bl = 0;
        if ((int8_t)al > 0) { x1 = F32(0x84a270) + Lm10; PF(0x84a270, x1); if (x1 >= x6) PF(0x84a270, x5); }
        else PF(0x84a270, x2);
        cl = 1;
        if (B15 == 1) P8(0x84a28c, cl);
    } else if (x7 > x0) {
        int store = 0;
        al = U8(0x84a28e);
        if (al == 0) {
            x1 = Lm10; x0 = x0 + x1; PF(0x84a268, x0);
            if (x0 >= x7) { x1 = x1 - x0; x0 = x7; PF(0x84a268, x0); P8(0x84a28c, 1); }
            x2 = x2 - x1; PF(0x84a264, x2);
        }
        x1 = Lmc;
        if (x2 >= x1 && x6 > x2) {
            int below;
            x3 = x6; x3 = x3 - x2; x2 = x1; x2 = x2 - x3; x3 = x6; x3 = x3 - x1; x3 = x3 + x0; x0 = x3;
            below = !(x0 >= x7);
            PF(0x84a264, x2); PF(0x84a268, x0);
            if (!below) { x0 = x7; PF(0x84a268, x0); P8(0x84a28c, 1); }
        }
        if (bl == 3) {
            if (x4 > x2) { x1 = x4; x1 = x1 - x2; x2 = F32(0x84a27c); x2 = x2 - x1; x2 = x2 - x4; store = 1; }
        } else {
            bl = WU8(0x19);
            if (bl) {
                int go = 0;
                x1 = x7; x1 = x1 - F32(0x84a278);
                if (x1 >= x0 && cl == 3 && U8(0x84a261) == 4) go = 1;
                if (!go && (int8_t)al > 0) go = 1;
                if (go && x2 >= Lm4) {
                    x1 = Lm8;
                    if (x1 > x2) {
                        al = (uint8_t)(al + 1);
                        x1 = (float)(int8_t)al;
                        P8(0x84a28e, al);
                        if (x1 > F32(0x84a288)) {
                            x0 = Lm8; x0 = x0 - x2; x2 = Lm4; x2 = x2 - x0; PF(0x84a264, x2);
                            x0 = fila_4bef90(ebp - 1); al = 0; PF(0x84a268, x0); P8(0x84a28e, 0);
                        }
                    }
                }
                x5 = x5 * 2.0f; x5 = x5 + x4; x5 = x5 + T->aa4; x1 = x5;
                if (x1 > x2) { x2 = x1; store = 1; }
            }
        }
        if (store) PF(0x84a264, x2);
        bl = 0;
        if ((int8_t)al > 0) {
            x1 = F32(0x84a270) - Lm10; x3 = Lmc; PF(0x84a270, x1);
            if (x3 > x1) { x6 = x6 - 1.0f; PF(0x84a270, x6); }
        } else PF(0x84a270, x2);
        cl = 1;
        if (B15 == 3) P8(0x84a28c, cl);
    } else { cl = 1; bl = 0; }
    x2 = x2 / F32(0x84a278);
    if (cvtt(x2) != ebp) {
        if (x0 > x7) {
            al = WU8(0x19); WP8(0x1a, al); al = (uint8_t)(al + 1); WP8(0x19, al);
            if (!((int8_t)al < (int8_t)dl)) {
                if (U8(0x84a290) == 3) { WP8(0x19, bl); WP8(0x18, cl); }
                else { dl = (uint8_t)(dl - 1); WP8(0x19, dl); WP8(0x18, cl); }
            } else { P8(0x84a28f, bl); WP8(0x18, cl); }
        } else if (x7 > x0) {
            al = WU8(0x19); WP8(0x1a, al); al = (uint8_t)(al - 1); WP8(0x19, al);
            if ((int8_t)al < 0) {
                if (U8(0x84a28f) == 3) { dl = (uint8_t)(dl - 1); WP8(0x19, dl); }
                else WP8(0x19, bl);
            } else P8(0x84a290, bl);
            WP8(0x18, 2);
        }
    }
    {   int h;
        if (U32(0x84a298) != U32(0x84a29c)) {
            suelta(0x8449c4);
            h = spr_crea_entera(U32(0x84a298), (int)U32(0x84a2a0), 2);
            P32(0x8449c4, (uint32_t)h); P32(0x84a29c, U32(0x84a298));
        } else h = H(0x8449c4);
        if (!a1) spr_pon_cuadro(h, cvtt(F32(0x84a280) + F32(0x84a270)));
        else { float v = F32(0x84a294) + F32(0x84a280); v = v + F32(0x84a270); spr_pon_cuadro(h, cvtt(v)); }
    }
}
/* 4C0340: el balance de la musica (0x10 sostenido 10 pasos; 84A21C 1 -> 3 -> 2 y +21) */
static void balance_4c0340(void)
{
    int edi;
    if (!U32(0x8449e8)) return;
    P32(0x84a238, 0); P8(0x84a2a4, 0);
    if (sostiene(0x10)) {
        int8_t n = I8(0x84a248);
        if (n < 0xa) { P8(0x84a248, (uint8_t)(n + 1)); return; }
        P32(0x84a238, 1); P8(0x84a2a4, 1);
        edi = repite_4bfac0();
        if (pulsa(0x400) || edi > 0) {
            switch (U32(0x84a21c)) {
            case 1: P32(0x84a21c, 3); WP8(0x21, 2); break;
            case 2: P32(0x84a21c, 1); WP8(0x21, 0); break;
            case 3: P32(0x84a21c, 2); WP8(0x21, 1); break;
            default: break;
            }
        }
        if (!pulsa(0x800) && edi >= 0) return;
        switch (U32(0x84a21c)) {
        case 1: WP8(0x21, 1); P32(0x84a21c, 2); return;
        case 2: P32(0x84a21c, 3); WP8(0x21, 2); return;
        case 3: P32(0x84a21c, 1); WP8(0x21, 0); return;
        default: return;
        }
    }
    {   int8_t n = I8(0x84a248);
        if (n > 0 && n < 0xa) {
            switch (U32(0x84a21c)) {
            case 1: P32(0x84a21c, 2); WP8(0x21, 1); break;
            case 2: P32(0x84a21c, 3); WP8(0x21, 2); break;
            case 3: P32(0x84a21c, 1); WP8(0x21, 0); break;
            default: break;
            }
        }
    }
    P8(0x84a248, 0);
}
static void fn9_init_4c04d0(void)
{
    P32(0x8449f4, 3);
    if (WU32(0x54) == 0) {
        P32(0x844950, 0);
        lista_4bee90(0x2e0028, 8, T->f70[3], T->f70[2], 740.0f, 7);
        WP8(8, WU8(8) & 0xfc); WP8(0xa, WU8(0xa) & 0xfc);
        WP16(0x10, 0x4b0); WP16(0x12, 0x3c);
        WP8(0x19, 0); WP8(0x1a, 0); WP8(0x48, 0); WP32(0x3c, 0); WP32(0x44, 0);
        WP8(0x49, 0); WP8(0x4a, 0); WP8(0x4b, 0);
        P32(0x844a00, T->mus[0][1]);
        WP32(0x54, 1);
    } else {
        WP8(8, WU8(8) & 0xfc); WP8(0xa, WU8(0xa) & 0xfc);
        P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
        WP32(0x3c, U32(0x84492c)); WP32(0x40, U32(0x844930));
        WP32(0x44, U32(0x844934)); WP32(0x48, U32(0x844938));
        WP8(0x19, U8(0x84493c)); WP8(0x1a, U8(0x84493d));
    }
    P32(0x8449e8, 0); P32(0x84a238, 0); P8(0x84a248, 0); P32(0x84a21c, 2);
}
static int musica_fsp = 0;
static void fn11_init_4c05f0(void)
{
    uint16_t f = WU16(8); int k, mu, p;
    if ((f & 3) == 3) return;
    WP16(8, (uint16_t)(f & 0xfffc));
    WP8(0xa, WU8(0xa) & 0xfc);
    k = WI8(0x1d);
    if (U32(0x844948) && colores_84a208 >= 0)
        WP8(0x1e, T->col[k][1 + (U32(0x844954) % (T->col[k][0] ? T->col[k][0] : 1))]);
    {   uint32_t kind = T->coche[k][0];
        if ((kind == 6 || kind == 5 || kind == 3 || kind == 9) && WU8(0x1e) == 4) WP8(0x1e, 5); }
    mu = WU8(0x20);
    if (mu == 7) p = rand() % 7;
    else if (mu == 0xf) p = rand() % 7 + 8;
    else p = (int8_t)mu;
    musica_fsp = (int)(T->mus[p & 15][0] & 0xff);
    if (WU8(0x1f) == 0xff) WP8(0x1f, 1);
}
static void disp6_4c1e80(void)
{
    int8_t bl = WI8(0x19);
    if (U32(0x844928)) {
        float f = (float)I32(0x844954), x1 = f * T->barra[3], x0 = f * T->barra[0] + T->barra[1], y = T->barra[2] - x1;
        inm(0x2e0000, x0, y, 0xa, 0);
        inm(T->rotulo[(bl & 0x7f) % 10], 0, 0, 9, 0);
    }
    panel_4c0a10();
}
static uint32_t cierra_4c2c60(void)
{
    int h = H(0x8449c4);
    if (h >= 0) {
        if (estado_h(h) != 3) return 0;
        P16(0x84494c, 1);
    }
    spr_suelta(h); suelta(0x8449cc);
    if ((WU8(8) & 3) == 2) WP8(0x15, (uint8_t)(WU8(0x15) + 1));
    else WP8(0x15, 8);
    WP8(0xa, WU8(0xa) | 1);
    secuencia_4c2ac0(WU8(0x15));
    P32(0x84a214, 0xc);
    return 1;
}
static void fn6_init_4c18c0(void)
{
    uint8_t a = WU8(0xa); uint32_t first = WU32(0x50);
    WP8(0x1f, 0xff); WP8(0x22, 0xff);
    P32(0x844950, 0); P32(0x844954, 0); P32(0x844948, 0); P32(0x844928, 0);
    colores_84a208 = -1; P8(0x84494e, 0); P32(0x84a2e0, 0); P32(0x84a2e4, 0);
    WP8(8, WU8(8) & 0xfc);
    P32(0x84a214, 0xc);
    WP8(0xa, a & 0xfc);
    WP16(0x10, 0x4b0); WP16(0x12, 0x3c);
    if (first == 1) lista_4bee90(T->tok19[0], 0xa, T->lx, T->ly, 0.0f, 7);
    WP8(0x1b, 1);
    musica_401000(0x1e);
    sonido(4);
}
static void lado_init(int flag_84)
{
    uint8_t v = eje_x > 0x64 ? 1 : 0;
    WP8(0x3e, v); WP8(0x19, v); WP8(0x1a, v);
    P8((uint32_t)flag_84, 0); P32(0x8449f4, 3);
}
static void fn7_init_4c1f10(void)
{
    WP8(8, WU8(8) & 0xfc); P32(0x844950, 0);
    WP8(0x1f, 0xff); WP8(0x22, 0xff);
    WP8(0xa, WU8(0xa) & 0xfc);
    WP16(0x10, 0x258); WP16(0x12, 0x3c);
    lado_init(0x844940);
}
static void fn3_init_4c2240(void)
{
    WP8(8, WU8(8) & 0xfc); P32(0x844950, 0);
    WP8(0xa, WU8(0xa) & 0xfc);
    WP16(0x10, 0x258); WP16(0x12, 0x3c);
    lado_init(0x84a2a8);
}
/* 4C1AC0: el color (0x10 sostenido 10 pasos entra en la lista de colores 844954 de la tabla 5C9204 del coche) y la
   lista de coches */
static void coche_4c1ac0(void)
{
    int k = WI8(0x19);
    if (sostiene(0x10)) {
        int8_t al = I8(0x84494e);
        if (al < 0xa) P8(0x84494e, (uint8_t)(al + 1));
        else {
            int esi, act = 0;
            P32(0x844948, 1); P32(0x844928, 1);
            colores_84a208 = k;
            P8(0x84a2a4, 1);
            esi = repite_4bfac0();
            if (pulsa(0x400) || pulsa(0x800) || esi != 0) act = 1;
            if (act) {
                int dec = pulsa(0x800) || esi < 0;
                int e = dec ? I32(0x844954) - 1 : I32(0x844954) + 1, n = (int)T->col[k][0];
                P32(0x844954, (uint32_t)e);
                if (e < 0) P32(0x844954, (uint32_t)(n - 1));
                else if (e >= n) P32(0x844954, 0);
            }
        }
    } else {
        int8_t al = I8(0x84494e);
        if (al > 0 && al < 0xa) {
            uint32_t e = U32(0x844954) + 1;
            P32(0x844954, e); P32(0x844948, 1);
            if (e >= T->col[k][0]) P32(0x844954, 0);
        }
        P32(0x844928, 0); P8(0x84494e, 0);
    }
    lista_4c0eb0(0, 1);
    if (WU8(0x18)) {
        int k2 = WI8(0x1a), s = WI8(0x18);
        suelta(0x84497c); suelta(0x844980); suelta(0x844984);
        P32(0x84497c, 0xffffffffu); P32(0x844980, 0xffffffffu); P32(0x844984, 0xffffffffu);
        if (s == 1 || s == 2) {
            int r = T->coche[(k2 & 0x7f) % 10][s == 1 ? 3 : 1], h;
            P32(0x844954, 0); colores_84a208 = WI8(0x19);
            suelta(0x8449cc);
            if (s == 1) h = crea(0x2e0017, 3, 1, T->fila[r][1], T->fila[r][2]);
            else h = crea(0x2e0017, 3, 1, T->fila[r][2], T->fila[r][1]);
            P32(0x8449cc, (uint32_t)h);
            spr_velocidad(h, s == 1 ? 1.0f : -1.0f);
        }
        sonido(1);
    }
    modelos_4bf260();
    if (!U32(0x84a2d8)) return;
    if (U32(0x84a2dc)) colores_84a208 = WI8(0x19);      /* (+12 del coche: lo lee demo/main.c con fsp_color) */
    if (WU8(0x18)) { P32(0x84a2dc, 0); P32(0x84a2e4, 1); return; }
    {   uint32_t st = U32(0x84a2e4);
        if (st == 1) { P32(0x84a2e4, 2); return; }
        if (st == 2) { P32(0x84a2e4, 3); return; }
        if (st != 3) return;
        if (U32(0x84a2e8 + T->ranura[(WI8(0x19) & 0x7f) % 10] * 4) != 2) return;
        colores_84a208 = WI8(0x19);
        if (!U32(0x84a218)) { P32(0x84a2dc, 1); P32(0x84a2e4, 0); return; }   /* 4BF340: la camara (sin portar) */
        (void)index_4bf370(0, 0);
        P32(0x84a2e4, 0);
    }
}
static void fn3_ctrl_4c3960(void)
{
    int edi = 0; int c = ta_84a240 < 0 ? 0 : ta_84a240; const uint32_t *rec = T->ta[0][c];
    if ((WU8(8) & 3) <= 1 && I32(0x8449a0) >= 0 && estado_h(H(0x8449a0)) == 3) {
        uint32_t a, b;
        suelta(0x8449a0);
        P32(0x8449a0, 0xffffffffu);
        if (WU8(0x3e) == 0) { a = rec[1]; b = rec[3]; } else { a = rec[2]; b = rec[4]; }
        if (U8(0x84a2a8) == 0) { P32(0x8449a0, crea_fila(a, 0xc, 1)); P8(0x84a2a8, 1); }
        else { P32(0x84499c, crea_fila(b, 0xc, 0)); P32(0x844958, 0); }
    }
    fondo_4c2430();
    if (U32(0x844958)) return;
    if ((WU8(8) & 3) > 1) {
        int h = H(0x8449a0); uint16_t f;
        if (h >= 0) { if (estado_h(h) != 3) return; }
        spr_suelta(h);
        f = WU16(8) & 3;
        WP8(0xa, WU8(0xa) | 1);
        if (f == 2) WP8(0x15, (uint8_t)(WU8(0x15) + 1));
        else if (f == 3) { WP8(0x23, 0xff); WP8(0x15, (uint8_t)(WU8(0x15) - 1)); }
        secuencia_4c2ac0(WU8(0x15));
        return;
    }
    WP8(0x18, 0);
    if (U32(0x8449a0) == 0xffffffffu) edi = palanca_4c0bd0(1);
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    if (!(WI16(0x10) > 0)) { uint16_t f = WU16(8); if (!(f & 3)) WP16(8, (uint16_t)((f & 0xfffd) | 1)); }
    if (!(pulsa(8) || r_4bfb20() || (WU8(8) & 3) == 1)) {
        int h, r;
        if (edi == -1) {
            uint8_t al;
            if (WU8(0x3e) == 0) return;
            al = WU8(0x19);
            WP8(0x3e, 0); WP8(0x1a, al); WP8(0x19, 0); WP8(0x18, 1);
            r = rec[5];
            suelta(0x84499c); suelta(0x8449a0);
            h = crea(T->fila[r][0], 0xc, 1, T->fila[r][2], T->fila[r][1]);
            P32(0x8449a0, (uint32_t)h); spr_velocidad(h, -3.0f);
        } else if (edi == 1) {
            uint8_t cl;
            if (WU8(0x3e) == 1) return;
            cl = WU8(0x19);
            WP8(0x3e, 1); WP8(0x1a, cl); WP8(0x19, 1); WP8(0x18, 1);
            r = rec[5];
            suelta(0x84499c); suelta(0x8449a0);
            h = crea(T->fila[r][0], 0xc, 1, T->fila[r][1], T->fila[r][2]);
            P32(0x8449a0, (uint32_t)h); spr_velocidad(h, 3.0f);
        } else return;
        sonido(0x164);
        return;
    }
    {   uint8_t al = WU8(0x3e); uint32_t kk;
        WP8(0x23, al);
        if (al == 0) { sonido_4bee60(T->son[0x48 / 4]); kk = rec[6]; }
        else { sonido_4bee60(T->son[0x4c / 4]); kk = rec[7]; }
        suelta(0x84499c);
        P32(0x84499c, 0xffffffffu); P32(0x8449a0, 0xffffffffu);
        P32(0x8449a0, crea_fila(kk, 0xc, 1));
        if (pulsa(8)) { WP8(8, WU8(8) | 3); WP8(0x23, 0xff); }
        else WP16(8, (uint16_t)((WU16(8) & 0xfffe) | 2));
        WP16(0x10, 0);
        sonido(0x40);
        P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
        {   int rr = WU32(0x58);
            suelta(0x8449cc); suelta(0x8449d8);
            if (rr >= 0 && rr < 564) {
                crea(WU32(0x5c), 4, 3, T->fila[rr][1], T->fila[rr][2]);
                crea(WU32(0x60), 9, 3, T->fila[rr][1], T->fila[rr][2]);
            }
        }
    }
}
static void fn7_ctrl_4c30b0(void)
{
    int esi = 0;
    if ((WU8(8) & 3) <= 1 && I32(0x844990) >= 0 && estado_h(H(0x844990)) == 3) {
        int a, b;
        suelta(0x844990);
        P32(0x844990, 0xffffffffu);
        if (WU8(0x3e) == 0) { a = 109; b = 111; } else { a = 110; b = 112; }   /* 5C768C / 5C76A4, 5C7698 / 5C76B0 */
        if (U8(0x844940) == 0) { P32(0x844990, crea_fila(a, 8, 1)); P8(0x844940, 1); }
        else { P32(0x84498c, crea_fila(b, 8, 0)); P32(0x844958, 0); }
    }
    fondo_4c2430();
    if (U32(0x844958)) return;
    if ((WU8(8) & 3) > 1) {
        int h = H(0x844990); uint8_t al;
        if (h >= 0) { if (estado_h(h) != 3) return; }
        spr_suelta(h);
        WP8(0xa, WU8(0xa) | 1);
        if ((WU8(8) & 3) == 2) al = (uint8_t)(WU8(0x15) + 2);   /* un jugador: +2 */
        else al = (uint8_t)(WU8(0x15) - 1);
        WP8(0x15, al);
        secuencia_4c2ac0(al);
        return;
    }
    WP8(0x18, 0);
    if (U32(0x844990) == 0xffffffffu) esi = palanca_4c0bd0(1);
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    if (!(WI16(0x10) > 0)) { uint16_t f = WU16(8); if (!(f & 3)) WP16(8, (uint16_t)((f & 0xfffd) | 1)); }
    if (!(pulsa(8) || r_4bfb20() || (WU8(8) & 3) == 1)) {
        int h;
        if (esi == -1) {
            uint8_t al;
            if (WU8(0x3e) == 0) return;
            al = WU8(0x19);
            WP8(0x3e, 0); WP8(0x1a, al); WP8(0x19, 0); WP8(0x18, 1);
            suelta(0x84498c); suelta(0x844990);
            h = crea(0x2e004f, 8, 1, 0x77, 0x5a);
            P32(0x844990, (uint32_t)h); spr_velocidad(h, -3.0f);
        } else if (esi == 1) {
            uint8_t al;
            if (WU8(0x3e) == 1) return;
            al = WU8(0x19);
            WP8(0x3e, 1); WP8(0x1a, al); WP8(0x19, 1); WP8(0x18, 1);
            suelta(0x84498c); suelta(0x844990);
            h = crea(0x2e004f, 8, 1, 0x5a, 0x77);
            P32(0x844990, (uint32_t)h); spr_velocidad(h, 3.0f);
        } else return;
        sonido(0x164);
        return;
    }
    WP8(0x22, WU8(0x3e));
    suelta(0x84498c);
    P32(0x84498c, 0xffffffffu); P32(0x844990, 0xffffffffu);
    if (!pulsa(8)) {
        int r;
        if (WU8(0x22) == 0) { sonido_4bee60(T->son[0x30 / 4]); r = 114; } else { sonido_4bee60(T->son[0x34 / 4]); r = 115; }   /* 5C76C8 / 5C76D4 */
        P32(0x844990, crea_fila(r, 8, 1));
    }
    if (pulsa(8)) { WP8(8, WU8(8) | 3); WP8(0x22, 0xff); }
    else WP16(8, (uint16_t)((WU16(8) & 0xfffe) | 2));
    WP16(0x10, 0);
    sonido(0x40);
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    {   int r = T->coche[(WI8(0x1d) & 0x7f) % 10][5];
        P16(0x84494c, 1);
        suelta(0x8449c4); suelta(0x844984);
        P32(0x8449c4, 0xffffffffu); P32(0x844984, 0xffffffffu);
        crea(T->tok1a[0], 4, 3, T->fila[r][1], T->fila[r][2]);
    }
}
static void fn6_ctrl_4c2d20(void)
{
    uint32_t st = U32(0x84a2e0); uint16_t ax, cx;
    if (st == 0) {                                      /* 4BF2E0: el coche lo carga demo/main.c (fsp_coche3d) */
        uint32_t a;
        for (a = 0x84a2e8; a <= 0x84a30c; a += 4) P32(a, 0);
        P32(0x84a2e0, 1);
        return;
    }
    if (st == 1) { P32(0x84a2e0, 2); return; }          /* 448980 / 4F21B0 */
    if (st == 2) {
        colores_84a208 = WI8(0x19);
        if (!U32(0x84a218)) P32(0x84a2dc, 1);           /* 4BF340 (camara: sin portar) */
        P32(0x84a2d8, 1);
        P32(0x84a2e0, 3);
        return;
    }
    if ((WU8(8) & 3) <= 1) f_4bf030();
    fondo_4c2430();
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    if (U32(0x844958)) return;
    inm(0x2e0005, 0, 0, 0xa, 0);                        /* (WU8(0x1b) <= 1: un jugador) */
    ax = WU16(8); cx = ax & 3;
    if (cx > 1) { cierra_4c2c60(); return; }
    if (!(WI16(0x10) > 0) && cx == 0) WP16(8, (uint16_t)((ax & 0xfffd) | 1));
    if (elige_4bfbd0() == 1) return;
    coche_4c1ac0();
}
static void fn5_ctrl_4c3dc0(void)
{
    uint32_t esi = WI8(0x3e) & 1, ebp = 0; uint16_t ax, cx;
#define CURSOR() (T->m50[U32(0x844944) & 1])
#define REC(i) (T->modo[esi][i])
    if (U32(0x8449e0) == 1) { P8(0x84a291, 5); P32(0x84a298, CURSOR()); }
    if ((WU8(8) & 3) <= 1 && I32(0x8449cc) >= 0 && estado_h(H(0x8449cc)) == 3) {
        f_4bf400();
        if (U32(0x844958)) {
            int y = cvtt(F32(0x84a284)), x = cvtt(F32(0x84a280));
            suelta(0x8449c4);
            P32(0x8449c4, crea(CURSOR(), 7, 2, x, y));
        }
        P32(0x844958, 0);
    }
    switch (WI8(0x19)) { case 0: case 1: case 2: P32(0x84a214, 0xc); break; case 3: case 4: P32(0x84a214, 0xb); break; default: break; }
    fondo_4c2430();
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    if (U32(0x844958)) return;
    ax = WU16(8); cx = ax & 3;
    if (cx > 1) {
        uint8_t st = WU8(0x41);
        if (st == 0) {
            int h = H(0x8449cc); uint8_t cl;
            if (h >= 0) { if (estado_h(h) != 3) return; }
            cl = WU8(0x19);
            P16(0x84494c, 1);
            if (cl == 0 || cl == 1 || cl == 3) { P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu); }
            cl = WU8(0x19);
            if (cl != 2 && cl != 4) {
                int r = REC(20);                        /* 689DF8 */
                spr_suelta(h); suelta(0x8449d8);
                P32(0x8449cc, 0xffffffffu); P32(0x8449d8, 0xffffffffu);
                P32(0x8449cc, crea(U32(0x84a24c + (esi + U32(0x844944) * 2) * 4), 4, 1, T->fila[r][1], T->fila[r][2]));
                P32(0x8449d8, crea(T->m58[esi], 9, 1, T->fila[r][1], T->fila[r][2]));
                WP8(0x41, WU8(0x41) + 1);
                return;
            }
            spr_suelta(h);
            P32(0x8449cc, 0xffffffffu);
            WP32(0x58, REC(20));
            WP32(0x5c, U32(0x84a24c + (esi + U32(0x844944) * 2) * 4));
            WP32(0x60, T->m58[esi]);
            {   int k = WI8(0x19) - 2, r = -1;
                if (k == 0) r = REC(7); else if (k == 2) r = REC(13);   /* 689DC4 / 689DDC */
                if (r >= 0) P32(0x8449cc, crea(U32(0x84a23c), 4, 0, T->fila[r][1], T->fila[r][2]));
            }
            WP8(0x41, WU8(0x41) + 1);
            return;
        }
        if (st != 1) return;
        {   uint8_t al = WU8(0x19); int h; int8_t v = WI8(0x3e); uint16_t f;
            if (al == 0 || al == 1 || al == 3) {
                h = H(0x8449cc);
                if (h >= 0) { if (estado_h(h) != 3) return; }
                spr_suelta(h); suelta(0x8449d8);
                h = -1; P32(0x8449cc, 0xffffffffu); P32(0x8449d8, 0xffffffffu);
            } else h = H(0x8449cc);
            switch (WI8(0x19)) {
            case 0: WP8(0x27, v == 1 ? 1 : 0); WP8(0x1f, 1); break;
            case 1: WP8(0x27, v == 1 ? 1 : 0); WP8(0x1f, 2); break;
            case 2:
                spr_suelta(h); suelta(0x8449d8); P32(0x8449cc, 0xffffffffu); P32(0x8449d8, 0xffffffffu);
                WP8(0x27, v == 1 ? 1 : 0); WP8(0x1f, 0); break;
            case 3: WP8(0x27, v == 1 ? 3 : 2); WP8(0x1f, 1); break;
            case 4:
                spr_suelta(h); suelta(0x8449d8); P32(0x8449cc, 0xffffffffu); P32(0x8449d8, 0xffffffffu);
                WP8(0x27, v == 1 ? 3 : 2); WP8(0x1f, 0); break;
            default: break;
            }
            WP8(0xa, WU8(0xa) | 1);
            f = WU16(8) & 3;
            if (f == 2) WP8(0x15, WU8(0x15) + 1);
            else if (f == 3) {
                al = (uint8_t)(WU8(0x15) - 1); WP8(0x15, al);
                if (seq_((int8_t)al) == 0xffffffffu) WP8(0x15, (uint8_t)(al - 1));
            }
            secuencia_4c2ac0(WU8(0x15));
            /* 43F940(+1F) / 43F950(+27) / 44DA00(preset): fsp_variante / fsp_preset al empezar la carrera */
        }
        return;
    }
    if (!(WI16(0x10) > 0) && cx == 0) WP16(8, (uint16_t)((ax & 0xfffd) | 1));
    if (pulsa(8) || r_4bfb20() || (WU8(8) & 3) == 1) {
        int snd = -1, s19 = WI8(0x19);
        WP8(0x23, 0xff);
        switch (s19) {
        case 0: ebp = REC(15); snd = T->son[0x3c / 4]; break;
        case 1: ebp = REC(16); snd = T->son[0x40 / 4]; break;
        case 2: snd = T->son[0x44 / 4]; ebp = REC(17); break;
        case 3: ebp = REC(18); snd = T->son[0x3c / 4]; break;
        case 4: ebp = REC(19); snd = T->son[0x44 / 4]; break;
        default: break;
        }
        suelta(0x8449cc); suelta(0x8449d8); suelta(0x8449c4);
        P32(0x8449cc, crea(U32(0x84a24c + (esi + U32(0x844944) * 2) * 4), 4, 1, T->fila[ebp][1], T->fila[ebp][2]));
        P32(0x8449d8, crea(T->m58[esi], 9, 1, T->fila[ebp][1], T->fila[ebp][2]));
        sonido(0x40);
        if (pulsa(8)) {
            WP8(8, WU8(8) | 3);
            WP8(0x22, 0xff); WP8(0x1f, 0xff); WP8(0x27, 0xff);
            WP16(0x10, 0);
            return;
        }
        if (snd >= 0) sonido_4bee60((uint32_t)snd);
        WP16(0x10, 0);
        WP16(8, (uint16_t)((WU16(8) & 0xfffe) | 2));
        return;
    }
    {   uint8_t busy = U8(0x84a293), al; uint32_t k;
        WP8(0x18, 0);
        if (busy == 0) {
            lista_4c0eb0((uint32_t)WI8(0x40), 0);
            if (pulsa(0x40000) && U8(0x84a292) == 0) {
                uint8_t cl = (uint8_t)(WU8(0x40) ^ 1), dl = (uint8_t)((cl == 0 ? 1 : 0) + 3);
                P8(0x84a292, 9); WP8(0x40, cl); WP8(0x18, dl);
            }
        } else if (I32(0x8449c4) >= 0 && estado_h(H(0x8449c4)) == 3) {
            int y = cvtt(F32(0x84a284)), x = cvtt(F32(0x84a280)), h;
            suelta(0x8449c4);
            h = crea(U32(0x84a298), 7, 2, x, y);
            P32(0x8449c4, (uint32_t)h);
            if (WU8(0x48) == 0) spr_pon_cuadro(h, cvtt(F32(0x84a270) + F32(0x84a280)));
            else { float v = F32(0x84a294) + F32(0x84a270); v = v + F32(0x84a280); spr_pon_cuadro(h, cvtt(v)); }
            f_4bf400();
            P8(0x84a293, 0);
        }
        al = WU8(0x18);
        if (al == 0) return;
        k = (uint32_t)(int8_t)al - 1;
        if (k > 3) return;
        if (k == 0 || k == 1) {
            uint32_t sel = (uint32_t)WI8(0x19); int h;
            suelta(0x8449cc);
            if (sel <= 4) ebp = REC(sel * 3 + (k == 0 ? 0 : 2));   /* 689DA8 / 689DB0 + sel x 0xC */
            if (k == 0) h = crea(U32(0x84a23c), 4, 1, T->fila[ebp][1], T->fila[ebp][2]);
            else h = crea(U32(0x84a23c), 4, 1, T->fila[ebp][2], T->fila[ebp][1]);
            P32(0x8449cc, (uint32_t)h);
            spr_velocidad(h, k == 0 ? 1.0f : -1.0f);
            sonido(1);
            return;
        }
        {   int right = k == 2; uint32_t a = right ? 0x456 : 0x473, b = right ? 0x473 : 0x456; float s = right ? 1.0f : -1.0f;
            WP8(0x3e, right ? 1 : 0);
            suelta(0x8449cc); suelta(0x8449d8); suelta(0x8449c4);
            P32(0x8449cc, crea(U32(0x84a23c), 4, 1, a, b));
            P32(0x8449d8, crea(0x2e0024, 9, 1, a, b));
            P32(0x8449c4, crea(CURSOR(), 7, 1, 0x456, 0x473));
            spr_velocidad(H(0x8449cc), s); spr_velocidad(H(0x8449d8), s);
            sonido(1);
            P8(0x84a293, 1);
        }
    }
#undef CURSOR
#undef REC
}
static void fn9_ctrl_4c4880(void)
{
#define MUS() (WU8(0x48) == 0 ? (int)WI8(0x19) : (int)WI8(0x19) + (int)I8(0x84a291))
#define COLA() do { balance_4c0340(); } while (0)
    if ((WU8(8) & 3) <= 1 && I32(0x8449cc) >= 0 && estado_h(H(0x8449cc)) == 3) {
        int h;
        suelta(0x8449cc);
        h = crea_fila(T->mus[MUS() & 15][3], 4, 0);
        P32(0x8449cc, (uint32_t)h); spr_velocidad(h, 1.0f);
        if (U32(0x844958)) {
            int y = cvtt(F32(0x84a284)), x = cvtt(F32(0x84a280));
            suelta(0x8449c4);
            P32(0x8449c4, crea(0x2e0028, 7, 2, x, y));
        }
        P32(0x8449e8, 1); P32(0x844958, 0);
    }
    P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu);
    pista_4bf970();
    if (U32(0x844958)) return;
    if ((WU8(8) & 3) > 1) {
        uint8_t st = WU8(0x4b), al;
        if (st == 0) {
            int h = H(0x8449c4);
            if (h >= 0) { if (estado_h(h) != 3) return; }
            P32(0x8449bc, 0xffffffffu); P32(0x8449c0, 0xffffffffu); P16(0x84494c, 1);
            spr_suelta(h);
            P32(0x8449c4, 0xffffffffu);
            P32(0x8449c4, crea_fila(WI8(0x19) >= 8 ? 0x178 : 0x16f, 4, 1));
            WP8(0x4b, WU8(0x4b) + 1);
            COLA();
            return;
        }
        if (st != 1) { COLA(); return; }
        {   int h = H(0x8449c4);
            if (h >= 0) { if (estado_h(h) != 3) return; spr_suelta(h); P32(0x8449c4, 0xffffffffu); }
        }
        suelta(0x8449cc); P32(0x8449cc, 0xffffffffu);
        WP8(0xa, WU8(0xa) | 1);
        al = WU8(0x15);
        if ((WU8(8) & 3) == 3) { WP8(0x20, 0xff); WP8(0x23, 0xff); al = (uint8_t)(al - 2); }
        else al = (uint8_t)(al + 2);
        WP8(0x15, al);
        P32(0x8449fc, 0x1e);
        musica_401000(0x1e);
        secuencia_4c2ac0(al);
        COLA();
        return;
    }
    fondo_4c2430();
    {   uint8_t busy = U8(0x84a293), al;
        WP8(0x18, 0);
        if (busy == 0) {
            lista_4c0eb0((uint32_t)WI8(0x4a), 1);
            if (pulsa(0x40000) && U8(0x84a292) == 0) {
                al = (uint8_t)(WU8(0x4a) ^ 1);
                P8(0x84a292, 9); WP8(0x4a, al);
                if (al == 0) { WP8(0x18, 4); WP8(0x48, 0); } else { WP8(0x18, 3); WP8(0x48, 1); }
            }
        } else if (I32(0x8449c4) >= 0 && estado_h(H(0x8449c4)) == 3) {
            int y = cvtt(F32(0x84a284)), x = cvtt(F32(0x84a280)), h;
            suelta(0x8449c4);
            h = crea(U32(0x84a298), 7, 2, x, y);
            P32(0x8449c4, (uint32_t)h);
            if (WU8(0x48) == 0) spr_pon_cuadro(h, cvtt(F32(0x84a270) + F32(0x84a280)));
            else { float v = F32(0x84a294) + F32(0x84a270); v = v + F32(0x84a280); spr_pon_cuadro(h, cvtt(v)); }
            P8(0x84a293, 0);
        }
        al = WU8(0x18);
        if (al) {
            uint32_t k = (uint32_t)(int8_t)al - 1;
            if (k <= 1) {
                int h;
                suelta(0x8449cc);
                h = crea_fila(T->mus[MUS() & 15][2], 4, 1);
                P32(0x8449cc, (uint32_t)h); spr_velocidad(h, 1.0f);
            } else if (k == 2 || k == 3) {
                uint32_t a = k == 2 ? 0x30c : 0x5f0, b = k == 2 ? 0x31f : 0x603;
                suelta(0x8449c4); suelta(0x8449cc);
                P32(0x8449c4, crea(U32(0x84a298), 7, 1, a, b));
                P32(0x8449cc, crea(0x2e0027, 4, 1, a, b));
                spr_velocidad(H(0x8449c4), 1.0f);
                if (k == 2) spr_velocidad(H(0x8449cc), 1.0f);
                P8(0x84a293, 1); P32(0x8449e8, 0);
            }
            P32(0x844a00, T->mus[MUS() & 15][1]);
            sonido(3);
        }
    }
    if (!(WI16(0x10) > 0)) { uint16_t f = WU16(8); if (!(f & 3)) WP16(8, (uint16_t)((f & 0xfffd) | 1)); }
    if (pulsa(8) || r_4bfb20() || (WU8(8) & 3) == 1) {
        uint8_t a = WU8(0x19); int p;
        suelta(0x8449c4); suelta(0x8449cc);
        P32(0x8449cc, 0xffffffffu);
        if (WU8(0x48) == 0) { WP8(0x20, a); p = (int8_t)a; }
        else { uint8_t cl = U8(0x84a291); WP8(0x20, (uint8_t)(a + cl)); p = (int8_t)a + (int8_t)cl; }
        P32(0x8449c4, crea_fila(T->mus[p & 15][4], 4, 1));
        P32(0x8449e8, 0);
        if (pulsa(8)) { WP8(8, WU8(8) | 3); WP32(0x54, 0); }
        else {
            sonido_4bee60(T->son[(0xac / 4 + WI8(0x20)) & 63]);
            WP16(8, (uint16_t)((WU16(8) & 0xfffe) | 2));
        }
        WP16(0x10, 0);
        sonido(0x40);
    }
    COLA();
#undef MUS
#undef COLA
}
static void disp9_4c22e0(void)
{
    if (U32(0x8449e8)) inm(0x2e0050, 0, 0, 0xa, U32(0x84a238) ? U32(0x84a21c) : 0);
    panel_4c0a10();
}

/* ---- el suceso 4 (440110 / 440200) y el modo 10 ---- */
static void fn_init(int f)
{
    switch (f) {
    case 1: fn1_init_4bfe00(); break;
    case 3: fn3_init_4c2240(); break;
    case 5: fn5_init_4c01e0(); break;
    case 6: fn6_init_4c18c0(); break;
    case 7: fn7_init_4c1f10(); break;
    case 9: fn9_init_4c04d0(); break;
    case 11: fn11_init_4c05f0(); break;
    case 12: fn12_init_4bfb90(); break;
    default: break;
    }
}
static void fn_ctrl(int f)
{
    switch (f) {
    case 1: fn1_ctrl_4c3510(); break;
    case 3: fn3_ctrl_4c3960(); break;
    case 5: fn5_ctrl_4c3dc0(); break;
    case 6: fn6_ctrl_4c2d20(); break;
    case 7: fn7_ctrl_4c30b0(); break;
    case 9: fn9_ctrl_4c4880(); break;
    case 11: fn11_ctrl_4bf500(); break;
    case 12: fn12_ctrl_4c2b70(); break;
    default: break;
    }
}
static void fn_disp(int f)
{
    switch (f) {
    case 3: case 7: panel_4c0a10(); break;
    case 5: panel_4c0a10(); break;
    case 6: disp6_4c1e80(); break;
    case 9: disp9_4c22e0(); break;
    default: break;
    }
}
static void fn_fin(int f)
{
    if (f == 6) fn6_dest_4bfdb0();
    if (f == 11) fn11_dest_4bf510();
}
static void pon_funcion(int f) { if (fn_actual) fn_fin(fn_actual); fn_actual = f; fn_init(f); }

void *(*fsp_hueco)(uint32_t tam);
static int res[8];                          /* lo elegido al salir (fsp_modelo.. despues de fsp_sale) */
#define HUECO_FSP (960 * 1024)              /* tablas de SPFE.DAT (la animacion del banco 0x2E: 744 KB) */
int fsp_entra(void)
{
    uint32_t tam; void *ram;
    fsp_sale();
    ram = fsp_hueco ? fsp_hueco(HUECO_FSP) : NULL;
    if (!(bk = spr_abre_en("SPFE.DAT", ram, ram ? HUECO_FSP : 0))) {
        dbglog(DBG_ERROR, "FSP: no se pudo abrir SPFE.DAT\n"); if (fsp_hueco) fsp_hueco(0); return -1;
    }
    T = (const tab_t *)spr_extra(bk, &tam);
    if (!T || tam < sizeof(tab_t) || memcmp(T->m, "FSP1", 4)) {
        dbglog(DBG_ERROR, "FSP: SPFE.DAT sin tablas\n"); spr_cierra(bk); bk = NULL; if (fsp_hueco) fsp_hueco(0); return -1;
    }
    memset(mm, 0, sizeof mm); memset(wk, 0, sizeof wk);
    umbral_mas = T->fb4; umbral_menos = T->fb8; suave_84a314 = 0; (void)fb_;
    pista_sonando = -1; colores_84a208 = -1; ta_84a240 = -1; musica_fsp = 0;
    spr_suelta_todas();
    /* 48B210 / 48AE90: estados 0..6 (bancos y coche del frontal) y el suceso 4 con la funcion [655B5C] = 1 */
    modo10 = 7; estado_ev = 2; activo = 1; salida = FSP_SIGUE; fn_actual = 0;
    pon_funcion(1);
    dbglog(DBG_INFO, "FSP: frontal de recreativa (SPFE.DAT, %u bytes de tablas), VRAM libre %uK\n", (unsigned)tam,
           (unsigned)(pvr_mem_available() >> 10));
    return 0;
}

int fsp_paso(unsigned nuevos)
{
    maple_device_t *d; cont_state_t *st; static unsigned ant;
    if (!activo) return FSP_SIGUE;
    bot_pulsados = 0; eje_x_ant = eje_x; eje_x = 0;
    if ((d = maple_enum_type(0, MAPLE_FUNC_CONTROLLER)) && (st = (cont_state_t *)maple_dev_status(d))) {
        bot_pulsados = st->buttons; eje_x = st->joyx;
        if (st->rtrig > 0x80) bot_pulsados |= 1u << 31;
        if (st->buttons & CONT_DPAD_RIGHT) eje_x = 127;
        if (st->buttons & CONT_DPAD_LEFT) eje_x = -128;
    }
    bot_nuevos = nuevos | (bot_pulsados & ~ant & (1u << 31));
    ant = bot_pulsados;
    /* 48AE90 estado 7: la funcion siguiente cuando su bandera +A bit 0 lo pide */
    if (WU8(0xa) & 1) {
        uint32_t f = seq_(WI8(0x15));
        WP32(4, f);
        pon_funcion((int)f + 1);
    }
    fn_ctrl(fn_actual);
    capa8_4bfa20();
    spr_paso();
    if (WU8(0xa) & 2) {                                 /* deja el suceso 4: modo 11 -> START (13) o 32 */
        int f = fn_actual;
        fn_fin(f); fn_actual = 0;
        if (salida == FSP_SIGUE) salida = FSP_CARRERA;
        dbglog(DBG_INFO, "FSP: %s (coche %d color %d, %s, variante %d, preset %d, musica %d, balance %d)\n",
               salida == FSP_CARRERA ? "carrera" : "al menu", fsp_modelo(), fsp_color(), fsp_manual() ? "MT" : "AT",
               fsp_variante(), fsp_preset(), fsp_musica(), fsp_balance());
        res[0] = fsp_modelo(); res[1] = fsp_color(); res[2] = fsp_manual(); res[3] = fsp_variante();
        res[4] = fsp_preset(); res[5] = fsp_musica(); res[6] = fsp_config(); res[7] = fsp_balance();
        activo = 0;
        return salida;
    }
    return FSP_SIGUE;
}

void fsp_dibuja(int tras_coche)
{
    if (!activo && !bk) return;
    fn_disp(fn_actual);
    if (tras_coche) { spr_dibuja(0, 8, 0.0002f, 1); spr_dibuja(8, SPR_CAPAS, 50.0f, 0); }
    else spr_dibuja(0, SPR_CAPAS, 50.0f, 0);
}

void fsp_sale(void)
{
    if (!bk) return;
    spr_suelta_todas();
    spr_cierra(bk); bk = NULL; T = NULL; activo = 0;
    if (fsp_hueco) fsp_hueco(0);
}
int fsp_activo(void) { return activo; }
int fsp_coche3d(void) { return activo && U32(0x84a318) == 2; }
int fsp_modelo(void) { if (!activo) return res[0]; return T ? (int)T->coche[(WI8(fn_actual == 6 ? 0x19 : 0x1d) & 0x7f) % 10][0] : 0; }
int fsp_color(void)
{
    int k;
    if (!activo) return res[1];
    if (!T) return 1;
    k = (WI8(fn_actual == 6 ? 0x19 : 0x1d) & 0x7f) % 10;
    if (fn_actual == 6 && U32(0x844948)) return (int)T->col[k][1 + U32(0x844954) % (T->col[k][0] ? T->col[k][0] : 1)];
    return WU8(0x1e) ? WU8(0x1e) : 1;
}
int fsp_manual(void) { return activo ? WU8(0x22) == 1 : res[2]; }
int fsp_variante(void) { return activo ? (WU8(0x1f) == 0xff ? 1 : WU8(0x1f)) : res[3]; }
int fsp_preset(void) { return activo ? (WU8(0x27) == 0xff ? 0 : WU8(0x27)) : res[4]; }
int fsp_musica(void) { return activo ? musica_fsp : res[5]; }
int fsp_config(void) { return activo ? (WU8(0x23) == 0xff ? -1 : WU8(0x23)) : res[6]; }
int fsp_balance(void) { return activo ? WU8(0x21) : res[7]; }
