/* Little Bloste: las matematicas del SH-4 propias de OutRun DC (Rev 2.9.2.c; sustituye a SH4ZAM, que ya no se usa).
   Codigo propio, solo cabeceras: funciones en linea con ensamblador del SH-4 (FRCHG, FMOV.S, FTRV) y su version en C
   (-DSIN_LBLOSTE, y la que se usa fuera de la Dreamcast). Documentacion: docs/LBLOSTE.md.

   La XMTRX: los 16 registros del banco de atras (XF0..XF15) que usa FTRV. El SH-4 la lee por columnas: FTRV XMTRX, FVn
   da r[i] = suma_j XF[i + 4 j] x v[j]. Cargada desde memoria en orden (XF0 = m[0], XF1 = m[1]...), una matriz de filas de
   KallistiOS (matrix_t, la de mat_load) queda con XF[i + 4 j] = m[j][i]: FTRV calcula el vector fila por la matriz,
   r = v M, que es lo que hace mat_trans_single de KallistiOS. Es la misma XMTRX de mat_load / mat_apply: quien la cambia
   (lb_xmtrx_carga, lb_xmtrx_filas, lb_mat_mul) tiene que dejarla como la necesite quien venga despues (guardarla antes
   con lb_xmtrx_guarda y volver a cargarla, o que el siguiente la cargue con mat_load, como el dibujo).

   Funciones:
     lb_xmtrx_carga(m)          XMTRX = m (16 floats en el orden de la memoria; m sin alinear vale)
     lb_xmtrx_guarda(m)         m = XMTRX (16 floats)
     lb_xmtrx_filas(r0..r3)     XMTRX tal que lb_ftrv da (r0 . v, r1 . v, r2 . v, r3 . v) (filas de 4 floats)
     lb_ftrv(&x, &y, &z, &w)    (x, y, z, w) = XMTRX por (x, y, z, w) (FTRV)
     lb_mat_mul(o, a, b)        o = a b de matrices de filas 4x4 (o puede ser a o b): XMTRX = b y cada fila de a por FTRV;
                                deja la XMTRX = b
   Precision: FTRV del SH-4 no redondea cada producto como C (suma con mas bits y redondea al final); la diferencia es
   del orden de 1 ulp. La autoprueba lb_prueba (-DLBLOSTE_PRUEBA en demo/main.c) compara cada funcion con su version en
   C al arrancar y escribe el error maximo. */
#ifndef LBLOSTE_H
#define LBLOSTE_H
#include <string.h>

#if !defined(SIN_LBLOSTE) && defined(_arch_dreamcast)
#define CON_LBLOSTE 1                                   /* con el ensamblador del SH-4 */

static inline __attribute__((always_inline)) void lb_xmtrx_carga(const float *m)
{
    const float *p = m;
    __asm__ __volatile__(
        "frchg\n\t"
        "fmov.s @%0+,fr0\n\t"  "fmov.s @%0+,fr1\n\t"  "fmov.s @%0+,fr2\n\t"  "fmov.s @%0+,fr3\n\t"
        "fmov.s @%0+,fr4\n\t"  "fmov.s @%0+,fr5\n\t"  "fmov.s @%0+,fr6\n\t"  "fmov.s @%0+,fr7\n\t"
        "fmov.s @%0+,fr8\n\t"  "fmov.s @%0+,fr9\n\t"  "fmov.s @%0+,fr10\n\t" "fmov.s @%0+,fr11\n\t"
        "fmov.s @%0+,fr12\n\t" "fmov.s @%0+,fr13\n\t" "fmov.s @%0+,fr14\n\t" "fmov.s @%0+,fr15\n\t"
        "frchg\n"
        : "+r"(p) : : "memory");
}
static inline __attribute__((always_inline)) void lb_xmtrx_guarda(float *m)
{
    float *p = m + 16;                                  /* (de atras adelante con predecremento) */
    __asm__ __volatile__(
        "frchg\n\t"
        "fmov.s fr15,@-%0\n\t" "fmov.s fr14,@-%0\n\t" "fmov.s fr13,@-%0\n\t" "fmov.s fr12,@-%0\n\t"
        "fmov.s fr11,@-%0\n\t" "fmov.s fr10,@-%0\n\t" "fmov.s fr9,@-%0\n\t"  "fmov.s fr8,@-%0\n\t"
        "fmov.s fr7,@-%0\n\t"  "fmov.s fr6,@-%0\n\t"  "fmov.s fr5,@-%0\n\t"  "fmov.s fr4,@-%0\n\t"
        "fmov.s fr3,@-%0\n\t"  "fmov.s fr2,@-%0\n\t"  "fmov.s fr1,@-%0\n\t"  "fmov.s fr0,@-%0\n\t"
        "frchg\n"
        : "+r"(p) : : "memory");
}
static inline __attribute__((always_inline)) void lb_ftrv(float *x, float *y, float *z, float *w)
{
    register float a __asm__("fr4") = *x, b __asm__("fr5") = *y, c __asm__("fr6") = *z, d __asm__("fr7") = *w;
    __asm__ __volatile__("ftrv xmtrx,fv4\n" : "+f"(a), "+f"(b), "+f"(c), "+f"(d));
    *x = a; *y = b; *z = c; *w = d;
}

#else                                                   /* en C (-DSIN_LBLOSTE o fuera de la Dreamcast): la misma XMTRX */
static float lb_xm[16];
static inline void lb_xmtrx_carga(const float *m) { memcpy(lb_xm, m, sizeof lb_xm); }
static inline void lb_xmtrx_guarda(float *m) { memcpy(m, lb_xm, sizeof lb_xm); }
static inline void lb_ftrv(float *x, float *y, float *z, float *w)
{
    const float v0 = *x, v1 = *y, v2 = *z, v3 = *w;
    *x = lb_xm[0] * v0 + lb_xm[4] * v1 + lb_xm[8] * v2 + lb_xm[12] * v3;
    *y = lb_xm[1] * v0 + lb_xm[5] * v1 + lb_xm[9] * v2 + lb_xm[13] * v3;
    *z = lb_xm[2] * v0 + lb_xm[6] * v1 + lb_xm[10] * v2 + lb_xm[14] * v3;
    *w = lb_xm[3] * v0 + lb_xm[7] * v1 + lb_xm[11] * v2 + lb_xm[15] * v3;
}
#endif

/* XMTRX con esas filas: XF[i + 4 j] = r_i[j], o sea la memoria m[4 j + i] = r_i[j] (la traspuesta) */
static inline void lb_xmtrx_filas(const float *r0, const float *r1, const float *r2, const float *r3)
{
    float m[16] __attribute__((aligned(8))); int j;
    for (j = 0; j < 4; j++) { m[4 * j] = r0[j]; m[4 * j + 1] = r1[j]; m[4 * j + 2] = r2[j]; m[4 * j + 3] = r3[j]; }
    lb_xmtrx_carga(m);
}
/* o = a b (filas): con la XMTRX = b (memoria en orden), FTRV de la fila i de a da sum_k a[i][k] b[k][j] = o[i][j] */
static inline void lb_mat_mul(float (*o)[4], const float (*a)[4], const float (*b)[4])
{
    float r[4][4]; int i;
    lb_xmtrx_carga(&b[0][0]);
    for (i = 0; i < 4; i++) { r[i][0] = a[i][0]; r[i][1] = a[i][1]; r[i][2] = a[i][2]; r[i][3] = a[i][3];
                              lb_ftrv(&r[i][0], &r[i][1], &r[i][2], &r[i][3]); }
    memcpy(o, r, sizeof r);
}
#endif
