/* HUD original (HUD.DAT, ver tools/conv_hud.py): carga y dibujo con la PowerVR a 640x480 */
#include <kos.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include "gd.h"
#include "hud.h"

typedef struct { uint16_t w, h, fmt, lista; uint32_t off, tam; } h_tex_t;
typedef struct { uint16_t tex, w, h, _r; float u0, v0, u1, v1; } h_spr_t;
typedef struct { char nombre[16]; uint16_t n, pri, np, _r; float paso; uint32_t _r2; } h_grp_t;
typedef struct { uint16_t grp, cuadro, spr, flags; float x, y, ax, ay, ang, sx, sy, alfa; } h_pie_t;
typedef struct { char nombre[16]; uint16_t grp, alt; int16_t x, y, avance, capa; uint16_t nchar, _r; } h_uso_t;

/* un fichero con formato HUD1 (HUD.DAT, MENU.DAT) */
struct hud_dat {
    uint8_t *dat; uint32_t meta, lba;                       /* meta: bytes en la RAM (parcial) o 0 (fichero entero) */
    h_tex_t *tex; h_spr_t *spr; h_grp_t *grp; h_pie_t *pie; h_uso_t *uso;
    uint32_t n_tex, n_grp, n_uso;
    pvr_ptr_t *vtex; uint8_t *usada;
    pvr_poly_hdr_t *cab, *cabf;                             /* por textura: encima de todo / detras de la escena 3D */
    int taco, g_fuera, off_taco;                            /* g_fuera: DIG_PUNTOS_FUERA (estilo 0xD, sin uso propio) */
};

static hud_dat_t *H, *hud_principal;                       /* H: el fichero que se esta dibujando */
static int ult_tex = -1, modo_fondo;
static float zcapa, zpaso = 0.001f;

static int busca_uso(const char *n)
{
    uint32_t i;
    for (i = 0; i < H->n_uso; i++) if (!strncmp(H->uso[i].nombre, n, 16)) return i;
    return -1;
}

/* grupos que se usan: todos menos los tacometros que no son el del coche */
static int grupo_util(int g)
{
    return strncmp(H->grp[g].nombre, "TACO_", 5) || g == H->taco;
}

static int lee_sect(uint32_t lba, uint32_t n, uint8_t *b)
{
    uint32_t hecho = 0;
    while (hecho < n) {                                     /* de 64 en 64 sectores, como gd_carga */
        int k = n - hecho > 64 ? 64 : (int)(n - hecho);
        if (gd_lee(lba + hecho, k, b + hecho * 2048)) return -1;
        hecho += k;
    }
    return 0;
}

hud_dat_t *hudd_abre(const char *fich, int modelo, int parcial)
{
    uint32_t sz, *c, i, j; hud_dat_t *h = calloc(1, sizeof *h);
    if (!h) return NULL;
    if (!parcial) {
        if (!(h->dat = gd_carga(fich, &sz))) { free(h); return NULL; }
    } else {                                                /* cabecera y tabla de texturas; luego hasta la 1a textura */
        uint8_t *b = memalign(32, 2048); uint32_t fin, ns, m = 0xffffffffu;
        if (!b || gd_busca(fich, &h->lba, &sz) || lee_sect(h->lba, 1, b)) { free(b); free(h); return NULL; }
        c = (uint32_t *)b; fin = c[6] + 16 * c[1]; ns = (fin + 2047) / 2048;
        if (ns > 1) { free(b); b = memalign(32, ns * 2048); if (!b || lee_sect(h->lba, ns, b)) { free(b); free(h); return NULL; } }
        c = (uint32_t *)b;
        for (i = 0; i < c[1]; i++) { h_tex_t *t = (h_tex_t *)(b + c[6]) + i; if (t->off < m) m = t->off; }
        free(b);
        h->meta = m; ns = (m + 2047) / 2048;
        if (!(h->dat = memalign(32, ns * 2048)) || lee_sect(h->lba, ns, h->dat)) { free(h->dat); free(h); return NULL; }
    }
    if (memcmp(h->dat, "HUD1", 4)) { free(h->dat); free(h); return NULL; }
    c = (uint32_t *)h->dat;
    h->n_tex = c[1]; h->n_grp = c[3]; h->n_uso = c[5];
    h->tex = (h_tex_t *)(h->dat + c[6]); h->spr = (h_spr_t *)(h->dat + c[7]); h->grp = (h_grp_t *)(h->dat + c[8]);
    h->pie = (h_pie_t *)(h->dat + c[9]); h->uso = (h_uso_t *)(h->dat + c[10]); h->off_taco = c[11];
    h->taco = h->dat[c[11] + (modelo & 15)]; if (h->taco == 0xff) h->taco = -1;
    H = h;
    for (i = 0, h->g_fuera = -1; i < h->n_grp; i++) if (!strncmp(h->grp[i].nombre, "DIG_PUNTOS_FUERA", 16)) h->g_fuera = i;
    h->vtex = calloc(h->n_tex, sizeof *h->vtex); h->usada = calloc(h->n_tex, 1);
    h->cab = memalign(32, h->n_tex * sizeof *h->cab); h->cabf = memalign(32, h->n_tex * sizeof *h->cabf);
    for (i = 0; i < h->n_grp; i++)
        if (grupo_util(i)) for (j = h->grp[i].pri; j < (uint32_t)h->grp[i].pri + h->grp[i].np; j++) h->usada[h->spr[h->pie[j].spr].tex] = 1;
    return h;
}

static void cabeceras(hud_dat_t *h, int i)
{
    static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
    pvr_poly_cxt_t cx;
    /* formato: bits 0-7 el de 16 bits; bit 8 VQ (libro de 256 entradas al principio de los datos) */
    uint32_t f = FMT[(h->tex[i].fmt & 0xff) < 3 ? (h->tex[i].fmt & 0xff) : 2] | PVR_TXRFMT_TWIDDLED;
    if (h->tex[i].fmt & 0x100) f |= PVR_TXRFMT_VQ_ENABLE;
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, f, h->tex[i].w, h->tex[i].h, h->vtex[i], PVR_FILTER_BILINEAR);
    cx.txr.env = PVR_TXRENV_MODULATEALPHA; cx.depth.write = PVR_DEPTHWRITE_DISABLE;
    cx.depth.comparison = PVR_DEPTHCMP_GREATER; pvr_poly_compile(h->cabf + i, &cx);
    cx.depth.comparison = PVR_DEPTHCMP_ALWAYS; pvr_poly_compile(h->cab + i, &cx);
}

int hudd_sube(hud_dat_t *h, int t0, int nt)
{
    int i, fin; uint32_t lo = 0xffffffffu, hi = 0, s0 = 0, ns; uint8_t *b = NULL;
    if (!h) return -1;
    if (nt < 0) nt = h->n_tex - t0;
    fin = t0 + nt; if (fin > (int)h->n_tex) fin = h->n_tex;
    for (i = t0; i < fin; i++) if (h->usada[i] && !h->vtex[i]) {
        if (h->tex[i].off < lo) lo = h->tex[i].off;
        if (h->tex[i].off + h->tex[i].tam > hi) hi = h->tex[i].off + h->tex[i].tam;
    }
    if (hi <= lo) return 0;
    if (h->meta) {                                          /* del GD: los sectores que cubren esas texturas */
        s0 = lo / 2048; ns = (hi + 2047) / 2048 - s0;
        if (!(b = memalign(32, ns * 2048))) return -3;
        if (lee_sect(h->lba + s0, ns, b)) { free(b); return -4; }
    }
    for (i = t0; i < fin; i++) {
        if (!h->usada[i] || h->vtex[i]) continue;
        if (!(h->vtex[i] = pvr_mem_malloc(h->tex[i].tam))) { free(b); hudd_baja(h, t0, nt); return -2; }
        pvr_txr_load(h->meta ? b + (h->tex[i].off - s0 * 2048) : h->dat + h->tex[i].off, h->vtex[i], h->tex[i].tam);
        cabeceras(h, i);
    }
    free(b);
    return 0;
}

void hudd_baja(hud_dat_t *h, int t0, int nt)
{
    int i, fin;
    if (!h) return;
    if (nt < 0) nt = h->n_tex - t0;
    fin = t0 + nt; if (fin > (int)h->n_tex) fin = h->n_tex;
    for (i = t0; i < fin; i++) if (h->vtex[i]) { pvr_mem_free(h->vtex[i]); h->vtex[i] = NULL; }
}

int hud_carga(const char *fich, int modelo)
{
    if (!(hud_principal = hudd_abre(fich, modelo, 0))) return -1;
    return hudd_sube(hud_principal, 0, -1) ? -2 : 0;
}
void hud_baja_tex(void) { hudd_baja(hud_principal, 0, -1); }
void hud_pon_modelo(int modelo)
{
    hud_dat_t *h = hud_principal; uint32_t i, j;
    if (!h) return;
    H = h; h->taco = h->dat[h->off_taco + (modelo & 15)]; if (h->taco == 0xff) h->taco = -1;
    memset(h->usada, 0, h->n_tex);                          /* texturas que se suben: las del tacometro nuevo */
    for (i = 0; i < h->n_grp; i++)
        if (grupo_util(i)) for (j = h->grp[i].pri; j < (uint32_t)h->grp[i].pri + h->grp[i].np; j++) h->usada[h->spr[h->pie[j].spr].tex] = 1;
}
int hud_sube_tex(void) { return hud_principal ? hudd_sube(hud_principal, 0, -1) : -1; }

static void pieza(const h_pie_t *p, const h_grp_t *g, int cuadro, float X, float Y, float alfa)
{
    const h_spr_t *s = H->spr + p->spr;
    float ang = p->ang, c, si, px[4], py[4], a = p->alfa * alfa; int k; pvr_vertex_t v;
    if (a <= 0.004f || !H->vtex[s->tex]) return;
    if (p->flags & 1) ang += g->paso * cuadro;
    ang *= (float)M_PI / 180.0f; c = cosf(ang); si = sinf(ang);
    for (k = 0; k < 4; k++) {
        float qx = ((k & 1) ? s->w : 0) - p->ax, qy = ((k & 2) ? s->h : 0) - p->ay;
        qx *= p->sx; qy *= p->sy;
        px[k] = qx * c - qy * si + p->x + X; py[k] = qx * si + qy * c + p->y + Y;
    }
    if (s->tex != ult_tex) { pvr_prim((modo_fondo ? H->cabf : H->cab) + s->tex, sizeof(pvr_poly_hdr_t)); ult_tex = s->tex; }
    if (a > 1) a = 1;
    v.argb = ((uint32_t)(a * 255) << 24) | 0xFFFFFF; v.oargb = 0; v.z = zcapa; zcapa += zpaso;
    for (k = 0; k < 4; k++) {
        v.flags = k == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = px[k]; v.y = py[k]; v.u = (k & 1) ? s->u1 : s->u0; v.v = (k & 2) ? s->v1 : s->v0;
        pvr_prim(&v, sizeof v);
    }
}

static void grupo(int g, int cuadro, float X, float Y, float alfa)
{
    const h_grp_t *G = H->grp + g; int j;
    for (j = G->pri; j < G->pri + G->np; j++)
        if (G->n <= 1 || H->pie[j].cuadro == cuadro) pieza(H->pie + j, G, cuadro, X, Y, alfa);
}

static void dibuja(const char *nombre, int alt, int cuadro, float alfa)
{
    int u = busca_uso(nombre); int g;
    if (u < 0) return;
    g = alt && H->uso[u].alt != 0xffff ? H->uso[u].alt : H->uso[u].grp;
    grupo(g, cuadro, H->uso[u].x, H->uso[u].y, alfa);
}

/* cifras de un uso con el grupo g en (x, y): un caracter por cifra (' ' solo avanza) */
static void cifras(int u, int g, const char *txt, float x, float y, float alfa)
{
    int i;
    for (i = 0; txt[i]; i++)
        if (txt[i] >= '0' && txt[i] <= '9') grupo(g, txt[i] - '0', x + i * H->uso[u].avance, y, alfa);
}
/* numero: en x (o en la del uso si x < -9000) */
static void numero(const char *nombre, int alt, const char *txt, float x)
{
    int u = busca_uso(nombre), g;
    if (u < 0) return;
    g = alt && H->uso[u].alt != 0xffff ? H->uso[u].alt : H->uso[u].grp;
    if (x < -9000) x = H->uso[u].x;
    cifras(u, g, txt, x, H->uso[u].y, 1);
}
/* grupo de las cifras de la puntuacion por estilo (0 normal, 1 rebufo = alternativo, 2 fuera) */
static int grupo_puntos(int u, int estilo)
{
    if (estilo == 2 && H->g_fuera >= 0) return H->g_fuera;
    if (estilo == 1 && H->uso[u].alt != 0xffff) return H->uso[u].alt;
    return H->uso[u].grp;
}

/* ---- acceso generico */
int hudd_uso(const hud_dat_t *h, const char *nombre)
{
    uint32_t i;
    if (!h) return -1;
    for (i = 0; i < h->n_uso; i++) if (!strncmp(h->uso[i].nombre, nombre, 16)) return i;
    return -1;
}
int hudd_cuadros(const hud_dat_t *h, int u) { return h && u >= 0 ? h->grp[h->uso[u].grp].n : 0; }
const uint8_t *hudd_extra(const hud_dat_t *h) { return h ? h->dat + h->off_taco + 16 : NULL; }

void hudd_dibuja(hud_dat_t *h, int u, int cuadro, float dx, float dy, float alfa, float z, int fondo)
{
    int n;
    if (!h || u < 0 || u >= (int)h->n_uso || alfa <= 0.004f) return;
    H = h; ult_tex = -1; modo_fondo = fondo;
    zcapa = z; zpaso = fondo ? 0.000001f : 0.001f;
    n = h->grp[h->uso[u].grp].n;
    if (cuadro < 0) cuadro = 0;
    if (n > 1 && cuadro >= n) cuadro = n - 1;
    grupo(h->uso[u].grp, cuadro, h->uso[u].x + dx, h->uso[u].y + dy, alfa);
    modo_fondo = 0; zpaso = 0.001f;
}

/* dentro de la lista TR, despues de todo lo demas (z por encima de la escena) */
void hud_dibuja(const hud_estado_t *e)
{
    char b[16]; int t, cs, m, cuadro;
    if (!hud_principal) return;
    H = hud_principal; ult_tex = -1; zcapa = 20.0f;
    /* capa 2: mapa de ruta y tacometro */
    dibuja("RUTA5_FONDO", 0, 0, 1);
    if (H->taco >= 0) {
        cuadro = (int)(e->rpm * 180.0f / 10000.0f); if (cuadro < 0) cuadro = 0; if (cuadro > 179) cuadro = 179;
        grupo(H->taco, cuadro, -219, 127, 1);
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
                  cifras(t, grupo_puntos(t, e->puntos_estilo), b, H->uso[t].x, H->uso[t].y, 1); }
    t = e->kmh < 0 ? 0 : e->kmh > 999 ? 999 : e->kmh;
    sprintf(b, "%03d", t); numero("VELOCIDAD", e->rebufo, b, -9999);
}

/* avisos animados (cuenta atras, GOAL, EXTEND TIME...): llamar despues de hud_dibuja; dibuja el cuadro dado del uso */
void hud_aviso(const char *nombre, int cuadro, float alfa)
{
    if (!hud_principal) return;
    H = hud_principal;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1;
    dibuja(nombre, 0, cuadro, alfa);
}

/* 4BD480: cada mensaje de la cola en (0x1E0, y + 0x49) con "%7d" */
void hud_puntos_msg(int puntos, int estilo, float dy, float alfa)
{
    char b[16]; int u;
    if (!hud_principal) return;
    H = hud_principal;
    if ((u = busca_uso("PUNTOS")) < 0 || alfa <= 0.004f) return;
    if (zcapa < 100.0f) zcapa = 100.0f;
    ult_tex = -1;
    sprintf(b, "%7d", puntos > 9999999 ? 9999999 : puntos < 0 ? 0 : puntos);
    cifras(u, grupo_puntos(u, estilo), b, H->uso[u].x - 1, H->uso[u].y + dy, alfa);
}
