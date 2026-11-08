#ifndef HUD_H
#define HUD_H
typedef struct {
    int tiempo, crono_ms, puntos, kmh, marcha, etapa, cuadro;
    float rpm;
    int rebufo, derrape, manual, seis, rev_on;
    int puntos_estilo;      /* cifras de la puntuacion (4BD480): 0 normal (estilo 1), 1 rebufo (0xC), 2 fuera (0xD) */
} hud_estado_t;
int hud_carga(const char *fich, int modelo);            /* 0 bien; modelo = indice de tacometro (6 = Testarossa) */
void hud_dibuja(const hud_estado_t *e);                  /* dentro de la lista TR */
void hud_aviso(const char *nombre, int cuadro, float alfa);   /* despues de hud_dibuja */
/* mensaje de puntos de la cola de 4B9770 ("%7d" bajo la puntuacion, 4BD480): estilo como puntos_estilo, dy pixeles
   por debajo de la puntuacion; despues de hud_dibuja */
void hud_puntos_msg(int puntos, int estilo, float dy, float alfa);
#endif
