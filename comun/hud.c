/* HUD original (HUD.DAT, ver tools/conv_hud.py): carga y dibujo con la PowerVR a 640x480 */
#include <kos.h>
#include <math.h>
#include <string.h>
#include "gd.h"
#include "hud.h"

typedef struct { uint16_t w, h, fmt, lista; uint32_t off, tam; } h_tex_t;
typedef struct { uint16_t tex, w, h, _r; float u0, v0, u1, v1; } h_spr_t;
typedef struct { char nombre[16]; uint16_t n, pri, np, _r; float paso; uint32_t _r2; } h_grp_t;
typedef struct { uint16_t grp, cuadro, spr, flags; float x, y, ax, ay, ang, sx, sy, alfa; } h_pie_t;
typedef struct { char nombre[16]; uint16_t grp, alt; int16_t x, y, avance, capa; uint16_t nchar, _r; } h_uso_t;

static uint8_t *dat;
static h_tex_t *tex; static h_spr_t *spr; static h_grp_t *grp; static h_pie_t *pie; static h_uso_t *uso;
static uint32_t n_tex, n_grp, n_uso;
static pvr_ptr_t *vtex;
static pvr_poly_hdr_t *cab;                                     /* una cabecera por textura (TR, modula alfa) */
static int taco = -1, ult_tex = -1, g_fuera = -1;           /* g_fuera: DIG_PUNTOS_FUERA (estilo 0xD, sin uso propio) */
static float zcapa;

static int busca_uso(const char *n)
{
    uint32_t i;
    for (i = 0; i < n_uso; i++) if (!strncmp(uso[i].nombre, n, 16)) return i;
    return -1;
}

/* grupos que se usan: todos menos los tacometros que no son el del coche */
static int grupo_util(int g)
{
    return strncmp(grp[g].nombre, "TACO_", 5) || g == taco;
}

int hud_carga(const char *fich, int modelo)
{
    uint32_t sz, *c, i, j; uint8_t *usada;
    if (!(dat = gd_carga(fich, &sz)) || memcmp(dat, "HUD1", 4)) return -1;
    c = (uint32_t *)dat;
    n_tex = c[1]; n_grp = c[3]; n_uso = c[5];
    tex = (h_tex_t *)(dat + c[6]); spr = (h_spr_t *)(dat + c[7]); grp = (h_grp_t *)(dat + c[8]);
    pie = (h_pie_t *)(dat + c[9]); uso = (h_uso_t *)(dat + c[10]);
    taco = dat[c[11] + (modelo & 15)]; if (taco == 0xff) taco = -1;
    for (i = 0, g_fuera = -1; i < n_grp; i++) if (!strncmp(grp[i].nombre, "DIG_PUNTOS_FUERA", 16)) g_fuera = i;
    vtex = calloc(n_tex, sizeof *vtex); cab = calloc(n_tex, sizeof *cab); usada = calloc(n_tex, 1);
    for (i = 0; i < n_grp; i++)
        if (grupo_util(i)) for (j = grp[i].pri; j < (uint32_t)grp[i].pri + grp[i].np; j++) usada[spr[pie[j].spr].tex] = 1;
    for (i = 0; i < n_tex; i++) {
        static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
        pvr_poly_cxt_t cx;
        if (!usada[i]) continue;
        if (!(vtex[i] = pvr_mem_malloc(tex[i].tam))) { free(usada); return -2; }
        pvr_txr_load(dat + tex[i].off, vtex[i], tex[i].tam);
        pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, FMT[tex[i].fmt] | PVR_TXRFMT_TWIDDLED, tex[i].w, tex[i].h, vtex[i], PVR_FILTER_BILINEAR);
        cx.txr.env = PVR_TXRENV_MODULATEALPHA; cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; cx.depth.write = PVR_DEPTHWRITE_DISABLE;
        pvr_poly_compile(cab + i, &cx);
    }
    free(usada);
    return 0;
}

static void pieza(const h_pie_t *p, const h_grp_t *g, int cuadro, float X, float Y, float alfa)
{
    const h_spr_t *s = spr + p->spr;
    float ang = p->ang, c, si, px[4], py[4], a = p->alfa * alfa; int k; pvr_vertex_t v;
    if (a <= 0.004f || !vtex[s->tex]) return;
    if (p->flags & 1) ang += g->paso * cuadro;
    ang *= (float)M_PI / 180.0f; c = cosf(ang); si = sinf(ang);
    for (k = 0; k < 4; k++) {
        float qx = ((k & 1) ? s->w : 0) - p->ax, qy = ((k & 2) ? s->h : 0) - p->ay;
        qx *= p->sx; qy *= p->sy;
        px[k] = qx * c - qy * si + p->x + X; py[k] = qx * si + qy * c + p->y + Y;
    }
    if (s->tex != ult_tex) { pvr_prim(cab + s->tex, sizeof(pvr_poly_hdr_t)); ult_tex = s->tex; }
    if (a > 1) a = 1;
    v.argb = ((uint32_t)(a * 255) << 24) | 0xFFFFFF; v.oargb = 0; v.z = zcapa; zcapa += 0.001f;
    for (k = 0; k < 4; k++) {
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = px[k]; v.y = py[k]; v.u = (k & 1) ? s->u1 : s->u0; v.v = (k & 2) ? s->v1 : s->v0;
        pvr_prim(&v, sizeof v);
    }
}

static void grupo(int g, int cuadro, float X, float Y, float alfa)
{
    const h_grp_t *G = grp + g; int j;
    for (j = G->pri; j < G->pri + G->np; j++)
        if (G->n <= 1 || pie[j].cuadro == cuadro) pieza(pie + j, G, cuadro, X, Y, alfa);
}

static void dibuja(const char *nombre, int alt, int cuadro, float alfa)
{
    int u = busca_uso(nombre); int g;
    if (u < 0) return;
    g = alt && uso[u].alt != 0xffff ? uso[u].alt : uso[u].grp;
    grupo(g, cuadro, uso[u].x, uso[u].y, alfa);
}

/* cifras de un uso con el grupo g en (x, y): un caracter por cifra (' ' solo avanza) */
static void cifras(int u, int g, const char *txt, float x, float y, float alfa)
{
    int i;
    for (i = 0; txt[i]; i++)
        if (txt[i] >= '0' && txt[i] <= '9') grupo(g, txt[i] - '0', x + i * uso[u].avance, y, alfa);
}
/* numero: en x (o en la del uso si x < -9000) */
static void numero(const char *nombre, int alt, const char *txt, float x)
{
    int u = busca_uso(nombre), g;
    if (u < 0) return;
    g = alt && uso[u].alt != 0xffff ? uso[u].alt : uso[u].grp;
    if (x < -9000) x = uso[u].x;
    cifras(u, g, txt, x, uso[u].y, 1);
}
/* grupo de las cifras de la puntuacion por estilo (0 normal, 1 rebufo = alternativo, 2 fuera) */
static int grupo_puntos(int u, int estilo)
{
    if (estilo == 2 && g_fuera >= 0) return g_fuera;
    if (estilo == 1 && uso[u].alt != 0xffff) return uso[u].alt;
    return uso[u].grp;
}

/* dentro de la lista TR, despues de todo lo demas (z por encima de la escena) */
void hud_dibuja(const hud_estado_t *e)
{
    char b[16]; int t, cs, m, cuadro;
    if (!dat) return;
    ult_tex = -1; zcapa = 20.0f;
    /* capa 2: mapa de ruta y tacometro */
    dibuja("RUTA5_FONDO", 0, 0, 1);
    if (taco >= 0) {
        cuadro = (int)(e->rpm * 180.0f / 10000.0f); if (cuadro < 0) cuadro = 0; if (cuadro > 179) cuadro = 179;
        grupo(taco, cuadro, -219, 127, 1);
    }
    zcapa = 30.0f;
    /* capa 3: paneles y etapas */
    dibuja("PANEL_TIEMPO", 0, 0, 1); dibuja("PANEL_PUNTOS", 0, 0, 1);
    for (t = 0; t < e->etapa; t++) dibuja("RUTA5_HECHA", 0, t, 1);
    dibuja("RUTA5_ACTUAL", 0, e->etapa, (e->cuadro & 32) ? 1.0f : 0.4f);
    zcapa = 40.0f;
    /* capa 4: REV (solo con cambio manual) */
    if (e->manual) { dibuja("REV_TEXTO", 0, 0, 1); dibuja("REV_LUZ", 0, 0, 1); if (e->rev_on && (e->cuadro & 8)) dibuja("REV_ON", 0, 0, 1); }
    zcapa = 50.0f;
    /* capa 5: unidades y rebufo */
    dibuja("KMH", e->rebufo, 0, 1);
    if (e->rebufo && (e->cuadro % 30) <= 20) dibuja("REBUFO", 0, 0, 1);
    zcapa = 80.0f;
    dibuja("CAJA", e->seis, 0, 1);
    zcapa = 90.0f;
    m = e->marcha; if (m < 1) m = 1; if (m > 6) m = 6;
    dibuja("MARCHA", 0, m, 1);
    /* capa 9: numeros */
    t = e->tiempo < 0 ? 0 : e->tiempo; if (t > 999) t = 999;
    if (t >= 100) { sprintf(b, "%03d", t); numero("TIEMPO", 0, b, 276); }
    else { sprintf(b, "%02d", t); numero("TIEMPO", 0, b, -9999); }
    cs = e->crono_ms; if (cs > 99 * 60000 + 59999) cs = 99 * 60000 + 59999;
    sprintf(b, "%2d %02d %03d", cs / 60000, (cs / 1000) % 60, cs % 1000); numero("CRONO", 0, b, -9999);
    t = busca_uso("PUNTOS");                                    /* 4BD480: "%07d", 9999999 como mucho */
    if (t >= 0) { sprintf(b, "%07d", e->puntos > 9999999 ? 9999999 : e->puntos < 0 ? 0 : e->puntos);
                  cifras(t, grupo_puntos(t, e->puntos_estilo), b, uso[t].x, uso[t].y, 1); }
    t = e->kmh < 0 ? 0 : e->kmh > 999 ? 999 : e->kmh;
    sprintf(b, "%03d", t); numero("VELOCIDAD", e->rebufo, b, -9999);
}

/* avisos animados (cuenta atras, GOAL, EXTEND TIME...): llamar despues de hud_dibuja; dibuja el cuadro dado del uso */
void hud_aviso(const char *nombre, int cuadro, float alfa)
{
    if (!dat) return;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1;
    dibuja(nombre, 0, cuadro, alfa);
}

/* 4BD480: cada mensaje de la cola en (0x1E0, y + 0x49) con "%7d" */
void hud_puntos_msg(int puntos, int estilo, float dy, float alfa)
{
    char b[16]; int u;
    if (!dat || (u = busca_uso("PUNTOS")) < 0 || alfa <= 0.004f) return;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1;
    sprintf(b, "%7d", puntos > 9999999 ? 9999999 : puntos < 0 ? 0 : puntos);
    cifras(u, grupo_puntos(u, estilo), b, uso[u].x - 1, uso[u].y + dy, alfa);
}
