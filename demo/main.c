/* Demo jugable del port de OutRun 2006 C2C a Dreamcast: la primera etapa (SEG00.DAT) y un coche (COCHE.DAT),
   convertidos de tu DVD con tools/conv_tramo.py y tools/conv_coche.py (formato ORD4, ver tools/modelo.py).
   Orden de carga: datos del GD a la RAM (con aviso en pantalla), PowerVR, texturas a la memoria de video, cabeceras
   de poligono, y a jugar. La conduccion va sobre la ruta de la etapa (distancia recorrida, desvio lateral y rumbo
   respecto a la carretera), con los bordes de la calzada como muros.
   Mando: stick o cruceta para girar, A o gatillo derecho acelera, B, X o gatillo izquierdo frena, START empieza o
   pausa, Y ensena/oculta las cifras de rendimiento, cruceta arriba/abajo distancia de dibujo. Sin tocar nada, el titulo pasa a una demostracion automatica. */
#include <kos.h>
#include <dc/matrix.h>
#include <dc/biosfont.h>
#include <math.h>
#include <malloc.h>
#include "../comun/gd.h"

/* ---------------------------------------------------------------- modelos */
typedef struct { uint32_t w, h, fmt, lista, off, tam; } tex_t;
typedef struct { float cx, cy, cz, r; uint32_t v0, n; int32_t tex; float dmax; } malla_t;
typedef struct { float x, y, z, u, v; uint32_t c; } vert_t;
typedef struct {
    tex_t *texs; malla_t *mallas; vert_t *verts; float *ruta;
    uint32_t nmallas, nvert, nruta, paso_ruta, ntex, inicio[5];
    uint8_t *datos, *blob; pvr_poly_hdr_t *cabs[2];               /* [0] con niebla, [1] sin niebla (cielo) */
} modelo_t;

static const uint32_t FMT[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
static const int LISTA[3] = { PVR_LIST_OP_POLY, PVR_LIST_PT_POLY, PVR_LIST_TR_POLY };
static pvr_poly_hdr_t cab_col[2][3];                            /* sin textura: [niebla][lista] */

static int lee_modelo(modelo_t *m, const char *nombre)
{
    uint32_t sz, h[10], i; uint8_t *b = gd_carga(nombre, &sz), *p;
    if (!b) return -1;
    if (memcmp(b, "ORD5", 4)) return -2;
    memcpy(h, b + 4, sizeof h);
    m->nmallas = h[0]; m->nvert = h[1]; m->nruta = h[2]; m->paso_ruta = h[3]; m->ntex = h[4];
    for (i = 0; i < 5; i++) m->inicio[i] = h[5 + i];
    p = b + 44;
    m->texs = (tex_t *)p; p += m->ntex * sizeof(tex_t);
    m->mallas = (malla_t *)p; p += m->nmallas * sizeof(malla_t);
    m->verts = (vert_t *)p; p += m->nvert * sizeof(vert_t);
    m->ruta = (float *)p; p += m->nruta * m->paso_ruta * 4;
    m->blob = b + ((p - b + 31) & ~31); m->datos = b;
    return 0;
}

/* texturas a la memoria de video y cabeceras; el blob de texeles ya no hace falta en la RAM tras esto */
static int sube_modelo(modelo_t *m)
{
    pvr_poly_cxt_t cx; uint32_t i, f;
    for (f = 0; f < 2; f++) m->cabs[f] = memalign(32, (m->ntex + 1) * sizeof(pvr_poly_hdr_t));
    for (i = 0; i < m->ntex; i++) {
        tex_t *t = m->texs + i; pvr_ptr_t v = pvr_mem_malloc(t->tam);
        if (!v) return -3;
        pvr_txr_load(m->blob + t->off, v, t->tam);
        for (f = 0; f < 2; f++) {
            pvr_poly_cxt_txr(&cx, LISTA[t->lista], FMT[t->fmt] | PVR_TXRFMT_TWIDDLED, t->w, t->h, v, PVR_FILTER_BILINEAR);
            cx.gen.fog_type = f ? PVR_FOG_DISABLE : PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
            cx.txr.env = PVR_TXRENV_MODULATE;
            pvr_poly_compile(&m->cabs[f][i], &cx);
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- camara y dibujo */
static float ojo[3], der[3], arr[3], ade[3], fx = 320.0f / 0.75f, cerca = 0.5f, lejos = 1000.0f;
static matrix_t vista __attribute__((aligned(32))), vista_cielo __attribute__((aligned(32))),
                mod_coche __attribute__((aligned(32)));
static uint32_t tri_enviados;

static void norm(float *v) { float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 0) { v[0] /= l; v[1] /= l; v[2] /= l; } }

/* matriz de pantalla: X = 320 W + fx xv, Y = 240 W - fx yv, W = Z = zv (columnas: ftrv suma columna_j * entrada_j) */
static void pon_vista(const float *o, const float *mira)
{
    int j; float k;
    for (j = 0; j < 3; j++) { ojo[j] = o[j]; ade[j] = mira[j] - o[j]; }
    norm(ade);
    der[0] = -ade[2]; der[1] = 0; der[2] = ade[0]; norm(der);
    arr[0] = der[1] * ade[2] - der[2] * ade[1]; arr[1] = der[2] * ade[0] - der[0] * ade[2]; arr[2] = der[0] * ade[1] - der[1] * ade[0];
    for (j = 0; j < 3; j++) {
        float X = fx * der[j] + 320.0f * ade[j], Y = -fx * arr[j] + 240.0f * ade[j], W = ade[j];
        vista[j][0] = X; vista[j][1] = Y; vista[j][2] = W; vista[j][3] = W;
    }
    memcpy(vista_cielo, vista, sizeof vista);
    vista_cielo[3][0] = vista_cielo[3][1] = vista_cielo[3][2] = vista_cielo[3][3] = 0;
    for (j = 0; j < 4; j++) {
        k = -(ojo[0] * vista[0][j] + ojo[1] * vista[1][j] + ojo[2] * vista[2][j]); vista[3][j] = k;
    }
}

typedef struct { float x, y, w, u, v; uint32_t c; } pt_t;
static inline void envia(const pt_t *p, int fin)
{
    pvr_vertex_t *v = pvr_dr_target();
    float iw = 1.0f / p->w;
    v->flags = fin ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v->x = p->x * iw; v->y = p->y * iw; v->z = iw; v->u = p->u; v->v = p->v; v->argb = p->c; v->oargb = 0;
    pvr_dr_commit(v);
}
static void tri_recortado(const pt_t *p)
{
    pt_t o[4]; int n = 0, i;
    for (i = 0; i < 3; i++) {
        const pt_t *a = p + i, *b = p + (i + 1) % 3;
        int da = a->w >= cerca, db = b->w >= cerca;
        if (da) o[n++] = *a;
        if (da != db) {
            float t = (cerca - a->w) / (b->w - a->w);
            o[n].x = a->x + t * (b->x - a->x); o[n].y = a->y + t * (b->y - a->y); o[n].w = cerca;
            o[n].u = a->u + t * (b->u - a->u); o[n].v = a->v + t * (b->v - a->v); o[n].c = a->c; n++;
        }
    }
    if (n < 3) return;
    if (n == 3) { envia(o, 0); envia(o + 1, 0); envia(o + 2, 1); tri_enviados++; }
    else { envia(o, 0); envia(o + 1, 0); envia(o + 3, 0); envia(o + 2, 1); tri_enviados += 2; }
}
static uint32_t color_fijo;
static pt_t pw[256];                                     /* depuracion: si no es 0, color de todas las tiras */
/* 1/w con FSRRA (1/raiz de w*w; w > 0): mucho mas rapida que la division */
static inline float inv_w(float w) { float r = w * w; __asm__("fsrra %0" : "+f"(r)); return r; }

/* malla entera delante del plano cercano: cada vertice se transforma y va directo a la PowerVR */
static void dibuja_malla_directa(const modelo_t *mo, const malla_t *m)
{
    const vert_t *v = mo->verts + m->v0; uint32_t i, n = m->n;
    for (i = 0; i < n; i++, v++) {
        float x = v->x, y = v->y, z = v->z, w, iw; pvr_vertex_t *d;
        mat_trans_single3_nodivw(x, y, z, w);
        iw = inv_w(w);
        d = pvr_dr_target();
        d->flags = i == n - 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        d->x = x * iw; d->y = y * iw; d->z = iw; d->u = v->u; d->v = v->v;
        d->argb = color_fijo ? color_fijo : v->c; d->oargb = 0;
        pvr_dr_commit(d);
    }
    tri_enviados += n - 2;
}

static void dibuja_malla(const modelo_t *mo, const malla_t *m)
{
    const vert_t *v = mo->verts + m->v0; uint32_t i, n = m->n; int detras = 0;
    if (n > 256) n = 256;
    for (i = 0; i < n; i++) {
        float x = v[i].x, y = v[i].y, z = v[i].z, w;
        mat_trans_single3_nodivw(x, y, z, w);
        pw[i].x = x; pw[i].y = y; pw[i].w = w; pw[i].u = v[i].u; pw[i].v = v[i].v; pw[i].c = color_fijo ? color_fijo : v[i].c;
        detras |= w < cerca;
    }
    if (!detras) {
        for (i = 0; i < n; i++) envia(pw + i, i == n - 1);
        tri_enviados += n - 2;
        return;
    }
    for (i = 0; i + 2 < n; i++) {
        if (pw[i].w < cerca && pw[i + 1].w < cerca && pw[i + 2].w < cerca) continue;
        tri_recortado(pw + i);
    }
}

/* mallas [k0, k1) de la lista l; recorte por esfera con la camara si recorta; sin_niebla para el cielo */
static void dibuja_rango(const modelo_t *mo, int l, uint32_t k0, uint32_t k1, int recorta, int sin_niebla, int solo_lista)
{
    float tanh = 320.0f / fx; uint32_t k; int ult = -2, seguro = 0;
    for (k = k0; k < k1; k++) {
        const malla_t *m = mo->mallas + k;
        if (solo_lista && (m->tex < 0 || (int)mo->texs[m->tex].lista != l)) continue;
        if (recorta) {
            float dx = m->cx - ojo[0], dy = m->cy - ojo[1], dz = m->cz - ojo[2];
            float zv = dx * ade[0] + dy * ade[1] + dz * ade[2], xv, yv;
            if (zv + m->r < cerca || zv - m->r > lejos) continue;
            seguro = zv - m->r > cerca * 1.5f && m->n <= 256;
            if (m->dmax > 0 && zv - m->r > m->dmax) continue;
            xv = dx * der[0] + dy * der[1] + dz * der[2];
            yv = dx * arr[0] + dy * arr[1] + dz * arr[2];
            if (fabsf(xv) > zv * tanh + m->r * 1.3f || fabsf(yv) > zv * tanh * 0.75f + m->r * 1.3f) continue;
        }
#ifdef DEP_TEX
        if (recorta && !sin_niebla) {                      /* depuracion: color = numero de textura */
            if (ult == -2) pvr_prim(&cab_col[1][l], sizeof(pvr_poly_hdr_t));
            ult = m->tex; color_fijo = 0xFF000000 | ((((m->tex + 1) & 31) << 3) << 16) | (((((m->tex + 1) >> 5) & 7) << 5) << 8) | 0x40;
            dibuja_malla(mo, m); color_fijo = 0; continue;
        }
#endif
        if (m->tex != ult) {
            ult = m->tex;
            if (m->tex >= 0) pvr_prim(&mo->cabs[sin_niebla][m->tex], sizeof(pvr_poly_hdr_t));
            else pvr_prim(&cab_col[sin_niebla][l], sizeof(pvr_poly_hdr_t));
        }
        if (recorta && seguro) dibuja_malla_directa(mo, m);
        else dibuja_malla(mo, m);
    }
}

/* ---------------------------------------------------------------- ruta de la etapa */
static modelo_t etapa, coche;
static float *rumbo, longitud;                                  /* rumbo de la carretera en cada punto (rad) */
#define PASO 4.0f
#define PR ((int)etapa.paso_ruta)
#define NPERF 16
static void ruta_prepara(void)
{
    uint32_t i, n = etapa.nruta; float ant = 0;
    rumbo = malloc(n * sizeof(float));
    for (i = 0; i < n; i++) {
        const float *a = etapa.ruta + PR * (i ? i - 1 : 0), *b = etapa.ruta + PR * (i + 1 < n ? i + 1 : n - 1);
        float t = atan2f(b[0] - a[0], -(b[2] - a[2]));            /* t = (sin, 0, -cos): girar a la derecha suma */
        while (t - ant > M_PI) t -= 2 * M_PI;
        while (t - ant < -M_PI) t += 2 * M_PI;
        rumbo[i] = ant = t;
    }
    longitud = (n - 1) * PASO;
}
/* en la distancia s: posicion del centro, rumbo y anchos (izquierdo, derecho) */
static void ruta_en(float s, float *p, float *th, float *iz, float *de)
{
    int i; float f; const float *a, *b;
    if (s < 0) s = 0;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)(s / PASO); f = s / PASO - i; a = etapa.ruta + PR * i; b = a + PR;
    p[0] = a[0] + f * (b[0] - a[0]); p[1] = a[1] + f * (b[1] - a[1]); p[2] = a[2] + f * (b[2] - a[2]);
    *th = rumbo[i] + f * (rumbo[i + 1] - rumbo[i]);
    if (iz) *iz = a[3] + f * (b[3] - a[3]);
    if (de) *de = a[4] + f * (b[4] - a[4]);
}

/* altura de la calzada en la distancia s y el desvio lateral x (perfil a lo ancho de cada punto de la ruta) */
static float altura(float s, float x)
{
    int i, j; float f, g, h0, h1; const float *a, *b;
    if (s < 0) s = 0;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)(s / PASO); f = s / PASO - i; a = etapa.ruta + PR * i; b = a + PR;
    {   /* posicion en el perfil de cada punto: de -izq-2 a der+2 en NPERF muestras */
        float ga = (x + a[3] + 2) / (a[3] + a[4] + 4) * (NPERF - 1), gb = (x + b[3] + 2) / (b[3] + b[4] + 4) * (NPERF - 1);
        ga = fmaxf(0, fminf(NPERF - 1.001f, ga)); gb = fmaxf(0, fminf(NPERF - 1.001f, gb));
        j = (int)ga; g = ga - j; h0 = a[5 + j] + g * (a[6 + j] - a[5 + j]);
        j = (int)gb; g = gb - j; h1 = b[5 + j] + g * (b[6 + j] - b[5 + j]);
    }
    return h0 + f * (h1 - h0);
}

/* ---------------------------------------------------------------- coche */
typedef struct { float s, x, a, v; float pos[3], fw[3], up[3]; int choque; } coche_t;
static coche_t yo;
#define VMAX 82.0f
/* posicion y orientacion en el mundo a partir de (s, x, a), apoyado en la calzada (altura, cabeceo, balanceo) */
static void coche_coloca(coche_t *c)
{
    float p[3], th, t[3], r[3], ca = cosf(c->a), sa = sinf(c->a), hd, ht, hi, hr, rr[3];
    ruta_en(c->s, p, &th, NULL, NULL);
    t[0] = sinf(th); t[1] = 0; t[2] = -cosf(th); r[0] = cosf(th); r[1] = 0; r[2] = sinf(th);
    c->pos[0] = p[0] + c->x * r[0]; c->pos[2] = p[2] + c->x * r[2];
    c->fw[0] = ca * t[0] + sa * r[0]; c->fw[2] = ca * t[2] + sa * r[2];
    hd = altura(c->s + 1.6f * ca, c->x + 1.6f * sa); ht = altura(c->s - 1.6f * ca, c->x - 1.6f * sa);
    hi = altura(c->s, c->x - 0.8f); hr = altura(c->s, c->x + 0.8f);
    c->pos[1] = 0.25f * (hd + ht + hi + hr);
    c->fw[1] = (hd - ht) / 3.2f; norm(c->fw);
    rr[0] = -c->fw[2]; rr[1] = (hr - hi) / 1.6f; rr[2] = c->fw[0]; norm(rr);
    c->up[0] = rr[1] * c->fw[2] - rr[2] * c->fw[1]; c->up[1] = rr[2] * c->fw[0] - rr[0] * c->fw[2];
    c->up[2] = rr[0] * c->fw[1] - rr[1] * c->fw[0]; norm(c->up);
}

/* ---------------------------------------------------------------- trafico */
#define NCARRIL 6
#define NTRAF 12
typedef struct { coche_t c; int carril, modelo; } trafico_t;
static trafico_t traf[NTRAF];
static modelo_t mtraf[3];
static uint32_t semilla = 12345;
static float azar(void) { semilla = semilla * 1103515245u + 12345u; return ((semilla >> 8) & 0xFFFF) / 65536.0f; }
static float carril_x(float s, int k)
{
    int i; float f; const float *a;
    if (s < 0) s = 0;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)(s / PASO); f = s / PASO - i; a = etapa.ruta + PR * i;
    return a[5 + NPERF + k] + f * (a[PR + 5 + NPERF + k] - a[5 + NPERF + k]);
}
static void traf_nace(trafico_t *t, float s)
{
    t->carril = (int)(azar() * NCARRIL) % NCARRIL; t->modelo = (int)(azar() * 3) % 3;
    t->c.s = s; t->c.v = 22.0f + azar() * 9.0f; t->c.a = 0;
    t->c.x = carril_x(s, t->carril); coche_coloca(&t->c);
}
static void traf_inicia(float s0)
{
    int i;
    for (i = 0; i < NTRAF; i++) traf_nace(traf + i, s0 + 120.0f + i * 110.0f + azar() * 60.0f);
}
static void traf_paso(float dt)
{
    int i;
    for (i = 0; i < NTRAF; i++) {
        trafico_t *t = traf + i; float x0 = t->c.x;
        t->c.s += t->c.v * dt;
        if (t->c.s < yo.s - 150.0f || t->c.s > longitud - 20.0f) traf_nace(t, yo.s + 500.0f + azar() * 500.0f);
        if (t->c.s > longitud - 20.0f) t->c.s = longitud - 20.0f;
        t->c.x = carril_x(t->c.s, t->carril);
        t->c.a = atan2f(t->c.x - x0, t->c.v * dt + 1e-4f);
        coche_coloca(&t->c);
        /* choque con el jugador: le frena y le aparta */
        if (fabsf(yo.s - t->c.s) < 4.4f && fabsf(yo.x - t->c.x) < 1.9f) {
            if (yo.s < t->c.s) { yo.s = t->c.s - 4.4f; if (yo.v > t->c.v) yo.v = t->c.v * 0.6f; }
            else { yo.s = t->c.s + 4.4f; }
            yo.x += yo.x > t->c.x ? 0.6f : -0.6f; yo.a *= -0.3f; yo.choque = 2;
            coche_coloca(&yo);
        }
    }
}

static void coche_paso(coche_t *c, float giro, float gas, float freno, float dt)
{
    float p[3], th0, th1, iz, de, g, yaw, lim;
    ruta_en(c->s, p, &th0, NULL, NULL);
    g = fminf(c->v / 8.0f, 1.0f) * (1.0f - 0.45f * c->v / VMAX);
    yaw = giro * 1.6f * g;
    c->a += yaw * dt;
    c->a -= c->a * 0.3f * dt;                                   /* un poco de agarre: el coche se endereza */
    c->v += (gas * 9.5f * (1.0f - c->v / VMAX) - freno * 18.0f - 0.0004f * c->v * c->v - 0.3f) * dt;
    if (c->v < 0) c->v = 0;
    c->s += c->v * cosf(c->a) * dt;
    c->x += c->v * sinf(c->a) * dt;
    ruta_en(c->s, p, &th1, &iz, &de);
    c->a -= th1 - th0;                                          /* la carretera gira bajo el coche */
    c->choque = 0;
    lim = iz - 1.0f;
    if (c->x < -lim) { c->x = -lim; if (c->a < 0) c->a = -c->a * 0.3f; c->v -= c->v * 1.5f * dt; c->choque = 1; }
    lim = de - 1.0f;
    if (c->x > lim) { c->x = lim; if (c->a > 0) c->a = -c->a * 0.3f; c->v -= c->v * 1.5f * dt; c->choque = 1; }
    if (c->a > 1.2f) c->a = 1.2f;
    if (c->a < -1.2f) c->a = -1.2f;
    coche_coloca(c);
}

/* piloto automatico: sigue el centro de la calzada y frena antes de las curvas cerradas */
static void piloto(const coche_t *c, float *giro, float *gas, float *freno)
{
    float p[3], th, th2, iz, de, kap = 0, xc, ad, need, g, vt; int k;
    ruta_en(c->s, p, &th, &iz, &de);
    for (k = 1; k <= 6; k++) {                                  /* curvatura maxima en los proximos 120 m */
        float pa[3], ta, pb[3], tb;
        ruta_en(c->s + (k - 1) * 20.0f + 20.0f, pa, &ta, NULL, NULL);
        ruta_en(c->s + k * 20.0f + 20.0f, pb, &tb, NULL, NULL);
        kap = fmaxf(kap, fabsf(tb - ta) / 20.0f);
    }
    ruta_en(c->s + 8.0f, p, &th2, NULL, NULL);
    {   /* el carril con el coche de delante mas lejos (y si empata, el mas cercano) */
        int k, i, mejor = 0; float dmej = -1;
        for (k = 0; k < NCARRIL; k++) {
            float d = 300.0f, xk = carril_x(c->s + 30.0f, k);
            for (i = 0; i < NTRAF; i++)
                if (traf[i].c.s > c->s - 3.0f && fabsf(traf[i].c.x - xk) < 2.2f) d = fminf(d, traf[i].c.s - c->s);
            d -= fabsf(xk - c->x) * 0.8f;
            if (d > dmej) { dmej = d; mejor = k; }
        }
        xc = carril_x(c->s + 30.0f, mejor);
    }
    ad = fmaxf(-0.25f, fminf(0.25f, 0.06f * (xc - c->x)));
    g = fmaxf(0.05f, fminf(c->v / 8.0f, 1.0f) * (1.0f - 0.45f * c->v / VMAX));
    need = (th2 - th) / 8.0f * c->v + (ad - c->a) * 3.0f;
    *giro = fmaxf(-1.0f, fminf(1.0f, need / (1.6f * g)));
    vt = kap > 0.0005f ? 0.75f * 0.9f / kap : VMAX;
    *gas = c->v < vt ? 1.0f : 0.0f;
    *freno = c->v > vt + 4.0f ? 1.0f : 0.0f;
}

/* matriz del coche: x del modelo a la derecha, y arriba, el morro (-z) hacia delante */
static void pon_coche(const coche_t *c, matrix_t mod_coche)
{
    float r[3];                                                 /* derecha = adelante x arriba */
    r[0] = c->fw[1] * c->up[2] - c->fw[2] * c->up[1]; r[1] = c->fw[2] * c->up[0] - c->fw[0] * c->up[2];
    r[2] = c->fw[0] * c->up[1] - c->fw[1] * c->up[0];
    memset(mod_coche, 0, sizeof(matrix_t));
    mod_coche[0][0] = r[0]; mod_coche[0][1] = r[1]; mod_coche[0][2] = r[2];
    mod_coche[1][0] = c->up[0]; mod_coche[1][1] = c->up[1]; mod_coche[1][2] = c->up[2];
    mod_coche[2][0] = -c->fw[0]; mod_coche[2][1] = -c->fw[1]; mod_coche[2][2] = -c->fw[2];
    mod_coche[3][0] = c->pos[0]; mod_coche[3][1] = c->pos[1]; mod_coche[3][2] = c->pos[2]; mod_coche[3][3] = 1.0f;
}

/* ---------------------------------------------------------------- textos */
static pvr_ptr_t txt_arr, txt_cen;
static uint16_t buf_arr[512 * 128] __attribute__((aligned(32))), buf_cen[256 * 32] __attribute__((aligned(32)));
static void texto_arriba(const char *l1, const char *l2, const char *l3)
{
    memset(buf_arr, 0, sizeof buf_arr);
    bfont_draw_str(buf_arr + 4 * 512 + 4, 512, 1, l1);
    if (l2) bfont_draw_str(buf_arr + 32 * 512 + 4, 512, 1, l2);
    if (l3) bfont_draw_str(buf_arr + 60 * 512 + 4, 512, 1, l3);
    pvr_txr_load(buf_arr, txt_arr, sizeof buf_arr);
}
static void texto_centro(const char *s)
{
    int n = strlen(s);
    memset(buf_cen, 0, sizeof buf_cen);
    bfont_draw_str(buf_cen + 4 * 256 + (256 - 12 * n) / 2, 256, 1, s);
    pvr_txr_load(buf_cen, txt_cen, sizeof buf_cen);
}
static void rect_txr(pvr_ptr_t t, int w, int h, float x, float y, float W, float H, int lista)
{
    pvr_poly_cxt_t cx; pvr_poly_hdr_t hd; pvr_vertex_t v;
    pvr_poly_cxt_txr(&cx, lista, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, w, h, t, PVR_FILTER_NONE);
    if (lista == PVR_LIST_TR_POLY) { cx.blend.src = PVR_BLEND_ONE; cx.blend.dst = PVR_BLEND_ONE; }
    pvr_poly_compile(&hd, &cx); pvr_prim(&hd, sizeof hd);
    v.argb = 0xFFFFFFFF; v.oargb = 0; v.z = 10.0f;
    v.flags = PVR_CMD_VERTEX; v.x = x; v.y = y; v.u = 0; v.v = 0; pvr_prim(&v, sizeof v);
    v.x = x + W; v.u = 1; pvr_prim(&v, sizeof v);
    v.x = x; v.y = y + H; v.u = 0; v.v = 1; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + W; v.u = 1; pvr_prim(&v, sizeof v);
}

/* rectangulo negro translucido (fondo del marcador) */
static void caja(float x, float y, float W, float H, uint32_t argb)
{
    pvr_poly_cxt_t cx; pvr_poly_hdr_t hd; pvr_vertex_t v;
    pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY); pvr_poly_compile(&hd, &cx); pvr_prim(&hd, sizeof hd);
    v.argb = argb; v.oargb = 0; v.z = 9.0f; v.u = v.v = 0;
    v.flags = PVR_CMD_VERTEX; v.x = x; v.y = y; pvr_prim(&v, sizeof v);
    v.x = x + W; pvr_prim(&v, sizeof v);
    v.x = x; v.y = y + H; pvr_prim(&v, sizeof v);
    v.flags = PVR_CMD_VERTEX_EOL; v.x = x + W; pvr_prim(&v, sizeof v);
}

static void aviso(const char *s)
{
    vid_clear(0, 0, 0);
    bfont_draw_str(vram_s + 220 * 640 + 60, 640, 1, s);
}

/* ---------------------------------------------------------------- juego */
enum { TITULO, CUENTA, CARRERA, META, AGOTADO };
#define TIEMPO_ETAPA 100.0f
#ifndef VER
#define VER 0
#endif
#ifndef SALIDA
#define SALIDA 100.0f                                           /* metros desde el principio de la ruta */
#endif

int main(int argc, char **argv)
{
    pvr_init_params_t pp = { { PVR_BINSIZE_16, 0, PVR_BINSIZE_16, 0, PVR_BINSIZE_16 }, 768 * 1024, 0, 0, 0, 3 };
    int estado = TITULO, auto_ = 0, ver = VER, pausa = 0, l, i, r;
    uint32_t cuadro = 0, bot_ant = 0, t_estado = 0; uint64_t t0, tcpu = 0, c0;
    float cam_y = 0, tiempo = TIEMPO_ETAPA, fps = 0, ms = 0, cam_fw[3] = { 0, 0, -1 }, ttit = 0;
    char s1[96], s2[96], s3[96], msg[32] = "";
    uint64_t c_log = 0, t_log = 0, t_ini = timer_ms_gettime64(); float ms_log = 0, t_carga = 0;
    (void)argc; (void)argv;

    /* 1: datos del disco a la RAM (la pantalla de texto aun sin PowerVR) */
    vid_set_mode(DM_640x480, PM_RGB565);
    aviso("OutRun 2006 C2C - demo Dreamcast. Cargando la etapa...");
    if ((r = lee_modelo(&etapa, "SEG00.DAT")) || (r = lee_modelo(&coche, "COCHE.DAT")) ||
        (r = lee_modelo(&mtraf[0], "TRAF0.DAT")) || (r = lee_modelo(&mtraf[1], "TRAF1.DAT")) ||
        (r = lee_modelo(&mtraf[2], "TRAF2.DAT"))) {
        sprintf(s1, "No se pudieron leer los datos (%d)", r); aviso(s1); for (;;) thd_sleep(1000);
    }
    ruta_prepara();
    /* 2: PowerVR, texturas a la memoria de video y cabeceras */
    pvr_init(&pp);
    pvr_set_bg_color(0.45f, 0.65f, 0.95f);
    txt_arr = pvr_mem_malloc(sizeof buf_arr); txt_cen = pvr_mem_malloc(sizeof buf_cen);
    if (sube_modelo(&etapa) || sube_modelo(&coche) || sube_modelo(&mtraf[0]) || sube_modelo(&mtraf[1]) ||
        sube_modelo(&mtraf[2])) {
        vid_set_mode(DM_640x480, PM_RGB565); aviso("Sin memoria de video para las texturas"); for (;;) thd_sleep(1000);
    }
    for (l = 0; l < 3; l++) for (i = 0; i < 2; i++) {
        pvr_poly_cxt_t cx; pvr_poly_cxt_col(&cx, LISTA[l]);
        cx.gen.fog_type = i ? PVR_FOG_DISABLE : PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&cab_col[i][l], &cx);
    }
    pvr_fog_table_color(1.0f, 0.55f, 0.70f, 0.92f);
    pvr_fog_table_linear(lejos * 0.55f, lejos);
    memset(&yo, 0, sizeof yo);
    coche_paso(&yo, 0, 0, 0, 0);
    t_carga = (timer_ms_gettime64() - t_ini) / 1000.0f;
    texto_arriba("", NULL, NULL); texto_centro("PULSA START");
    t0 = timer_ms_gettime64();

    for (;;) {
        maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
        uint32_t b = st ? st->buttons : 0, nuevo = b & ~bot_ant;
        float giro = 0, gas = 0, freno = 0, ojo_[3], mira[3];
        bot_ant = b; t_estado++; c_log = timer_us_gettime64();
        if (nuevo & CONT_Y) ver ^= 1;
        if ((nuevo & CONT_DPAD_UP) && lejos < 1500) { lejos += 100; pvr_fog_table_linear(lejos * 0.55f, lejos); }
        if ((nuevo & CONT_DPAD_DOWN) && lejos > 300) { lejos -= 100; pvr_fog_table_linear(lejos * 0.55f, lejos); }
        if (st) {
            giro = st->joyx / 110.0f;
            if (b & CONT_DPAD_LEFT) giro = -1;
            if (b & CONT_DPAD_RIGHT) giro = 1;
            if (giro > 1) giro = 1;
            if (giro < -1) giro = -1;
            if (fabsf(giro) < 0.08f) giro = 0;
            gas = st->rtrig / 255.0f; if (b & CONT_A) gas = 1;
            freno = st->ltrig / 255.0f; if (b & (CONT_B | CONT_X)) freno = 1;
        }

        /* estados */
        if (estado == TITULO) {
            ttit += 1.0f / 60;
            if ((nuevo & CONT_START) || ttit > 8.0f) {
                auto_ = !(nuevo & CONT_START);
                memset(&yo, 0, sizeof yo); yo.s = SALIDA; yo.x = carril_x(SALIDA, 4); coche_paso(&yo, 0, 0, 0, 0); traf_inicia(SALIDA);
                tiempo = TIEMPO_ETAPA; estado = CUENTA; t_estado = 0; pausa = 0;
            }
        } else if (estado == CUENTA) {
            int n = 3 - (int)(t_estado / 60);
            sprintf(s1, n > 0 ? "%d" : "GO!", n);
            if (strcmp(s1, msg)) { strcpy(msg, s1); texto_centro(msg); }
            if (t_estado >= 180) { estado = CARRERA; t_estado = 0; }
            if (auto_ && (nuevo & CONT_START)) { estado = TITULO; ttit = -30; texto_centro("PULSA START"); }
            coche_paso(&yo, 0, 0, 0, 1.0f / 60);
        } else if (estado == CARRERA) {
            if (t_estado == 50) texto_centro(auto_ ? "DEMO" : "");
            if (auto_ && (nuevo & CONT_START)) { estado = TITULO; ttit = 9.0f; nuevo = 0; auto_ = 0; }
            else if (nuevo & CONT_START) pausa ^= 1;
            if (!pausa) {
                if (auto_) piloto(&yo, &giro, &gas, &freno);
                coche_paso(&yo, giro, gas, freno, 1.0f / 60);
                traf_paso(1.0f / 60);
                tiempo -= 1.0f / 60;
                if (yo.s >= longitud - 40.0f) { estado = META; t_estado = 0; texto_centro("GOAL!"); }
                else if (tiempo <= 0) { tiempo = 0; estado = AGOTADO; t_estado = 0; texto_centro("TIEMPO AGOTADO"); }
            }
        } else {                                                /* META o AGOTADO: el coche se para solo */
            coche_paso(&yo, 0, 0, 0.6f, 1.0f / 60);
            if ((nuevo & CONT_START) || t_estado > 360) { estado = TITULO; ttit = auto_ ? 0 : 0; texto_centro("PULSA START"); }
        }

        /* camara: en el titulo vuela por la etapa; en carrera, detras del coche */
        if (estado == TITULO) {
            float p[3], th, q[3], th2; float s = fmodf(ttit * 25.0f + 200.0f, longitud - 100.0f);
            ruta_en(s, p, &th, NULL, NULL); ruta_en(s + 30.0f, q, &th2, NULL, NULL); cam_y = p[1];
            ojo_[0] = p[0]; ojo_[1] = p[1] + 12.0f; ojo_[2] = p[2];
            mira[0] = q[0]; mira[1] = q[1] + 4.0f; mira[2] = q[2];
            yo.s = s; coche_paso(&yo, 0, 0, 0, 0);
        } else {
            for (i = 0; i < 3; i += 2) cam_fw[i] += (yo.fw[i] - cam_fw[i]) * 0.12f;
            norm(cam_fw);
            cam_y += (yo.pos[1] - cam_y) * 0.15f;
            ojo_[0] = yo.pos[0] - 6.5f * cam_fw[0]; ojo_[1] = cam_y + 2.4f; ojo_[2] = yo.pos[2] - 6.5f * cam_fw[2];
            mira[0] = yo.pos[0] + 6.0f * cam_fw[0]; mira[1] = cam_y + 1.1f; mira[2] = yo.pos[2] + 6.0f * cam_fw[2];
        }

        /* marcador (cada 6 cuadros) */
        if (cuadro % 6 == 0) {
            if (estado == TITULO) sprintf(s1, "OUTRUN 2006 C2C - DEMO DREAMCAST");
            else sprintf(s1, "TIEMPO %3d   %3d km/h   %3d%%%s", (int)ceilf(tiempo), (int)(yo.v * 3.6f),
                         (int)(100 * yo.s / longitud), yo.choque ? "  !" : "");
            if (ver) {
                pvr_stats_t ps; struct mallinfo mi = mallinfo();
                pvr_get_stats(&ps);
                sprintf(s2, "%.1ffps %lutri cpu %.1f+%.1f pvr %.1fms", (double)fps, (unsigned long)tri_enviados,
                        (double)ms_log, (double)ms, (double)(ps.rnd_last_time / 1000000.0));
                sprintf(s3, "d%d vb%uK ram%uK vram%uK carga%.1fs", (int)lejos, (unsigned)(ps.vtx_buffer_used >> 10),
                        (unsigned)(mi.uordblks >> 10), (unsigned)(pvr_mem_available() >> 10), (double)t_carga);
            }
            texto_arriba(s1, ver ? s2 : NULL, ver ? s3 : NULL);
        }

        t_log += timer_us_gettime64() - c_log;
        pvr_wait_ready();
        c0 = timer_us_gettime64();
        pvr_scene_begin();
        pon_vista(ojo_, mira);
        pon_coche(&yo, mod_coche);
        tri_enviados = 0;
        for (l = 0; l < 3; l++) {
            pvr_list_begin(LISTA[l]);
            mat_load(&vista_cielo);
            dibuja_rango(&etapa, l, etapa.inicio[3], etapa.inicio[4], 0, 1, 1);
            mat_load(&vista);
#ifdef DEP_LISTA
            color_fijo = l == DEP_LISTA ? 0xFFFF00FF : 0;
#endif
            dibuja_rango(&etapa, l, etapa.inicio[l], etapa.inicio[l + 1], 1, 0, 0);
            color_fijo = 0;
            if (estado != TITULO) {
                mat_load(&vista); mat_apply(&mod_coche);
                dibuja_rango(&coche, l, coche.inicio[l], coche.inicio[l + 1], 0, 0, 0);
            }
            for (i = 0; i < NTRAF; i++) {                       /* trafico: solo el que esta delante a la vista */
                static matrix_t mt __attribute__((aligned(32)));
                trafico_t *t = traf + i; float dx = t->c.pos[0] - ojo[0], dy = t->c.pos[1] - ojo[1], dz = t->c.pos[2] - ojo[2];
                float zv = dx * ade[0] + dy * ade[1] + dz * ade[2], xv = dx * der[0] + dy * der[1] + dz * der[2];
                if (estado == TITULO || zv < -3.0f || zv > 450.0f || fabsf(xv) > zv * (320.0f / fx) + 4.0f) continue;
                pon_coche(&t->c, mt);
                mat_load(&vista); mat_apply(&mt);
                dibuja_rango(&mtraf[t->modelo], l, mtraf[t->modelo].inicio[l], mtraf[t->modelo].inicio[l + 1], 0, 0, 0);
            }
            if (l == 2) {
                caja(36, 18, 568, ver ? 92 : 34, 0x90000000);
                rect_txr(txt_arr, 512, 128, 40, 20, 512, 128, PVR_LIST_TR_POLY);
                if (estado != CARRERA || t_estado < 50 || auto_) rect_txr(txt_cen, 256, 32, 64, 180, 512, 64, PVR_LIST_TR_POLY);
            }
            pvr_list_finish();
        }
        pvr_scene_finish();
        tcpu += timer_us_gettime64() - c0;
        if (++cuadro % 30 == 0) {
            uint64_t t1 = timer_ms_gettime64();
            fps = 30000.0f / (float)(t1 - t0); ms = tcpu / 30000.0f; ms_log = t_log / 30000.0f; t0 = t1; tcpu = 0; t_log = 0;
        }
    }
    return 0;
}
