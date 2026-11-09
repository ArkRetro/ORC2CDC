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
