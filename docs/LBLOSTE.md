# Little Bloste

Las matematicas del SH-4 propias de OutRun DC (desde la Rev 2.9.2.c / Mod 2.5.2.c). Sustituye a SH4ZAM, que el port
usaba en tres sitios y ya no se usa ni se instala. Codigo propio, solo cabeceras: `comun/lbloste.h`.

"Bloste" sera, mas adelante, la capa propia que sustituya a KallistiOS (no empezada: ver al final). Little Bloste es su
primera pieza.

## La XMTRX y FTRV

El SH-4 tiene un segundo banco de 16 registros de coma flotante (XF0..XF15), la XMTRX, y la instruccion `FTRV XMTRX, FVn`,
que multiplica esa matriz por un vector de 4 (FV0 = FR0..FR3, FV4 = FR4..FR7...) en una sola instruccion:

    r[i] = suma_j XF[i + 4 j] x v[j]

Cargada desde memoria en orden (XF0 = m[0], XF1 = m[1]...), una matriz de filas (`matrix_t` de KallistiOS, la de
`mat_load`) queda con `XF[i + 4 j] = m[j][i]`, asi que FTRV calcula el vector fila por la matriz, `r = v M`: lo mismo que
`mat_trans_single` de KallistiOS. La XMTRX es la misma que usan `mat_load` / `mat_apply`: quien la cambia tiene que
dejarla como la necesite quien venga despues.

## Funciones (`comun/lbloste.h`)

| Funcion | Que hace | Como |
|---|---|---|
| `lb_xmtrx_carga(m)` | XMTRX = m (16 floats en el orden de la memoria, sin alinear vale) | `FRCHG`, 16 `FMOV.S @Rn+`, `FRCHG` |
| `lb_xmtrx_guarda(m)` | m = XMTRX | `FRCHG`, 16 `FMOV.S FRn,@-Rm`, `FRCHG` |
| `lb_xmtrx_filas(r0, r1, r2, r3)` | XMTRX tal que FTRV da (r0 . v, r1 . v, r2 . v, r3 . v) | carga la traspuesta: `m[4 j + i] = r_i[j]` |
| `lb_ftrv(&x, &y, &z, &w)` | (x, y, z, w) = XMTRX por (x, y, z, w) | `FTRV XMTRX, FV4` con FR4..FR7 como variables de registro |
| `lb_mat_mul(o, a, b)` | o = a b (matrices de filas 4x4; o puede ser a o b) | XMTRX = b y cada fila de a por FTRV; deja la XMTRX = b |

Con `-DSIN_LBLOSTE` (y fuera de la Dreamcast) todas tienen su version en C con una XMTRX en memoria: mismas cuentas,
para comparar y para compilar sin el ensamblador. Cada uso se puede quitar solo: `-DSIN_LB_NODOS`, `-DSIN_LB_ENTORNO`,
`-DSIN_LB_ROBOT`.

## Donde se usa

1. **Recorte de nodos** (`demo/main.c`, `nodos_visibles`): guarda la XMTRX de quien llama, carga las filas derecha,
   arriba y adelante de la camara y saca de una vez (xv, yv, zv) de cada nodo con FTRV; al salir repone la XMTRX.
2. **UV del mapa de entorno del coche** (`demo/main.c`, `coche_entorno`): filas (0,5, a) y (0,5, b) por (1, nx, ny, nz)
   de cada vertice: u = 0,5 + a . n, v = 0,5 + b . n.
3. **Matrices de los personajes** (`demo/main.c` `m_por` y `comun/robot.c` `m_mul`): o = a b de las paletas de huesos
   del piloto, la acompanante y el banderero.

## Comprobado

- **Autoprueba** (`-DLBLOSTE_PRUEBA`: al arrancar, 4000 casos al azar con valores de -100 a 100, contra la cuenta en C):
  guardar lo cargado da la misma XMTRX (0 diferencias); el error maximo respecto a la suma de los |terminos| es
  1,8e-7 (FTRV), 1,7e-7 (filas) y 2,1e-7 (producto de matrices), unos 3 ulp, del mismo orden que la propia cuenta en C
  (2,1e-7 / 1,8e-7 / 2,5e-7: Flycast hace las dos en doble y redondea al final). En la consola FTRV suma con mas bits
  que C y redondea una vez: la diferencia sigue siendo de ese orden.
- **Rendimiento** (Flycast, PALM con el piloto, 95 medidas): 41,50 fps con Little Bloste, 41,31 con las cuentas en C.
- **Imagen**: coche, piloto y acompanante y el recorte de la etapa como antes.
- **NO verificado en consola real** (como todo el port: ver docs/DREAMCAST.md).

## Bloste (pendiente, no empezado)

La idea es una capa propia que sustituya a KallistiOS: arranque, PowerVR, AICA (con su programa del ARM), GD-ROM, Maple
y VMU, hilos y video. Por fases, cada una con su revision: primero una capa `bloste_*` que por dentro llame a KallistiOS y
despues cada parte reemplazada y probada en Flycast y en la consola. Ver la conversacion del 2026-11-23 en el historial
del proyecto: aplazado.
