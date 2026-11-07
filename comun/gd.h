#ifndef GD_H
#define GD_H
#include <stdint.h>
int gd_busca(const char *nombre, uint32_t *lba, uint32_t *tam);
void *gd_carga(const char *nombre, uint32_t *tam);
#endif
