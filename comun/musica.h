#ifndef MUSICA_H
#define MUSICA_H
void musica_inicia(void);
int musica_pon(int n);              /* MUSn.PCM desde el principio (en bucle) */
int musica_pon_una(int n);          /* lo mismo, una sola vez (sin bucle) */
void musica_para(void);
void musica_poll(void);             /* una vez por cuadro */
void musica_pausa_lectura(int p);   /* deja libre el GD (cargas) */
void musica_nivel(float n);         /* volumen de la musica 0..1 (opcion de musica / 10, 0x100210 del original) */
#endif
