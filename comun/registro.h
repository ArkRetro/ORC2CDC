/* consola serie sin esperas en carrera (registro.c) */
#ifndef REGISTRO_H
#define REGISTRO_H
void registro_inicia(void);          /* al arrancar: el manejador delante del SCIF (si la salida es el SCIF) */
void registro_diferido(int si);      /* 1: las lineas al anillo (carrera); 0: se vacia y se escribe como antes */
void registro_paso(void);            /* cada cuadro: saca lo que cabe en la cola del SCIF sin esperar */
void registro_guarda(void);          /* desde otro hilo (la musica): si el bucle se para, vacia lo pendiente */
#endif
