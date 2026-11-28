/* reflejo.c: el color del entorno visto desde un punto en un conjunto de direcciones (trazado de rayos por fuerza bruta).
   Lo usa tools/conv_reflejo.py (con ctypes, tools/nativo.py) para los mapas del reflejo de la carroceria.

   rayos(tri, col, ntri, o, dir, nd, tmin, fondo, sal):
     tri: ntri x 9 floats (tres vertices); col: ntri x 3 floats (su color); o: el punto (3 floats); dir: nd x 3 floats
     (unitarias); tmin: distancia minima del choque; fondo: nd x 3 floats, el color si el rayo no choca (el cielo);
     sal: nd x 3 floats, el color del triangulo mas cercano en cada direccion o el del fondo. Devuelve los rayos que
     chocan. Los triangulos se ven por las dos caras (las de las etapas no tienen un lado fijo). */
#include <math.h>

int rayos(const float *tri, const float *col, int ntri, const float *o, const float *dir, int nd, float tmin,
          const float *fondo, float *sal)
{
    int r, j, n = 0;
    for (r = 0; r < nd; r++) {
        const float dx = dir[3 * r], dy = dir[3 * r + 1], dz = dir[3 * r + 2];
        float mejor = 1e30f; int k = -1;
        for (j = 0; j < ntri; j++) {
            const float *t = tri + 9 * j;
            float e1x = t[3] - t[0], e1y = t[4] - t[1], e1z = t[5] - t[2];
            float e2x = t[6] - t[0], e2y = t[7] - t[1], e2z = t[8] - t[2];
            float px = dy * e2z - dz * e2y, py = dz * e2x - dx * e2z, pz = dx * e2y - dy * e2x;
            float det = px * e1x + py * e1y + pz * e1z, inv, tx, ty, tz, u, qx, qy, qz, v, d;
            if (fabsf(det) < 1e-12f) continue;
            inv = 1.0f / det; tx = o[0] - t[0]; ty = o[1] - t[1]; tz = o[2] - t[2];
            u = (tx * px + ty * py + tz * pz) * inv;
            if (u < 0.0f || u > 1.0f) continue;
            qx = ty * e1z - tz * e1y; qy = tz * e1x - tx * e1z; qz = tx * e1y - ty * e1x;
            v = (dx * qx + dy * qy + dz * qz) * inv;
            if (v < 0.0f || u + v > 1.0f) continue;
            d = (e2x * qx + e2y * qy + e2z * qz) * inv;
            if (d > tmin && d < mejor) { mejor = d; k = j; }
        }
        if (k >= 0) { sal[3 * r] = col[3 * k]; sal[3 * r + 1] = col[3 * k + 1]; sal[3 * r + 2] = col[3 * k + 2]; n++; }
        else { sal[3 * r] = fondo[3 * r]; sal[3 * r + 1] = fondo[3 * r + 1]; sal[3 * r + 2] = fondo[3 * r + 2]; }
    }
    return n;
}
