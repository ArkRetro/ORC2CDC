#!/usr/bin/env python3
"""orden.py <flycast.txt> <iso>: compara el recorrido del arbol que hizo la demo (lineas ORDEN / SALIDA / RECORRIDO /
CHECKPOINT / MAPA / GOAL de la consola serie; GDI con -DPRUEBA_ORDEN, ver tools/prueba/orden.sh) con la tabla del
original leida de tu ISO (los registros de 0x78 de SCRIPTS: csc_data_*, *_goals, Races.bin; conv_union.crudos).

Lo que hace el original con la rama r de cada union (race_area_control 44E590 / runtime_apply_course_data_44d720):
el hijo es el registro de la clave +0x2c (izquierda) o +0x30 (derecha); con -1 el registro no cambia (se vuelve a
cargar la misma etapa); sin hijos, la meta. El nivel es la profundidad de 44C850 (en profundidad, solo hacia claves
mayores) y la casilla del mapa de ruta del HUD la de 0x1bd2a8 (+0x74 - 1 si no es 0, si no la clave), en el mapa de 15
casillas con los presets 2 / 3 y en ninguno en las misiones y la Contrarreloj de una etapa (0x1be240).
Sale con error si algo no coincide (o si no hay carreras en el registro)."""
import os, re, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

def tabla(A, arbol):
    """[(clave, zona, hijo izq, hijo der (claves), nivel, casilla)] del arbol (id de CURSO.DAT / CURSOTA.DAT)"""
    import conv_union as cu
    E = A.elf; base, _, _ = cu.tablas(A)
    zona = lambda ti: cu.zona_de(cu.cadena(E, cu.u32(E, base + 4 * ti) + 0x0c))[:4]   # (la consola serie: %.4s)
    rr = cu.crudos(A, arbol)
    if rr is None:                                       # Contrarreloj de una etapa: 44D720 con 635F34 (hijos 0 / 0)
        return [(0, zona(cu.ta_tramo(arbol - 200)), 0, 0, 0, 0)]
    clave = [r[1] for r in rr]; nivel = [0] * len(rr)
    idx = lambda k: clave.index(k) if k in clave else -1
    def baja(i, d):                                      # 44C850
        nivel[i] = d
        for k in (rr[i][11], rr[i][12]):
            if k != -1 and k > clave[i]: baja(idx(k), d + 1)
    baja(0, 0)
    return [(r[1], zona(r[7]), r[11], r[12], nivel[i], r[29] - 1 if r[29] else r[1]) for i, r in enumerate(rr)]

def esperado(T, rama, ta_sola):
    """[(nodo, zona)] de las etapas en orden y la de la meta (None si se acaba por repetir)"""
    clave = [t[0] for t in T]; i = 0; sal = [(0, T[0][1])]; rep = 0
    if ta_sola: return sal, 0
    while True:
        h = T[i][2 + rama]
        if T[i][2] == -1 and T[i][3] == -1: return sal, i
        j = i if h == -1 else clave.index(h)
        rep = rep + 1 if j == i else 0
        sal.append((j, T[j][1])); i = j
        if rep >= 3: return sal, None

def main():
    log, iso = sys.argv[1], sys.argv[2]
    from ps2.assets import Assets
    A = Assets(iso)
    txt = open(log, errors='replace').read().splitlines()
    carreras = []; c = None
    for l in txt:
        m = re.search(r'ORDEN: carrera (\d+) variante (\d+) juego (-?\d+) codigo (-?\d+) mision (-?\d+) rama (-?\d+)', l)
        if m: c = dict(k=int(m[1]), var=int(m[2]), juego=int(m[3]), cod=int(m[4]), mis=int(m[5]), rama=int(m[6]),
                       arbol=None, etapas=[], mapa=[], meta=None, rec=[]); carreras.append(c); continue
        if c is None: continue
        m = re.search(r'SALIDA: arbol (\d+) etapa (\d+) (\w+)', l)
        if m and c['arbol'] is None: c['arbol'] = int(m[1]); c['etapas'].append((int(m[2]), m[3])); continue
        m = re.search(r'CHECKPOINT: etapa (\d+) \((\w+)\)', l)
        if m: c['etapas'].append((int(m[1]), m[2])); continue
        m = re.search(r'RECORRIDO: arbol \d+ nodo (\d+) \w+ rama (\d) -> nodo (\d+)', l)
        if m: c['rec'].append((int(m[1]), int(m[2]), int(m[3]))); continue
        m = re.search(r'MAPA: arbol \d+ nodo (\d+) \w+ nivel (-?\d+) casilla (-?\d+) mapa (\d)', l)
        if m: c['mapa'].append(tuple(int(x) for x in m.groups())); continue
        if 'ORDEN: atascado' in l: c['atascado'] = True; continue
        m = re.search(r'CURSO: GOAL en (\w+)', l)
        if m: c['meta'] = m[1]
    carreras = [c for c in carreras if c['arbol'] is not None]
    if not carreras: print('orden: sin carreras en %s' % log); sys.exit(1)
    mal = 0
    for c in carreras:
        T = tabla(A, c['arbol']); ta_sola = 200 <= c['arbol'] < 200 + 0x3c
        exp, fin = esperado(T, c['rama'], ta_sola)
        hecho = c['etapas']; n = min(len(exp), len(hecho))
        prob = []
        if c['rama'] < 0: prob.append('sin PRUEBA_RAMA_FIJA')
        if c.get('atascado'): prob.append('atascado (5 min sin checkpoint)')
        if hecho[:n] != exp[:n]: prob.append('etapas %s, tabla %s' % (hecho, exp))
        acabada = c['meta'] is not None
        if acabada and len(hecho) != len(exp): prob.append('GOAL tras %d etapas, la tabla %d' % (len(hecho), len(exp)))
        if acabada and (fin is None or T[fin][1] != c['meta'] if not ta_sola else T[0][1] != c['meta']):
            prob.append('GOAL en %s, la tabla %s' % (c['meta'], 'sin meta' if fin is None else T[fin][1]))
        for nodo, rama, sig in c['rec']:
            h = T[nodo][2 + rama]; e = nodo if h == -1 else [t[0] for t in T].index(h)
            if rama != c['rama'] or sig != e: prob.append('rama %d en el nodo %d -> %d, la tabla %d' % (rama, nodo, sig, e))
        tipo = 0 if c['var'] == 6 or ta_sola else 2 if c['arbol'] in (290, 291, 280, 281, 282, 283) else 1
        for nodo, nv, cas, mp in c['mapa']:
            if (nv, cas, mp) != (T[nodo][4], T[nodo][5], tipo): prob.append('mapa nodo %d: nivel %d casilla %d mapa %d, la tabla %d %d %d' % (nodo, nv, cas, mp, T[nodo][4], T[nodo][5], tipo))
        if not c['mapa']: prob.append('sin lineas MAPA')
        estado = 'GOAL en %s' % c['meta'] if acabada else 'cortada (%d etapas)' % len(hecho)
        print('carrera %d: variante %d juego %d codigo %d mision %d arbol %d rama %d: %s, %s: %s' % (
            c['k'], c['var'], c['juego'], c['cod'], c['mis'], c['arbol'], c['rama'], ' '.join(z for _, z in hecho), estado,
            'BIEN' if not prob else 'MAL: ' + '; '.join(prob)))
        mal += bool(prob)
    print('orden: %d carreras, %d mal' % (len(carreras), mal))
    sys.exit(1 if mal else 0)

if __name__ == '__main__':
    main()
