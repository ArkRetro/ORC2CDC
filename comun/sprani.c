/* Reproductor de escenas SPRANI (ver sprani.h y tools/conv_sprani.py).

   Evaluacion (la de conv_hud.escena, que sigue a 429460 / 429350 / 429220 / 48B970 del PC): la escena es la ultima
   componente de su tabla; su lienzo (ancho x alto) va centrado en la pantalla de 640x480 (429010) con la matriz de la
   instancia; cada capa {primero, ultimo, desfase, estiramiento / 4096, footage o componente, padre, ancla, posicion,
   escala %, giro, opacidad %, 6 canales con claves de Hermite} se dibuja si es visible (+0x48 & 2) y el cuadro cae en
   [primero, ultimo); su tiempo local es (cuadro - primero) x estiramiento + desfase; matriz T(-ancla) S R T(pos) por
   la del padre (+0xC) y por la de fuera; footage: el cuadro min(n - 1, ritmo x local) de su lista; componente: sus
   capas de la ultima a la primera si el tiempo local cae en su intervalo. Mascaras (+0x48 & 8): la capa no se dibuja
   y la siguiente que salga lleva su alfa (horneado por el conversor: se busca por escena, cuadro y capa).
   Grupo de instancias (FrontendSprites de frontend_sprites.cpp = 427F70 / 4282B0 / 4285A0 del PC): 21 capas x 64,
   el cursor de cada capa avanza siempre uno; cada paso de 60 Hz el cuadro avanza rate x velocidad / 60 (todas las
   escenas van a 60: un cuadro); fuera de [primero, ultimo]: modo 0 da la vuelta (estado 2), 1 se queda en el borde
   (estado 3), 5 igual (estado 4), 3 se suelta sola; el modo 2 no avanza. Se dibujan por capas de la 0 a la 20 (42D710)
   y en cada capa por orden de hueco; las inmediatas (429530) de la capa, despues (PROBABLE: el orden entre las del
   grupo y las inmediatas de una misma capa no esta localizado). */
#include <kos.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "hud.h"
#include "sprani.h"

struct spr_banco {
    hud_dat_t *h;
    const uint8_t *blq, *ani;
    int banco, n_esc, n_spr, n_masc;
    const float *frac;
    const uint32_t *tex_esc;                            /* por escena: {desplazamiento de la lista, n} */
    const uint8_t *masc;
    const uint8_t *extra; uint32_t tam_extra;
};
static spr_banco_t *bancos[0x30];                       /* por banco - 0x20 */

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static float rdf(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }

spr_banco_t *spr_abre(const char *fich) { return spr_abre_en(fich, NULL, 0); }
spr_banco_t *spr_abre_en(const char *fich, void *ram, uint32_t cap)
{
    spr_banco_t *b = calloc(1, sizeof *b); const uint8_t *x; uint32_t c[12];
    if (!b) return NULL;
    hudd_tablas_en(ram, cap);
    if (!(b->h = hudd_abre(fich, 0, 1))) { hudd_tablas_en(NULL, 0); free(b); return NULL; }
    x = hudd_extra(b->h);
    if (memcmp(x, "SPR1", 4)) { hudd_cierra(b->h); free(b); return NULL; }
    memcpy(c, x + 4, sizeof c - 4);
    b->blq = x; b->banco = c[0]; b->n_esc = c[1]; b->n_spr = c[2]; b->ani = x + c[3];
    b->frac = (const float *)(x + c[5]); b->tex_esc = (const uint32_t *)(x + c[6]);
    b->masc = x + c[7]; b->n_masc = c[8]; b->extra = c[10] ? x + c[9] : NULL; b->tam_extra = c[10];
    if (b->banco < 0x20 || b->banco >= 0x50) { hudd_cierra(b->h); free(b); return NULL; }
    if (bancos[b->banco - 0x20]) spr_cierra(bancos[b->banco - 0x20]);
    bancos[b->banco - 0x20] = b;
    return b;
}

const uint8_t *spr_extra(const spr_banco_t *b, uint32_t *tam) { if (tam) *tam = b ? b->tam_extra : 0; return b ? b->extra : NULL; }

/* ---- grupo de instancias ---- */
typedef struct {
    uint8_t viva, modo, estado; spr_banco_t *b; int esc;
    float cuadro, primero, ultimo, vel, tx, ty;
} inst_t;
#define HUECOS 64
static inst_t ins[SPR_CAPAS * HUECOS];
static uint8_t usadas[SPR_CAPAS], cursor[SPR_CAPAS];
typedef struct { spr_banco_t *b; int esc, capa, cuadro; float x, y; } inm_t;
#define N_INM 96
static inm_t inm[N_INM]; static int n_inm;

static spr_banco_t *banco_de(uint32_t tok, int *esc)
{
    uint32_t bk = tok >> 16; spr_banco_t *b;
    if (bk < 0x20 || bk >= 0x50 || !(b = bancos[bk - 0x20])) return NULL;
    *esc = tok & 0xffff;
    return *esc < b->n_esc ? b : NULL;
}
static const uint8_t *raiz(const spr_banco_t *b, int k)
{
    uint32_t sc = rd32(b->ani + 4 * k), nc = rd32(b->ani + sc), pc = rd32(b->ani + sc + 4);
    return b->ani + pc + (nc - 1) * 0x24;
}

/* texturas de la escena a la VRAM; si no caben, se echan las del banco que no usa ninguna instancia viva */
static int sube(spr_banco_t *b, int k)
{
    const uint16_t *l = (const uint16_t *)(b->blq + b->tex_esc[2 * k]); uint32_t n = b->tex_esc[2 * k + 1], i;
    int r = 0, intento;
    for (intento = 0; intento < 2; intento++) {
        for (i = 0; i < n; i++) if (!hudd_residente(b->h, l[i]) && (r = hudd_sube(b->h, l[i], 1))) break;
        if (i == n) return 0;
        if (r != -2 || intento) break;
        {   /* sin VRAM: fuera las que no hacen falta */
            static uint8_t falta[1024]; int j, t; uint32_t nt = 0;
            memset(falta, 0, sizeof falta);
            for (j = 0; j < SPR_CAPAS * HUECOS; j++) if (ins[j].viva && ins[j].b == b) {
                const uint16_t *m = (const uint16_t *)(b->blq + b->tex_esc[2 * ins[j].esc]);
                uint32_t q;
                for (q = 0; q < b->tex_esc[2 * ins[j].esc + 1]; q++) if (m[q] < sizeof falta) falta[m[q]] = 1;
            }
            for (i = 0; i < n; i++) if (l[i] < sizeof falta) falta[l[i]] = 1;
            for (t = 0; t < (int)sizeof falta; t++) if (!falta[t] && hudd_residente(b->h, t)) { hudd_baja(b->h, t, 1); nt++; }
            hudd_vacia();
            dbglog(DBG_INFO, "SPRANI: sin VRAM para %X%04X, %u texturas fuera\n", b->banco, k, (unsigned)nt);
        }
    }
    dbglog(DBG_ERROR, "SPRANI: no caben las texturas de %X%04X (%d)\n", b->banco, k, r);
    return r;
}
int spr_sube_escena(uint32_t token) { int k; spr_banco_t *b = banco_de(token, &k); return b ? sube(b, k) : -1; }
void spr_baja_texturas(spr_banco_t *b) { if (b) hudd_baja(b->h, 0, -1); }

int spr_crea(uint32_t token, int capa, int modo, int primero, int ultimo)
{
    int k, hueco, h; spr_banco_t *b = banco_de(token, &k); inst_t *s;
    if (!b || capa < 0 || capa >= SPR_CAPAS || usadas[capa] >= HUECOS) return -1;
    do { hueco = cursor[capa]; cursor[capa] = (hueco + 1) % HUECOS; } while (ins[capa * HUECOS + hueco].viva);
    h = capa * HUECOS + hueco; s = ins + h;
    memset(s, 0, sizeof *s);
    s->viva = 1; s->b = b; s->esc = k; s->modo = modo; s->estado = 1; s->vel = 1;
    s->cuadro = primero; s->primero = primero >= 0 ? primero : 0;
    s->ultimo = ultimo < 0 ? rdf(raiz(b, k) + 8) - 1 : ultimo;
    usadas[capa]++;
    sube(b, k);
    return h;
}
int spr_crea_entera(uint32_t token, int capa, int modo)
{
    int k, h; spr_banco_t *b = banco_de(token, &k);
    if (!b) return -1;
    h = spr_crea(token, capa, modo, 0, -1);
    if (h >= 0) { ins[h].ultimo = (float)(int)rdf(raiz(b, k) + 8) - 1; ins[h].cuadro = 0; }
    return h;
}
void spr_suelta(int h)
{
    int capa;
    if (h < 0 || h >= SPR_CAPAS * HUECOS) return;
    ins[h].viva = 0; capa = h / HUECOS;
    if (usadas[capa]) usadas[capa]--;
    cursor[capa] = h % HUECOS;
}
void spr_velocidad(int h, float v) { if (h >= 0 && h < SPR_CAPAS * HUECOS) ins[h].vel = v; }
void spr_pon_cuadro(int h, int c) { if (h >= 0 && h < SPR_CAPAS * HUECOS) ins[h].cuadro = c; }
void spr_traslada(int h, float x, float y) { if (h >= 0 && h < SPR_CAPAS * HUECOS) { ins[h].tx = x; ins[h].ty = y; } }
int spr_estado(int h) { return h >= 0 && h < SPR_CAPAS * HUECOS ? ins[h].estado : 0; }
float spr_cuadro(int h) { return h >= 0 && h < SPR_CAPAS * HUECOS ? ins[h].cuadro : 0; }
void spr_suelta_todas(void) { memset(usadas, 0, sizeof usadas); memset(cursor, 0, sizeof cursor); { int i; for (i = 0; i < SPR_CAPAS * HUECOS; i++) ins[i].viva = 0; } n_inm = 0; }

void spr_cierra(spr_banco_t *b)
{
    int i;
    if (!b) return;
    for (i = 0; i < SPR_CAPAS * HUECOS; i++) if (ins[i].viva && ins[i].b == b) spr_suelta(i);
    for (i = 0; i < n_inm; i++) if (inm[i].b == b) inm[i].b = NULL;
    if (bancos[b->banco - 0x20] == b) bancos[b->banco - 0x20] = NULL;
    pvr_wait_ready();
    hudd_cierra(b->h); free(b);
}

void spr_paso(void)
{
    int capa, j;
    for (capa = 0; capa < SPR_CAPAS; capa++) for (j = 0; j < HUECOS; j++) {
        inst_t *s = ins + capa * HUECOS + j; float lo, hi;
        if (!s->viva || s->modo == 2) continue;
        s->cuadro += 60.0f * s->vel * (1.0f / 60);      /* rate (60 en todas las escenas) x velocidad x 1/60 */
        lo = s->primero < s->ultimo ? s->primero : s->ultimo; hi = s->primero < s->ultimo ? s->ultimo : s->primero;
        if (s->cuadro > hi || s->cuadro < lo) {
            if (s->modo == 0) { s->cuadro = s->cuadro > hi ? lo : hi; s->estado = 2; }
            else if (s->modo == 1 || s->modo == 5) { s->cuadro = s->cuadro > hi ? hi : lo; s->estado = s->modo == 1 ? 3 : 4; }
            else if (s->modo == 3) spr_suelta(capa * HUECOS + j);
        }
    }
}

void spr_inmediata(uint32_t token, float x, float y, int capa, int cuadro)
{
    int k; spr_banco_t *b = banco_de(token, &k);
    if (!b || n_inm >= N_INM || capa < 0 || capa >= SPR_CAPAS) return;
    if (sube(b, k)) return;
    inm[n_inm].b = b; inm[n_inm].esc = k; inm[n_inm].capa = capa; inm[n_inm].cuadro = cuadro; inm[n_inm].x = x; inm[n_inm].y = y;
    n_inm++;
}

/* ---- evaluacion ---- */
static const spr_banco_t *E; static int e_esc, e_cuadro;
static int m_pend; static float m_alfa;                 /* mascara pendiente (su alfa) */

static float canal(uint32_t ch, float f)
{
    const uint8_t *c = E->ani + ch; int n = rd16(c), j; float sv = 1.0f / (1 << c[4]), st = 1.0f / (1 << c[5]);
    int16_t p[4], q[4]; float dt, t, t2, t3;
    memcpy(p, c + 6, 8);
    if (n <= 0) return 0;
    if (f <= p[0]) return p[1] * sv;
    memcpy(q, c + 6 + 8 * (n - 1), 8);
    if (f >= q[0]) return q[1] * sv;
    for (j = 1; ; j++) { memcpy(q, c + 6 + 8 * j, 8); if (q[0] >= f) break; }
    memcpy(p, c + 6 + 8 * (j - 1), 8);
    if (p[1] == q[1] && p[3] == q[2]) return p[1] * sv;
    dt = q[0] - p[0]; t = (f - p[0]) / dt; t2 = t * t; t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * p[1] * sv + (3 * t2 - 2 * t3) * q[1] * sv
           + dt * (t3 - 2 * t2 + t) * p[3] * st + dt * (t3 - t2) * q[2] * st;
}
static void mul(const float *a, const float *b, float *o)  /* o = a @ b (filas) */
{
    float r[6];
    r[0] = a[0] * b[0] + a[1] * b[2]; r[1] = a[0] * b[1] + a[1] * b[3];
    r[2] = a[2] * b[0] + a[3] * b[2]; r[3] = a[2] * b[1] + a[3] * b[3];
    r[4] = a[4] * b[0] + a[5] * b[2] + b[4]; r[5] = a[4] * b[1] + a[5] * b[3] + b[5];
    memcpy(o, r, sizeof r);
}
static float capa_mat(uint32_t L, float f, float *m)
{
    const uint8_t *c = E->ani + L; float v[8], sx, sy, g, cs, sn; int k; uint8_t mk = c[0x49]; uint32_t padre;
    for (k = 0; k < 8; k++) v[k] = rdf(c + 0x10 + 4 * k);
    for (k = 0; k < 6; k++) if (mk >> k & 1) v[2 + k] = canal(rd32(c + 0x30 + 4 * k), f);
    sx = v[4] / 100; sy = v[5] / 100; g = v[6] * (float)M_PI / 180; cs = cosf(g); sn = sinf(g);
    m[0] = sx * cs; m[1] = sx * sn; m[2] = -sy * sn; m[3] = sy * cs;
    m[4] = -v[0] * m[0] - v[1] * m[2] + v[2]; m[5] = -v[0] * m[1] - v[1] * m[3] + v[3];
    if ((padre = rd32(c + 0xc))) { float mp[6]; capa_mat(padre, f, mp); mul(m, mp, m); }
    return v[7] / 100;
}
static int busca_masc(uint32_t L)
{
    int lo = 0, hi = E->n_masc - 1;
    while (lo <= hi) {
        int md = (lo + hi) / 2; const uint8_t *r = E->masc + 12 * md;
        uint16_t k = r[0] | r[1] << 8, f = r[2] | r[3] << 8; uint32_t l = rd32(r + 4);
        int cmp = k != e_esc ? (k < e_esc ? -1 : 1) : f != e_cuadro ? (f < e_cuadro ? -1 : 1) : l != L ? (l < L ? -1 : 1) : 0;
        if (!cmp) return r[8] | r[9] << 8;
        if (cmp < 0) lo = md + 1; else hi = md - 1;
    }
    return -1;
}
static void pieza(int s, const float *m, float w, float h, float alfa, int adi)
{
    float q[4]; const float *fr;
    if (s < 0 || s >= E->n_spr || alfa < 0.004f || fabsf(m[0] * m[3] - m[1] * m[2]) < 1e-6f) return;
    fr = E->frac + 4 * s; q[0] = fr[0] * w; q[1] = fr[1] * h; q[2] = fr[2] * w; q[3] = fr[3] * h;
    hudd_sprite(s, m, q, alfa, adi);
}
static void capa(uint32_t L, float f, const float *M, float al)
{
    const uint8_t *c = E->ani + L; uint32_t p = rd32(c + 8); int16_t pri, ult, d4, e6; uint8_t fl = c[0x48];
    float lm[6], op, loc;
    if (!p) return;
    pri = rd16(c); ult = rd16(c + 2); d4 = rd16(c + 4); e6 = rd16(c + 6);
    op = capa_mat(L, f, lm);
    if (!((fl & 2) && pri <= f && f < ult) && !m_pend) return;
    loc = (f - pri) * (e6 / 4096.0f) + d4; al *= op; mul(lm, M, lm);
    if (fl & 1) {
        const uint8_t *fo = E->ani + p; int w = rd32(fo), h = rd32(fo + 4), ritmo = rd32(fo + 8), n = rd32(fo + 12), i, s;
        uint32_t fr = rd32(fo + 16), t;
        if (n < 1) return;
        i = n == 1 ? 0 : (int)(ritmo * loc + 0.001f); if (i > n - 1) i = n - 1; if (i < 0) i = 0;   /* int() de Python */
        t = rd32(E->ani + fr + 20 * i);
        if (t == 0xffffffffu) return;
        if ((fl & 8) && !m_pend) { m_pend = 1; m_alfa = al; return; }
        if (m_pend) {                                   /* con la mascara horneada: alfa (a AND mascara) */
            m_pend = 0;
            al = ((int)(al * 255.5f) & (int)(m_alfa * 255.5f)) / 255.0f;
            s = busca_masc(L);
        } else s = (int)(t & 0xffff);
        pieza(s, lm, w, h, al, c[0x48] & 4);
    } else {
        const uint8_t *co = E->ani + p; float ini = rdf(co + 4), fin = rdf(co + 8); uint32_t pl = rd32(co + 0x20);
        int k;
        if (!(ini <= loc && loc < fin)) return;
        for (k = (int)rd32(co + 0x1c) - 1; k >= 0; k--) capa(pl + k * 0x4c, loc, lm, al);
    }
}
static void escena(const spr_banco_t *b, int k, float f, float tx, float ty)
{
    const uint8_t *r = raiz(b, k); float M[6]; int q; uint32_t pl = rd32(r + 0x20);
    E = b; e_esc = k; e_cuadro = (int)f; m_pend = 0;
    M[0] = 1; M[1] = 0; M[2] = 0; M[3] = 1; M[4] = 320 - rd16(r) / 2.0f + tx; M[5] = 240 - rd16(r + 2) / 2.0f + ty;
    for (q = (int)rd32(r + 0x1c) - 1; q >= 0; q--) capa(pl + q * 0x4c, f, M, 1.0f);
}

void spr_dibuja(int c0, int c1, float z, int fondo)
{
    int capa, j, i; spr_banco_t *ult = NULL; int primero = 1;
#define CAMBIA(bb) do { if ((bb) != ult) { ult = (bb); hudd_capa(ult->h, primero ? z : hudd_z(), fondo); primero = 0; } } while (0)
    for (capa = c0; capa < c1 && capa < SPR_CAPAS; capa++) {
        for (j = 0; j < HUECOS; j++) {
            inst_t *s = ins + capa * HUECOS + j;
            if (!s->viva) continue;
            CAMBIA(s->b);
            escena(s->b, s->esc, s->cuadro, s->tx, s->ty);
        }
        for (i = 0; i < n_inm; i++) if (inm[i].b && inm[i].capa == capa) {
            CAMBIA(inm[i].b);
            escena(inm[i].b, inm[i].esc, inm[i].cuadro, inm[i].x, inm[i].y);
        }
    }
#undef CAMBIA
    for (i = j = 0; i < n_inm; i++) if (inm[i].b && (inm[i].capa < c0 || inm[i].capa >= c1)) inm[j++] = inm[i];
    n_inm = j;
}
