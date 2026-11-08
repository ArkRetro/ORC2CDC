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
#include "../comun/musica.h"
#include "../comun/or2_fisica.h"
#include "../comun/hud.h"
#include "../comun/sfx.h"
#include "../comun/menu.h"

/* ---------------------------------------------------------------- modelos */
typedef struct { uint32_t w, h, fmt, lista, off, tam; } tex_t;
typedef struct { float cx, cy, cz, r; uint32_t v0, n; int32_t tex; float dmax; } malla_t;
typedef struct { float x, y, z, u, v; uint32_t c; } vert_t;
typedef struct {
    tex_t *texs; malla_t *mallas; vert_t *verts; float *ruta;
    uint32_t nmallas, nvert, nruta, paso_ruta, ntex, inicio[5];
    uint8_t *datos, *blob; pvr_poly_hdr_t *cabs[2];               /* [0] con niebla, [1] sin niebla (cielo) */
    pvr_ptr_t *tv;                                                /* texturas en la memoria de video */
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
    m->tv = calloc(m->ntex + 1, sizeof(pvr_ptr_t));
    for (i = 0; i < m->ntex; i++) {
        tex_t *t = m->texs + i; pvr_ptr_t v = pvr_mem_malloc(t->tam);
        if (!v) return -3;
        m->tv[i] = v;
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

/* libera un modelo: texturas de la memoria de video, cabeceras y datos */
static void libera_modelo(modelo_t *m)
{
    uint32_t i;
    for (i = 0; i < m->ntex; i++) if (m->tv && m->tv[i]) pvr_mem_free(m->tv[i]);
    free(m->tv); free(m->cabs[0]); free(m->cabs[1]); free(m->datos);
    memset(m, 0, sizeof *m);
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
static uint32_t vtx_env, vtx_ult;                        /* vertices enviados en el cuadro (y en el anterior) */
/* La PowerVR real se cuelga si el TA llena el bufer de vertices (Flycast no). El TA guarda unos 34 bytes por triangulo
   en tiras: con 1 MB de bufer caben ~30000. Tope de vertices de la etapa por cuadro; el coche (~6700), el trafico y
   los efectos van aparte. */
#define VTX_ETAPA_MAX 19000
static inline void envia(const pt_t *p, int fin)
{
    pvr_vertex_t *v = pvr_dr_target(); vtx_env++;
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
    tri_enviados += n - 2; vtx_env += n;
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
            if (vtx_env > VTX_ETAPA_MAX) continue;
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
    free(rumbo); rumbo = malloc(n * sizeof(float));
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

/* Columnas de la colision del original detras de los carriles (tools/conv_tramo.py superficies): limite transitable
   izquierdo y derecho (hasta el muro o el vacio), NEXT alturas a cada lado entre el perfil y el limite y NBANDA
   franjas de superficie (desvio donde empieza, indice de material de la COLI). Los ficheros antiguos no las tienen. */
#define NEXT 3
#define NBANDA 10
#define C_LIM (5 + NPERF + 6)                                   /* 6 = NCARRIL */
#define C_EXT (C_LIM + 2)
#define C_BANDA (C_EXT + 2 * NEXT)
#define CON_SUP (PR >= C_BANDA + 2 * NBANDA)
#define SUP_MURO 0x010f4060u                                    /* materiales de muro (CbwColiWall) */
/* altura en el desvio x de un punto de la ruta: el perfil (de -izq-2 a der+2) y fuera de el, hasta el limite, las
   alturas exteriores */
static float altura_punto(const float *a, float x)
{
    float g = (x + a[3] + 2) / (a[3] + a[4] + 4) * (NPERF - 1); int j;
    if (CON_SUP && (g < 0 || g > NPERF - 1)) {
        int lado = g < 0; float x0 = lado ? -a[3] - 2 : a[4] + 2, x1 = lado ? -a[C_LIM] : a[C_LIM + 1];
        const float *h = a + C_EXT + (lado ? 0 : NEXT);
        float h0 = lado ? a[5] : a[5 + NPERF - 1], t;
        if (fabsf(x1) <= fabsf(x0) + 0.01f) return h0;
        t = (x - x0) / (x1 - x0) * NEXT;                        /* 0 en el fin del perfil, NEXT en el limite */
        if (t <= 0) return h0;
        if (t >= NEXT) return h[NEXT - 1];
        j = (int)t; t -= j;
        return (j ? h[j - 1] : h0) + t * (h[j] - (j ? h[j - 1] : h0));
    }
    g = fmaxf(0, fminf(NPERF - 1.001f, g));
    j = (int)g; g -= j;
    return a[5 + j] + g * (a[6 + j] - a[5 + j]);
}
/* altura de la calzada en la distancia s y el desvio lateral x (perfil a lo ancho de cada punto de la ruta) */
static float altura(float s, float x)
{
    int i; float f; const float *a;
    if (s < 0) s = 0;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)(s / PASO); f = s / PASO - i; a = etapa.ruta + PR * i;
    return altura_punto(a, x) + f * (altura_punto(a + PR, x) - altura_punto(a, x));
}
/* limites transitables (positivos) en s: los de la colision, o sin ella los bordes del asfalto */
static void limites(float s, float *li, float *ld)
{
    int i, c = CON_SUP ? C_LIM : 3; float f; const float *a;
    if (s < 0) s = 0;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)(s / PASO); f = s / PASO - i; a = etapa.ruta + PR * i;
    *li = a[c] + f * (a[PR + c] - a[c]); *ld = a[c + 1] + f * (a[PR + c + 1] - a[c + 1]);
}
/* franjas del punto de la ruta mas cercano a s */
static const float *franjas(float s)
{
    int i;
    if (s < 0) s = 0;
    if (s > longitud - 0.01f) s = longitud - 0.01f;
    i = (int)(s / PASO + 0.5f); if (i > (int)etapa.nruta - 1) i = etapa.nruta - 1;
    return etapa.ruta + PR * i + C_BANDA;
}
/* bandera de superficie (1 << material de la COLI) en (s, x), como wheel+0x14 del original; 0 sin datos. Fuera de
   los limites, vacio (1) */
static uint32_t superficie(float s, float x)
{
    const float *b; float li, ld; int k;
    if (!CON_SUP) return 0;
    limites(s, &li, &ld);
    if (x < -li || x > ld) return 1;
    b = franjas(s);
    for (k = NBANDA - 1; k > 0 && x < b[2 * k]; k--) ;
    return 1u << ((int)b[2 * k + 1] & 31);
}
/* franja de muro (o vacio) dentro de los limites que toca el intervalo [x - m, x + m]: 1 y sus bordes */
static int muro_interior(float s, float x, float m, float *xa, float *xb)
{
    const float *b; float li, ld; int k;
    if (!CON_SUP) return 0;
    limites(s, &li, &ld); b = franjas(s);
    for (k = 0; k < NBANDA; k++) {
        float a0 = b[2 * k], a1 = k < NBANDA - 1 ? b[2 * k + 2] : ld; uint32_t f = 1u << ((int)b[2 * k + 1] & 31);
        if (a1 - a0 < 0.01f || !(f & (SUP_MURO | 1u))) continue;
        if (x + m > a0 && x - m < a1) { *xa = a0; *xb = a1; return 1; }
    }
    return 0;
}

/* ---------------------------------------------------------------- coche */
typedef struct { float s, x, a, v; float pos[3], fw[3], up[3]; int choque, derrape, t_derrape, t_muro; float deriva, freno_ant, gas_ant;
                 float pose_t[3], pose_r[3]; } coche_t;   /* pose: animacion de choque del original (solo al dibujar) */
static coche_t yo;
/* fisica del original para el coche del jugador (comun/or2_fisica.c) con la tabla de PARAM.DAT */
#ifndef MODELO
#define MODELO 6                                                /* modelo del coche (car+0x11): 6 = Testarossa */
#endif
#ifndef ACEL
#define ACEL 1.35f                                              /* multiplicador del par motor */
#endif
static int manual_, pide_cambio, auto_piloto, sfx_ok, golpe_cd;
static or2_param_t fpar; static or2_coche_t fis; static int fis_ok, columna = -1;
static or2_choques_t choq;                                      /* choques del original (bloques de PARAM.DAT) */
static float *param_dat;                                        /* se queda en memoria: las animaciones apuntan aqui */
/* grua del choque fuerte: PlWrecker avanza 10 posiciones de la carretera del original (pc_advance_on_road_place);
   como en rumbo_adelante (16 posiciones ~ 40 m) se toman ~2,5 m por posicion */
#define GRUA_AVANCE 25.0f
static float grua_s, grua_x; static int grua_on;
static int fisica_carga(void)
{
    uint32_t sz; float *t = gd_carga("PARAM.DAT", &sz);
    if (!t || sz < (200 * 19 + 162 + 256) * 4) return -1;
    or2_carga_choques(t, sz, &choq);
#ifdef COLUMNA
    columna = COLUMNA;
#else
    columna = or2_columna_modelo(&choq, MODELO);                /* la tabla de columnas del juego (5051D0) */
    if (columna < 0) columna = 4;                               /* la del Testarossa si el fichero es antiguo */
#endif
    if (or2_carga_param(t, 200, columna, &fpar) < 0) return -2;
    or2_carga_curvas(&fpar, t + 200 * 19, t + 200 * 19 + 162);
    fpar.par_mult *= ACEL;                                      /* mas aceleracion (a peticion del tester) */
    param_dat = t; fis_ok = 1; return 0;
}
static float dif_ang(float a) { while (a > M_PI) a -= 2 * M_PI; while (a < -M_PI) a += 2 * M_PI; return a; }
#define VMAX 100.0f                                          /* techo teorico; punta real unos 300 km/h */
/* posicion y orientacion en el mundo a partir de (s, x, a), apoyado en la calzada (altura, cabeceo, balanceo) */
static void coche_coloca(coche_t *c)
{
    float p[3], th, t[3], r[3], ca = cosf(c->a), sa = sinf(c->a), hd, ht, hi, hr, rr[3];
    float cv = cosf(c->a + c->deriva), sv = sinf(c->a + c->deriva);  /* la carroceria, cruzada al derrapar */
    ruta_en(c->s, p, &th, NULL, NULL);
    t[0] = sinf(th); t[1] = 0; t[2] = -cosf(th); r[0] = cosf(th); r[1] = 0; r[2] = sinf(th);
    c->pos[0] = p[0] + c->x * r[0]; c->pos[2] = p[2] + c->x * r[2];
    c->fw[0] = cv * t[0] + sv * r[0]; c->fw[2] = cv * t[2] + sv * r[2];
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
#define NTRAF 11                                                /* un 10 % menos que antes */
/* Campos del original (car work del PC): b08 desplazamiento lateral respecto al centro del carril (m del original:
   4 m = un carril, + hacia el carril siguiente), b14/b18 cuadros que faltan / totales del cambio de carril, dir su
   sentido, b58 deriva tras el golpe (0xB / 0xC), b60 cuadros de recuperacion, b64 cuadros sin choques, b6c golpeado */
typedef struct { coche_t c; int carril, modelo, destino; float v_des;
                 int golpe, lado; float t_golpe, vlat, xo;      /* golpe: estado de choque (16..19) o -1 */
                 float b08; int b14, b18, dir, b58, b60, b64, b6c;
                 int pt_marca, b72; } trafico_t;               /* puntuacion: +C5C (PT_*) y +B72 (rebufo reciente) */
static trafico_t traf[NTRAF];
/* +C5C del coche para la puntuacion (4F2860): 0x40 ya adelantado, 0x400000 tocado (puntos a la mitad), 0x800000
   chocado (sin puntos) */
#define PT_PASADO 0x40
#define PT_MITAD  0x400000
#define PT_SIN    0x800000
static int pt_toca;                                             /* contacto con el trafico en este cuadro: 1, 3 golpe */
static uint32_t sup_jug = 2u;                                   /* +244 del jugador: OR de las superficies de las ruedas */
#define NMODTRAF 9                                              /* coches de calle (TRAFn.DAT) */
static modelo_t mtraf[NMODTRAF];
static float largo_traf[NMODTRAF];                              /* mitad del largo de cada modelo */
/* TRAFC.DAT (tools/conv_trafico.py): por modelo, el registro de 4866C0 (clase +324, radio) y la primera caja de choque
   (520950); y la tabla por clase de 472090 (5B3E08 del PC): referencia, aceleracion, frenada, aceleracion fuerte y
   frenada fuerte, en m/cuadro por cuadro */
typedef struct { int modelo, clase, se, k320; float radio, caja[7]; } trafc_t;
static trafc_t trc[NMODTRAF];
static float acl[7][5];
static int trc_ok, cambio_cd;                                   /* cambio_cd: cuadros desde el ultimo cambio (804350) */
static void traf_medidas(void)
{
    int m; uint32_t i;
    for (m = 0; m < NMODTRAF; m++) {
        float lo = 0, hi = 0;
        for (i = 0; i < mtraf[m].nvert; i++) { float z = mtraf[m].verts[i].z; if (z < lo) lo = z; if (z > hi) hi = z; }
        largo_traf[m] = (hi - lo) * 0.5f;
        trc[m].clase = 3; trc[m].caja[0] = 0.9f; trc[m].caja[2] = largo_traf[m]; trc[m].caja[5] = -largo_traf[m];
    }
}
static void traf_tablas(void)
{
    uint32_t sz, n, m; const uint8_t *d = gd_carga("TRAFC.DAT", &sz);
    if (!d || sz < 8 || memcmp(d, "TRC1", 4)) return;
    memcpy(&n, d + 4, 4);
    if (n != NMODTRAF || sz < 8 + n * 52 + 8 + 35 * 4 || memcmp(d + 8 + n * 52, "ACL1", 4)) return;
    for (m = 0; m < n; m++) memcpy(&trc[m], d + 8 + m * 52, 52);
    memcpy(acl, d + 8 + n * 52 + 8, sizeof acl);
    trc_ok = 1;
}
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
/* carril mas cercano a x en s */
static int carril_cerca(float s, float x)
{
    int k, m = 0; float d = 1e9f;
    for (k = 0; k < NCARRIL; k++) { float e = fabsf(carril_x(s, k) - x); if (e < d) { d = e; m = k; } }
    return m;
}
/* separacion (con signo) entre el carril k y el siguiente: lo que en el original son 4 m */
static float carril_paso(float s, int k)
{
    return k < NCARRIL - 1 ? carril_x(s, k + 1) - carril_x(s, k) : carril_x(s, k) - carril_x(s, k - 1);
}
/* posicion lateral en carriles (carril + b08 / 4): la +268 del original, en unidades de carril */
static float traf_pos(const trafico_t *t) { return t->carril + t->b08 * 0.25f; }
static float yo_pos(float s)
{
    int k = carril_cerca(s, yo.x);
    return k + (yo.x - carril_x(s, k)) / carril_paso(s, k);
}
/* el carril k esta libre entre s-atras y s+delante (sin contar al propio coche) */
static int carril_libre(const trafico_t *yo_t, int k, float s, float atras, float delante)
{
    int i;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *t = traf + i; float d;
        if (t == yo_t) continue;
        if (t->carril != k && t->destino != k) continue;
        d = t->c.s - s;
        if (d > -atras && d < delante) return 0;
    }
    return 1;
}
static void traf_nace(trafico_t *t, float s)
{
    int k, n;
    t->modelo = (int)(azar() * NMODTRAF) % NMODTRAF;
    k = (int)(azar() * NCARRIL) % NCARRIL;
    for (n = 0; n < NCARRIL && !carril_libre(t, k, s, 25.0f, 25.0f); n++) k = (k + 1) % NCARRIL;   /* sin montarse */
    t->carril = t->destino = k;
    t->c.s = s; t->v_des = t->c.v = 36.0f + azar() * 14.0f; t->c.a = 0;
    t->golpe = -1; t->t_golpe = 0; t->vlat = 0; t->xo = 0; memset(t->c.pose_t, 0, 12); memset(t->c.pose_r, 0, 12);
    t->b08 = 0; t->b14 = t->b18 = 0; t->dir = 0; t->b58 = t->b60 = t->b64 = t->b6c = 0;   /* 4704A0 */
    t->pt_marca = 0; t->b72 = 0;
    t->c.x = carril_x(s, t->carril); coche_coloca(&t->c);
}
static void traf_inicia(float s0)
{
    int i;
    for (i = 0; i < NTRAF; i++) traf[i].carril = traf[i].destino = -1;
    for (i = 0; i < NTRAF; i++) traf_nace(traf + i, s0 + 130.0f + i * 121.0f + azar() * 66.0f);
    cambio_cd = 0;
#ifdef PRUEBA_TRAFICO
    for (i = 0; i < 3; i++) { traf_nace(traf + i, s0 + 25.0f + i * 18.0f); traf[i].carril = traf[i].destino = 2 + i; traf[i].modelo = i * 4;
        traf[i].c.v = traf[i].v_des = 5.0f; traf[i].c.x = carril_x(traf[i].c.s, traf[i].carril); coche_coloca(&traf[i].c); }
#endif
}
/* choque del jugador con un coche de trafico (4F9450 del original via or2_choque_coche): separacion repartida,
   intercambio de velocidad, salto y derrape del jugador (estado 7) y animacion de golpe del trafico (16..19) */
static void traf_choca(trafico_t *t, float l, float ancho)
{
    or2_otro_t o; float p[3], th, ds = t->c.s - yo.s, dx = t->c.x - yo.x, ps, px, ht, f[2], r[2], v_yo = yo.v;
    ruta_en(yo.s, p, &th, NULL, NULL);
    f[0] = sinf(th); f[1] = cosf(th); r[0] = cosf(th); r[1] = -sinf(th);
    ps = l - fabsf(ds); px = ancho - fabsf(dx);                 /* penetracion a lo largo y de lado */
    memset(&o, 0, sizeof o);
    o.dx = f[0] * ds + r[0] * dx; o.dy = f[1] * ds + r[1] * dx;
    ht = th + t->c.a;
    o.vx = sinf(ht) * t->c.v + r[0] * t->vlat; o.vy = cosf(ht) * t->c.v + r[1] * t->vlat;
    o.clase = trc[t->modelo].clase;                             /* +324 del modelo (TRAFC.DAT): 2 autobus, 3..4 coches */
    o.frontal = ps < px;                                        /* se separan a lo largo: golpe por delante/detras */
    if (fis_ok) or2_choque_coche(&fis, &fpar, &choq, &o);
    else { o.reparto = 0.5f; o.golpe = -1; o.cambia_vel = 0; }   /* sin PARAM.DAT: sin golpe ni intercambio */
    /* 4F2860: el contacto con un coche aun no adelantado le quita los puntos (con impulso, +4 0x4000000 del
       jugador: +C5C 0x800000) o los deja a la mitad (solo contacto: 0x400000); y las estadisticas de 4F2CD0 */
    if (!(t->pt_marca & PT_PASADO)) t->pt_marca |= (o.golpe >= 0 || o.cambia_vel) ? PT_SIN : PT_MITAD;
    pt_toca |= o.golpe >= 0 ? 3 : 1;
    if (o.frontal) {                                            /* 4F8490: separacion repartida */
        float sg = ds > 0 ? 1.0f : -1.0f;
        yo.s -= sg * ps * o.reparto; t->c.s += sg * ps * (1.0f - o.reparto);
    } else {
        float sg = dx > 0 ? 1.0f : -1.0f;
        yo.x -= sg * px * o.reparto; t->xo += sg * px * (1.0f - o.reparto);
    }
    if (o.golpe >= 0) {
        /* 476040: el golpeado deriva hacia el lado contrario al del jugador (+B58 0xB / 0xC segun +26C del jugador;
           aqui segun su posicion lateral respecto al trafico), 120 cuadros de recuperacion (+B60) y otros 120 sin
           choques (+B64); la velocidad pasa a 1,5 veces la del jugador (5B005C) salvo que el intercambio de 4F7BF0,
           que va despues, la cambie */
        t->b58 = yo_pos(t->c.s) > traf_pos(t) ? 0xB : 0xC;
        t->b6c = 1; t->b60 = 120; t->b64 = 120;
        if (!o.cambia_vel) t->c.v = v_yo * 1.5f;
    }
    if (o.cambia_vel) {                                         /* la velocidad nueva del otro, en la carretera */
        float vl = o.vx * f[0] + o.vy * f[1];
        t->vlat = o.vx * r[0] + o.vy * r[1];
        t->c.v = vl > 0 ? vl : 0;
    }
    if (o.golpe >= 0 && t->golpe < 0) { t->golpe = o.golpe; t->t_golpe = 0; t->lado = dx < 0; }
    if (fis_ok) {
        float h = atan2f(fis.vx, fis.vy);
        yo.v = sqrtf(fis.vx * fis.vx + fis.vy * fis.vy);
        yo.a = dif_ang(h - th); yo.deriva = dif_ang(-fis.psi - h);
    }
    yo.choque = 2;
    if (sfx_ok && !golpe_cd && fis.golpe_sonido) {
        golpe_cd = 20; sfx_golpe(fminf(0.4f + fabsf(yo.v - t->c.v) / 25.0f, 1.0f));
    }
    fis.golpe_sonido = 0;
    coche_coloca(&yo);
}

/* --- IA del trafico del original (47E4A0 drive -> 472450 velocidad, 473F30 recuperacion, 47E110 carriles).
   La demo no tiene los registros de velocidad por tramo de la carretera (0x46 bytes, 4F0030: velocidades por clase,
   distancia de seguimiento +8, tramos +4/+6), que van en los datos de la etapa: se toman los valores del registro fijo
   de 4EF730 para las distancias (seguimiento 50 m, lejos 10 y cerca 1 posiciones de ~2,5 m) y la velocidad de crucero
   de cada coche (v_des) como objetivo. Unidades del original: m/cuadro (60 por segundo). */
#define TR_HUECO 50.0f                                          /* rec2+8: distancia de seguimiento (m) */
#define TR_LEJOS 10                                             /* rec2+4 (posiciones) */
#define TR_CERCA 1                                              /* rec2+6 */
#define TR_POS   2.5f                                           /* m por posicion de la carretera (como GRUA_AVANCE) */
#define TR_CAMBIO 0xB4                                          /* 470A30 / 470AB0: el cambio de carril dura 180 cuadros */
/* 472090: lleva la velocidad hacia el objetivo con la tabla de la clase; f0/f1 los dos indicadores del objetivo */
static void traf_acelera(trafico_t *t, float rate, float obj, int f0, int f1)
{
    const float *tb = acl[trc[t->modelo].clase < 0 ? 0 : (trc[t->modelo].clase > 6 ? 6 : trc[t->modelo].clase)];
    float s = t->c.v / 60.0f, o = obj / 60.0f;
    if (!trc_ok) {                                              /* sin TRAFC.DAT: aceleracion y frenada de la demo */
        t->c.v += fmaxf(-6.0f / 60.0f, fminf(2.5f / 60.0f, obj - t->c.v)); return;
    }
    if (o > s * 1.2f || (o > s && f0 == 1)) { s += tb[3] * rate; if (s > o) s = o; }      /* 5B43B4 = 1,2 */
    else if (o > s) { s += tb[1] * rate; if (s > o) s = o; }
    else {
        int fuerte = (s * 0.8f > o && !f1) || f0 == 1;          /* 62821C = 0,8 */
        s -= tb[fuerte ? 4 : 2] * rate; if (s < o) s = o;
    }
    t->c.v = s * 60.0f;
}
/* coches por carril (800D2C) y 4734C0: -1 si los carriles a la derecha de k llevan mas trafico que los de la izquierda */
static int traf_lado_libre(int k)
{
    int n[NCARRIL] = { 0 }, i; float a = 0, b = 0;
    for (i = 0; i < NTRAF; i++) if (traf[i].carril >= 0) n[traf[i].carril]++;
    for (i = k; i < NCARRIL; i++) a += n[i];
    for (i = 0; i <= k; i++) b += n[i];
    return a / (NCARRIL - k) > b / (k + 1) ? -1 : 1;
}
/* 471010: el carril k es seguro (el jugador por delante en ese carril, o uno por detras al que adelanto, a menos de la
   distancia de seguimiento y a mas de 4 m: no) */
static int traf_seguro(const trafico_t *t, int k)
{
    int i; float g = 1000.0f, d, py = yo_pos(t->c.s), pt = traf_pos(t);
    d = yo.s - t->c.s;
    if (d > 0 && fabsf(k - py) <= 1.0f && fabsf(pt - py) > fabsf(k - py) && yo.v > t->c.v) g = d;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *o = traf + i;
        if (o == t || (int)(fabsf(k - traf_pos(o))) != 0) continue;
        d = t->c.s - o->c.s;
        if (d <= 0 || d > TR_HUECO) continue;
        if (g > d && t->c.v > o->c.v) g = d;
    }
    return !(g > 4.0f && TR_HUECO > g);
}
/* 471380: 0 si el jugador tapa el cambio al carril k (le tengo delante a 4..50 m, al lado del carril destino) */
static int traf_jugador_deja(const trafico_t *t, int k)
{
    float py = yo_pos(t->c.s), d = yo.s - t->c.s;
    if ((int)fabsf(k - py) > 1) return 1;
    if (!(fabsf(traf_pos(t) - py) > fabsf(k - py))) return 1;
    return !(d > 4.0f && d < TR_HUECO);
}
/* 4736D0: el cambio de carril en curso (b08 hacia 4,5 m con t^2; al acabar, pasa al carril nuevo) */
static void traf_cambio(trafico_t *t)
{
    float f = (float)(t->b18 - t->b14) / (float)t->b18;
    if (!traf_jugador_deja(t, t->destino)) return;              /* espera (+B14 + 1) */
    t->b14--;
    t->b08 = (1.0f - f) * t->b08 + t->dir * 4.5f * f * f;       /* 5B43CC = 4,5 */
    if (t->b14 > 0) return;
    t->carril = t->destino; t->b14 = 0;
    t->b08 -= t->dir * 4.0f;                                    /* lo que queda respecto al carril nuevo (473620) */
    if (t->b08 > 4.0f) t->b08 = 4.0f;
    if (t->b08 < -4.0f) t->b08 = -4.0f;
}
static void traf_empieza_cambio(trafico_t *t, int k)
{
    t->destino = k; t->dir = k > t->carril ? 1 : -1; t->b14 = t->b18 = TR_CAMBIO; cambio_cd = 0;
}
/* 47DC10: decide un cambio de carril (clases 3 y 4, mas rapido que el de delante, uno por segundo en total); delante y
   detras: los vecinos de la lista ordenada por distancia (+318 / +314), de cualquier carril */
static void traf_decide(trafico_t *t, const trafico_t *delante, int jugador_delante, const trafico_t *detras)
{
    int cl = t->carril, lo = t->carril < 3 ? 0 : 3, hi = lo + 2, cls = trc[t->modelo].clase;
    float e18 = 1000.0f;
    if ((delante || jugador_delante) && (jugador_delante || t->c.v > delante->c.v) && (cls == 3 || cls == 4)
        && cambio_cd > 60) {
        int r = traf_lado_libre(t->carril);
        if (r > 0) { if (t->carril < hi) cl = t->carril + 1; }
        else if (t->carril > lo) cl = t->carril - 1;
    }
    if (cl == t->carril) return;
    if (!traf_seguro(t, cl)) return;
    if (detras && (int)fabsf(cl - traf_pos(detras)) == 0 && t->c.v > detras->c.v) e18 = t->c.s - detras->c.s;
    if (!(e18 > TR_HUECO)) return;
    if (!carril_libre(t, cl, t->c.s, 15.0f, 30.0f)) return;     /* la demo no tiene choques entre coches de trafico */
    traf_empieza_cambio(t, cl);
}
/* 473B90: se aparta del vecino (delante o detras, a menos de 4 m a lo largo y 2 m de lado) 0,05 m por cuadro */
static int traf_aparta(trafico_t *t)
{
    int i; const trafico_t *c = NULL; float dm = 100.0f;
    for (i = 0; i < NTRAF; i++) {
        const trafico_t *o = traf + i; float d = fabsf(o->c.s - t->c.s);
        if (o == t || d >= dm) continue;
        dm = d; c = o;
    }
    if (!c || !(4.0f > dm) || !(2.0f > fabsf(traf_pos(t) - traf_pos(c)) * 4.0f)) return 0;
    if (traf_pos(t) > traf_pos(c)) { if (1.0f > t->b08) t->b08 += 0.05f; }
    else if (t->b08 > -1.0f) t->b08 -= 0.05f;
    return 1;
}
/* 473F30: tras el golpe deriva 0,2 m por cuadro (hasta 12 m) mientras haya sitio; al acabar los 120 cuadros los
   pesados (clases 0..2) vuelven al carril mas cercano de su calzada */
static void traf_recupera(trafico_t *t)
{
    float p = traf_pos(t); int lo = t->carril < 3 ? 0 : 3;     /* sitio: +268 / +264, los bordes de su calzada */
    if (t->b58 == 0xB) { if (p > lo - 0.5f && t->b08 > -12.0f) t->b08 -= 0.2f; }
    else if (t->b58 == 0xC) { if (p < lo + 2.5f && t->b08 < 12.0f) t->b08 += 0.2f; }
    if (--t->b60 > 0) return;
    t->b60 = 0; t->b58 = 0; t->b6c = 0;
    if (trc[t->modelo].clase > 2) return;
    {
        int k = t->carril + (int)(t->b08 * 0.25f);
        if (k < lo) k = lo;
        if (k > lo + 2) k = lo + 2;
        t->b08 = (p - k) * 4.0f; t->carril = t->destino = k;
    }
}
static void traf_paso(float dt)
{
    int i, j;
    cambio_cd++;
    for (i = 0; i < NTRAF; i++) {
        trafico_t *t = traf + i; const trafico_t *del = NULL, *sig = NULL, *ant = NULL;
        float x0 = t->c.x, hueco = 1e9f, obj, rate, b0, dsig = 1e9f, dant = 1e9f;
        int d, aparte, f0 = 0, f1 = 0, jug_del;
        if (t->b64 > 0) t->b64--;
        if (t->xo != 0) {                                       /* el empuje de la separacion (4F8490) pasa a b08 */
            t->b08 += t->xo * 4.0f / carril_paso(t->c.s, t->carril); t->xo = 0;
        }
        for (j = 0; j < NTRAF; j++) {                           /* el de delante en mi carril (o en el que voy) */
            const trafico_t *o = traf + j; float dd = o->c.s - t->c.s;
            if (o == t) continue;
            if (dd > 0 && dd < dsig) { dsig = dd; sig = o; }
            if (dd <= 0 && -dd < dant) { dant = -dd; ant = o; }
            if (dd <= 0 || dd > 60.0f) continue;
            if (o->carril != t->carril && o->carril != t->destino && o->destino != t->carril) continue;
            if (dd < hueco) { hueco = dd; del = o; }
        }
        jug_del = yo.s > t->c.s && yo.s - t->c.s < dsig;            /* el siguiente de la lista es el jugador */
        /* 472450: objetivo y ritmo segun la distancia al jugador (en posiciones de la carretera) */
        obj = t->v_des;
        if (del && hueco < TR_HUECO && t->c.v > del->c.v) obj = fminf(obj, del->c.v);   /* 472A50 */
        if (del && hueco < 10.0f) obj = fminf(obj, del->c.v * 0.9f);  /* la demo: sin choques entre coches de trafico */
        d = (int)floorf((t->c.s - yo.s) / TR_POS);
        aparte = (int)(fabsf(traf_pos(t) - yo_pos(t->c.s)));
        if (d > TR_LEJOS) rate = 1.0f;
        else {
            rate = 0;
            if (d >= 0 && d <= 3 && obj > t->c.v) {
                rate = (d * 0.2f / TR_LEJOS + 0.8f) * (1.1f - aparte * 0.0833333f);
                if (rate < 0) rate = 0;
                if (rate > 1.0f) rate = 1.0f;
            } else if (d < TR_CERCA && d > 0 && t->c.v > obj) rate = 1.0f;
            if (0.25f > rate) rate = 1.0f * ((d < 0 ? 0 : d) * 0.75f / TR_LEJOS + 0.25f);   /* 6282A0 = 0,75 */
        }
        if (aparte < 1 && t->c.v > obj && d >= 0 && d <= 3) { f0 = 0; f1 = 1; rate = 0.05f; }
        traf_acelera(t, rate, obj, f0, f1);
        /* 47E4A0: golpeado, recuperacion; si no, carriles (47E110) */
        b0 = t->b08;
        if (t->b6c) traf_recupera(t);
        else {
            int l0 = t->carril;
            if (t->b14 > 0) { traf_cambio(t); traf_aparta(t); }
            else { traf_decide(t, jug_del ? NULL : sig, jug_del, ant); traf_aparta(t); }
            if (l0 == t->carril && t->b14 <= 0) {               /* 6280EC: el desplazamiento baja 0,02 m por cuadro */
                if (b0 > 0) t->b08 = fmaxf(t->b08, b0 - 0.02f);
                if (b0 < 0) t->b08 = fminf(t->b08, b0 + 0.02f);
                if (t->b08 > 0) t->b08 = fmaxf(0, t->b08 - 0.02f);   /* vuelta al centro (473620 no portada) */
                else if (t->b08 < 0) t->b08 = fminf(0, t->b08 + 0.02f);
            }
        }
        if (t->golpe >= 0) {                                    /* la animacion del golpe (16..19) */
            t->t_golpe += dt;
            if (!or2_pose(&choq, t->golpe, t->t_golpe, t->lado, t->c.pose_t, t->c.pose_r)) t->golpe = -1;
        }
        t->c.s += t->c.v * dt;
        if (t->c.s < yo.s - 150.0f || t->c.s > longitud - 20.0f) { traf_nace(t, yo.s + 550.0f + azar() * 550.0f); continue; }
        if (t->c.s > longitud - 20.0f) t->c.s = longitud - 20.0f;
        t->c.x = carril_x(t->c.s, t->carril) + t->b08 * 0.25f * carril_paso(t->c.s, t->carril);
        t->vlat = (t->c.x - x0) / dt;
        t->c.a = atan2f(t->c.x - x0, t->c.v * dt + 1e-4f);
        coche_coloca(&t->c);
        /* choque con el jugador (no durante los 120 cuadros de +B64) */
        if (t->b64 <= 0) {
            const float *cj = trc[t->modelo].caja;
            float l = (cj[2] - cj[5]) * 0.5f + 2.3f, an = cj[0] + 1.0f;   /* caja del modelo (520950) + el Ferrari */
            if (fabsf(yo.s - t->c.s) < l && fabsf(yo.x - t->c.x) < an) traf_choca(t, l, an);
        }
    }
}

/* Conduccion, siguiendo el modelo del original (SlipAngleCtrl / RearGripCtrl / CorneringCtrl de la decompilacion de
   PC), simplificado: a = direccion de avance respecto a la carretera; deriva = carroceria respecto al avance.
   - derrape: un toque de freno (o soltar el gas frenando) arranca un temporizador de 40 cuadros; se mantiene mientras
     la deriva pasa de 8,4 grados y acaba 12 cuadros despues de bajar de ahi.
   - en derrape la carroceria apunta hacia dentro de la curva: la deriva busca giro x 0,6 rad mas el sesgo de la
     curva que viene (la curva marca el angulo de equilibrio); contravolante la reduce; tope 73 grados.
   - el derrape casi no quita velocidad; una ayuda lleva el avance hacia la carretera en proporcion a |sin deriva|.
   - sin derrape la deriva vuelve deprisa a una pequena (giro x 0,05). */
#define DERIVA_MAX 1.276f                                       /* 0x3400 = 73 grados */
static void coche_paso(coche_t *c, float giro, float gas, float freno, float dt)
{
    float p[3], th0, th1, iz, de, g, yaw, lim, sesgo, obj, x0; int f;
    ruta_en(c->s, p, &th0, &iz, &de);
    if (c == &yo && fis_ok && dt > 0) {                        /* el modelo del original */
        or2_entrada_t e; float q[3], tq, dx, dz;
        ruta_en(c->s + 40.0f, q, &tq, NULL, NULL);
        dx = q[0] - p[0]; dz = q[2] - p[2];
        memset(&e, 0, sizeof e);
        e.stick = giro; e.acelerador = gas; e.freno = freno; e.mu = 1.0f; e.en_suelo = 1;
        e.rumbo_carretera = th0; e.rumbo_adelante = th0 + dif_ang(atan2f(dx, -dz) - th0);
        e.dist_izq = iz + c->x; e.dist_der = de - c->x;
        e.manual = manual_ && !auto_piloto; e.cambio_pide = pide_cambio; pide_cambio = 0;
        {   /* superficie bajo cada rueda y bajo el centro (calc_ground_coli_face: wheel+0x14 y event+0x248): de las
               franjas de la COLI; la rueda i en (cg_del | -cg_tras, +-via/2) del cuerpo, que va a a + deriva de la
               carretera */
            float h = c->a + c->deriva, ch = cosf(h), sh = sinf(h); int i;
            for (i = 0; i < 4; i++) {
                float fx = i < 2 ? fpar.cg_del : -fpar.cg_tras, ly = fpar.via[i >> 1] * ((i & 1) ? -0.5f : 0.5f);
                e.sup[i] = superficie(c->s + fx * ch + ly * sh, c->x + fx * sh - ly * ch);
            }
            e.sup_centro = superficie(c->s, c->x);
            sup_jug = CON_SUP ? e.sup[0] | e.sup[1] | e.sup[2] | e.sup[3] : 2u;   /* +244 (sin franjas: asfalto) */
        }
        if (fis.grua > 0) {                                     /* grua del choque fuerte (PlWrecker) */
            float g[3], tg, gx, gz;
            if (!grua_on) {                                     /* el punto: 10 posiciones de carretera por delante */
                grua_s = fminf(c->s + GRUA_AVANCE, longitud - 50.0f); if (grua_s < c->s) grua_s = c->s;
                /* el original toma el centro de la fila de poligonos de la carretera (GetCsRoadInfoByCsLen); aqui
                   la ruta abarca las dos calzadas y la mediana, asi que el centro del carril mas cercano */
                grua_x = carril_x(grua_s, carril_cerca(grua_s, c->x));
                grua_on = 1;
            }
            ruta_en(grua_s, g, &tg, NULL, NULL);
            gx = g[0] + grua_x * cosf(tg); gz = g[2] + grua_x * sinf(tg);
            e.grua_ok = 1; e.grua_dx = gx - c->pos[0]; e.grua_dy = -(gz - c->pos[2]); e.grua_rumbo = tg;
        } else grua_on = 0;
        {   /* rebufo (CheckSlipStream): los coches de trafico cercanos */
            or2_otro_t ot[NTRAF]; int i, n = 0, cual[NTRAF]; float f0 = sinf(th0), f1 = cosf(th0), r0 = cosf(th0), r1 = -sinf(th0);
            for (i = 0; i < NTRAF; i++) {
                const trafico_t *t = traf + i; float ds = t->c.s - c->s, dx = t->c.x - c->x, ht = th0 + t->c.a;
                if (ds < -60.0f || ds > 60.0f) continue;
                ot[n].dx = f0 * ds + r0 * dx; ot[n].dy = f1 * ds + r1 * dx;
                ot[n].vx = sinf(ht) * t->c.v + r0 * t->vlat; ot[n].vy = cosf(ht) * t->c.v + r1 * t->vlat; cual[n++] = i;
            }
            or2_rebufo(&fis, ot, n);
            /* +B72 de cada candidato: 180 al ser el mejor con rebufo y luego uno menos por cuadro (doble de puntos
               si se le adelanta mientras dura) */
            for (i = 0; i < n; i++) if (fis.rebufo_marca & (1u << i)) traf[cual[i]].b72 = 180;
            for (i = 0; i < NTRAF; i++) if (traf[i].b72 > 0) traf[i].b72--;
        }
        x0 = c->x;
        or2_paso(&fis, &fpar, &e, 0.0f);
        c->v = fis.velocidad; c->a = dif_ang(fis.rumbo_vel - th0); c->deriva = fis.beta;
        c->derrape = fis.temporizador > 0 ? (fis.beta >= 0 ? 1 : -1) : 0; c->t_derrape = fis.temporizador;
        c->s += c->v * cosf(c->a) * dt;
        c->x += c->v * sinf(c->a) * dt + fis.corr_lat;         /* + la correccion lateral del rebote */
        fis.corr_lat = 0;
        if (fis.recolocar) { fis.recolocar = 0; grua_on = 0; c->s = grua_s; c->x = grua_x; }   /* fin de la grua */
        ruta_en(c->s, p, &th1, &iz, &de);
        c->a = dif_ang(c->a - (th1 - th0));
        {   /* muros: CbwColiWall del original (roce o rebote) con la normal hacia la calzada. El limite es el de la
               colision del original (franjas de muro o el vacio pasado el arcen, la hierba o la arena); sin esos datos,
               el borde del asfalto */
            float nx = 0, ny = 0, pen = 0, h, li, ld, xa, xb; c->choque = 0; if (c->t_muro > 0) c->t_muro--;
            limites(c->s, &li, &ld);
            lim = li - 1.0f;
            if (c->x < -lim) { pen = -lim - c->x; c->x = -lim; nx = cosf(th1); ny = -sinf(th1); }
            lim = ld - 1.0f;
            if (c->x > lim) { pen = c->x - lim; c->x = lim; nx = -cosf(th1); ny = sinf(th1); }
            if (muro_interior(c->s, c->x, 1.0f, &xa, &xb)) {     /* un muro entre las calzadas */
                if (x0 < (xa + xb) * 0.5f) { pen = c->x + 1.0f - xa; c->x = xa - 1.0f; nx = -cosf(th1); ny = sinf(th1); }
                else { pen = xb - (c->x - 1.0f); c->x = xb + 1.0f; nx = cosf(th1); ny = -sinf(th1); }
            }
            if (nx != 0 || ny != 0) {
                int golpe = or2_choque_muro(&fis, &fpar, nx, ny, pen);   /* 2 = choque fuerte (estados 1, 2, 5) */
                c->choque = 1; c->t_muro = 30;
                if (golpe && sfx_ok && !golpe_cd) { golpe_cd = 20; sfx_golpe(fminf(c->v / 40.0f, 1.0f)); }
                h = atan2f(fis.vx, fis.vy);                     /* rumbo de la velocidad (+ derecha) */
                c->v = sqrtf(fis.vx * fis.vx + fis.vy * fis.vy);
                c->a = dif_ang(h - th1); c->deriva = dif_ang(-fis.psi - h);
            }
        }
        memcpy(c->pose_t, fis.pose_t, sizeof c->pose_t); memcpy(c->pose_r, fis.pose_r, sizeof c->pose_r);
        coche_coloca(c);
        return;
    }
    g = fminf(c->v / 8.0f, 1.0f) * (1.0f - 0.45f * c->v / VMAX);
    /* temporizador de derrape (RearGripCtrl): toque de freno con el gas no a fondo, o soltar gas frenando */
    f = c->t_derrape > 0 ? c->t_derrape - 1 : 0;
    if (c->v > 16.0f && fabsf(giro) > 0.2f &&
        ((freno > 0.04f && c->freno_ant <= 0.04f && gas < 0.98f) || (gas < 0.98f && c->gas_ant >= 0.98f && freno > 0.04f))) f = 40;
    if (f > 0 && fabsf(c->deriva) > 0.147f) f = f > 12 ? f : 12;      /* se mantiene mientras deriva > 8,4 grados */
    c->t_derrape = f; c->derrape = f > 0 ? (c->deriva >= 0 ? 1 : -1) : 0;
    c->freno_ant = freno; c->gas_ant = gas;
    {   /* sesgo de la curva que viene: el angulo de equilibrio del derrape */
        float q[3], tq; ruta_en(c->s + 40.0f, q, &tq, NULL, NULL);
        sesgo = fmaxf(-0.6f, fminf(0.6f, 2.5f * (tq - th0)));
    }
    if (f > 0) {
        obj = giro * 0.6f + sesgo * 0.8f;
        if (giro * c->deriva < 0) obj = c->deriva * (1.0f - 1.5f * fabsf(giro)) * 0.5f;   /* contravolante: endereza */
        c->deriva += (obj - c->deriva) * 2.2f * dt;
        if (c->deriva > DERIVA_MAX) c->deriva = DERIVA_MAX;
        if (c->deriva < -DERIVA_MAX) c->deriva = -DERIVA_MAX;
        freno *= 0.25f;                                         /* el derrape casi no frena */
        yaw = giro * 1.6f * g * 1.25f;                          /* derrapando se cierra mas la curva */
        c->a -= c->a * 1.5f * fabsf(sinf(c->deriva)) * dt;      /* ayuda: el avance va hacia la carretera */
    } else {
        c->deriva += (giro * 0.05f * fminf(c->v / 30.0f, 1.0f) - c->deriva) * 8.0f * dt;
        yaw = giro * 1.6f * g;
        c->a -= c->a * 0.15f * dt;
    }
    c->a += yaw * dt;
    c->v += (gas * 13.0f * (1.0f - c->v / VMAX) - freno * 20.0f - 0.00025f * c->v * c->v - 0.3f) * dt;
    if (c->v < 0) c->v = 0;
    c->s += c->v * cosf(c->a) * dt;
    c->x += c->v * sinf(c->a) * dt;
    ruta_en(c->s, p, &th1, &iz, &de);
    c->a -= th1 - th0;                                          /* la carretera gira bajo el coche */
    {   /* muros: un golpe al tocar (15 %) y un roce suave mientras se sigue pegado; el coche rebota hacia dentro */
        int antes = c->t_muro > 0; c->choque = 0; if (c->t_muro > 0) c->t_muro--;
        lim = iz - 1.0f;
        if (c->x < -lim) { c->x = -lim; if (c->a < 0) c->a = -c->a * 0.4f; c->choque = 1; }
        lim = de - 1.0f;
        if (c->x > lim) { c->x = lim; if (c->a > 0) c->a = -c->a * 0.4f; c->choque = 1; }
        if (c->choque) { c->v -= c->v * (antes ? 0.35f * dt : 0.15f); c->deriva *= 0.9f; c->t_muro = 30; }
    }
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
    vt = kap > 0.0005f ? fminf(sqrtf(40.0f / kap), 140.0f) : 140.0f;   /* aceleracion lateral prudente: unos 4 g */
    *gas = c->v < vt ? 1.0f : 0.0f;
    *freno = c->v > vt + 4.0f ? 1.0f : 0.0f;
}

/* matriz del coche: x del modelo a la derecha, y arriba, el morro (-z) hacia delante */
static void pon_coche(const coche_t *c, matrix_t mod_coche)
{
    float r[3], fw[3], up[3], r2[3], k, sn; int i;               /* derecha = adelante x arriba */
    memcpy(fw, c->fw, 12); memcpy(up, c->up, 12);
    r[0] = fw[1] * up[2] - fw[2] * up[1]; r[1] = fw[2] * up[0] - fw[0] * up[2]; r[2] = fw[0] * up[1] - fw[1] * up[0];
    /* animacion de choque (pc_sample_crash_pose): giro y (+ izquierda), x (morro arriba), z, y desplazamiento */
    if (c->pose_r[1] != 0) { k = cosf(c->pose_r[1]); sn = sinf(c->pose_r[1]);
        for (i = 0; i < 3; i++) { r2[i] = r[i] * k + fw[i] * sn; fw[i] = fw[i] * k - r[i] * sn; r[i] = r2[i]; } }
    if (c->pose_r[0] != 0) { k = cosf(c->pose_r[0]); sn = sinf(c->pose_r[0]);
        for (i = 0; i < 3; i++) { r2[i] = fw[i] * k + up[i] * sn; up[i] = up[i] * k - fw[i] * sn; fw[i] = r2[i]; } }
    if (c->pose_r[2] != 0) { k = cosf(c->pose_r[2]); sn = sinf(c->pose_r[2]);
        for (i = 0; i < 3; i++) { r2[i] = up[i] * k - r[i] * sn; r[i] = r[i] * k + up[i] * sn; up[i] = r2[i]; } }
    memset(mod_coche, 0, sizeof(matrix_t));
    mod_coche[0][0] = r[0]; mod_coche[0][1] = r[1]; mod_coche[0][2] = r[2];
    mod_coche[1][0] = up[0]; mod_coche[1][1] = up[1]; mod_coche[1][2] = up[2];
    mod_coche[2][0] = -fw[0]; mod_coche[2][1] = -fw[1]; mod_coche[2][2] = -fw[2];
    for (i = 0; i < 3; i++)
        mod_coche[3][i] = c->pos[i] + r[i] * c->pose_t[0] + up[i] * c->pose_t[1] - fw[i] * c->pose_t[2];
    mod_coche[3][3] = 1.0f;
}

/* ---------------------------------------------------------------- huellas y humo del derrape */
#define NHUELLA 256
#define NHUMO 32
typedef struct { float p[4][3]; } huella_t;
static huella_t huellas[NHUELLA]; static int nhuella, huella_sig;
static float rueda_ant[2][3]; static int hay_ant;
typedef struct { float p[3], edad; } humo_t;
static humo_t humos[NHUMO]; static int humo_sig;
static pvr_ptr_t tex_humo; static pvr_poly_hdr_t cab_humo, cab_huella;

static void efectos_inicia(void)
{
    static uint16_t t[32 * 32] __attribute__((aligned(32))); int x, y; pvr_poly_cxt_t cx;
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++) {           /* mancha redonda y suave (ARGB4444, lineal) */
        float d = sqrtf((x - 15.5f) * (x - 15.5f) + (y - 15.5f) * (y - 15.5f)) / 15.5f, a = fmaxf(0, 1 - d);
        t[y * 32 + x] = ((int)(a * a * 15) << 12) | 0x0EEE;
    }
    tex_humo = pvr_mem_malloc(sizeof t); pvr_txr_load(t, tex_humo, sizeof t);
    pvr_poly_cxt_txr(&cx, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, 32, 32, tex_humo, PVR_FILTER_BILINEAR);
    cx.gen.fog_type = PVR_FOG_TABLE; cx.txr.env = PVR_TXRENV_MODULATEALPHA; pvr_poly_compile(&cab_humo, &cx);
    pvr_poly_cxt_col(&cx, PVR_LIST_TR_POLY); cx.gen.fog_type = PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&cab_huella, &cx);
}
static void efectos_borra(void) { nhuella = huella_sig = 0; hay_ant = 0; memset(humos, 0, sizeof humos); }

/* cada cuadro: huellas de las ruedas traseras y humo mientras se derrapa */
static void efectos_paso(const coche_t *c, float dt)
{
    int i, k; float r[3] = { -c->fw[2], 0, c->fw[0] }, q[2][3];
    for (i = 0; i < NHUMO; i++) if (humos[i].edad > 0) { humos[i].edad += dt; humos[i].p[1] += 0.8f * dt; if (humos[i].edad > 1.0f) humos[i].edad = 0; }
    if (!c->derrape) { hay_ant = 0; return; }
    for (k = 0; k < 2; k++) {
        float lado = k ? 0.75f : -0.75f;
        q[k][0] = c->pos[0] - 1.3f * c->fw[0] + lado * r[0]; q[k][2] = c->pos[2] - 1.3f * c->fw[2] + lado * r[2];
        q[k][1] = c->pos[1] + 0.04f;
    }
    if (hay_ant) for (k = 0; k < 2; k++) {
        huella_t *h = huellas + huella_sig; float a = 0.13f;
        h->p[0][0] = rueda_ant[k][0] - a * r[0]; h->p[0][1] = rueda_ant[k][1]; h->p[0][2] = rueda_ant[k][2] - a * r[2];
        h->p[1][0] = rueda_ant[k][0] + a * r[0]; h->p[1][1] = rueda_ant[k][1]; h->p[1][2] = rueda_ant[k][2] + a * r[2];
        h->p[2][0] = q[k][0] - a * r[0]; h->p[2][1] = q[k][1]; h->p[2][2] = q[k][2] - a * r[2];
        h->p[3][0] = q[k][0] + a * r[0]; h->p[3][1] = q[k][1]; h->p[3][2] = q[k][2] + a * r[2];
        huella_sig = (huella_sig + 1) % NHUELLA; if (nhuella < NHUELLA) nhuella++;
    }
    memcpy(rueda_ant, q, sizeof q); hay_ant = 1;
    {   static int alterna; if (++alterna & 1) { humo_t *h = humos + humo_sig; memcpy(h->p, q[alterna & 2 ? 1 : 0], 12); h->edad = 0.01f; humo_sig = (humo_sig + 1) % NHUMO; } }
}

static void pt_envia(const float *p, float u, float v, uint32_t c, int fin)
{
    float x = p[0], y = p[1], z = p[2], w; pt_t t;
    mat_trans_single3_nodivw(x, y, z, w);
    if (w < cerca) return;
    t.x = x; t.y = y; t.w = w; t.u = u; t.v = v; t.c = c; envia(&t, fin);
}
/* en la lista translucida, con la matriz de vista cargada */
static void efectos_dibuja(void)
{
    int i;
    pvr_prim(&cab_huella, sizeof cab_huella);
    for (i = 0; i < nhuella; i++) {
        const huella_t *h = huellas + i;
        float dx = h->p[0][0] - ojo[0], dz = h->p[0][2] - ojo[2];
        if (dx * dx + dz * dz > 250.0f * 250.0f || dx * ade[0] + dz * ade[2] < 1.0f) continue;
        pt_envia(h->p[0], 0, 0, 0x60101010, 0); pt_envia(h->p[1], 0, 0, 0x60101010, 0);
        pt_envia(h->p[2], 0, 0, 0x60101010, 0); pt_envia(h->p[3], 0, 0, 0x60101010, 1);
    }
    pvr_prim(&cab_humo, sizeof cab_humo);
    for (i = 0; i < NHUMO; i++) {
        const humo_t *h = humos + i; float t = h->edad, tam, p[4][3]; int k; uint32_t col;
        if (t <= 0) continue;
        tam = 0.6f + 2.6f * t; col = ((uint32_t)((1 - t) * 140) << 24) | 0xE0E0E0;
        for (k = 0; k < 4; k++) {
            float sx = (k & 1) ? tam : -tam, sy = (k & 2) ? -tam : tam;
            p[k][0] = h->p[0] + sx * der[0] + sy * arr[0]; p[k][1] = h->p[1] + sx * der[1] + sy * arr[1] + tam * 0.5f;
            p[k][2] = h->p[2] + sx * der[2] + sy * arr[2];
        }
        pt_envia(p[0], 0, 0, col, 0); pt_envia(p[1], 1, 0, col, 0); pt_envia(p[2], 0, 1, col, 0); pt_envia(p[3], 1, 1, col, 1);
    }
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
#define NPISTAS 3
/* camara de la seleccion de coche: distancia, altura y punto de mira sobre el coche (m) */
#define MENU_CAM_D 6.0f
#define MENU_CAM_Y 1.4f
#define MENU_CAM_MIRA 0.2f
#define PERIODO 16683                                           /* us por paso de logica (59,94 Hz) */
/* bifurcacion al final de la primera etapa: rama izquierda y derecha */
static const char *RAMA_FICH[2] = { "SEG07.DAT", "SEG03.DAT" }, *RAMA_NOMBRE[2] = { "BOSQUE", "INDUSTRIA" };
static int t_cuenta = 9999, t_ext = 9999, t_bif;
static int etapa_n, hud_ok, crono, puntos; static float vmax;
/* ---------------------------------------------------------------- puntuacion del modo OutRun
   Del original (PS2 0x28F8C8 / PC 4F2DF0 cada cuadro; 4F2860 adelantamientos; 4F2CD0 estadisticas; 4F2B20 bonus de
   etapa; 4B9770 suma y cola de mensajes; 4BD480 dibujo). puntos = [8447DC] del jugador. La posicion de la
   carretera (+64, +18C la del cuadro anterior) se toma de TR_POS metros, como en el resto de la demo. */
#define PT_MSGS 10
typedef struct { int fila, tipo, pts, edad, y, estilo; float alfa; } pt_msg_t;   /* 8444C8 (0x24 bytes) */
static pt_msg_t pt_msg[PT_MSGS];
static int pt_pos_ant, pt_odo_pos, pt_cifra, pt_estilo, pt_crono0;
static float pt_odo;
static int pt_est[7];                                           /* 84DF1C..84DF34 */
static int pt_pos(void) { return (int)floorf(yo.s / TR_POS); }
/* 4B9770(coche, otro, puntos, tipo): suma y, para los tipos 1..4 y 6, un mensaje en el primer hueco de los diez.
   estilo de las cifras del mensaje (+18): 0 normal (1 y 0x11), 1 rebufo (0xC, 0xE), 2 tocado (0xD, 0xF) */
static void pt_suma(int pts, int tipo, int estilo)
{
    int k, j, vivos = 0, viejos = 0; pt_msg_t *r;
    puntos += pts;
    if (tipo <= 0 || (tipo > 4 && tipo != 6)) return;
    for (k = 0; k < PT_MSGS && pt_msg[k].edad > 0; k++) ;
    if (k == PT_MSGS) return;
    for (j = 0; j < PT_MSGS; j++) if (pt_msg[j].edad > 0) { vivos++; if (pt_msg[j].edad >= 0x8c) viejos++; }
    r = pt_msg + k;
    r->fila = vivos; r->y = (vivos + 1) * 0x1a; r->tipo = tipo; r->pts = pts; r->edad = viejos + 0x8c; r->alfa = 0;
    r->estilo = estilo;
}
static void pt_reinicia_est(void) { memset(pt_est, 0, sizeof pt_est); pt_crono0 = crono; }
static void pt_inicia(void)
{
    puntos = 0; memset(pt_msg, 0, sizeof pt_msg); pt_reinicia_est();
    pt_pos_ant = pt_odo_pos = pt_pos(); pt_odo = 0; pt_cifra = 0; pt_estilo = 0;
}
/* 4F2B20(segundos de la etapa): bonus de fin de etapa (tiempo por debajo de 60 s, conduccion limpia, rebufo) */
static void pt_bonus_etapa(void)
{
    int t = (int)((float)(crono - pt_crono0) / 60.2f), b = t < 60 ? (60 - t) * 1000 : 0;
    if (!pt_est[0]) b += 2500;                                  /* sin choques de estado 1 */
    if (!pt_est[1]) b += 2500;                                  /* sin choques de estados 2, 5, 6, 12, 13 */
    if (!pt_est[2]) b += 5000;                                  /* sin tocar el muro (+4 0x20000) */
    if (!pt_est[3]) b += 10000;                                 /* sin salirse del asfalto (+244 bit 2) */
    if (!pt_est[4]) b += 2500;                                  /* sin tocar el trafico (+4 0x10000) */
    if (!pt_est[5]) b += 10000;                                 /* sin golpes con el trafico (+4 0x400000) */
    if (pt_est[6]) b += pt_est[6] * 1000 / 60;                  /* 1000 por segundo de rebufo */
    pt_suma(b - b % 10 + 10, 4, 0);
    pt_reinicia_est();
}
/* un cuadro de carrera: 4F2860 (adelantamientos del trafico), 4F2CD0 (estadisticas) y los puntos por avance */
static void pt_paso(void)
{
    int i, pos = pt_pos(), gap, rebufo, ang, fuera;
    for (i = 0; i < NTRAF; i++) {                               /* 4F2860 */
        trafico_t *t = traf + i; int clase, pts, tipo;
        if (t->carril < 0 || (t->pt_marca & PT_PASADO)) continue;
        if (pos - (int)floorf(t->c.s / TR_POS) < -2 || t->c.s - yo.s > 0.0f) continue;   /* aun delante */
        clase = trc_ok ? trc[t->modelo].clase : 3;
        if (clase == 5 && pos - (int)floorf(t->c.s / TR_POS) < 2) continue;
        t->pt_marca |= PT_PASADO;
        if (t->pt_marca & PT_SIN) continue;
        if (clase == 5) { pts = (t->pt_marca & PT_MITAD) ? 12500 : t->b72 > 0 ? 50000 : 25000; tipo = 3; }
        else {
            tipo = 1; pts = 750;
            if (clase < 3) { tipo = 2; pts = 7500; }            /* pesados: autobuses, camiones */
            if (t->pt_marca & PT_MITAD) pts >>= 1; else if (t->b72 > 0) pts <<= 1;
        }
        pt_suma(pts, tipo, (t->pt_marca & PT_MITAD) ? 2 : t->b72 > 0 ? 1 : 0);
    }
    ang = fis_ok ? (int)(fabsf(fis.beta) * 10430.3779f) : 0x7fff;   /* |+162| */
    rebufo = fis_ok && fis.rebufo > 0.180000007f && ang < 1500;
    fuera = (sup_jug & 0x9cu) || yo.choque == 1;                /* +244 & 0x9C: superficies 2, 3, 4, 7 o muro */
    /* 4F2CD0 */
    if (fis_ok && fis.t_choque > 0 && fis.choque >= 1 && fis.choque <= 13) {
        static const uint8_t B[13] = { 0, 1, 2, 2, 1, 1, 2, 2, 2, 2, 2, 1, 1 };
        if (B[fis.choque - 1] < 2) pt_est[B[fis.choque - 1]]++;
    }
    if (!(sup_jug & 2u)) pt_est[3]++;
    if (pt_toca) pt_est[4]++;
    if (yo.choque == 1) pt_est[2]++;
    if (pt_toca & 2) pt_est[5]++;
    if (rebufo) pt_est[6]++;
    pt_toca = 0;
    /* PS2 0x28F984: |+64 - +18C| x 10, x 110 con rebufo por encima de la punta (+1C4 > +E64); nada fuera */
    gap = pos > pt_pos_ant ? pos - pt_pos_ant : pt_pos_ant - pos; pt_pos_ant = pos;
    rebufo = rebufo && fis.vel_mf > fpar.v_tope;
    if (!fuera) pt_suma((int)((float)gap * (rebufo ? 110.0f : 10.0f)), 0, 0);
    pt_estilo = rebufo ? 1 : fuera ? 2 : 0;
    /* 4B96F0: la ultima cifra corre con la velocidad (+1F8 x 0,00915) y vuelve a 0 en cada posicion */
    if (!fuera) {
        pt_odo += (fis_ok ? fis.velocidad_hud : yo.v * 3.6f) * 0.00915000029f;
        if (pt_odo_pos != pos) { pt_odo_pos = pos; pt_odo = 0; }
    }
    pt_cifra = (int)pt_odo % 10;
}
/* 4BD480: la cola de mensajes (orden, y, alfa y edad); una vez por cuadro fuera de la pausa */
static void pt_msg_paso(void)
{
    int k, j;
    for (k = 0; k < PT_MSGS; k++) {
        pt_msg_t *r = pt_msg + k; int ord = 0;
        if (r->edad <= 0) continue;
        for (j = 0; j < PT_MSGS; j++) if (j != k && pt_msg[j].edad > 0 && pt_msg[j].edad <= r->edad) ord++;
        r->fila = ord;
        if (r->edad < 10) r->y = (int)((float)r->y * 0.666666687f);
        else { int t = (ord + 1) * 0x1a; r->y = t - (int)((float)(r->y - t) * -0.666666687f); }
        if (r->edad < 60) {
            if (r->edad >= 50) r->alfa += 0.100000001f;
            if (r->edad < 10) r->alfa *= 0.800000012f;
        }
        r->edad--;
    }
}
/* lo que muestra el HUD: el total mas la cifra que corre, menos los puntos de los mensajes que aun estan en cola */
static int pt_visible(void)
{
    int k, v = puntos + pt_cifra;
    for (k = 0; k < PT_MSGS; k++) if (pt_msg[k].edad > 0) v -= pt_msg[k].pts;
    return v;
}
static void texto_arriba(const char *l1, const char *l2, const char *l3);
/* panel de resultados al terminar (meta o tiempo agotado) */
static void resultados(int meta)
{
    char a[64], b[64], c[64]; int ms = (int)(crono * 1000.0f / 60.0f);
    sprintf(a, meta ? "   GOAL!   %s" : "   TIEMPO AGOTADO", meta ? (etapa_n ? "RECORRIDO COMPLETO" : "") : "");
    sprintf(b, "   TIEMPO TOTAL %d'%02d\"%03d", ms / 60000, (ms / 1000) % 60, ms % 1000);
    sprintf(c, "   PUNTOS %07d   MAX %d km/h", puntos, (int)vmax);
    texto_arriba(a, b, c);
}                                             /* 0: la primera; 1: ya en una rama */
#define TIEMPO_ETAPA 80.0f
#ifndef VER
#define VER 0
#endif
#ifndef SALIDA
#define SALIDA 100.0f                                           /* metros desde el principio de la ruta */
#endif

int main(int argc, char **argv)
{
    /* bufer de vertices de 1 MB (x2) y 4 OPB de reserva: en la consola real un bufer u OPB lleno cuelga el TA */
    pvr_init_params_t pp = { { PVR_BINSIZE_16, 0, PVR_BINSIZE_16, 0, PVR_BINSIZE_16 }, 1024 * 1024, 0, 0, 0, 4 };
    int estado = TITULO, auto_ = 0, ver = VER, pausa = 0, l, i, r;
    uint32_t cuadro = 0, bot_ant = 0, t_estado = 0; uint64_t t0, tcpu = 0, c0;
    float cam_y = 0, tiempo = TIEMPO_ETAPA, fps = 0, ms = 0, cam_fw[3] = { 0, 0, -1 }, ttit = 0;
    char s1[96], s2[96], s3[96], msg[32] = ""; int aviso_rama = 0, pista = 0, pasos = 1, paso, saltos = 0, saltos_ver = 0;
    uint64_t reloj = 0; float lejos_obj = lejos;
    uint64_t c_log = 0, t_log = 0, t_ini = timer_ms_gettime64(); float ms_log = 0, t_carga = 0;
    (void)argc; (void)argv;

    /* 1: datos del disco a la RAM (la pantalla de texto aun sin PowerVR) */
    vid_set_mode(DM_640x480, PM_RGB565);
    aviso("OutRun 2006 C2C - demo Dreamcast. Cargando la etapa...");
    if ((r = lee_modelo(&etapa, "SEG00.DAT")) || (r = lee_modelo(&coche, "COCHE.DAT"))) {
        sprintf(s1, "No se pudieron leer los datos (%d)", r); aviso(s1); for (;;) thd_sleep(1000);
    }
    for (i = 0; i < NMODTRAF; i++) {
        char nom[16]; sprintf(nom, "TRAF%d.DAT", i);
        if ((r = lee_modelo(&mtraf[i], nom))) { sprintf(s1, "No se pudo leer %s (%d)", nom, r); aviso(s1); for (;;) thd_sleep(1000); }
    }
    traf_medidas();
    traf_tablas();                                              /* clases y cajas del trafico (TRAFC.DAT) */
    ruta_prepara();
    fisica_carga();
    /* 2: PowerVR, texturas a la memoria de video y cabeceras */
    pvr_init(&pp);
    pvr_set_bg_color(0.45f, 0.65f, 0.95f);
    txt_arr = pvr_mem_malloc(sizeof buf_arr); txt_cen = pvr_mem_malloc(sizeof buf_cen);
    for (i = 0, r = 0; i < NMODTRAF; i++) r |= sube_modelo(&mtraf[i]);
    if (r || sube_modelo(&etapa) || sube_modelo(&coche)) {
        vid_set_mode(DM_640x480, PM_RGB565); aviso("Sin memoria de video para las texturas"); for (;;) thd_sleep(1000);
    }
    hud_ok = !hud_carga("HUD.DAT", 6);                          /* 6 = tacometro del Testarossa */
    for (l = 0; l < 3; l++) for (i = 0; i < 2; i++) {
        pvr_poly_cxt_t cx; pvr_poly_cxt_col(&cx, LISTA[l]);
        cx.gen.fog_type = i ? PVR_FOG_DISABLE : PVR_FOG_TABLE; cx.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&cab_col[i][l], &cx);
    }
    efectos_inicia();
    pvr_fog_table_color(1.0f, 0.55f, 0.70f, 0.92f);
    pvr_fog_table_linear(lejos * 0.55f, lejos);
    memset(&yo, 0, sizeof yo);
    coche_paso(&yo, 0, 0, 0, 0);
    t_carga = (timer_ms_gettime64() - t_ini) / 1000.0f;
    musica_inicia();                                            /* la musica la pone el menu (sin MENU.DAT, aqui) */
    sfx_ok = !sfx_inicia();
    texto_arriba("", NULL, NULL); texto_centro("");
    /* menus del original (MENU.DAT): usan la VRAM del HUD de carrera, que se baja mientras tanto */
    if (menu_inicia()) { sprintf(s1, "Sin MENU.DAT"); texto_centro(s1); musica_pon(pista); }
    if (hud_ok) hud_baja_tex();
    menu_entra(manual_, pista, NPISTAS);
    t0 = timer_ms_gettime64();

    for (;;) {
        maple_device_t *mc = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = mc ? (cont_state_t *)maple_dev_status(mc) : NULL;
        uint32_t b = st ? st->buttons : 0, nuevo = b & ~bot_ant;
        float giro = 0, gas = 0, freno = 0, ojo_[3], mira[3];
        bot_ant = b; auto_piloto = auto_; t_estado++; c_log = timer_us_gettime64();
        {   /* salto de cuadros: tantos pasos de logica (60 Hz) como haya pasado de tiempo real, hasta 3 */
            uint64_t ahora = timer_us_gettime64();
            if (!reloj) reloj = ahora;
            pasos = (int)((ahora - reloj + PERIODO / 2) / PERIODO);
            if (pasos < 1) pasos = 1;
            if (pasos > 3) { pasos = 3; reloj = ahora - 3 * PERIODO; }
            reloj += (uint64_t)pasos * PERIODO; saltos += pasos - 1;
        }
        musica_poll();
        if (estado != TITULO && (nuevo & CONT_Y)) ver ^= 1;      /* en el menu, Y es el tema al azar */
        if (estado != TITULO && manual_) {                      /* cambio manual: cruceta arriba sube, abajo baja */
            if (nuevo & CONT_DPAD_UP) pide_cambio = 1;
            if (nuevo & CONT_DPAD_DOWN) pide_cambio = -1;
        }
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

        /* estados (una vez por paso de logica) */
        for (paso = 0; paso < pasos; paso++) {
        if (paso) { nuevo = 0; t_estado++; }
        if (t_cuenta < 9999) t_cuenta++;
        if (t_ext < 9999) t_ext++;
        if (golpe_cd) golpe_cd--;
        t_bif++;
        if (estado == TITULO) {
            int rm = menu_paso(nuevo);                          /* titulo, modos, coche, cambio y musica */
            ttit += 1.0f / 60;
#ifdef PRUEBA_JUGADOR
            rm = MENU_CARRERA;                                  /* prueba: directo a la carrera con el mando */
#endif
            if (rm != MENU_SIGUE) {
                auto_ = rm == MENU_DEMO;
                if (!auto_) { manual_ = menu_manual(); pista = menu_pista(); }
                menu_sale();                                    /* la VRAM del menu vuelve al HUD */
                if (hud_ok && hud_sube_tex()) hud_ok = 0;
                if (etapa_n) {                                  /* volver a la primera etapa */
                    libera_modelo(&etapa);
                    if (lee_modelo(&etapa, "SEG00.DAT") || sube_modelo(&etapa)) { vid_set_mode(DM_640x480, PM_RGB565); aviso("Error"); for (;;) thd_sleep(1000); }
                    ruta_prepara(); etapa_n = 0;
                }
                memset(&yo, 0, sizeof yo); yo.s = SALIDA; yo.x = carril_x(SALIDA, 4);
#ifdef PRUEBA_X
                yo.x = PRUEBA_X;                                /* prueba: salida en otro desvio (p.ej. la hierba) */
#endif
                coche_paso(&yo, 0, 0, 0, 0); traf_inicia(SALIDA); efectos_borra();
                if (fis_ok) { float p[3], th; ruta_en(yo.s, p, &th, NULL, NULL); or2_coloca(&fis, &fpar, th, 0); }
                tiempo = TIEMPO_ETAPA; estado = CUENTA; t_estado = 0; t_cuenta = 0; pausa = 0; crono = 0; vmax = 0; pt_inicia();
            }
        } else if (estado == CUENTA) {
            int n = 3 - (int)(t_estado / 60);
            sprintf(s1, n > 0 ? "%d" : "GO!", n);
            if (strcmp(s1, msg)) { strcpy(msg, s1); texto_centro(msg); }
            if (t_estado >= 240) { estado = CARRERA; t_estado = 0; }      /* GO! a los 4 s de la escena */
            if (auto_ && (nuevo & CONT_START)) { estado = TITULO; auto_ = 0; nuevo = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS); }
            coche_paso(&yo, 0, 0, 0, 1.0f / 60);
        } else if (estado == CARRERA) {
            if (t_estado == 50 && !aviso_rama) texto_centro(auto_ ? "DEMO" : "");
            if (auto_ && (nuevo & CONT_START)) { estado = TITULO; nuevo = 0; auto_ = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS); }
            else if (nuevo & CONT_START) pausa ^= 1;
            if (!pausa) {
#ifdef PRUEBA_CHOQUE
                if (t_estado == 60 && fis_ok) {                 /* prueba: a ~280 km/h y 50 grados contra el muro derecho */
                    float p[3], th; ruta_en(yo.s, p, &th, NULL, NULL);
                    or2_coloca(&fis, &fpar, th + 0.87f, 280.0f / (fpar.hud * 3.6f));
                }
#endif
                if (auto_) piloto(&yo, &giro, &gas, &freno);
                coche_paso(&yo, giro, gas, freno, 1.0f / 60);
                traf_paso(1.0f / 60);
                efectos_paso(&yo, 1.0f / 60);
                tiempo -= 1.0f / 60;
                crono++; if (fis_ok && fis.velocidad_hud > vmax) vmax = fis.velocidad_hud;
                pt_paso(); pt_msg_paso();                       /* puntuacion del original */
                if (etapa_n == 0 && yo.s >= longitud - 400.0f && yo.s < longitud - 40.0f && t_estado % 30 == 0) {
                    char b[40]; sprintf(b, "< %s  %s >", RAMA_NOMBRE[0], RAMA_NOMBRE[1]); texto_centro(b); aviso_rama = 1;
                }
                if (etapa_n == 0 && yo.s >= longitud - 40.0f) {   /* bifurcacion: la rama del lado por el que va */
                    int rama = yo.x < carril_x(yo.s, 0) * 0.5f + carril_x(yo.s, NCARRIL - 1) * 0.5f ? 0 : 1;
                    uint64_t tc = timer_ms_gettime64(); float v = yo.v;
                    pt_bonus_etapa();                           /* 4F2B20 al pasar la etapa */
                    texto_centro("CARGANDO");
                    pvr_wait_ready(); pvr_scene_begin();
                    pvr_list_begin(PVR_LIST_OP_POLY); pvr_list_finish();
                    pvr_list_begin(PVR_LIST_TR_POLY); rect_txr(txt_cen, 256, 32, 64, 200, 512, 64, PVR_LIST_TR_POLY); pvr_list_finish();
                    pvr_scene_finish(); pvr_wait_ready();
                    libera_modelo(&etapa);
                    if (lee_modelo(&etapa, RAMA_FICH[rama]) || sube_modelo(&etapa)) {
                        vid_set_mode(DM_640x480, PM_RGB565); aviso("No se pudo cargar la etapa siguiente"); for (;;) thd_sleep(1000);
                    }
                    ruta_prepara(); etapa_n = 1;
                    t_carga = (timer_ms_gettime64() - tc) / 1000.0f;
                    memset(&yo, 0, sizeof yo); yo.s = 40.0f; yo.x = carril_x(40.0f, 4); yo.v = v; coche_paso(&yo, 0, 0, 0, 0);
                    if (fis_ok) { float p[3], th; ruta_en(yo.s, p, &th, NULL, NULL); or2_coloca(&fis, &fpar, th, v); }
                    traf_inicia(yo.s); efectos_borra(); tiempo += 60.0f; aviso_rama = 0; t_ext = 0;
                    pt_pos_ant = pt_odo_pos = pt_pos(); pt_odo = 0;
                    sprintf(msg, "%s  +60", RAMA_NOMBRE[rama]); texto_centro(msg); t_estado = 0;
                }
                else if (yo.s >= longitud - 40.0f) { estado = META; t_estado = 0; pt_bonus_etapa(); texto_centro("GOAL!"); resultados(1); }
                else if (tiempo <= 0) { tiempo = 0; estado = AGOTADO; t_estado = 0; texto_centro("TIEMPO AGOTADO"); resultados(0); }
            }
        } else {                                                /* META o AGOTADO: el coche se para solo */
            coche_paso(&yo, 0, 0, 0.6f, 1.0f / 60);
            pt_msg_paso();
            if ((nuevo & CONT_START) || t_estado > 600) {
                estado = TITULO; auto_ = 0; nuevo = 0; texto_centro(""); hud_baja_tex(); menu_entra(manual_, pista, NPISTAS);
            }
        }
        }

        /* camara: en el menu gira alrededor del coche (seleccion de coche); en carrera, detras del coche */
        if (estado == TITULO) {
            float a = ttit * 0.6f;
            yo.s = SALIDA; yo.x = carril_x(SALIDA, 4); yo.v = 0; coche_paso(&yo, 0, 0, 0, 0); cam_y = yo.pos[1];
            ojo_[0] = yo.pos[0] + MENU_CAM_D * sinf(a); ojo_[1] = yo.pos[1] + MENU_CAM_Y; ojo_[2] = yo.pos[2] + MENU_CAM_D * cosf(a);
            mira[0] = yo.pos[0]; mira[1] = yo.pos[1] + MENU_CAM_MIRA; mira[2] = yo.pos[2];
        } else {
            {   /* camara (SetCarCamera): entre el avance y la carroceria, mas hacia la carroceria con mas deriva */
                float p[3], th, d = yo.deriva * (0.5f + 0.3f * fabsf(sinf(yo.deriva))), cy, fwc[3];
                ruta_en(yo.s, p, &th, NULL, NULL); cy = th + yo.a + d;
                fwc[0] = sinf(cy); fwc[2] = -cosf(cy);
                for (i = 0; i < 3; i += 2) cam_fw[i] += (fwc[i] - cam_fw[i]) * 0.15f;
            }
            norm(cam_fw);
            cam_y += (yo.pos[1] - cam_y) * 0.15f;
            ojo_[0] = yo.pos[0] - 6.5f * cam_fw[0]; ojo_[1] = cam_y + 2.4f; ojo_[2] = yo.pos[2] - 6.5f * cam_fw[2];
            mira[0] = yo.pos[0] + 6.0f * cam_fw[0]; mira[1] = cam_y + 1.1f; mira[2] = yo.pos[2] + 6.0f * cam_fw[2];
        }

        /* efectos de sonido del coche (una vez por cuadro) */
        if (sfx_ok) {
            if (estado == TITULO || pausa || !fis_ok) sfx_para();
            else {
                sfx_motor((float)fis.rpm_i, fis.ac_motor / 256.0f, fis.marcha);
                sfx_derrape(yo.t_derrape ? fminf(fabsf(fis.desliz_grados) / 30.0f, 1.0f) : 0.0f);
            }
        }

        /* marcador (cada 6 cuadros) */
        if (cuadro % 6 == 0 && estado != META && estado != AGOTADO) {
            if (estado == TITULO) sprintf(s1, "MENU  VRAM menu %uK libre %uK", menu_vram() >> 10, (unsigned)(pvr_mem_available() >> 10));
            else sprintf(s1, "TIEMPO %3d  %3d km/h %d  %3d%%%s", (int)ceilf(tiempo), fis_ok ? (int)fis.velocidad_hud : (int)(yo.v * 3.6f), fis.marcha,
                         (int)(100 * yo.s / longitud), yo.choque ? "  !" : "");
            if (ver) {
                pvr_stats_t ps; struct mallinfo mi = mallinfo();
                pvr_get_stats(&ps);
                sprintf(s2, "%.1ffps %lutri cpu %.1f+%.1f pvr %.1fms", (double)fps, (unsigned long)tri_enviados,
                        (double)ms_log, (double)ms, (double)(ps.rnd_last_time / 1000000.0));
                sprintf(s3, "d%d/%d salto%d der%d%s ram%uK vram%uK c%.1fs x%.1f mu%.1f%.1f", (int)lejos, (int)lejos_obj, saltos_ver, (int)(yo.deriva * 57.3f), yo.t_derrape ? "D" : "",
                        (unsigned)(mi.uordblks >> 10), (unsigned)(pvr_mem_available() >> 10), (double)t_carga,
                        (double)yo.x, (double)fis.r[0].mu, (double)fis.r[2].mu);   /* desvio y mu delante/detras izq. */
            }
            texto_arriba(s1, ver ? s2 : NULL, ver ? s3 : NULL);
        }

        t_log += timer_us_gettime64() - c_log;
        pvr_wait_ready();
        c0 = timer_us_gettime64();
        if (estado == TITULO) pvr_set_bg_color(54 / 255.0f, 62 / 255.0f, 74 / 255.0f);   /* fondo gris del frontal */
        else pvr_set_bg_color(0.45f, 0.65f, 0.95f);
        pvr_scene_begin();
        fx = 320.0f / (0.75f + 0.07f * fminf(yo.v / 85.0f, 1.0f));   /* algo mas angular cuanto mas rapido */
        pon_vista(ojo_, mira);
        pon_coche(&yo, mod_coche);
        tri_enviados = 0; vtx_ult = vtx_env; vtx_env = 0;
        for (l = 0; l < 3; l++) {
            pvr_list_begin(LISTA[l]);
            if (estado != TITULO) {                             /* en el menu no se ve la etapa */
                mat_load(&vista_cielo);
                dibuja_rango(&etapa, l, etapa.inicio[3], etapa.inicio[4], 0, 1, 1);
                mat_load(&vista);
#ifdef DEP_LISTA
                color_fijo = l == DEP_LISTA ? 0xFFFF00FF : 0;
#endif
                dibuja_rango(&etapa, l, etapa.inicio[l], etapa.inicio[l + 1], 1, 0, 0);
                color_fijo = 0;
            }
            if (estado != TITULO || menu_coche3d()) {
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
            if (l == 2 && estado != TITULO) { mat_load(&vista); efectos_dibuja(); }
            if (l == 2) {
                int texto = ver || (!hud_ok && estado != TITULO);
                if (estado == TITULO) menu_dibuja();
                if (hud_ok && estado != TITULO) {
                    hud_estado_t he;
                    he.tiempo = (int)ceilf(tiempo); he.crono_ms = (int)(crono * 1000.0f / 60.0f); he.puntos = pt_visible(); he.puntos_estilo = pt_estilo;
                    he.kmh = fis_ok ? (int)fis.velocidad_hud : (int)(yo.v * 3.6f); he.marcha = fis_ok ? fis.marcha : 1;
                    he.etapa = etapa_n; he.cuadro = cuadro; he.rpm = fis_ok ? (float)fis.rpm_suave : 0;
                    he.rebufo = fis_ok && fis.rebufo > 0.18f; he.derrape = yo.t_derrape != 0; he.manual = manual_ && !auto_; he.rev_on = he.manual && fis_ok && fis.rpm_i > 7900 && fis.marcha < fpar.marchas; he.seis = fis_ok && fpar.marchas >= 6;
                    hud_dibuja(&he);
                    for (i = 0; i < PT_MSGS; i++)               /* mensajes de puntos (4BD480) */
                        if (pt_msg[i].edad > 0) hud_puntos_msg(pt_msg[i].pts, pt_msg[i].estilo, (float)pt_msg[i].y, pt_msg[i].alfa);
                    if (t_cuenta < 300) {                       /* 3, 2, 1, GO! (la escena dura 5 s; GO a los 4) */
                        static const char *const CU[5] = { "CUENTA_PRE", "CUENTA_3", "CUENTA_2", "CUENTA_1", "CUENTA_GO" };
                        hud_aviso(CU[t_cuenta / 60], t_cuenta % 60, 1);
                    }
                    if (estado == CARRERA && aviso_rama) hud_aviso("BIFURCA", t_bif % 60, 1);
                    if (t_ext < 240) hud_aviso("TIEMPO_EXT", t_ext / 5, 1);
                    if (estado == META) hud_aviso("META", t_estado < 59 ? t_estado : 59, 1);
                    if (estado == AGOTADO) hud_aviso("FIN_TIEMPO", t_estado < 40 ? t_estado : 40 + (t_estado - 40) % 8, 1);
                    if (he.rev_on) hud_aviso("CAMBIA", cuadro % 30, 1);
                }
                if (estado == META || estado == AGOTADO) {      /* resultados y fundido a negro al final */
                    caja(64, 150, 512, 110, 0xA0000000);
                    rect_txr(txt_arr, 512, 128, 68, 156, 512, 128, PVR_LIST_TR_POLY);
                    if (t_estado > 540) caja(0, 0, 640, 480, (uint32_t)(fminf((t_estado - 540) / 60.0f, 1.0f) * 255) << 24);
                    texto = 0;
                }
                if (texto) caja(36, 18, 568, ver ? 92 : 34, 0x90000000);
                if (texto) rect_txr(txt_arr, 512, 128, 40, 20, 512, 128, PVR_LIST_TR_POLY);
                if (estado != TITULO && (!hud_ok ? (estado != CARRERA || t_estado < 50 || auto_ || aviso_rama) : (estado == CARRERA && auto_ && t_estado >= 50))) rect_txr(txt_cen, 256, 32, 64, 180, 512, 64, PVR_LIST_TR_POLY);
            }
            pvr_list_finish();
        }
#ifdef DIAG_VTX
        if (cuadro % 60 == 0) dbglog(DBG_INFO, "DIAG est%d v%u tri %u lejos %d vram %u\n", estado, (unsigned)vtx_env, (unsigned)tri_enviados, (int)lejos, (unsigned)pvr_mem_available());
#endif
        pvr_scene_finish();
        tcpu += timer_us_gettime64() - c0;
        if (++cuadro % 30 == 0) {
            uint64_t t1 = timer_ms_gettime64();
            fps = 30000.0f / (float)(t1 - t0); ms = tcpu / 30000.0f; ms_log = t_log / 30000.0f; t0 = t1; tcpu = 0; t_log = 0;
            /* distancia adaptativa: si la geometria no cabe en el cuadro, se acorta (minimo 600 m) y vuelve a crecer */
            if ((ms + ms_log > 15.0f || vtx_ult > VTX_ETAPA_MAX + 6000) && lejos > 600.0f) { lejos -= 50.0f; pvr_fog_table_linear(lejos * 0.55f, lejos); }
            else if (ms + ms_log < 11.5f && vtx_ult < VTX_ETAPA_MAX && lejos < lejos_obj) { lejos += 50.0f; pvr_fog_table_linear(lejos * 0.55f, lejos); }
            saltos_ver = saltos; saltos = 0;
        }
    }
    return 0;
}
