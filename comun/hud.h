#ifndef HUD_H
#define HUD_H
typedef struct {
    int tiempo, crono_ms, puntos, kmh, marcha, etapa, cuadro;
    float rpm;
    int rebufo, derrape, manual, seis, rev_on;
} hud_estado_t;
int hud_carga(const char *fich, int modelo);            /* 0 bien; modelo = indice de tacometro (6 = Testarossa) */
void hud_dibuja(const hud_estado_t *e);                  /* dentro de la lista TR */
void hud_aviso(const char *nombre, int cuadro, float alfa);   /* despues de hud_dibuja */
#endif
