# Modo performance: guía

*skiff-atlatl en el M-VAVE FM-1 · [English version](PERFORMANCE.md)*

skiff-atlatl está hecho para tocarse en vivo, como una groovebox de hardware: los patrones se ponen en cola y cambian a tiempo, los mutes y las escenas
entran juntos, las perillas "lanzan" valores que regresan solas, hay una capa de efectos sobre el master que suenan
mientras los mantienes, y las condiciones de trig, la probabilidad y los p-locks mantienen vivo un loop sin tocarlo.

## Lo básico

- **Capas:** mantén presionado un botón de página (FX, GLO, ARP, SEQ...) y su capa se abre sobre la pantalla: las teclas
  y las 4 perillas pasan a ser los controles de esa capa mientras lo mantienes. **SHIFT + el botón** (HOME presionado,
  luego el botón) la deja abierta ("LOCKED"); el botón otra vez la cierra.
- **Tracks:** son seis, T1–T6. **ALGORITHM** elige el track en cualquier página.
- **Teclas negras F#3 G#3 A#3 C#4 D#4 F#4:** siempre son los mutes de T1–T6 en las capas GLO, FX y PERFORM.

## 1. Abrir PERFORM

**Mantén ARP.** El mapa de PERFORM muestra una fila por track con las entradas de su lista (A, B, C...): la que suena
enmarcada, la que espera marcada, y abajo una fila de escenas (SCN). Para tocar con las manos libres: **SHIFT + ARP**;
ARP otra vez para salir.

| Control | Qué hace |
| --- | --- |
| **SELECT** (girar) | el punto de **LAUNCH**: **LOOP** (fin de la vuelta del track), **BAR**, **BEAT** o **NOW** |
| **PRESETS** (girar) | las **KEYS**: **TRACK** (las teclas blancas son las entradas del track elegido) o **SCENE** (son las 16 escenas) |
| **Teclas blancas** | lanzan una entrada o llaman una escena, en el punto de LAUNCH |
| **Teclas negras** | mutes de los tracks |
| **PERILLAS 1–4** | throws (abajo) |
| **OCT−** | cancela lo que está en cola y aún no entra |
| **OCT+** | deja los throws donde están en vez de que regresen |

## 2. Lanzar patrones (KEYS TRACK)

Cada track tiene una lista de 16 entradas, **A–P** (cada una un patrón y cuántas veces se repite), que armas en
**SEQ › SONG**. Elige el track con **ALGORITHM**, pon **LAUNCH** en BAR (o NOW para stutters) y toca una tecla blanca:
F3 es la entrada A, G3 la B, etc. La entrada queda **en cola** (parpadea) y cambia justo en el punto de LAUNCH. Con
**SONG ON** cada track avanza solo por su lista; con **SONG OFF** repite la entrada que lanzaste.

## 3. Escenas (KEYS SCENE)

Una escena guarda, para los seis tracks, la entrada en la que está cada uno y todos los mutes. **Guardar:** arma los
tracks, mantén **SAVE** y toca una tecla blanca. **Llamar:** toca la tecla; todo entra junto en el punto de LAUNCH.
Las 16 escenas se guardan con el proyecto.

## 4. Mutes

En GLO, FX y PERFORM las teclas negras F#3–F#4 son los mutes de T1–T6. **Un toque** mutea o desmutea (en PERFORM espera
al punto de LAUNCH, así cae en la rejilla; en GLO y FX es inmediato). **Mantenida**, la tecla corta el track solo
mientras la sostienes.

## 5. Throws (perillas en PERFORM)

| Perilla | Throw |
| --- | --- |
| 1 | el tono: CUTOFF o el control de tono del motor (en SYNTH, SHAPE A) |
| 2 | envío a REVERB |
| 3 | envío a DELAY |
| 4 | DIST |

Suelta la perilla y el valor regresa en el tiempo de **MENU › CONTROL › RETURN** (**SNAP**, **1 BEAT** o **1 BAR**);
**OCT+** lo deja donde está. Nunca se guardan y van encima de la automatización y los locks.

## 6. Capa FX: efectos de performance sobre el master

**Mantén FX.** Las teclas blancas son efectos que duran mientras las mantienes:

| Tecla | Efecto | Tecla | Efecto |
| --- | --- | --- | --- |
| F3 | REPEAT 1/8 | F4 | FREEZE |
| G3 | REPEAT 1/16 | G4 | OCT UP |
| A3 | REPEAT 1/32 | A4 | OCT DN |
| B3 | REVERSE | B4 | GATE (picado en 16avos) |
| C4 | barrido LPF (cierra en un compás) | C5 | DUB (al delay, los ecos siguen) |
| D4 | barrido HPF (abre en un compás) | D5 | FLANGE |
| E4 | TAPE STOP | E5 | VINYL |

REPEAT y REVERSE arrancan en el siguiente 16avo. **Perillas 1–4:** FILTER (izquierda pasa-bajos, derecha pasa-altos),
CRUSH, THROW (la mezcla al delay y la reverb) y DEPTH. Al soltar FX regresan como los throws; SHIFT + FX los deja
enganchados.

## 7. Capa GLO: fills, solos y tempo

**Mantén GLO.** **F4 FILL:** mientras la mantienes suenan los pasos con condición **FILL** y callan los **!FILL** (el
fill al final de una frase). **F3–D4:** solo de T1–T6 mientras las mantienes; **E4** desmutea todo;
**G4** tap tempo. **Perillas 1–4:** volumen de los cuatro tracks visibles. **GLO + PLAY** vuelve al inicio sin parar.

## 8. Que el loop evolucione solo

En **SEQ › NOTES / FEEL**, selecciona pasos y gira:

- **CHANCE** (FEEL, perilla 1): 1–100 % de probabilidad; más allá de 100 % están las **condiciones de trig**:
  **1:2 … 4:4** (la vuelta N de cada M), **FILL / !FILL**, **PRE / !PRE** (si la condición anterior de este track pasó o
  no), **1ST / !1ST** (solo la primera vuelta tras PLAY, o todas las demás), **NEI / !NEI** (si la última condición del
  track *anterior* pasó o no: T2 mira a T1, T3 a T2…, para pregunta y respuesta entre tracks).
- **RATCHET** (FEEL, perilla 2): ×1–×4 repeticiones dentro del paso, o FLAM; en la página SEQ **RETRIG**, **RETRIG VEL**
  hace que suban (+) o bajen (−).
- **NUDGE** (FEEL, perilla 3): mueve el paso fuera de la rejilla, en 64avos.
- **P-locks:** mantén un paso y gira las perillas 1–4: esos valores cambian solo en ese paso. **SEQ › AUTOMATION** los
  lista todos.
- **MOMENTARY:** mantén **LFO** y gira perillas en una página de sonido o de FX: al soltar regresan.

## 9. El arpegiador en vivo

En **ARP / ARP 2**: MODE, RATE (con tresillos **4T, 8T, 16T, 32T**), OCTAVE, GATE, SWING, HOLD. La página **ARP STEP**
agrega un patrón de pasos al arpegiador: **perilla 1 STEPS** (1–16; corre contra el ciclo de las notas: polimetría),
**perilla 2 STEP** (el paso a editar; PRESETS también), **perilla 3 TRANSP** (−24…+24 semitonos), **perilla 4 STATE**
(**ON**, **OFF** = silencio, **STACC** = un cuarto del gate). 16 pasos ON en 0 es el arpegiador de siempre. Se guarda con
el proyecto y se edita también en el editor web (pestaña Sound, ARP STEP). Muchos presets de SYNTH traen su propio
arpegiador y LFO.

## 10. Una forma de tocar un set

1. Arma las listas en SEQ › SONG (A intro, B parte principal, C break...), con **SONG OFF**.
2. Guarda los momentos clave como escenas (SAVE mantenido + tecla, en PERFORM con KEYS SCENE).
3. Bloquea PERFORM (**SHIFT + ARP**) con **LAUNCH BAR**: cambia de escena con las teclas blancas, entra y saca tracks con
   las negras (toque = engancha, mantener = corta) y lanza cutoff, reverb y delay con las perillas.
4. Mantén **FX** para repeats, tape stops y barridos en las transiciones, y **GLO + F4** para los fills.
5. Deja que **CHANCE**, las **condiciones de trig** y el **ARP STEP** muevan los loops entre tus cambios.

Para ver la pantalla en grande, abre la pestaña **Screen** del editor web (o la app
[FM-1 Mirror](https://github.com/pfkellogg/fm1-mirror)).

---

<sub>Prophet-5, Sequential, Oberheim, Mutable Instruments, Plaits, Rings, Roland, TR-808, TR-909, TR-626, TB-303 y Devil Fish son marcas o nombres de sus respectivos dueños. Se mencionan solo para describir en qué se inspiró cada función; skiff-atlatl no está afiliado a ellos ni cuenta con su respaldo.</sub>
