#ifndef GHO_H
#define GHO_H
#include <stdint.h>
/* Rev 2.9.9.c: el bloque GHO1 (coche fantasma) de un COCHEnn[L].DAT del GD, sin leer el fichero entero (ver gho.c).
   dst: los triangulos (20 bytes cada uno, alineado a 4; cap bytes), sector: 2 KB de cache (alineado a 32). 0 bien, *n
   triangulos */
int gho_lee(const char *fich, void *dst, uint32_t cap, uint8_t *sector, uint32_t *n);
#endif
