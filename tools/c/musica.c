/* musica.c entrada salida.adp [wav] [desplazamiento] [intercalado] (con PCM=1 en el entorno, PCM de 16 bits): una pista de MUSIC.PS2 (ADPCM de PS2, estereo
   intercalado por bloques) a ADPCM de Yamaha para el AICA de la Dreamcast, estereo intercalado por nibbles como lo
   espera snd_stream de KallistiOS (nibble bajo: derecho, alto: izquierdo). Con wav, tambien un WAV de 16 bits.
   Sale de tu disco: no se distribuye. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const int F[5][2] = { { 0, 0 }, { 60, 0 }, { 115, -52 }, { 98, -55 }, { 122, -60 } };

/* un bloque de 16 bytes de ADPCM de PS2 -> 28 muestras */
static void ps2_bloque(const uint8_t *b, int16_t *out, int *h1, int *h2)
{
    int pred = (b[0] >> 4) & 7, sh = b[0] & 15, i;
    if (pred > 4) pred = 0;
    for (i = 0; i < 28; i++) {
        int n = (b[2 + i / 2] >> ((i & 1) * 4)) & 15, s;
        if (n & 8) n -= 16;
        s = (n << 12) >> sh;
        s += (*h1 * F[pred][0] + *h2 * F[pred][1] + 32) >> 6;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        *h2 = *h1; *h1 = s; out[i] = (int16_t)s;
    }
}

/* ADPCM de Yamaha (AICA), como wav2adpcm de KallistiOS */
static const int DIF[16] = { 1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15 };
static const int ESC[8] = { 0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 };
typedef struct { int senal, paso; } yam_t;
static int yam_cod(yam_t *e, int x)
{
    int d = x - e->senal, v;
    d = d * 8 / e->paso;
    v = abs(d) / 2;
    if (v > 7) v = 7;
    if (d < 0) v += 8;
    e->senal += e->paso * DIF[v] / 8;
    if (e->senal > 32767) e->senal = 32767;
    if (e->senal < -32768) e->senal = -32768;
    e->paso = e->paso * ESC[v & 7] >> 8;
    if (e->paso < 0x7f) e->paso = 0x7f;
    if (e->paso > 0x6000) e->paso = 0x6000;
    return v;
}

int main(int argc, char **argv)
{
    FILE *f; uint8_t *d; long n, p; int16_t *L, *R; long ns = 0, cap, i;
    long ini = argc > 4 ? strtol(argv[4], 0, 0) : 16, inter = argc > 5 ? strtol(argv[5], 0, 0) : 0x200;
    int h[2][2] = { { 0, 0 }, { 0, 0 } }; yam_t e[2] = { { 0, 0x7f }, { 0, 0x7f } }; uint8_t *o;
    if (argc < 3) { fprintf(stderr, "uso: musica entrada salida.adp [wav] [desplazamiento] [intercalado]\n"); return 1; }
    f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    d = malloc(n); fread(d, 1, n, f); fclose(f);
    cap = (n / 16) * 28; L = malloc(cap * 2); R = malloc(cap * 2);
    for (p = ini; p + 2 * inter <= n; p += 2 * inter) {
        long k;
        for (k = 0; k < inter; k += 16) {
            int16_t a[28], b[28]; int j;
            ps2_bloque(d + p + k, a, &h[0][0], &h[0][1]);
            ps2_bloque(d + p + inter + k, b, &h[1][0], &h[1][1]);
            for (j = 0; j < 28; j++) { L[ns] = a[j]; R[ns] = b[j]; ns++; }
        }
    }
    ns &= ~1L;
    if (getenv("PCM")) {                                    /* PCM de 16 bits a 44,1 kHz (lineal desde 48 kHz) */
        long m = (long)((double)ns * 44100 / 48000), j; int16_t *q = malloc(m * 4);
        for (j = 0; j < m; j++) {
            double t = (double)j * 48000 / 44100; long k = (long)t; double a = t - k;
            if (k + 1 >= ns) k = ns - 2;
            q[2 * j] = (int16_t)(L[k] * (1 - a) + L[k + 1] * a); q[2 * j + 1] = (int16_t)(R[k] * (1 - a) + R[k + 1] * a);
        }
        f = fopen(argv[2], "wb"); fwrite(q, 4, m, f); fclose(f);
        fprintf(stderr, "PCM 44,1 kHz: %ld muestras\n", m);
        return 0;
    }
    o = malloc(ns);
    for (i = 0; i < ns; i++) o[i] = (uint8_t)(yam_cod(&e[1], R[i]) | (yam_cod(&e[0], L[i]) << 4));
    f = fopen(argv[2], "wb"); fwrite(o, 1, ns, f); fclose(f);
    if (argc > 3 && strcmp(argv[3], "-")) {
        uint32_t t = 44 + ns * 4, r = 48000, br = r * 4; uint16_t c = 2, ba = 4, bits = 16, fmt = 1; uint32_t sz = 16, ds = ns * 4;
        f = fopen(argv[3], "wb");
        fwrite("RIFF", 1, 4, f); fwrite(&t, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&sz, 4, 1, f);
        fwrite(&fmt, 2, 1, f); fwrite(&c, 2, 1, f); fwrite(&r, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&ds, 4, 1, f);
        for (i = 0; i < ns; i++) { fwrite(&L[i], 2, 1, f); fwrite(&R[i], 2, 1, f); }
        fclose(f);
    }
    fprintf(stderr, "%ld muestras por canal (%.1f s a 48 kHz, %.1f s a 44,1 kHz)\n", ns, ns / 48000.0, ns / 44100.0);
    return 0;
}
