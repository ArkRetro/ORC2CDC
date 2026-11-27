#ifndef MUSICA_H
#define MUSICA_H
void musica_inicia(void);
int musica_pon(int n);              /* MUSn.PCM desde el principio (en bucle) */
int musica_pon_una(int n);          /* lo mismo, una sola vez (sin bucle) */
void musica_para(void);
void musica_poll(void);             /* una vez por cuadro */
void musica_pausa_lectura(int p);   /* deja libre el GD (cargas) */
void musica_nivel(float n);         /* volumen de la musica 0..1 (opcion de musica / 10, 0x100210 del original) */
/* Tema de mas del selector (SPLASH WAVE REMASTER de SUPNEO, tools/menu_musica.py; docs/PARIDAD.md "Tarjeta SPLASH
   WAVE REMASTER"): el tema 28, la ultima tarjeta de la lista 2, solo si MENU.DAT la lleva (MUSICA_MARCA_EXTRA en el
   primer campo libre de la pantalla MUSICA2 de MNU1). Sus pistas no son las del original (la 28 es la del final de
   la recreativa y la 61 = 0x21 + 28 la de los resultados de OUTRUN2SP): MUS70.ADP el tema y MUS71.ADP el avance */
#define MUSICA_TEMA_EXTRA 28
#define MUSICA_PISTA_EXTRA 70
#define MUSICA_AVANCE_EXTRA 71
#define MUSICA_MARCA_EXTRA 0x31525753u     /* 'SWR1' */
int musica_pista_tema(int k);       /* pista de carrera del tema k del selector (k; el 28, la 70) */
int musica_avance_tema(int k);      /* pista del avance del tema k (0x21 + k; el 28, la 71) */
int musica_tema_pista(int n);       /* al reves (la 70: el 28) */
#endif
