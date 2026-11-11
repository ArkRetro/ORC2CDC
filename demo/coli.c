/* Consulta del suelo del original: ver coli.h. Port de la decompilacion del PC (src/driving/pc_course_world.cpp,
   pc_course_query.cpp, pc_course_spline.cpp, pc_collision.cpp) en float (el PC calcula parte en x87). Los vecinos de
   ColiGetForward / Back / Left / RightPolygonNumber vienen calculados en el fichero (tools/conv_coli.py). */
#include <math.h>
#include <string.h>
#include <float.h>
#include "coli.h"

typedef float v3[3];
typedef v3 quad[4];

int coli_abre(coli_t *c, const uint8_t *d, uint32_t tam, const float *marco)
{
    uint32_t h[12], nv, nn, nr, nl, o;
    memset(c, 0, sizeof *c);
    if (!d || tam < 52 || memcmp(d, "COL1", 4)) return -1;
    memcpy(h, d + 4, sizeof h);
    c->n = h[0]; nv = h[1]; nn = h[2]; nr = h[3]; c->x0 = h[4]; c->z0 = h[5]; c->gw = h[6]; c->gh = h[7]; nl = h[8];
    c->tipo = h[9];
    o = 52;
    c->v = (const float *)(d + o); o += nv * 12;
    c->nrm = (const float *)(d + o); o += nn * 12;
    c->p = (const coli_pol_t *)(d + o); o += (c->n * sizeof(coli_pol_t) + 3) & ~3u;
    c->rec = (const uint16_t *)(d + o); o += (nr * 8 + 3) & ~3u;
    c->rej = (const uint16_t *)(d + o); o += (c->gw * c->gh * 2 + 3) & ~3u;
    c->li = (const uint16_t *)(d + o); o += (nl * 2 + 3) & ~3u;
    if (sizeof(coli_pol_t) != 14 || o > tam) return -2;
    if (marco) memcpy(c->marco, marco, sizeof c->marco);
    c->mat = h[11] & 1;                                         /* (tools/conv_coli.py: los ficheros de antes no lo traen) */
    c->ok = 1;
    return 0;
}

static const float *vert(const coli_t *c, int i, int k) { return c->v + 3 * c->p[i].v[k]; }
static void poligono(const coli_t *c, int i, quad q)
{
    int k;
    for (k = 0; k < 4; k++) memcpy(q[k], vert(c, i, k), 12);
}

/* ---- CalcYPosSpl (pc_course_spline.cpp) */
static void vsub(v3 o, const float *a, const float *b) { o[0] = a[0] - b[0]; o[1] = a[1] - b[1]; o[2] = a[2] - b[2]; }
static float dist(const float *a, const float *b)
{
    float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(z * z + x * x + y * y);
}
static int normaliza_escala(v3 o, const v3 v, float escala)
{
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]), k;
    if (!(l > 0.0001f)) return 0;
    k = escala / l; o[0] = k * v[0]; o[1] = k * v[1]; o[2] = k * v[2];
    return 1;
}
/* CalcNormalToDelta */
static void normal_a_delta(const float *a, const float *b, const float *na, const float *nb, float escala, v3 da, v3 db)
{
    float dx = a[0] - b[0], dz = b[2] - a[2], obj = (dz * dz + dx * dx) * escala * escala, la, lb;
    v3 va, vb;
    va[0] = 0.0f - na[1] * dx; va[1] = na[0] * dx - na[2] * dz; va[2] = na[1] * dz;
    vb[0] = 0.0f - nb[1] * dx; vb[1] = nb[0] * dx - nb[2] * dz; vb[2] = nb[1] * dz;
    la = va[0] * va[0] + va[1] * va[1] + va[2] * va[2]; lb = vb[0] * vb[0] + vb[1] * vb[1] + vb[2] * vb[2];
    if (la > 0.0f) la = sqrtf(obj / la);
    if (lb > 0.0f) lb = sqrtf(obj / lb);
    da[0] = la * va[0]; da[1] = la * va[1]; da[2] = la * va[2];
    db[0] = lb * vb[0]; db[1] = lb * vb[1]; db[2] = lb * vb[2];
}
/* vecinos en las ranuras de CalcYPosSpl: GetRoadCond pasa Back, Forward, Left, Right (pc_course_query.cpp) */
typedef struct { const float (*fwd)[3], (*back)[3], (*left)[3], (*right)[3]; } vecinos_t;
/* CalcHermiteTangent (con las normales de las esquinas) */
static void tangente(v3 t[8], const quad p, const quad n)
{
    normal_a_delta(p[0], p[1], n[0], n[1], 1.0f, t[0], t[1]);   /* CourseSplineTuning: 1,0 (PC 0x67D94C / 0x67D950) */
    normal_a_delta(p[3], p[2], n[3], n[2], 1.0f, t[2], t[3]);
    normal_a_delta(p[0], p[3], n[0], n[3], 1.0f, t[4], t[5]);
    normal_a_delta(p[1], p[2], n[1], n[2], 1.0f, t[6], t[7]);
}
/* CalcHermiteDirectionVector2 + CalcHermiteTangent2 (con los cuatro vecinos) */
static void tangente2(v3 t[8], const quad p, const vecinos_t *n)
{
    v3 u[4], v[4], d[8]; float s[8]; int i;
    vsub(v[0], p[3], n->back ? n->back[0] : p[0]);
    vsub(v[1], p[2], n->back ? n->back[1] : p[1]);
    vsub(v[2], n->fwd ? n->fwd[2] : p[2], p[1]);
    vsub(v[3], n->fwd ? n->fwd[3] : p[3], p[0]);
    vsub(u[0], p[1], n->left ? n->left[0] : p[0]);
    vsub(u[3], p[2], n->left ? n->left[3] : p[3]);
    vsub(u[1], n->right ? n->right[1] : p[1], p[0]);
    vsub(u[2], n->right ? n->right[2] : p[2], p[3]);
    s[0] = s[1] = dist(p[0], p[1]); s[2] = s[3] = dist(p[3], p[2]);
    s[4] = s[5] = dist(p[0], p[3]); s[6] = s[7] = dist(p[1], p[2]);
    memcpy(d[0], u[0], 12); memcpy(d[1], u[1], 12); memcpy(d[2], u[3], 12); memcpy(d[3], u[2], 12);
    memcpy(d[4], v[0], 12); memcpy(d[5], v[3], 12); memcpy(d[6], v[1], 12); memcpy(d[7], v[2], 12);
    for (i = 0; i < 8; i++) if (!normaliza_escala(t[i], d[i], s[i])) t[i][0] = t[i][1] = t[i][2] = 0;
}
static void coeficientes(float c[3], const float *a, const float *b, const float *ta, const float *tb)
{
    float dx = b[0] - a[0], dz = b[2] - a[2]; int ux = fabsf(dx) > fabsf(dz);
    float inv = 1.0f / (ux ? dx : dz), c1 = ux ? ta[0] : ta[2], c2 = ux ? tb[0] : tb[2];
    c[0] = (c2 + c1) * inv - 2.0f; c[1] = 3.0f - (c1 * 2.0f + c2) * inv; c[2] = inv * c1;
}
static float acarreo(float val, const float c[3])               /* CalcCarryVariable */
{
    float a = c[0], b = c[1], d = c[2], ext, k, va, r, pend; int j;
    if (a == 0.0f) return val;
    ext = b / (a * -3.0f);
    if (0.0f >= ext || ext >= 1.0f) return val;
    k = 0.0f - val;
    if (((ext * a + b) * ext + d) * ext + k == 0.0f) return ext;
    for (j = 0; j < 3; j++) {
        va = val * a; r = ((va + b) * val + d) * val + k;
        if (r == 0.0f) break;
        pend = (va * 3.0f + b * 2.0f) * val + d;
        val = val - r / pend;
    }
    return val;
}
static float resuelve(float x, float z, float u, float v, const quad p, v3 t[8])   /* SolveHermite: la y */
{
    float ca[3], cb[3], va, vb, s, q, he, hs, pe, ps, fr, l2; v3 a, b, d; int k;
    coeficientes(ca, p[0], p[3], t[4], t[5]); coeficientes(cb, p[1], p[2], t[6], t[7]);
    va = acarreo(v, ca); vb = acarreo(v, cb);
    s = (vb - va) * u + va;
    q = s * s - s; he = q * s; hs = he - q; pe = 0.0f - (q * 2.0f - s) * s; ps = 1.0f - pe;
    for (k = 0; k < 3; k++) {
        a[k] = t[4][k] * hs + p[3][k] * pe + t[5][k] * he + p[0][k] * ps;
        b[k] = p[2][k] * pe + t[6][k] * hs + p[1][k] * ps + t[7][k] * he;
    }
    vsub(d, b, a); d[1] = 0.0f;
    if (fabsf(d[0]) < 0x1p-23f && fabsf(d[1]) < 0x1p-23f && fabsf(d[2]) < 0x1p-23f) fr = 0.0f;
    else {
        l2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
        fr = ((z - a[2]) * d[2] + (0.0f - a[1]) * d[1] + (x - a[0]) * d[0]) / l2;
    }
    return a[1] * (1.0f - fr) + b[1] * fr;
}
static float altura_spl(float x, float z, const quad p, const quad nrm, const vecinos_t *n)
{
    float u = 0.5f, v = 0.5f, ax = p[2][0] - p[1][0], az = p[2][2] - p[1][2], bx = p[3][0] - p[0][0], bz = p[3][2] - p[0][2];
    float px = p[0][0] - x, pz = p[0][2] - z, qx = p[1][0] - x, qz = p[1][2] - z;
    float a = ax * bz - az * bx, c = qx * pz - qz * px, b = (pz * ax + qx * bz) - (px * az + qz * bx);
    int calc = 0; v3 t[8];
    if (fabsf(a) > 0.00001f) {
        float a2 = a * 2.0f, disc = b * b - c * 2.0f * a2, raiz = sqrtf(disc), otro, d1, d2;
        b = b > 0.0f ? ((0.0f - b) - raiz) / a2 : (raiz - b) / a2;
        otro = c / (b * a);
        d1 = fabsf(b - 0.5f); d2 = fabsf(otro - 0.5f);
        if (!(d1 < d2) && !isnan(d1) && !isnan(d2)) b = otro;
        if (!(b < -0.001f || b > 1.001f)) { if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f; calc = 1; }
    } else if (b != 0.0f) {
        b = 0.0f - c / b;
        if (!(b <= -0.01f || b >= 1.01f)) { if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f; calc = 1; }
    }
    if (calc) {
        float ex = p[1][0] - p[0][0], ez = p[1][2] - p[0][2], h;
        float dx = ((p[2][0] - p[3][0]) - ex) * b + ex, dz = ((p[2][2] - p[3][2]) - ez) * b + ez;
        if (!(fabsf(dz) < fabsf(dx))) { h = 0.0f - (b * bz + pz); if (h != 0.0f) h = h / dz; }
        else { h = 0.0f - (b * bx + px); if (h != 0.0f) h = h / dx; }
        if (h < 0.0f) h = 0.0f; else if (h > 1.0f) h = 1.0f;
        u = b; v = h;
    }
    /* CalcHermite2: con delante y detras CalcHermiteTangent2, si no CalcHermiteTangent (normales) */
    if (n->fwd && n->back) tangente2(t, p, n); else tangente(t, p, nrm);
    return resuelve(x, z, v, u, p, t);                          /* lateral primero, longitudinal despues */
}

/* ---- GetRoadCond (pc_course_query.cpp) */
static int fuera(const float *a, const float *b, float x, float z)
{
    return (b[2] - a[2]) * (x - a[0]) > (b[0] - a[0]) * (z - a[2]);
}
static float plano_tri(const float *p0, const float *p1, const float *p2, float x, float z)   /* 43CC90 */
{
    v3 a, b; float nx, ny, nz;
    vsub(a, p1, p0); vsub(b, p2, p0);
    nx = a[1] * b[2] - a[2] * b[1]; ny = a[2] * b[0] - a[0] * b[2]; nz = a[0] * b[1] - a[1] * b[0];
    return p0[1] - (nz * (z - p0[2]) + nx * (x - p0[0])) / ny;
}
static float plano_quad(const coli_t *c, int i, float x, float z)
{
    const float *p0 = vert(c, i, 0), *p2 = vert(c, i, 2);
    if ((p2[0] - p0[0]) * (z - p0[2]) - (p2[2] - p0[2]) * (x - p0[0]) >= 0.0f) return plano_tri(p0, vert(c, i, 1), p2, x, z);
    return plano_tri(p0, p2, vert(c, i, 3), x, z);
}
static float altura_spl_pol(const coli_t *c, int i, float x, float z)
{
    quad p, nrm, f, b, l, r; vecinos_t n; const coli_pol_t *q = c->p + i; int k, fs = q->vecinos & 63, bs = (q->vecinos >> 6) & 63;
    memset(&n, 0, sizeof n); poligono(c, i, p);
    /* las ranuras: back <- ColiGetForward, fwd <- ColiGetBack */
    if (fs) { poligono(c, i + fs, f); n.back = (const float (*)[3])f; }
    if (bs) { poligono(c, i - bs, b); n.fwd = (const float (*)[3])b; }
    if (q->vecinos & 0x1000) { poligono(c, i - 1, l); n.left = (const float (*)[3])l; }
    if (q->vecinos & 0x2000) { poligono(c, i + 1, r); n.right = (const float (*)[3])r; }
    if (q->normal != 0xFFFF) for (k = 0; k < 4; k++) memcpy(nrm[k], c->nrm + 3 * c->rec[4 * q->normal + k], 12);
    else memset(nrm, 0, sizeof nrm);
    return altura_spl(x, z, (const float (*)[3])p, (const float (*)[3])nrm, &n);
}
static int condicion(const coli_t *c, uint16_t lista, uint32_t modo, float x, float yref, float z, float *y)
{
    const uint16_t *l = c->li + lista; uint32_t n = l[0], k; float mejor_y = FLT_MAX; int mejor = -1;
    for (k = 0; k < n; k++) {
        int i = l[1 + k]; const coli_pol_t *q = c->p + i; float h;
        const float *p0 = vert(c, i, 0), *p1 = vert(c, i, 1), *p2 = vert(c, i, 2), *p3 = vert(c, i, 3);
        if (fuera(p0, p1, x, z) || fuera(p1, p2, x, z)) continue;
        if (q->banderas & 1) { if (fuera(p2, p0, x, z)) continue; }
        else if (fuera(p2, p3, x, z) || fuera(p3, p0, x, z)) continue;
        h = p0[1];
        if (modo == 0x100) h = (q->banderas & 2) ? plano_quad(c, i, x, z) : plano_tri(p0, p1, p2, x, z);
        else if (modo == 0x400) h = altura_spl_pol(c, i, x, z);
        if (q->banderas & 4) { *y = h; return i; }
        if (fabsf(mejor_y - yref) > fabsf(h - yref)) { mejor_y = h; mejor = i; }
    }
    *y = mejor_y;
    return mejor;
}

/* ---- GetYPositionProg (pc_course_world.cpp get_y_position_prog_types) */
static uint16_t celda(const coli_t *c, float x, float z)        /* CalcCollisionArea, en la rejilla recortada */
{
    int cx = (int)(x + 3072.0f) / 24, cz = (int)(z + 3072.0f) / 24;
    cx = cx < 0 ? 0 : cx > 255 ? 255 : cx; cz = cz < 0 ? 0 : cz > 255 ? 255 : cz;
    cx -= (int)c->x0; cz -= (int)c->z0;
    if (cx < 0 || cz < 0 || cx >= (int)c->gw || cz >= (int)c->gh) return 0;
    return c->rej[cz * c->gw + cx];
}
int coli_altura(coli_t *const raices[2], coli_pred_t *pr, uint32_t modo, float x, float *y, float z, int *pol, uint32_t *clase)
{
    uint32_t prim = pr->easy, seg = prim == 0 ? 1 : 0, tipo = prim, intento, k, n;
    for (intento = 0; intento < 2; intento++) {
        const coli_t *c; float cs, sn, dx, dz, lx, lz, ly, h; uint16_t lista; int i;
        tipo = intento ? seg : prim;
        c = raices[tipo];
        if (!c || !c->ok) continue;
        cs = cosf(c->marco[3]); sn = sinf(c->marco[3]);
        dx = x - c->marco[0]; dz = z - c->marco[2];
        lx = dx * cs - dz * sn; lz = dx * sn + dz * cs; ly = *y - c->marco[1];   /* inverso de modelo_coloca */
        lista = celda(c, lx, lz);
        if (!lista) continue;                                   /* (la lista 0 no da acierto) */
        i = condicion(c, lista, modo, lx, ly, lz, &h);
        if (i < 0) continue;
        pr->cursor = (pr->cursor + 1) & 15; pr->recent[pr->cursor] = tipo;   /* UpdateEasyLctPredictionTable */
        for (k = n = 0; k < 15; k++) n += pr->recent[k] != 0;
        pr->easy = n >= 8;
        *y = h + c->marco[1]; *pol = i; *clase = 1u << (c->p[i].tipo & 31);
        return (int)tipo;
    }
    *y = -0.1f; *pol = -1; *clase = 1;
    return (int)seg;
}
int coli_altura_spl(coli_t *const raices[2], coli_pred_t *pr, float x, float *y, float z, int *pol, uint32_t *clase)
{
    float y0 = *y, h = y0; int a, t = 0;
    for (a = 0; a < 4; a++) {
        h = y0;
        t = coli_altura(raices, pr, 0x400, x + ((a & 1) ? 0.001f : 0.0f), &h, z + ((a & 2) ? 0.001f : 0.0f), pol, clase);
        if (!(*clase & 1)) break;
    }
    *y = h;
    return t;
}

/* ---- Muros de la carroceria: CarBodyWallColiCheck / CbwColiWall (ver coli.h) */
#define MURO_CLASES 0x010f4060u
/* modo de la cara por el codigo de superficie & 15 (PS2 0x3dede8, la de PC 0x5E0DE0): bits 0-3 = 2 cara especial
   del triangulo, 4-7 los vertices que la forman */
static const uint8_t MODO_CARA[16] = { 0x00, 0x01, 0x01, 0xc2, 0x00, 0x01, 0x01, 0xc2, 0x01, 0x42, 0x02, 0x03, 0x01, 0x42, 0x02, 0x03 };
static void punto_m(const float M[4][3], const float *p, v3 o)   /* mxCalcPoint: p * M (vectores fila) */
{
    int k;
    for (k = 0; k < 3; k++) o[k] = p[0] * M[0][k] + p[1] * M[1][k] + p[2] * M[2][k] + M[3][k];
}
/* CopColiPoint 43D1D0: los cuatro vertices del poligono en el marco de la carrera (el de su raiz) */
static void vertices_mundo(const coli_t *c, int i, quad q)
{
    float cs = cosf(c->marco[3]), sn = sinf(c->marco[3]); int k;
    for (k = 0; k < 4; k++) {
        const float *v = vert(c, i, k);
        q[k][0] = v[0] * cs + v[2] * sn + c->marco[0];
        q[k][1] = v[1] + c->marco[1];
        q[k][2] = -v[0] * sn + v[2] * cs + c->marco[2];
    }
}
/* pc_direction_xz (pc_wall_rebound.cpp, PC 487790): (a - b) unitario en xz; con longitud 0, z = 1 */
static void dir_xz(float ax, float az, float bx, float bz, float *ox, float *oz)
{
    float x = ax - bx, z = az - bz, l = sqrtf(x * x + z * z);
    if (l > 1.0e-4f) { x = x / l; z = z / l; }
    if (l == 0.0f) z = 1.0f;
    *ox = x; *oz = z;
}
/* regular_special_face: normal en p0 (de p1 - p0, las salidas cruzadas: x -> p0.z, z -> p0.x), borde p1 y la
   cara desplazada 0,01 (0x3C23D70A) en p3 */
static void cara_especial(quad p)
{
    float ox, oz;
    dir_xz(p[1][0], p[1][2], p[0][0], p[0][2], &ox, &oz);
    p[0][2] = ox; p[0][0] = oz;
    p[0][1] = 0.0f; p[0][2] = 0.0f - p[0][2];
    p[3][0] = p[0][0] * 0.01f + p[1][0]; p[3][1] = p[1][1]; p[3][2] = p[0][2] * 0.01f + p[1][2];
}
/* alternate_special_face (codigos 8 | 2 | 1): la salida z va a p1.z y p0.x se queda con el vertice (como el PC) */
static void cara_especial_alt(quad p)
{
    float ox, oz; v3 v2;
    dir_xz(p[0][0], p[0][2], p[1][0], p[1][2], &ox, &oz);
    p[0][2] = ox; p[1][2] = oz;
    memcpy(v2, p[2], 12);
    p[0][1] = 0.0f; p[0][2] = 0.0f - p[0][2];
    p[3][0] = p[0][0] * 0.01f + v2[0]; p[3][1] = v2[1]; p[3][2] = p[0][2] * 0.01f + v2[2];
    memcpy(p[1], v2, 12);
}
/* CalcColiWallFace 502E20 con a = p1, b = p0, c = p3, d = p2 (el orden en que CbwColiWall los pasa): el lado del
   centro del coche (x, z) respecto a la linea entre los centros de las aristas a-b y c-d elige la arista a-d o c-b;
   b sale con la normal (en xz), c con la cara desplazada 0,01 (y = 0, como el PC) y a con el borde */
static void cara_muro(float x, float z, quad p)
{
    float *a = p[1], *b = p[0], *c = p[3], *d = p[2], bz, bx, az, ax, t0, t1, cr, lado, dx, dy, dz, l, inv;
    bz = c[2] + d[2]; bx = c[0] + d[0]; az = a[2] + b[2]; ax = a[0] + b[0];
    t0 = (bz - az) * x; t1 = (bx - ax) * z;
    cr = bz * ax; cr = cr - az * bx; cr = cr * 0.5f;
    lado = t1 - t0; lado = lado + cr;
    if (lado >= 0.0f) { memcpy(d, b, 12); memcpy(a, c, 12); }
    b[0] = a[0] - d[0];
    dy = a[1] - d[1]; dx = b[0]; b[1] = dy;
    dz = a[2] - d[2]; b[2] = dz;
    l = sqrtf(dz * dz + dx * dx); inv = 1.0f / l;
    b[2] = b[0] * inv; b[1] = 0.0f; b[0] = 0.0f - inv * dz;
    c[0] = b[0] * 0.01f + a[0]; c[1] = b[1]; c[2] = b[2] * 0.01f + a[2];
}
void coli_muro(coli_t *const raices[2], coli_pred_t *pr, const float M[4][3], const float (*forma)[3], int n,
               const float suelo_n[3], coli_muro_t *o)
{
    int k, j, pol[8], raiz[8], primero = 0; uint32_t cl[8], agg = 0, inter = 0xffffffffu, sel = 0, todo = 0;
    v3 w[8], pos, cara0[3]; float maxd = 0.0f;
    memset(o, 0, sizeof *o); memset(cara0, 0, sizeof cara0);
    if (n > 8) n = 8;                                           /* (8 registros CBW_GETY_WORK en la pila del PC) */
    for (k = 0; k < n; k++) {
        float y;
        punto_m(M, forma[k], w[k]); y = w[k][1];
        raiz[k] = coli_altura(raices, pr, 0, w[k][0], &y, w[k][2], &pol[k], &cl[k]);
        agg |= cl[k]; inter &= cl[k];
    }
    o->agregado = agg;
    if (inter & 1u) { o->accion = 2; return; }                  /* ningun punto pisa suelo: PlWrecker(1) */
    if ((inter & 2u) || !(agg & MURO_CLASES)) return;           /* work+670 = 0 */
    o->accion = 1;
    memcpy(pos, M[3], 12);                                      /* work+40 */
    for (k = 0; k < n; k++) {
        const coli_t *c; quad p; int mat, esp = 0; uint32_t masc = 0; float mn = 0.0f, dist;
        if (!(cl[k] & MURO_CLASES)) continue;
        c = raices[raiz[k]];
        vertices_mundo(c, pol[k], p);
        mat = c->mat ? (c->p[pol[k]].banderas >> 4) & 15 : 0;   /* 0x556e40[tipo][poligono] & 15 */
        if (p[3][0] == p[2][0] && p[3][2] == p[2][2]) {
            int modo = MODO_CARA[mat];
            if ((modo & 15) == 2) {
                v3 a, b; int alto = modo & 0xf0;
                if (alto == 0x40) { memcpy(a, p[0], 12); memcpy(b, p[3], 12); memcpy(p[1], a, 12); memcpy(p[0], b, 12); }
                else if (alto == 0x80) { memcpy(a, p[3], 12); memcpy(b, p[2], 12); memcpy(p[1], a, 12); memcpy(p[0], b, 12); }
                else if (alto == 0xc0) { memcpy(a, p[1], 12); memcpy(b, p[2], 12); memcpy(p[0], a, 12); memcpy(p[1], b, 12); }
                cara_especial(p); esp = 1;
            } else if ((mat & 10) == 10) {
                if (mat & 1) cara_especial_alt(p); else cara_especial(p);
                esp = 1;
            }
        }
        if (!esp) cara_muro(pos[0], pos[2], p);
        /* la cara: normal p0, desplazada p3, borde p1. PushOutPosMatObsolete: los puntos con la matriz sin mover */
        for (j = 0; j < n; j++) {
            float in = (w[j][0] - p[3][0]) * p[0][0] + (w[j][1] - p[3][1]) * p[0][1] + (w[j][2] - p[3][2]) * p[0][2];
            float fu = (w[j][0] - p[1][0]) * p[0][0] + (w[j][1] - p[1][1]) * p[0][1] + (w[j][2] - p[1][2]) * p[0][2];
            if (in < 0.0f) masc |= 0x80000000u >> j;
            if (mn > fu) mn = fu;
        }
        dist = fabsf(mn);
        pos[0] += p[0][0] * dist; pos[1] += p[0][1] * dist; pos[2] += p[0][2] * dist;
        if (dist > maxd) maxd = dist;
        todo |= masc;
        if (!primero) { memcpy(cara0[0], p[0], 12); memcpy(cara0[1], p[3], 12); memcpy(cara0[2], p[1], 12); sel = masc; primero = 1; }
    }
    o->mascara = todo; o->dist = maxd;
    o->empuje[0] = pos[0] - M[3][0]; o->empuje[1] = pos[1] - M[3][1]; o->empuje[2] = pos[2] - M[3][2];
    {   /* ColiSetObsolete con la primera cara: la normal en el marco del cuerpo y la media de los puntos entrados que
           quedan detras de ella, pesada por su proyeccion */
        float nl[3], suma = 0.0f, c3[3] = { 0, 0, 0 }; int usados = 0;
        for (k = 0; k < 3; k++) nl[k] = cara0[0][0] * M[k][0] + cara0[0][1] * M[k][1] + cara0[0][2] * M[k][2];
        for (j = 0; j < n; j++) {
            float d;
            if (!(sel & (0x80000000u >> j))) continue;
            d = nl[0] * forma[j][0] + nl[1] * forma[j][1] + nl[2] * forma[j][2];
            if (d < 0.0f) {
                suma = d + suma; usados++;
                for (k = 0; k < 3; k++) c3[k] = forma[j][k] * d + c3[k];
            }
        }
        if (!usados) return;                                    /* sin contacto: CbwColiWall acaba aqui */
        for (k = 0; k < 3; k++) c3[k] = c3[k] * (1.0f / suma);
        o->contacto = 1;
        o->cuadrante = c3[0] > 0.0f ? (c3[2] > 0.0f ? 2 : 3) : (c3[2] > 0.0f ? 1 : 0);
    }
    {   /* work+64C = (n.z, n.y, -n.x) x work+628 */
        float a[3] = { cara0[0][2], cara0[0][1], 0.0f - cara0[0][0] };
        o->normal[0] = a[1] * suelo_n[2] - a[2] * suelo_n[1];
        o->normal[1] = a[2] * suelo_n[0] - a[0] * suelo_n[2];
        o->normal[2] = a[0] * suelo_n[1] - a[1] * suelo_n[0];
    }
}
