#ifndef MUSICA_H
#define MUSICA_H
/* ALL I WANT (Crazy Taxi): tema 28 del selector (una tarjeta mas en la lista 2); su pista es MUS67.ADP */
#define MUSICA_TEMA_ALL_I_WANT 28
#define MUSICA_PISTA_ALL_I_WANT 67
void musica_inicia(void);
int musica_pon(int n);              /* MUSn.PCM desde el principio (en bucle) */
int musica_pon_una(int n);          /* lo mismo, una sola vez (sin bucle) */
void musica_para(void);
void musica_poll(void);             /* una vez por cuadro */
void musica_pausa_lectura(int p);   /* deja libre el GD (cargas) */
void musica_nivel(float n);         /* volumen de la musica 0..1 (opcion de musica / 10, 0x100210 del original) */
#endif
