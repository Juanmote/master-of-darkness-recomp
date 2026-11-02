# Pack de música

El launcher puede sustituir la música del juego por la que tú quieras. La música **no viene con la
app**: cada persona prepara su propio pack (con música que tenga derecho a usar) y lo carga desde el menú.

## Cómo se carga

1. En el juego, abre el menú (SELECT / ATRÁS) › **Opciones** › **Música** y elige **OST 1**, **OST 2** u **OST 3**
   (tres ranuras, para tener hasta tres packs a la vez).
2. En la fila de debajo (**Pack OST n**) elige tu archivo `.zip`. Se guarda dentro de la app: no hay que volver a
   elegirlo.
3. **Música** cambia entre Original, OST 1, OST 2 y OST 3 cuando quieras.
4. **Probar música** (en el mismo menú) hace sonar cualquier pista, para comprobar cómo queda cada una.

En el ordenador, copia los packs como `musica.zip`, `musica2.zip` y `musica3.zip` en la carpeta de datos de la app.

## Formato del pack

- Un archivo **`.zip`** con archivos de audio **Ogg Vorbis (`.ogg`)**. Pueden ir sueltos o dentro de carpetas.
- El nombre de cada archivo **empieza por el código de la pista** (dos caracteres, ver la tabla). Lo que va
  detrás da igual, así que puedes poner el nombre que quieras:
  - `8B.ogg`
  - `8B - Ronda 1 (rock).ogg`
- **Introducción opcional:** si añades `8B_intro.ogg` (o cualquier nombre que empiece por `8B` y **termine** en
  `intro`), suena una vez y después empieza `8B.ogg`, que es la parte que se repite.
- Las pistas que no estén en el pack siguen sonando con la música original.
- Cualquier frecuencia de muestreo (recomendado 44100 Hz). El estéreo se mezcla a mono (la consola es mono).
- Los efectos de sonido del juego se siguen oyendo encima de la música.

## Pistas

| Código | Dónde suena | ¿Se repite? |
|:-----:|-------------|:-----------:|
| `87` | Introducción ("...and the legend goes") | No (dura 1:38; la escena, 2:11) |
| `83` | Pantalla de título | Sí |
| `85` | Presentación de cada ronda (antes de empezar) | No, es corta |
| `8B` | Ronda 1 | Sí |
| `89` | Ronda 2 | Sí |
| `8F` | Ronda 3 | Sí |
| `8C` | Ronda 4 | Sí |
| `84` | Ronda 5 | Sí |
| `88` | Jefes (rondas 1 a 4) | Sí |
| `8E` | Jefe final (ronda 5) | Sí |
| `81` | Escena superada | No, es corta |
| `86` | Ronda superada | No, es corta |
| `8A` | Vida perdida | No, es corta |
| `82` | Continuar / fin de partida | Sí |
| `8D` | Historia y final | No (dura 2:20) |

Las que **se repiten** vuelven a empezar desde el principio de su `.ogg` (o desde el principio de la parte
principal si hay `_intro`). Para que el bucle no se note, corta el archivo justo en el punto de repetición.
Las que **no se repiten** suenan una vez; lo ideal es que duren parecido a la original (las cortas, de 2 a 5 segundos).

Duración de las cortas en el original: `81` 2,6 s, `85` 3 s, `86` 4,8 s, `8A` 1,8 s.

## Consejos

- Ajusta todas las pistas a un volumen parecido. Unos -17 LUFS suenan igual de fuertes que la música original.
- Si el juego hace un fundido (al acabar una escena), la música del pack también se funde.
- En pausa, la música del pack se para y sigue al quitar la pausa.

---

**English:** the music pack is a `.zip` with Ogg Vorbis files whose names start with the track code from the
table above (`8B.ogg`, `8B - Round 1.ogg`, optional `8B_intro.ogg` played once before the loop). Pick OST 1, 2
or 3 in Options › Music and load a pack into that slot (up to three packs). Missing tracks use the original music. Packs are not distributed with the app.
