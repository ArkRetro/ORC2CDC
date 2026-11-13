/* SH4ZAM: la libreria de matematicas del SH-4 de Falco Girgis (MIT, https://github.com/gyrovorbis/sh4zam, v0.10.x).
   Se instala aparte (kos-ports/sh4zam: docs/DREAMCAST.md, Rendimiento; tools/instala_entorno.sh) y va enlazada en el
   1ST_READ.BIN; aqui no hay codigo suyo. Solo donde ahorra CPU medida (FTRV de la XMTRX en vez de cuentas en C) y sin
   cambiar la imagen ni la fisica. -DSIN_SH4ZAM la quita entera (las cuentas en C de antes); cada uso tiene ademas su
   SIN_SHZ_<parte>.
   Cuidado: las funciones shz_xmtrx_* cargan la XMTRX (la matriz de mat_load / mat_trans de KallistiOS): quien la use
   despues tiene que volver a cargarla (o guardarla antes con shz_xmtrx_store_*). */
#ifndef SHZ_PORT_H
#define SHZ_PORT_H
#if !defined(SIN_SH4ZAM) && defined(_arch_dreamcast)
#define CON_SH4ZAM 1
#include <sh4zam/shz_sh4zam.h>
#endif
#endif
