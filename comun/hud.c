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
    pvr_ptr_t *vtex; uint8_t *usada; uint32_t *dibujada;      /* dibujada: ultimo cuadro en que se uso la textura */
    pvr_poly_hdr_t *cab, *cabf;                             /* por textura: encima de todo / detras de la escena 3D */
    int taco, g_fuera, off_taco;                            /* g_fuera: DIG_PUNTOS_FUERA (estilo 0xD, sin uso propio) */
    int16_t *pal;                                           /* por textura: paleta (pal_reparte) o -1; NULL sin paletas */
    uint8_t *con_pal;                                       /* por textura: subida con su paleta (cabeceras) */
    uint8_t *falta;                                         /* por textura: echada por hud_suelta_quietas y pedida */
};

static hud_dat_t *H, *hud_principal;                       /* H: el fichero que se esta dibujando */

/* Bufer de RAM prestado (hudd_bufer) para lo temporal de las subidas: los sectores leidos del GD de un fichero parcial
   (MENU.DAT: 100-400 KB por pantalla) y los indices de las texturas con paleta. Sin el (o si no cabe) se piden al
   monton como antes. demo/main.c presta el hueco de la union mientras no hay carrera (menus): el monton del menu ya no
   tiene esos bloques seguidos (docs/DREAMCAST.md, RAM del menu) */
static uint8_t *pres; static uint32_t pres_tam; static int pres_ocupado;
void hudd_bufer(void *p, uint32_t tam)
{
    pres = p && !((uintptr_t)p & 31) ? (uint8_t *)p : NULL; pres_tam = pres ? tam : 0; pres_ocupado = 0;
}
static uint8_t *toma(uint32_t n)
{
    if (pres && !pres_ocupado && n <= pres_tam) { pres_ocupado = 1; return pres; }
    return memalign(32, n);
}
static void deja(uint8_t *b) { if (b && b == pres) pres_ocupado = 0; else free(b); }
static int ult_tex = -1, modo_fondo;
static uint32_t tinte = 0xFFFFFF;                           /* color que multiplica las piezas (textos) */

/* Liberacion diferida de la VRAM: la PowerVR dibuja el cuadro anterior mientras la CPU prepara el siguiente, asi que
   una textura que se uso hace menos de HUDD_ESPERA cuadros puede estar leyendose todavia; si se libera y en su sitio se
   sube otra, ese cuadro sale con basura (fallos graficos al cambiar de pantalla en el menu). Se apunta y se libera
   cuando ya no la puede leer nadie (hudd_fin_cuadro, una vez por cuadro). */
#define HUDD_ESPERA 3
#define HUDD_PEND 128
static struct { pvr_ptr_t p; uint32_t cuadro; } pend[HUDD_PEND];
static int n_pend;
static uint32_t cuadro_hud = 16;                            /* cuadros dibujados (empieza lejos de 0) */

static void libera_vram(pvr_ptr_t p, uint32_t usada)
{
    if (cuadro_hud - usada >= HUDD_ESPERA || n_pend >= HUDD_PEND) { pvr_mem_free(p); return; }
    pend[n_pend].p = p; pend[n_pend].cuadro = usada; n_pend++;
}
void hudd_fin_cuadro(void)
{
    int i, j; hud_dat_t *h = hud_principal;
    if (h && h->falta)                                      /* las echadas en un apuro que se han querido dibujar */
        for (i = 0; i < (int)h->n_tex; i++) if (h->falta[i]) { h->falta[i] = 0; if (h->usada[i] && !h->vtex[i]) hudd_sube(h, i, 1); }
    cuadro_hud++;
    for (i = j = 0; i < n_pend; i++) {
        if (cuadro_hud - pend[i].cuadro >= HUDD_ESPERA) pvr_mem_free(pend[i].p);
        else pend[j++] = pend[i];
    }
    n_pend = j;
}
int hudd_pendiente(void) { return n_pend; }
/* libera ya lo pendiente: espera a que la PowerVR acabe los cuadros en curso (cargas: carrera, coche nuevo) */
void hudd_vacia(void)
{
    int i;
    if (!n_pend) return;
    thd_sleep(40);                                          /* mas de dos cuadros de dibujo */
    for (i = 0; i < n_pend; i++) pvr_mem_free(pend[i].p);
    n_pend = 0;
}
void hudd_color(uint32_t rgb) { tinte = rgb & 0xFFFFFF; }
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
    h->vtex = calloc(h->n_tex, sizeof *h->vtex); h->usada = calloc(h->n_tex, 1); h->dibujada = calloc(h->n_tex, 4);
    h->cab = memalign(32, h->n_tex * sizeof *h->cab); h->cabf = memalign(32, h->n_tex * sizeof *h->cabf);
    for (i = 0; i < h->n_grp; i++)
        if (grupo_util(i)) for (j = h->grp[i].pri; j < (uint32_t)h->grp[i].pri + h->grp[i].np; j++) h->usada[h->spr[h->pie[j].spr].tex] = 1;
    return h;
}

/* ---- paletas (solo el HUD de carrera, HUD.DAT; docs/DREAMCAST.md, Memoria de video)
   Casi todas las texturas ARGB4444 del HUD vienen de texturas de PS2 con paleta (CLUT de 16 o 256 colores) y tienen
   16 colores distintos o menos (paneles, cifras, marchas, mapa de ruta) o 256 o menos (cifras del tiempo, tacometros,
   rebufo, avisos de bifurcacion y tiempo extra). Esas se suben con indices de 4 u 8 bits y una paleta ARGB4444 de la
   PowerVR con exactamente sus colores: cada texel da el mismo valor de 16 bits que antes (sin perdida; la imagen es la
   misma) y ocupan 1/4 o 1/2. Las demas (la de los avisos de la salida y la meta, 576 colores; las ARGB1555 de la
   velocidad, que no caben exactas en una paleta 4444; las VQ) se quedan como estaban.
   La PowerVR tiene una tabla de 1024 entradas y un formato para todas (aqui ARGB4444; nadie mas usa paletas): las de
   8 bits usan un banco de 256 (4 bancos) y las de 4 bits uno de 16 (64). pal_reparte llena los bancos de 8 bits por
   orden de colores (las que caben juntas en 256 comparten banco: los tacometros, las cifras del tiempo) y pone las de
   4 bits en las entradas que sobran al final de los bancos (o con otra de 4 bits si caben juntas en 16); la que no
   cabe se queda en 16 bits. Los colores de cada banco van ordenados: el indice de un texel es su posicion (pal_idx). */
static uint16_t pal_col[1024];                              /* la tabla de la PowerVR (colores ordenados por banco) */
static uint16_t pal_n8[4];                                  /* entradas usadas al principio de cada banco de 8 bits */
static uint8_t pal_n4[64];                                  /* entradas usadas de cada banco de 4 bits (0: libre) */
#define PAL_4 0x4000                                        /* pal[]: banco de 4 bits (si no, de 8); bits 0-9: entrada */

/* colores distintos de la textura i (ordenados) en c[] si son <= 256; devuelve cuantos (257: mas) */
static int pal_colores(const hud_dat_t *h, int i, uint16_t *c)
{
    static uint32_t visto[2048];                            /* 65536 bits */
    const uint16_t *t = (const uint16_t *)(h->dat + h->tex[i].off); uint32_t k, n = 0, m = h->tex[i].w * h->tex[i].h;
    for (k = 0; k < m && n <= 256; k++) {
        uint16_t v = t[k];
        if (!(visto[v >> 5] & (1u << (v & 31)))) { visto[v >> 5] |= 1u << (v & 31); if (n < 256) c[n] = v; n++; }
    }
    for (k = 0; k < n && k < 256; k++) visto[c[k] >> 5] &= ~(1u << (c[k] & 31));
    if (n > 256) { memset(visto, 0, sizeof visto); return 257; }
    for (k = 1; k < n; k++) { uint16_t v = c[k]; int j = k; while (j > 0 && c[j - 1] > v) { c[j] = c[j - 1]; j--; } c[j] = v; }
    return n;
}
/* union ordenada de a (na) y b (nb) en o (hasta max; devuelve el tamano, max + 1 si no cabe) */
static int pal_une(const uint16_t *a, int na, const uint16_t *b, int nb, uint16_t *o, int max)
{
    int i = 0, j = 0, n = 0;
    while (i < na || j < nb) {
        uint16_t v = j >= nb || (i < na && a[i] <= b[j]) ? a[i] : b[j];
        if (i < na && a[i] == v) i++;
        if (j < nb && b[j] == v) j++;
        if (n == max) return max + 1;
        o[n++] = v;
    }
    return n;
}
static uint32_t tam_vram(const hud_dat_t *h, int i);
static void pal_reparte(hud_dat_t *h)
{
    uint16_t *col, *nc, tmp[256]; int16_t *orden; uint32_t i, j, n = h->n_tex; int k, b, s;
    h->pal = malloc(n * sizeof *h->pal); h->con_pal = calloc(n, 1);
    col = malloc(n * 256 * sizeof *col); nc = malloc(n * sizeof *nc); orden = malloc(n * sizeof *orden);
    if (!h->pal || !h->con_pal || !col || !nc || !orden) {
        free(h->pal); free(h->con_pal); h->pal = NULL; h->con_pal = NULL; free(col); free(nc); free(orden); return;
    }
    memset(pal_n8, 0, sizeof pal_n8); memset(pal_n4, 0, sizeof pal_n4);
    for (i = 0; i < n; i++) {                               /* solo ARGB4444 sin VQ (las 1555 no caben exactas) */
        h->pal[i] = -1;
        nc[i] = h->tex[i].fmt == 2 && h->tex[i].w >= 8 && h->tex[i].h >= 8 ? pal_colores(h, i, col + 256 * i) : 257;
        orden[i] = i;
    }
    for (i = 1; i < n; i++) { int16_t v = orden[i]; j = i; while (j > 0 && nc[orden[j - 1]] < nc[v]) { orden[j] = orden[j - 1]; j--; } orden[j] = v; }
    for (j = 0; j < n; j++) {                               /* 8 bits, de mas a menos colores: el primer banco que vale */
        i = orden[j];
        if (nc[i] <= 16 || nc[i] > 256) continue;
        for (b = 0; b < 4; b++) {
            k = pal_une(pal_col + 256 * b, pal_n8[b], col + 256 * i, nc[i], tmp, 256);
            if (k <= 256) { memcpy(pal_col + 256 * b, tmp, k * 2); pal_n8[b] = k; h->pal[i] = 256 * b; break; }
        }
    }
    for (j = 0; j < n; j++) {                               /* 4 bits: con otra si caben, si no en un banco libre */
        i = orden[j];
        if (nc[i] > 16) continue;
        for (s = 63; s >= 0; s--) {
            if (!pal_n4[s]) continue;
            k = pal_une(pal_col + 16 * s, pal_n4[s], col + 256 * i, nc[i], tmp, 16);
            if (k <= 16) { memcpy(pal_col + 16 * s, tmp, k * 2); pal_n4[s] = k; h->pal[i] = PAL_4 | 16 * s; break; }
        }
        if (h->pal[i] >= 0) continue;
        for (s = 63; s >= 0; s--)                           /* libre: ni de 4 bits ni dentro de lo usado de su banco */
            if (!pal_n4[s] && 16 * s >= 256 * (s / 16) + pal_n8[s / 16]) {
                memcpy(pal_col + 16 * s, col + 256 * i, nc[i] * 2); pal_n4[s] = nc[i] ? nc[i] : 1; h->pal[i] = PAL_4 | 16 * s; break;
            }
        if (h->pal[i] >= 0) continue;
        for (b = 0; b < 4; b++) {                           /* sin banco de 4 bits: en uno de 8 que no pise los de 4 */
            k = pal_une(pal_col + 256 * b, pal_n8[b], col + 256 * i, nc[i], tmp, 256);
            for (s = 16 * b; k <= 256 && s < 16 * b + 16; s++) if (pal_n4[s] && 16 * s < 256 * b + k) k = 257;
            if (k <= 256) { memcpy(pal_col + 256 * b, tmp, k * 2); pal_n8[b] = k; h->pal[i] = 256 * b; break; }
        }
    }
    pvr_set_pal_format(PVR_PAL_ARGB4444);
    for (i = 0; i < 1024; i++) pvr_set_pal_entry(i, pal_col[i]);
    {
        uint32_t ahorro = 0; int n8 = 0, n4 = 0;
        for (i = 0; i < n; i++) if (h->pal[i] >= 0 && h->usada[i]) ahorro += h->tex[i].tam - tam_vram(h, i);
        for (b = 0; b < 4; b++) n8 += pal_n8[b] > 0;
        for (s = 0; s < 64; s++) n4 += pal_n4[s] > 0;
        dbglog(DBG_INFO, "HUD: paletas en %d bancos de 8 bits y %d de 4, %uK menos de VRAM\n", n8, n4, (unsigned)(ahorro >> 10));
    }
    free(col); free(nc); free(orden);
}
/* indice del texel v en el banco de la textura (busqueda en sus colores ordenados) */
static inline uint32_t pal_idx(int16_t p, uint16_t v)
{
    const uint16_t *c = pal_col + (p & 1023); int lo = 0, hi = (p & PAL_4) ? pal_n4[(p & 1023) >> 4] : pal_n8[(p & 1023) >> 8];
    while (lo < hi) { int m = (lo + hi) >> 1; if (c[m] < v) lo = m + 1; else hi = m; }
    return lo;
}
/* bytes de la textura i en la VRAM (con su paleta: 1/2 o 1/4) */
static uint32_t tam_vram(const hud_dat_t *h, int i)
{
    if (!h->pal || h->pal[i] < 0) return h->tex[i].tam;
    return ((h->tex[i].w * h->tex[i].h >> ((h->pal[i] & PAL_4) ? 1 : 0)) + 31) & ~31u;
}
/* sube la textura i (los texeles en d) a v: con su paleta si la tiene y hay RAM para los indices; 1 con paleta */
static int sube_tex(hud_dat_t *h, int i, const uint8_t *d, pvr_ptr_t v)
{
    int16_t p = h->pal ? h->pal[i] : -1; uint32_t k, m = h->tex[i].w * h->tex[i].h, tam = tam_vram(h, i); uint8_t *o;
    const uint16_t *t = (const uint16_t *)d;
    if (p < 0 || !(o = toma(tam))) { pvr_txr_load(d, v, h->tex[i].tam); return 0; }
    if (p & PAL_4) for (k = 0; k < m; k += 2) o[k >> 1] = pal_idx(p, t[k]) | pal_idx(p, t[k + 1]) << 4;   /* par: bajo */
    else for (k = 0; k < m; k++) o[k] = pal_idx(p, t[k]);  /* (twiddled: el mismo orden de texeles que la de 16 bits) */
    pvr_txr_load(o, v, tam);
    deja(o);
    return 1;
}

static void cabeceras(hud_dat_t *h, int i)
{
    static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
    pvr_poly_cxt_t cx;
    /* formato: bits 0-7 el de 16 bits; bit 8 VQ (libro de 256 entradas al principio de los datos) */
    uint32_t f = FMT[(h->tex[i].fmt & 0xff) < 3 ? (h->tex[i].fmt & 0xff) : 2] | PVR_TXRFMT_TWIDDLED;
    if (h->tex[i].fmt & 0x100) f |= PVR_TXRFMT_VQ_ENABLE;
    if (h->con_pal && h->con_pal[i]) {                      /* con paleta (pal_reparte): siempre twiddled */
        int16_t p = h->pal[i];
        f = (p & PAL_4) ? PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL((p & 1023) >> 4) : PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL((p & 1023) >> 8);
    }
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
        if (!(b = toma(ns * 2048))) return -3;
        if (lee_sect(h->lba + s0, ns, b)) { deja(b); return -4; }
    }
    for (i = t0; i < fin; i++) {
        if (!h->usada[i] || h->vtex[i]) continue;
        if (!(h->vtex[i] = pvr_mem_malloc(tam_vram(h, i)))) { deja(b); hudd_baja(h, t0, nt); return -2; }
        if (h->con_pal) h->con_pal[i] = sube_tex(h, i, h->meta ? b + (h->tex[i].off - s0 * 2048) : h->dat + h->tex[i].off, h->vtex[i]);
        else pvr_txr_load(h->meta ? b + (h->tex[i].off - s0 * 2048) : h->dat + h->tex[i].off, h->vtex[i], h->tex[i].tam);
        cabeceras(h, i);
    }
    deja(b);
    return 0;
}

void hudd_baja(hud_dat_t *h, int t0, int nt)
{
    int i, fin;
    if (!h) return;
    if (nt < 0) nt = h->n_tex - t0;
    fin = t0 + nt; if (fin > (int)h->n_tex) fin = h->n_tex;
    for (i = t0; i < fin; i++) if (h->vtex[i]) { libera_vram(h->vtex[i], h->dibujada[i]); h->vtex[i] = NULL; }
}

/* cierra un fichero de hudd_abre: su VRAM (con la espera de hudd_baja) y su RAM */
void hudd_cierra(hud_dat_t *h)
{
    if (!h || h == hud_principal) return;
    hudd_baja(h, 0, -1); hudd_vacia();
    if (H == h) H = NULL;
    free(h->dat); free(h->vtex); free(h->usada); free(h->dibujada); free(h->cab); free(h->cabf);
    free(h->pal); free(h->con_pal); free(h->falta); free(h);
}

int hud_carga(const char *fich, int modelo)
{
    if (!(hud_principal = hudd_abre(fich, modelo, 0))) return -1;
    pal_reparte(hud_principal);                             /* el fichero entero esta en la RAM */
    hud_principal->falta = calloc(hud_principal->n_tex, 1);
    return hudd_sube(hud_principal, 0, -1) ? -2 : 0;
}
void hud_baja_tex(void) { hudd_baja(hud_principal, 0, -1); }
/* en un apuro de VRAM (demo/main.c vram_apuro): echa ya las texturas del HUD de carrera que no se han dibujado en los
   ultimos `cuadros` cuadros (los avisos de la salida, la meta, el tiempo extra...; la PowerVR ya no las lee) y vuelven
   a subirse solas el cuadro en que se quieran dibujar (si caben; si no, ese aviso no se ve). Devuelve los bytes */
uint32_t hud_suelta_quietas(uint32_t cuadros)
{
    hud_dat_t *h = hud_principal; uint32_t i, b = 0;
    if (!h) return 0;
    for (i = 0; i < h->n_tex; i++)
        if (h->vtex[i] && cuadro_hud - h->dibujada[i] > cuadros) { pvr_mem_free(h->vtex[i]); h->vtex[i] = NULL; b += tam_vram(h, i); }
    return b;
}
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
    if (a <= 0.004f) return;
    if (!H->vtex[s->tex]) { if (H->falta && H->usada[s->tex]) H->falta[s->tex] = 1; return; }   /* hud_suelta_quietas */
    if (p->flags & 1) ang += g->paso * cuadro;
    ang *= (float)M_PI / 180.0f; c = cosf(ang); si = sinf(ang);
    for (k = 0; k < 4; k++) {
        float qx = ((k & 1) ? s->w : 0) - p->ax, qy = ((k & 2) ? s->h : 0) - p->ay;
        qx *= p->sx; qy *= p->sy;
        px[k] = qx * c - qy * si + p->x + X; py[k] = qx * si + qy * c + p->y + Y;
    }
    if (s->tex != ult_tex) { pvr_prim((modo_fondo ? H->cabf : H->cab) + s->tex, sizeof(pvr_poly_hdr_t)); ult_tex = s->tex; }
    H->dibujada[s->tex] = cuadro_hud;
    if (a > 1) a = 1;
    v.argb = ((uint32_t)(a * 255) << 24) | tinte; v.oargb = 0; v.z = zcapa; zcapa += zpaso;
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
        if (G->n <= 1 || H->pie[j].cuadro == cuadro ||
            (cuadro > H->pie[j].cuadro && cuadro < H->pie[j].cuadro + (H->pie[j].flags >> 4)))   /* MENU.DAT: vale */
            pieza(H->pie + j, G, cuadro, X, Y, alfa);                                          /* flags >> 4 cuadros */
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
