# Auditoria del motor grafico (iteracion 1, Rev 2.8 / Mod 2.4, 2026-11-22)

Encargo: auditar el motor del port y corregir, por iteraciones medibles, la geometria que invade la carretera, el
popping, la visibilidad, las texturas, los elementos ausentes y el rendimiento. Aqui: lo que se ha comprobado en el
codigo y en los datos, lo medido y lo que queda. Todo en Flycast; **nada verificado en consola real**.

## Fase 1: el motor tal como esta (comprobado en el codigo)

| Parte | Donde (demo/main.c) | Como funciona |
|---|---|---|
| Lectura de modelos | `lee_modelo_en` (432) | Ficheros ORD7 de tools/modelo.py: mallas (una tira cada una), vertices ya en el marco de la etapa, texturas en formato de la PowerVR (twiddled, VQ con libro de 256 o reducido, mipmaps), nodos del original con su esfera, LOD y grupo, ruta cada 4 m, PVS por secciones de 8 m. Al leer: radio de nodo agrandado a sus vertices, grupos de nodos (> 150 m fuera), decor_reduce (351). |
| Colocacion | `modelo_coloca` (1111) | Uniones y etapa siguiente: x, y, z y giro en el marco de la carrera (transforma vertices y nodos una vez, en el hilo de carga). Las matrices del PS2 se convierten en tools/ (conv_tramo.py, conv_union.py); en la DC no hay matrices por objeto salvo carteles (giran hacia la camara) y modelos (coche, trafico, personajes). |
| VRAM | `ar_reserva` (824), `vram_pide` (865), `tex_suelta` (724), `sube_modelo` (638) | Una "zona del curso" (~1280 KB) que se llena desde los dos extremos (etapa y union / siguiente); cada textura reserva su tamano exacto (libros reducidos incluidos); lo que no cabe va a pvr_mem o se rebaja. Medido con -DDIAG_VRAMLOG (docs/REGISTRO_VRAM.md). |
| Carga | hilo de carga (`carga_pide`, `carga_hilo`) | La etapa siguiente y su union se leen en otro hilo mientras se corre (GD-ROM); la subida de texturas va por partes en varios cuadros. |
| Visibilidad | `pvs_paso` (7750), `nodos_visibles` (7842), `nodos_visibles_todos` (8227) | Esfera del nodo contra la piramide de la vista (con bandas de guarda), lejos / detras / lados, LOD del original por radio proyectado, PVS precalculada por seccion (nodos enteros y tiras) con gracia de 15 cuadros, nodos cercanos enteros (PVS_CERCA), estructuras grandes enteras en las zonas de PVS_GRANDE_ZONAS (Rev 2.8). |
| Reparto del TA | `prio_ajusta` (8337), `ta_inicia_cuadro` (1417), `ta_cabe` (1398) | El bufer de vertices del TA (1920 KB x 2) es el limite real por cuadro: cada nodo tiene una clase de distancia y de LOD y el reparto corta por ahi lo que no cabe, con histeresis (Schmitt, 2 cuadros seguidos); lo cercano se manda primero (`dibuja_etapa`, 1898). |
| Dibujo | `dibuja_etapa` (1898), `dibuja_rango` (1791) | Por listas de la PowerVR (opaca, recorte, transparente), tiras con FSRRA para 1/w, cabeceras de `pvr_poly_compile` (envuelta: Strip_Len 6 por defecto, 58). |
| LOD | nodos del original + `tools/lod_veg.py` | LOD por radio proyectado del original (umbral 6 / 7); impostores de la vegetacion lejana (cuadrilatero orientado, LOD_LEJ_PX 48 px con histeresis 1,25). |
| Union / etapa siguiente | `curso_paso` (6598), `geo_vista` (8856), `DETRAS_ENTRADA` (194) | La etapa siguiente crece aplastada desde su base durante 200 m (como el original, 44C530); lo de detras de su entrada no se dibuja (margen 40 m). |

## Fase 2: geometria que invade la carretera

Herramienta nueva `tools/invasion.py`: para cada etapa, los triangulos (listas 0-2, sin cielo, agua, carteles ni
versiones lejanas) con algun punto dentro del espacio por donde circula el coche: sobre la calzada transitable (el
menor entre el ancho del asfalto de la ruta y el limite de la colision del original, menos 2 m de cada borde) y de 0,6
a 3,5 m por encima del perfil de la calzada. Recorre las 62 etapas en 1,5 minutos.

Resultado (Rev 2.8, revisado a mano con vistas cenitales de cada caso dudoso):

- **Falsos positivos explicados**: guardarrailes, bordillos y vallas en el borde (por eso el margen), separadores
  entre carriles donde la colision del original deja pasar al contrario (SEG64 y las etapas de ciudad), y los
  carteles, cuyos vertices van en su marco local (SEG10: 12 nodos de publico y banderines que parecian estar en el
  centro de la calzada en s = 0).
- **Poligonos gigantes que cortan la calzada**: SEG20 / SEG52 (planos verticales translucidos de 80-100 m x 150-210 m
  con la textura 12, un velo casi transparente con bandas: un efecto de bruma / luz del original que se atraviesa) y
  SEG28 / SEG60 (pinos gigantes de 100 m, textura 36, con las ramas sobre la carretera). Son del original, en su
  sitio: no se tocan.
- **No se ha encontrado** geometria mal transformada, fuera de su sitio o con indices rotos en los datos de las
  etapas. Los casos de "muros sobre la carretera" corregidos antes (Rev 1.4: LASV -> MAYA, la etapa siguiente detras
  de su entrada; Rev 2.3: la pancarta de CHECK POINT) eran de la union y del crecimiento de la etapa siguiente, en
  tiempo de ejecucion, no de los datos.

**Iteracion 2 (Rev 2.9)**: el muro de la salida del tunel de ALAS (queja del usuario) SI estaba en los datos y el
detector lo marco con un triangulo (nodo 227, textura 89): no se reviso porque era uno solo. Es una placa de una cara
vista por detras (la demo no descarta caras): corregido con tapas_busca / tapas_paso (docs/PARIDAD.md). Leccion: cada
fila del detector se revisa, aunque sea de un triangulo.

Pendiente: lo que solo pasa en tiempo de ejecucion (la etapa siguiente creciendo, uniones, trafico). Para eso hace
falta la etapa y el momento: con ellos se reproduce con el piloto (PRUEBA_CURSO / PRUEBA_NODO) y se captura.

## Fases 3-7: estado (de iteraciones anteriores, medido)

| Fase | Que hay | Medido |
|---|---|---|
| 3 Popping | Frustum por esfera con bandas de guarda; PVS con gracia; histeresis del LOD y del reparto; carga anticipada de la etapa siguiente en el hilo de carga; grupos de nodos (todo o nada); estructuras grandes enteras en RUIN | RUIN 10,0 -> 4,8 saltos grandes / s (Rev 2.8). Auditoria de la Rev 2.0 por etapas en docs/PARIDAD.md |
| 4 LOD | LOD del original + impostores de vegetacion; 15 % de vegetacion y 5 % de farolas fuera (pedido) | PALM: saltos de la vegetacion por el tope 2,76 -> 0,01 / s con los impostores (revision 46) |
| 5 Texturas | VQ casi todo, libros reducidos (64 / 128 entradas, pvrtex), twiddled, mipmaps del original | VRAM libre media 573 -> 624 KB (Rev 2.2); informe por textura con tools/auditar_vq_pvr.py |
| 6 Rendimiento | Perfil del SH4 (-DDIAG_PERFIL), tiempos por cuadro (-DDIAG_MS), cuadros tarde | LASV 41,1 -> 41,7 fps (Rev 2.8, faros); el reflejo quitado (Rev 2.7: 38,5 -> 41,4) |
| 7 Ausentes | La pancarta CHECK POINT recuperada (Rev 2.3); objetos de las uniones y de las etapas del original convertidos | - |

## Fase 8: validacion

- Flycast con el mando emulado (xdotool sobre la ventana: cruceta, A, B, START, gatillos F / V) y VMU nueva en cada
  prueba (tools/prueba/: nav, nav3, pal, frena, abandona, sp, resultados, final, vmu, vmu_diag, seguidas, muro...; ver
  tools/prueba/README). Las pruebas de rendimiento usan el piloto automatico (-DPRUEBA_AUTO): mide, no prueba el mando.
- En esta iteracion: nav3 (menus con el mando, coche, carrera) en el clasico y el taxi; sin "Out of memory".
- **Consola real: no probado** (no hay hardware aqui).

## Siguiente iteracion (propuesta)

1. Con la etapa y el momento de cada caso de geometria invasiva que se vea en consola, reproducirlo con el piloto y
   corregir su causa.
2. Ampliar `invasion.py` a las uniones (BKnn.DAT) con su propia ruta.
3. Auditoria de popping por etapa con la Rev 2.8 (las 15 de cada arbol) para elegir las siguientes de
   PVS_GRANDE_ZONAS con medida de fps.
