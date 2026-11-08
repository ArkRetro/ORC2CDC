/* visible.c: que triangulos se ven desde un conjunto de camaras (z-buffer por software). Lo usa tools/conv_coche.py
   (lo compila solo con cc y lo llama con ctypes) para vaciar los coches por dentro.

   visibles(tri, ntri, opaco, cams, ncam, ancho, alto, cuenta):
     tri: ntri x 9 floats (tres vertices x, y, z); opaco: ntri bytes (1 = tapa lo de detras, 0 = cristal o recorte:
     no tapa pero se ve); cams: ncam x 16 floats, matriz 4x4 de fila (x, y, z, 1) -> (X, Y, Z, W) con X/W e Y/W en
     -1..1 y W > 0 delante; cuenta: ntri enteros, se suma 1 por cada camara en la que el triangulo gana algun pixel
     (los no opacos cuentan si no hay un opaco delante en algun pixel suyo). */
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void proyecta(const float *m, const float *p, float *o)
{
    int j;
    for (j = 0; j < 4; j++) o[j] = p[0] * m[j] + p[1] * m[4 + j] + p[2] * m[8 + j] + m[12 + j];
}

/* rasteriza el triangulo t (en pantalla: x, y, 1/w) llamando a f por pixel */
#define RASTER(T, CUERPO) do { \
    float x0 = T[0], y0 = T[1], x1 = T[3], y1 = T[4], x2 = T[6], y2 = T[7]; \
    float ar = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0); \
    int bx0, bx1, by0, by1, px, py; \
    if (fabsf(ar) < 1e-12f) break; \
    bx0 = (int)floorf(fminf(x0, fminf(x1, x2))); bx1 = (int)ceilf(fmaxf(x0, fmaxf(x1, x2))); \
    by0 = (int)floorf(fminf(y0, fminf(y1, y2))); by1 = (int)ceilf(fmaxf(y0, fmaxf(y1, y2))); \
    if (bx0 < 0) bx0 = 0; if (by0 < 0) by0 = 0; if (bx1 > an - 1) bx1 = an - 1; if (by1 > al - 1) by1 = al - 1; \
    for (py = by0; py <= by1; py++) for (px = bx0; px <= bx1; px++) { \
        float cx = px + 0.5f, cy = py + 0.5f; \
        float w0 = ((x1 - cx) * (y2 - cy) - (x2 - cx) * (y1 - cy)) / ar; \
        float w1 = ((x2 - cx) * (y0 - cy) - (x0 - cx) * (y2 - cy)) / ar; \
        float w2 = 1.0f - w0 - w1, iz; \
        if (w0 < 0 || w1 < 0 || w2 < 0) continue; \
        iz = w0 * T[2] + w1 * T[5] + w2 * T[8]; \
        CUERPO \
    } } while (0)

int visibles(const float *tri, int ntri, const unsigned char *opaco, const float *cams, int ncam, int an, int al, int *cuenta)
{
    float *zb = malloc(sizeof(float) * an * al), *sc = malloc(sizeof(float) * 9 * ntri);
    int *id = malloc(sizeof(int) * an * al), c, i, k;
    unsigned char *ok = malloc(ntri), *visto = malloc(ntri);
    if (!zb || !sc || !id || !ok || !visto) return -1;
    for (c = 0; c < ncam; c++) {
        const float *m = cams + 16 * c;
        for (i = 0; i < an * al; i++) { zb[i] = 0; id[i] = -1; }        /* 1/w: mas grande = mas cerca */
        memset(visto, 0, ntri);
        for (i = 0; i < ntri; i++) {
            float o[4]; ok[i] = 1;
            for (k = 0; k < 3; k++) {
                proyecta(m, tri + 9 * i + 3 * k, o);
                if (o[3] < 0.05f) { ok[i] = 0; break; }
                sc[9 * i + 3 * k] = (o[0] / o[3] * 0.5f + 0.5f) * an;
                sc[9 * i + 3 * k + 1] = (0.5f - o[1] / o[3] * 0.5f) * al;
                sc[9 * i + 3 * k + 2] = 1.0f / o[3];
            }
        }
        for (i = 0; i < ntri; i++) {                                    /* los opacos: z-buffer con su numero */
            const float *t = sc + 9 * i;
            if (!ok[i] || !opaco[i]) continue;
            /* a igual profundidad gana el dibujado despues (el orden de llamadas de conv_coche.py): ZTEST GEQUAL del
               GS / D3DCMP_LESSEQUAL del PC; asi una capa que coincide con otra posterior (el +14 del F40, una copia
               entera de la carroceria con la pintura) queda tapada como en el original */
            RASTER(t, if (iz >= zb[py * an + px]) { zb[py * an + px] = iz; id[py * an + px] = i; });
        }
        for (i = 0; i < an * al; i++) if (id[i] >= 0) visto[id[i]] = 1;
        for (i = 0; i < ntri; i++) {                                    /* los transparentes: delante del opaco */
            const float *t = sc + 9 * i; int v = 0;
            if (!ok[i] || opaco[i]) continue;
            RASTER(t, if (iz >= zb[py * an + px]) { v = 1; px = bx1; py = by1; });
            visto[i] = v;
        }
        for (i = 0; i < ntri; i++) cuenta[i] += visto[i];
    }
    free(zb); free(sc); free(id); free(ok); free(visto);
    return 0;
}
