/* pvs.c: visibilidad precalculada (PVS) de una etapa o union desde un conjunto de posiciones del ojo, por triangulo.
   Lo usa tools/pvs.py (lo compila con cc y lo llama con ctypes).

   Desde cada posicion se dibujan por software cuatro caras alrededor (rumbos 0, 90, 180 y 270 grados; en cada una
   tan horizontal -1..1 y vertical VMIN..VMAX), asi que vale para cualquier rumbo y cabeceo de la camara del juego (la
   piramide de la vista se recorta luego en cada cuadro, en la demo). En cada cara:
   1) los oclusores (triangulos de mallas que el LOD del original dibuja seguro desde ahi: occ_lim) van a un z-buffer
      (1/z por pixel, centro del pixel): los opacos (lista 0) enteros; los de recorte (lista 1, prueba de alfa) solo
      donde su textura tapa seguro: el texel de la UV del centro del pixel (con la perspectiva) y sus ocho vecinos
      opacos (mascara erosionada que da tools/pvs.py: el filtro bilineal y el redondeo de la UV no abren huecos);
   2) erosion conservadora: el ojo de verdad puede estar hasta eps metros de la muestra, y un borde de oclusor a z
      metros se mueve como mucho eps / z radianes respecto a lo de detras; cada pixel solo tapa con la profundidad mas
      lejana de su entorno de ese radio (mas un pixel; filtro de minimo de 1/z por niveles: medios lados 1, 3, 7...);
   3) cada malla: la caja de su esfera (si todo esta tapado, ninguno de sus triangulos se ve) y luego cada triangulo
      que aun no se haya visto en la seccion de la muestra, con rasterizado conservador (todo pixel que el triangulo
      toca) y la 1/z mas cercana del triangulo en ese pixel: se ve si en algun pixel no queda detras del z-buffer
      erosionado. Los triangulos degenerados (area nula en 3D) no pintan nada y no se prueban. Los carteles (orientados
      a la camara) se prueban por su esfera.
   Los triangulos se recortan contra el plano cercano (0,1 m), los oclusores tambien.

   pvs(tri, opa, uv, tex, mascaras, m_ofs, m_w, m_h, t0, nm, esfera, fl, occ_lim, pos, sec, npos, nsec, K, eps,
       nhilos, sal_t, sal_m):
     tri: 9 floats por triangulo, ordenados por malla; opa: 1 byte por triangulo (bit 0 tapa entero, bit 1 degenerado,
     bit 2 tapa con la mascara de su textura); uv: 6 floats por triangulo; tex: textura del triangulo (mascara);
     mascaras: 1 byte por texel (1 = tapa) de cada textura desde m_ofs, de m_w x m_h; t0[nm + 1]: primer triangulo de
     cada malla; esfera: 4 floats por malla; fl: bit 0 cartel (por la esfera), bit 1 sus triangulos tapan, bit 2
     ignorar; occ_lim: distancia al centro hasta la que la malla tapa (0 = nunca); pos: 3 floats por posicion; sec: la
     seccion de cada posicion; K: pixeles por unidad de tangente; eps: radio de la erosion (m); sal_t: nsec x
     techo(ntri / 32) u32 (bit del triangulo visto desde alguna muestra de la seccion); sal_m: nsec x techo(nm / 32)
     (la malla: algun triangulo o la esfera del cartel). Las salidas se acumulan (OR atomico entre hilos). */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#define CERCA 0.1f
#define LEJOS 4000.0f
#define VMIN (-1.0f)
#define VMAX 1.732f
#define NIV 7                                                    /* medios lados 0, 1, 3, 7, 15, 31, 63 */

typedef struct {
    const float *tri; const unsigned char *opa; const float *uv; const int *tex; const unsigned char *mas; const int *m_ofs, *m_w, *m_h;
    const int *t0; int nn; const float *esf; const unsigned char *fl; const float *occ;
    const float *pos; const int *sec; int npos; float K, eps; unsigned *sal_t, *sal_m; int ntw, nmw;
    int sig; pthread_mutex_t mu;
} trabajo_t;

typedef struct { int W, H; float *niv[NIV]; float *tmp; float *E; } buf_t;

static float F[4][3] = { { 0, 0, -1 }, { 1, 0, 0 }, { 0, 0, 1 }, { -1, 0, 0 } };
static float R[4][3] = { { 1, 0, 0 }, { 0, 0, 1 }, { -1, 0, 0 }, { 0, 0, -1 } };

/* vertice en la vista de la cara: (x, y, z) */
static inline void vista(const float *p, const float *o, int f, float *v)
{
    float d0 = p[0] - o[0], d1 = p[1] - o[1], d2 = p[2] - o[2];
    v[0] = d0 * R[f][0] + d2 * R[f][2]; v[1] = d1; v[2] = d0 * F[f][0] + d2 * F[f][2];
}

/* recorta el triangulo contra z >= CERCA; deja hasta 4 vertices en pantalla (px, py, 1/z) y devuelve cuantos */
static int recorta(const float v[3][3], float K, float s[4][3])
{
    float q[4][3]; int n = 0, i;
    for (i = 0; i < 3; i++) {
        const float *a = v[i], *b = v[(i + 1) % 3];
        int ia = a[2] >= CERCA, ib = b[2] >= CERCA;
        if (ia) { q[n][0] = a[0]; q[n][1] = a[1]; q[n][2] = a[2]; n++; }
        if (ia != ib) {
            float t = (CERCA - a[2]) / (b[2] - a[2]);
            q[n][0] = a[0] + t * (b[0] - a[0]); q[n][1] = a[1] + t * (b[1] - a[1]); q[n][2] = CERCA; n++;
        }
    }
    for (i = 0; i < n; i++) {
        float iz = 1.0f / q[i][2];
        s[i][0] = (q[i][0] * iz + 1.0f) * K; s[i][1] = (VMAX - q[i][1] * iz) * K; s[i][2] = iz;
    }
    return n;
}

/* recorta un triangulo con UV (v: x, y, z, u, v) contra z >= CERCA; en pantalla (px, py, 1/z, u/z, v/z) */
static int recorta_uv(const float v[3][5], float K, float s[4][5])
{
    float q[4][5]; int n = 0, i, c;
    for (i = 0; i < 3; i++) {
        const float *a = v[i], *b = v[(i + 1) % 3];
        int ia = a[2] >= CERCA, ib = b[2] >= CERCA;
        if (ia) { for (c = 0; c < 5; c++) q[n][c] = a[c]; n++; }
        if (ia != ib) {
            float t = (CERCA - a[2]) / (b[2] - a[2]);
            for (c = 0; c < 5; c++) q[n][c] = a[c] + t * (b[c] - a[c]);
            q[n][2] = CERCA; n++;
        }
    }
    for (i = 0; i < n; i++) {
        float iz = 1.0f / q[i][2];
        s[i][0] = (q[i][0] * iz + 1.0f) * K; s[i][1] = (VMAX - q[i][1] * iz) * K; s[i][2] = iz; s[i][3] = q[i][3] * iz; s[i][4] = q[i][4] * iz;
    }
    return n;
}

/* oclusor de recorte: como raster_ocl, solo en los pixeles cuyo texel (UV con la perspectiva) tapa en la mascara */
static void raster_ocl_alfa(buf_t *b, const float *A, const float *B, const float *C, const unsigned char *m, int mw, int mh)
{
    float ar = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]);
    int x0, x1, y0, y1, x, y; float *zb = b->niv[0];
    if (fabsf(ar) < 1e-9f) return;
    x0 = (int)floorf(fminf(A[0], fminf(B[0], C[0]))); x1 = (int)ceilf(fmaxf(A[0], fmaxf(B[0], C[0])));
    y0 = (int)floorf(fminf(A[1], fminf(B[1], C[1]))); y1 = (int)ceilf(fmaxf(A[1], fmaxf(B[1], C[1])));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > b->W - 1) x1 = b->W - 1; if (y1 > b->H - 1) y1 = b->H - 1;
    if (x0 > x1 || y0 > y1) return;
    {
        float i_ar = 1.0f / ar;
        float a0 = (B[1] - C[1]) * i_ar, b0 = (C[0] - B[0]) * i_ar, c0 = (B[0] * C[1] - C[0] * B[1]) * i_ar;
        float a1 = (C[1] - A[1]) * i_ar, b1 = (A[0] - C[0]) * i_ar, c1 = (C[0] * A[1] - A[0] * C[1]) * i_ar;
        for (y = y0; y <= y1; y++) {
            float cy = y + 0.5f; float *fila = zb + y * b->W;
            for (x = x0; x <= x1; x++) {
                float cx = x + 0.5f, w0 = a0 * cx + b0 * cy + c0, w1 = a1 * cx + b1 * cy + c1, w2 = 1.0f - w0 - w1, iz, u, v;
                int iu, iv;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                iz = w0 * A[2] + w1 * B[2] + w2 * C[2];
                if (iz <= fila[x]) continue;
                u = (w0 * A[3] + w1 * B[3] + w2 * C[3]) / iz * mw; v = (w0 * A[4] + w1 * B[4] + w2 * C[4]) / iz * mh;
                if (!(fabsf(u) < 1e6f && fabsf(v) < 1e6f)) continue;
                iu = (int)floorf(u) % mw; iv = (int)floorf(v) % mh; if (iu < 0) iu += mw; if (iv < 0) iv += mh;
                if (m[iv * mw + iu]) fila[x] = iz;
            }
        }
    }
}

/* oclusor: triangulo en pantalla al z-buffer (pixeles con el centro dentro; 1/z mayor = mas cerca) */
static void raster_ocl(buf_t *b, const float *A, const float *B, const float *C)
{
    float ar = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]);
    int x0, x1, y0, y1, x, y; float *zb = b->niv[0];
    if (fabsf(ar) < 1e-9f) return;
    x0 = (int)floorf(fminf(A[0], fminf(B[0], C[0]))); x1 = (int)ceilf(fmaxf(A[0], fmaxf(B[0], C[0])));
    y0 = (int)floorf(fminf(A[1], fminf(B[1], C[1]))); y1 = (int)ceilf(fmaxf(A[1], fmaxf(B[1], C[1])));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > b->W - 1) x1 = b->W - 1; if (y1 > b->H - 1) y1 = b->H - 1;
    if (x0 > x1 || y0 > y1) return;
    {
        /* funciones de arista: w0 frente a BC, w1 frente a CA (baricentricas sin normalizar) */
        float i_ar = 1.0f / ar;
        float a0 = (B[1] - C[1]) * i_ar, b0 = (C[0] - B[0]) * i_ar, c0 = (B[0] * C[1] - C[0] * B[1]) * i_ar;
        float a1 = (C[1] - A[1]) * i_ar, b1 = (A[0] - C[0]) * i_ar, c1 = (C[0] * A[1] - A[0] * C[1]) * i_ar;
        for (y = y0; y <= y1; y++) {
            float cy = y + 0.5f; float *fila = zb + y * b->W;
            for (x = x0; x <= x1; x++) {
                float cx = x + 0.5f, w0 = a0 * cx + b0 * cy + c0, w1 = a1 * cx + b1 * cy + c1, w2 = 1.0f - w0 - w1, iz;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                iz = w0 * A[2] + w1 * B[2] + w2 * C[2];
                if (iz > fila[x]) fila[x] = iz;
            }
        }
    }
}

/* candidato: algun pixel que el triangulo toque con su 1/z mas cercana en el pixel >= la erosionada. 1 si se ve */
static int raster_ve(const buf_t *b, const float *A, const float *B, const float *C)
{
    float ar = (B[0] - A[0]) * (C[1] - A[1]) - (C[0] - A[0]) * (B[1] - A[1]);
    int x0, x1, y0, y1, x, y; float izmax = fmaxf(A[2], fmaxf(B[2], C[2]));
    x0 = (int)floorf(fminf(A[0], fminf(B[0], C[0]))); x1 = (int)floorf(fmaxf(A[0], fmaxf(B[0], C[0])));
    y0 = (int)floorf(fminf(A[1], fminf(B[1], C[1]))); y1 = (int)floorf(fmaxf(A[1], fmaxf(B[1], C[1])));
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > b->W - 1) x1 = b->W - 1; if (y1 > b->H - 1) y1 = b->H - 1;
    if (x0 > x1 || y0 > y1) return 0;
    if (fabsf(ar) < 1e-9f) {                                    /* degenerado: su caja, con la 1/z mas cercana */
        for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) if (izmax * 1.002f >= b->E[y * b->W + x]) return 1;
        return 0;
    }
    {
        float i_ar = 1.0f / ar;
        float a0 = (B[1] - C[1]) * i_ar, b0 = (C[0] - B[0]) * i_ar, c0 = (B[0] * C[1] - C[0] * B[1]) * i_ar;
        float a1 = (C[1] - A[1]) * i_ar, b1 = (A[0] - C[0]) * i_ar, c1 = (C[0] * A[1] - A[0] * C[1]) * i_ar;
        float a2 = -a0 - a1, b2 = -b0 - b1;
        /* medio pixel en cada eje: el pixel toca el triangulo si cada arista, en su esquina mas favorable, >= 0 */
        float m0 = 0.5f * (fabsf(a0) + fabsf(b0)), m1 = 0.5f * (fabsf(a1) + fabsf(b1)), m2 = 0.5f * (fabsf(a2) + fabsf(b2));
        float dzx = a0 * A[2] + a1 * B[2] + a2 * C[2], dzy = b0 * A[2] + b1 * B[2] + b2 * C[2];
        float mz = 0.5f * (fabsf(dzx) + fabsf(dzy));
        for (y = y0; y <= y1; y++) {
            float cy = y + 0.5f; const float *fila = b->E + y * b->W;
            for (x = x0; x <= x1; x++) {
                float cx = x + 0.5f, w0 = a0 * cx + b0 * cy + c0, w1 = a1 * cx + b1 * cy + c1, w2 = 1.0f - w0 - w1, iz;
                if (w0 < -m0 || w1 < -m1 || w2 < -m2) continue;
                iz = w0 * A[2] + w1 * B[2] + w2 * C[2] + mz;
                if (iz > izmax) iz = izmax;
                if (iz * 1.002f >= fila[x]) return 1;
            }
        }
    }
    return 0;
}

/* esfera en la vista de la cara: 0 fuera, 1 dentro */
static inline int esfera_en(const float *c, float r)
{
    const float k2 = 1.41422f, kv = 2.0f;                       /* sqrt(1 + 1), sqrt(1 + VMAX^2) */
    if (c[2] + r < CERCA || c[2] - r > LEJOS) return 0;
    if (fabsf(c[0]) - c[2] > r * k2) return 0;
    if (c[1] - c[2] * VMAX > r * kv) return 0;
    if (-c[1] + c[2] * VMIN > r * k2) return 0;
    return 1;
}

/* caja conservadora en pantalla de la esfera c, r (c[2] - r >= CERCA) */
static void caja_esfera(const buf_t *b, const float *c, float r, float K, int *x0, int *x1, int *y0, int *y1)
{
    float zn = c[2] - r, zf = c[2] + r;
    float u0 = fminf((c[0] - r) / zn, (c[0] - r) / zf), u1 = fmaxf((c[0] + r) / zn, (c[0] + r) / zf);
    float v0 = fminf((c[1] - r) / zn, (c[1] - r) / zf), v1 = fmaxf((c[1] + r) / zn, (c[1] + r) / zf);
    float X0 = (u0 + 1) * K, X1 = (u1 + 1) * K, Y0 = (VMAX - v1) * K, Y1 = (VMAX - v0) * K;
    *x0 = X0 < 0 ? 0 : X0 > b->W - 1 ? b->W : (int)X0; *x1 = X1 > b->W - 1 ? b->W - 1 : X1 < 0 ? -1 : (int)X1;
    *y0 = Y0 < 0 ? 0 : Y0 > b->H - 1 ? b->H : (int)Y0; *y1 = Y1 > b->H - 1 ? b->H - 1 : Y1 < 0 ? -1 : (int)Y1;
}

/* el z-buffer erosionado: E = minimo de 1/z en el cuadrado de medio lado r(p) = eps K (1 + u^2 + v^2) / z + 1 */
static void erosiona(buf_t *b, float K, float eps)
{
    int W = b->W, H = b->H, k, x, y, h = 0;
    static const int medio[NIV] = { 0, 1, 3, 7, 15, 31, 63 };
    for (k = 1; k < NIV; k++) {
        const float *L = b->niv[k - 1]; float *T = b->tmp, *O = b->niv[k]; int s = h + 1;
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
            float m = L[y * W + x];
            float l = L[y * W + (x - s >= 0 ? x - s : 0)], r = L[y * W + (x + s < W ? x + s : W - 1)];
            if (l < m) m = l; if (r < m) m = r;
            T[y * W + x] = m;
        }
        for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
            float m = T[y * W + x];
            float u = T[(y - s >= 0 ? y - s : 0) * W + x], d = T[(y + s < H ? y + s : H - 1) * W + x];
            if (u < m) m = u; if (d < m) m = d;
            O[y * W + x] = m;
        }
        h = 2 * h + 1;
    }
    for (y = 0; y < H; y++) {
        float v = VMAX - (y + 0.5f) / K;
        for (x = 0; x < W; x++) {
            float u = (x + 0.5f) / K - 1.0f, iz = b->niv[0][y * W + x], r;
            if (iz <= 0) { b->E[y * W + x] = 0; continue; }
            r = eps * K * (1.0f + u * u + v * v) * iz + 1.0f;
            for (k = 1; k < NIV - 1 && medio[k] < r; k++) ;
            b->E[y * W + x] = b->niv[k][y * W + x];
        }
    }
}

float *pvs_volcado;                                              /* depuracion: z-buffer y erosion de la posicion 0 */
#define PON(p, i) __atomic_fetch_or((p) + ((i) >> 5), 1u << ((i) & 31), __ATOMIC_RELAXED)
#define VISTO(p, i) (__atomic_load_n((p) + ((i) >> 5), __ATOMIC_RELAXED) & (1u << ((i) & 31)))
static void una_posicion(trabajo_t *T, buf_t *b, int ip)
{
    const float *o = T->pos + 3 * ip; int f, i, j, nn = T->nn;
    unsigned *st = T->sal_t + (size_t)T->sec[ip] * T->ntw, *sm = T->sal_m + (size_t)T->sec[ip] * T->nmw;
    for (f = 0; f < 4; f++) {
        float c[3];
        memset(b->niv[0], 0, sizeof(float) * b->W * b->H);
        /* 1) oclusores */
        for (i = 0; i < nn; i++) {
            const float *e = T->esf + 4 * i; float dx = e[0] - o[0], dy = e[1] - o[1], dz = e[2] - o[2];
            if (!(T->fl[i] & 2) || (T->fl[i] & 5)) continue;
            if (dx * dx + dy * dy + dz * dz > T->occ[i] * T->occ[i]) continue;
            vista(e, o, f, c);
            if (!esfera_en(c, e[3])) continue;
            for (j = T->t0[i]; j < T->t0[i + 1]; j++) {
                int n, k;
                if (!(T->opa[j] & 5) || (T->opa[j] & 2)) continue;
                if (T->opa[j] & 1) {
                    float v[3][3], s[4][3];
                    for (k = 0; k < 3; k++) vista(T->tri + 9 * j + 3 * k, o, f, v[k]);
                    n = recorta(v, T->K, s);
                    if (n >= 3) raster_ocl(b, s[0], s[1], s[2]);
                    if (n == 4) raster_ocl(b, s[0], s[2], s[3]);
                } else {
                    float v[3][5], s[4][5]; int t = T->tex[j];
                    for (k = 0; k < 3; k++) { vista(T->tri + 9 * j + 3 * k, o, f, v[k]); v[k][3] = T->uv[6 * j + 2 * k]; v[k][4] = T->uv[6 * j + 2 * k + 1]; }
                    n = recorta_uv(v, T->K, s);
                    if (n >= 3) raster_ocl_alfa(b, s[0], s[1], s[2], T->mas + T->m_ofs[t], T->m_w[t], T->m_h[t]);
                    if (n == 4) raster_ocl_alfa(b, s[0], s[2], s[3], T->mas + T->m_ofs[t], T->m_w[t], T->m_h[t]);
                }
            }
        }
        /* 2) erosion */
        erosiona(b, T->K, T->eps);
        if (pvs_volcado && ip == 0) {
            memcpy(pvs_volcado + (size_t)(2 * f) * b->W * b->H, b->niv[0], sizeof(float) * b->W * b->H);
            memcpy(pvs_volcado + (size_t)(2 * f + 1) * b->W * b->H, b->E, sizeof(float) * b->W * b->H);
        }
        /* 3) candidatos: la caja de la esfera de la malla y luego cada triangulo aun sin ver en la seccion */
        for (i = 0; i < nn; i++) {
            const float *e = T->esf + 4 * i; int x0, x1, y0, y1, x, y, ve = 0, todos = 1;
            if (T->fl[i] & 4) continue;
            if ((T->fl[i] & 1) && VISTO(sm, i)) continue;
            if (!(T->fl[i] & 1)) {
                for (j = T->t0[i]; j < T->t0[i + 1] && todos; j++) if (!(T->opa[j] & 2) && !VISTO(st, j)) todos = 0;
                if (todos) continue;                            /* ya vistos todos en la seccion */
            }
            vista(e, o, f, c);
            if (!esfera_en(c, e[3])) continue;
            if (c[2] - e[3] < CERCA) ve = 2;                    /* el ojo dentro o casi: cada triangulo en la cara */
            else {
                /* la caja de la esfera con su 1/z mas cercana: si todo esta tapado, la malla tambien */
                float izn = 1.0f / (c[2] - e[3]);
                caja_esfera(b, c, e[3], T->K, &x0, &x1, &y0, &y1);
                for (y = y0; y <= y1 && !ve; y++) for (x = x0; x <= x1; x++) if (izn * 1.002f >= b->E[y * b->W + x]) { ve = 1; break; }
            }
            if (!ve) continue;
            if (T->fl[i] & 1) { PON(sm, i); continue; }         /* cartel: por la esfera */
            for (j = T->t0[i]; j < T->t0[i + 1]; j++) {
                float v[3][3], s[4][3]; int n, k;
                if ((T->opa[j] & 2) || VISTO(st, j)) continue;
                for (k = 0; k < 3; k++) vista(T->tri + 9 * j + 3 * k, o, f, v[k]);
                n = recorta(v, T->K, s);
                if ((n >= 3 && raster_ve(b, s[0], s[1], s[2])) || (n == 4 && raster_ve(b, s[0], s[2], s[3]))) { PON(st, j); PON(sm, i); }
            }
        }
    }
}

static void *hilo(void *p)
{
    trabajo_t *T = p; buf_t b; int k, ip;
    b.W = (int)(2 * T->K); b.H = (int)((VMAX - VMIN) * T->K);
    for (k = 0; k < NIV; k++) b.niv[k] = malloc(sizeof(float) * b.W * b.H);
    b.tmp = malloc(sizeof(float) * b.W * b.H); b.E = malloc(sizeof(float) * b.W * b.H);
    for (;;) {
        pthread_mutex_lock(&T->mu); ip = T->sig++; pthread_mutex_unlock(&T->mu);
        if (ip >= T->npos) break;
        una_posicion(T, &b, ip);
    }
    for (k = 0; k < NIV; k++) free(b.niv[k]);
    free(b.tmp); free(b.E);
    return NULL;
}

int pvs(const float *tri, const unsigned char *opa, const float *uv, const int *tex, const unsigned char *mas, const int *m_ofs,
        const int *m_w, const int *m_h, const int *t0, int nm, const float *esf, const unsigned char *fl, const float *occ,
        const float *pos, const int *sec, int npos, int nsec, float K, float eps, int nhilos, unsigned *sal_t, unsigned *sal_m)
{
    trabajo_t T; pthread_t th[64]; int i;
    T.tri = tri; T.opa = opa; T.uv = uv; T.tex = tex; T.mas = mas; T.m_ofs = m_ofs; T.m_w = m_w; T.m_h = m_h;
    T.t0 = t0; T.nn = nm; T.esf = esf; T.fl = fl; T.occ = occ; T.pos = pos; T.sec = sec; T.npos = npos;
    T.K = K; T.eps = eps; T.sal_t = sal_t; T.sal_m = sal_m; T.ntw = (t0[nm] + 31) / 32; T.nmw = (nm + 31) / 32; T.sig = 0;
    (void)nsec;
    pthread_mutex_init(&T.mu, NULL);
    if (nhilos < 1) nhilos = 1; if (nhilos > 64) nhilos = 64;
    for (i = 0; i < nhilos; i++) pthread_create(&th[i], NULL, hilo, &T);
    for (i = 0; i < nhilos; i++) pthread_join(th[i], NULL);
    pthread_mutex_destroy(&T.mu);
    return 0;
}
