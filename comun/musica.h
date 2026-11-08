#ifndef MUSICA_H
#define MUSICA_H
void musica_inicia(void);
int musica_pon(int n);              /* MUSn.PCM desde el principio (en bucle) */
void musica_para(void);
void musica_poll(void);             /* una vez por cuadro */
void musica_pausa_lectura(int p);   /* deja libre el GD (cargas) */
#endif
