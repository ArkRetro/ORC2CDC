#ifndef MUSICA_H
#define MUSICA_H
/* ALL I WANT (Crazy Taxi): tema 28 del selector (una tarjeta mas en la lista 2); su pista es MUS67.ADP (sin avance:
   en el selector suena entera). La conversion tema -> pista la hacen musica_pista_tema / musica_tema_pista (abajo), no
   musica_pon: la pista 28 es la del final de la recreativa (fin.c) y tiene que seguir siendo MUS28.ADP */
#define MUSICA_TEMA_ALL_I_WANT 28
#define MUSICA_PISTA_ALL_I_WANT 67
void musica_inicia(void);
int musica_pon(int n);              /* MUSn.PCM desde el principio (en bucle) */
int musica_pon_una(int n);          /* lo mismo, una sola vez (sin bucle) */
void musica_para(void);
void musica_poll(void);             /* una vez por cuadro */
void musica_pausa_lectura(int p);   /* deja libre el GD (cargas) */
void musica_nivel(float n);         /* volumen de la musica 0..1 (opcion de musica / 10, 0x100210 del original) */
/* Tema de mas del selector (SPLASH WAVE REMASTER de SUPNEO, tools/menu_musica.py; docs/PARIDAD.md "Tarjeta SPLASH
   WAVE REMASTER"): en la clasica el tema 28; en el Mod taxi el 29, la tarjeta de detras de ALL I WANT (la 14.a de la
   lista 2), solo si MENU.DAT la lleva (MUSICA_MARCA_EXTRA en el primer campo libre de la pantalla MUSICA2 de MNU1;
   tools/menu_musica.py la pone detras de ALL I WANT con la marca 'SWR2'). Sus pistas no son las del original (la 28 es
   la del final de la recreativa y la 61 = 0x21 + 28 la de los resultados de OUTRUN2SP): MUS70.ADP el tema y MUS71.ADP
   el avance */
#define MUSICA_TEMA_EXTRA 29
#define MUSICA_PISTA_EXTRA 70
#define MUSICA_AVANCE_EXTRA 71
#define MUSICA_MARCA_EXTRA 0x32525753u     /* 'SWR2' (detras de ALL I WANT; 'SWR1' es la de la clasica, en el 28) */
int musica_pista_tema(int k);       /* pista de carrera del tema k del selector (k; el 28, la 67; el 29, la 70) */
int musica_avance_tema(int k);      /* pista del avance del tema k (0x21 + k; el 29, la 71; el 28 no tiene) */
int musica_tema_pista(int n);       /* al reves (la 67: el 28; la 70: el 29) */
#endif
