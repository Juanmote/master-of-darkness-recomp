# Estado del proyecto — Master of Darkness Re-Master-Ed

Última actualización: 2 de noviembre de 2026. Versión publicada: **v0.9.8**.

Launcher para Android (y escritorio) que ejecuta *Master of Darkness* (Master System) desde la ROM del
usuario, con 16:9 real, trucos, opciones de juego, filtros, paletas, pack de música y controles táctiles.
Detalles técnicos de cada versión: [`docs/NOTAS_TECNICAS.md`](docs/NOTAS_TECNICAS.md). Pack de música:
[`docs/PACK_MUSICA.md`](docs/PACK_MUSICA.md). Guion de narración: [`docs/GUION_NARRACION.md`](docs/GUION_NARRACION.md).

ROM necesaria: *Master of Darkness (Europe, Brazil) (En).sms*, CRC32 `96FB4D4B`. Con otra ROM el juego
arranca, pero las mejoras se desactivan solas (se comprueban los bytes de cada punto enganchado).

---

## Lo que funciona

**Pantalla de inicio y app**
- Pantalla de inicio con el arte del usuario: Jugar, Opciones, Controles, Créditos, Salir.
- Salir cierra la app del todo: `GameActivity` hace `finishAndRemoveTask()` y `System.exit(0)`.
- Icono con el logo.
- Interfaz en inglés por defecto y en español si el sistema está en español; se cambia en *Sonido e idioma*.
  Los botones del launcher tienen versión en inglés (`assets/launcher/en_*.png`).
- La primera vez se elige la ROM con el selector de archivos. Los packs de música se eligen igual y se copian dentro
  de la app.
- Instalación nueva = **versión remasterizada** (ver Juego).

**Imagen**
- Formatos 16:9 panorámico real (43 px de escenario extra por lado, reconstruidos del mapa del juego en RAM),
  4:3 y 8:7 (tele de tubo).
- En 4:3 y 8:7 se rellena la columna de 8 px que el juego tapa a la izquierda.
- Escalado: ajustar, entero, estirar. Margen de pantalla para móviles: no, pequeño o mediano.
- Filtros: nítido, nítido suavizado, suave, scanlines, CRT y CRT curvo. Los dos CRT van por GPU (máscara de
  fósforo, perfil del haz, resplandor; el curvo usa `SDL_RenderGeometry`).
- Colores: Original, Tele antigua, Vivos, Nocturna, Sepia, Pantalla verde. Solo cambia el RGB de los 64 colores.
- Traje del Dr. Social: 8 colores, más Camaleón, que va cambiando solo.

**Juego**
- Dificultad Fácil, Normal o Difícil (escala el daño recibido y causado).
- Reaparición de enemigos: Sí (original), No, o Al volver a la zona. Solo cuentan las muertes reales; "No" dura
  toda la ronda. v0.9.8: el enemigo se reconoce por su sitio en el mapa, así que tampoco vuelve al dar marcha atrás
  (antes salía por la lista de aparición del otro sentido: era el fallo "a veces sí, a veces no").
- Doble salto.
- Ralentizaciones eliminadas u originales (CPU al doble durante la partida).
- "Versión remasterizada (recomendada)": pide confirmación. Dificultad normal, reaparición sí, sin doble salto,
  sin ralentizaciones, sin trucos, 16:9 ajustado, scanlines, colores y traje originales, sin margen, música
  original y táctiles solo sin mando. Es también lo que trae una instalación nueva.
- "Volver al juego original": pide confirmación. Apaga los trucos y deja reglas, traje y música originales;
  no toca imagen, controles, volumen ni idiomas.

**Trucos**
- Invencible (el oficial del juego, `DFB4`) y vidas infinitas.
- Munición infinita.
- Subarmas sin límite: hasta 8 en pantalla en vez de 2. Usa 6 huecos extra en RAM libre `DC00`, movidos por
  el código original del juego.
- Bloquear armas: recoger objetos no cambia el arma ni la subarma.
- Elegir arma y subarma sin salir del menú.
- Empezar en cualquier ronda.

**Controles**
- Saltar y subarma configurables "pulsando el botón" (A, B, X, Y, y también C y Z de mandos de 6 botones como
  Retroid Pocket Classic o Arc).
- L2 rebobina (hasta 20 s) y R2 acelera.
- SELECT o ATRÁS abren el menú.
- START en las pantallas de título, intro, historia y continuar empieza la partida (hace de botón 1); en la
  partida es la pausa. La pantalla se reconoce por la música que pide el juego.
- Controles táctiles: modo No, Sí o Solo sin mando; tres tamaños.
- La cruceta táctil ilumina solo la dirección pulsada.
- Botón táctil de subarma con el dibujo del arma que se lleva (`b3_*.png`); se ve apagado sin munición.
- **Vibración** (Controles › Vibración), con fuerza No/Suave/Media/Fuerte para cada cosa: controles táctiles (y
  toques en menús), al atacar, al acertar (más al destruir), al recibir golpes (más al perder una vida), al caer al
  suelo y jefes (aparece, le aciertas, lo derrotas). Si el mando vibra, vibra el mando; si no, el aparato.

**Sonido**
- Volumen.
- **Texto del juego: inglés o español con nuestra propia traducción**, incluida en la app (sin parches externos).
  La primera vez sigue el idioma de los menús. Con tildes, Ñ, ¿ y ¡ en introducción, historia y final; en los
  diálogos de jefes, marcador y rótulos van sin tildes (ahí la consola no tiene sitio para letras nuevas).
- Packs de música opcionales: `.zip` de `.ogg` nombrados por número de pista, con intro opcional. Tres ranuras:
  «Música: Original / OST 1 / OST 2 / OST 3» (v0.9.7).
- Prueba de pistas.

**Guardar**
- Guardar y cargar estado (1 ranura).

Probado por el usuario y compañeros en Odin 3 Max, Retroid Pocket Classic y móvil. Confirmado en consola
(v0.9.6): C y Z, filtros, START en el título, y sin parpadeos con subarmas sin límite.

---

## Errores y puntos pendientes de comprobar

- **v0.9.8 sin probar en consola:** reaparición por posición, vibración (en especial la fuerza en móviles sin
  control de intensidad, que solo notan la duración), versión remasterizada y traducción.
- **Traducción:** comprobada en el emulador en intro, ouija, historia tras la ronda 0, rótulos, marcador y final.
  Las páginas entre rondas y los diálogos de jefes no se han podido ver (el bot no llega); conviene leerlas en
  consola. El texto se corrige en `tools/traduccion/es.txt` y se monta con `construir.py`.
- **Packs de música:** bucles hechos por análisis; si una unión chirría, decir pista y OST y se busca otro punto.
- **START en el título** depende de la última música pedida. Justo después de cargar un estado puede tardar
  hasta la siguiente pista en reconocer la pantalla.
- **Pequeña diferencia de sincronía:** durante 42 fotogramas, la huella de la emulación difiere de la
  grabada antes del gancho de música (unos pocos ciclos, la imagen es idéntica y luego vuelve a coincidir).
  Ya estaba en v0.8.2; no tiene efecto visible.
- **4-2:** los enemigos reaparecen tras coger la gema. Es comportamiento del juego original (comprobado en 4:3).
- **GitHub:** queda la etiqueta `build-27` sin versión. Es de la v0.9.2 descartada, no molesta y se puede
  borrar a mano. Desde esta sesión GitHub no deja borrar versiones, etiquetas ni ramas.

---

## Decisiones importantes y por qué

- **100 % legal.** El APK no lleva la ROM ni música con derechos. La traducción es texto nuestro (no la de Wave,
  que se dejó de usar para no depender de permisos); las letras con tilde se dibujan en el momento a partir de la
  fuente del juego, sin gráficos sacados de la ROM.
- **El código recompilado no se sube.** La traducción Z80→C (`tools/recomp`, carpeta `generated/` ignorada)
  es solo una herramienta de análisis, porque es una traducción del programa del juego. Las mejoras son código
  propio enganchado en direcciones concretas, y cada enganche se verifica por firma de bytes.
- **16:9 reconstruido del mapa, sin estirar.** Los laterales se dibujan desde el mapa de la sección en RAM.
  Las salas de una pantalla tienen bordes ajustados a mano (`edge_rules`), con muros sólidos y sin oscurecer
  los laterales, por preferencia del usuario. Se pulió en tres revisiones completas del usuario.
- **Nada táctil abre el menú por accidente.** Solo el botón START táctil lo abre, a petición del usuario.
- **Los packs de música son externos y opcionales.** Música con derechos de terceros no va en el APK. El
  gancho del driver de sonido sustituye la pista solo si el pack la trae.
- **Descartados, por decisión del usuario y con la misma conclusión:** mucho trabajo y poca mejora real frente
  a lo que se recuerda del juego.
  - Texturas HD: el encanto es el pixel art.
  - Modo 3D tipo Bigodao: efecto vistoso, pero "de usar una vez".
  - 21:9: implicaba rehacer todos los bordes ajustados del 16:9.
  - Dos jugadores.
- **v0.9.2 revertida.** El problema de mandos de la Retroid era de la consola (se arregló reiniciándola).
  Desactivar HIDAPI y leer joysticks genéricos no hacía falta, y el genérico además cogía el acelerómetro.
- **C y Z como un botón más, sin opciones aparte.** El usuario lo prefirió más limpio que una lista de acciones.
- **"Volver al juego original" no toca imagen ni controles.** Son ajustes de cada aparato.
- **Fondo del launcher:** en pantallas más anchas solo se recorta por abajo, para que el logo quede entero.

---

## Cómo compilar y probar

**Android (lo que se publica).** Cada `push` a `main` lanza `.github/workflows/android.yml`:
- Clona SDL `release-2.30.12` en `android/app/jni/SDL` y compila con Gradle.
- Firma y publica el APK en Releases como `build-N` (nombre `MasterOfDarkness-<versión>-bN.apk`).
- La versión sale de `MOD_VERSION` en el workflow y debe coincidir con `APP_VERSION` en `src/app/main.c`.
- Para subir solo documentación sin generar versión, poner `[skip ci]` en el mensaje del commit.

**Escritorio (para probar).** Necesita SDL2 (CMake + Ninja):
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/masterofdarkness mod.sms
```

**Modo de prueba automática del ejecutable de escritorio** (sin ventana: `SDL_VIDEODRIVER=offscreen
SDL_AUDIODRIVER=dummy`). Saca una captura tras N fotogramas:
- `MOD_AUTOTEST="frames:salida.bmp"`
- `MOD_INPUT=entrada.txt`: líneas `desde hasta botones`, con botones `U D L R 1 2`.
- `MOD_LAUNCHER=1`: pantalla de inicio.
- `MOD_MENU=fila` y `MOD_PAGE=página`: menú abierto.
- `MOD_CREDITS=1`.
- `MOD_LANG=es|en`.
- `MOD_WINSIZE=AxB`: forma de pantalla.
- `MOD_TOUCHDIR=r|d`: un dedo en la cruceta.
- `MOD_SUB` y `MOD_AMMO`: subarma y munición.
- `MOD_ASSETS=android/app/src/main/assets/launcher`: el arte.
- `XDG_DATA_HOME=...`: carpeta de configuración (`config.ini`).

**Emulador sin ventana** (`build/headless`), para análisis y regresiones:
```sh
headless mod.sms -f FOTOGRAMAS [-i entrada.txt] [-E] [-x ext] [-s f1,f2 -o dir] [-H huellas.txt] [-r semilla] [-p desde-hasta:ADDR=VAL]
```
- `-E`: mejoras activadas.
- `-H`: huella de imagen, RAM y audio por fotograma (para comparar versiones).
- `-r`: bot aleatorio desde el fotograma 1300.
- `-p`: escribe en RAM (por ejemplo `-p 0-9000:DFB4=FF` para invencible).
- Variables de entorno: `OBJLOG`, `WATCH`, `TRACE`, `RAMDUMP`, `RAMMAP`, `MUSHOOK`, `PSGMUTE`, `VDPREG`,
  `HERO`, `DJ`, `DIFF`, `RESPAWN`, `WPN`, `MULTI`, `MSLOG`.

**Comprobación de regresión.** Comparar `-H` con la versión anterior en la misma entrada: con las opciones
apagadas, la huella no debe cambiar.

---

## Próximas tareas

1. **Guion de narración: hecho** (`docs/GUION_NARRACION.md`, códigos N01–N36), con nuestra traducción en la
   columna de español: ya no hace falta pedir permiso a nadie.
2. **Narración en la app.**
   - Reconocer cada página de texto en pantalla, en los dos idiomas.
   - Reproducir su voz desde un pack opcional y bajar la música mientras habla.
   - Si una página se cierra sola antes de acabar la voz, mirar si se puede hacer esperar al juego.
3. **Packs de música del usuario: montados** (fuera del repositorio, los tiene el usuario): `MoD_OST_1.zip`,
   `MoD_OST_2.zip` (SUNO, con cortas) y `MoD_OST_YT.zip` (de YouTube, sin cortas: suenan las originales).
   - Proceso: mono, filtro de 30 Hz, -17 LUFS, techo -1,5 dBFS, bucle buscado por similitud (rasgos de espectro y
     acordes), ajustado a pulso y a la forma de onda, con fundido cruzado de un pulso; la parte anterior va como
     `_intro`. Cortas recortadas a la duración original con fundido. 87 recortada a la escena (unos 2 min).
   - OST 2: la 89 (Ronda 2) se cambió el 2-11-2026 por la buena (antes era la del OST 1); intro 18 s, bucle 56 s.
   - Pendiente: que el usuario pruebe los bucles y diga si alguno se nota.
4. **Selector de OST en la app: hecho** (v0.9.7).
5. **Pendiente de confirmar por el usuario en consola:** reaparición, vibración, traducción y el OST 2 nuevo.
