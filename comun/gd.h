#ifndef GD_H
#define GD_H
#include <stdint.h>
int gd_busca(const char *nombre, uint32_t *lba, uint32_t *tam);
void *gd_carga(const char *nombre, uint32_t *tam);
int gd_lee(uint32_t lba, int n, void *dst);           /* sectores de 2048 desde lba (por DMA si dst va alineado a 32) */
#endif
