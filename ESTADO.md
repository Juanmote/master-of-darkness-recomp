# Estado del proyecto — Master of Darkness Re-Master-Ed

Última actualización: 4 de noviembre de 2026. Versión publicada: **v0.13.1**.

Launcher para Android (y escritorio) que ejecuta *Master of Darkness* (Master System) desde la ROM del
usuario, con 16:9 real, trucos, opciones de juego, filtros, paletas, pack de música y controles táctiles.
Detalles técnicos de cada versión: [`docs/NOTAS_TECNICAS.md`](docs/NOTAS_TECNICAS.md). Pack de música:
[`docs/PACK_MUSICA.md`](docs/PACK_MUSICA.md). Guion de narración: [`docs/GUION_NARRACION.md`](docs/GUION_NARRACION.md).

ROM necesaria: *Master of Darkness (Europe, Brazil) (En).sms*, CRC32 `96FB4D4B`. Con otra ROM el juego
arranca, pero las mejoras se desactivan solas (se comprueban los bytes de cada punto enganchado).

---

## Lo que funciona

**Pantalla de inicio y app**
- Pantalla de inicio con 7 secciones y Jugar en el medio: Opciones, Controles, Galería, Jugar, Manual, Créditos, Salir.
  Los botones de Galería y Manual imitan el arte del usuario (placa de Créditos, fuente Almendra); se cambian sustituyendo
  `galeria.png`, `manual.png` y `en_*.png`.
- **Galería y Manual (v0.11.0):** packs opcionales `.zip` de `.jpg`/`.png`, como los de música: la primera vez se elige el archivo
  y se copia dentro de la app. El nombre del archivo, sin el número inicial, es el pie. Mando: izquierda/derecha o L1/R1 pasan,
  A hace zoom, cruceta y stick mueven la imagen con zoom, X cambia de pack, B cierra. Los botones y el pie van en el borde de abajo de la
  pantalla y se ocultan a los 2 s (v0.12.0). Táctil: deslizar o tocar los bordes pasa,
  doble toque o pellizco hacen zoom, arrastrar mueve. Packs del usuario, fuera del repositorio: `MoD_Galeria.zip` (16 imágenes) y
  `MoD_Manual.zip` (portada y páginas 2-27). Con material de Sega: no van en el APK.
- **Huevo de pascua (v0.11.0):** en Créditos, 3 pulsaciones sobre el nombre (toque, o A/Intro con mando) enseñan el ticket
  de compra (`launcher/ticket.jpg`, sí va en el APK: es del usuario y no tiene datos personales). Ahora Créditos se cierra con B o tocando en otro sitio.
- Salir cierra la app del todo: `GameActivity` hace `finishAndRemoveTask()` y `System.exit(0)`.
- Icono adaptable con el logo a todo el ancho, sobre fondo oscuro (`tools/gen_icon.py`). Antes era un icono cuadrado
  antiguo: Android lo encogía y le ponía un plato blanco.
- Interfaz en inglés por defecto y en español si el sistema está en español; se cambia en *Sonido e idioma*.
  Los botones del launcher tienen versión en inglés (`assets/launcher/en_*.png`).
- La primera vez se elige la ROM con el selector de archivos. Los packs de música se eligen igual y se copian dentro
  de la app.
- Instalación nueva = **versión remasterizada** (ver Juego).

**Imagen**
- Formatos 16:9 panorámico real (43 px de escenario extra por lado, reconstruidos del mapa del juego en RAM),
  4:3 y 8:7 (tele de tubo). Al elegir 8:7 se pone Ajustar (sin deformar) y "Bandas vacías: Ambiente" (v0.12.1): las bandas
  que quedan en pantallas casi cuadradas toman los colores de la escena, difuminados y oscuros. Llenarla sin deformar no es posible (encima
  y debajo del escenario no hay mapa). Bandas vacías: Negras o Ambiente, en cualquier formato.
- En 4:3 y 8:7 se rellena la columna de 8 px que el juego tapa a la izquierda. Fuera de la partida (v0.12.0) se rellena si el fondo
  junto a ella es de un color liso (pantalla roja del final).
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
  sin ralentizaciones, 16:9 ajustado, scanlines, colores y traje originales, sin margen, música original, táctiles
  solo sin mando, **subarmas sin límite** (único truco activo) y vibración con los valores recomendados
  (Suave, Suave, Media, Fuerte, Suave, Fuerte). Es también lo que trae una instalación nueva.
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

**Modos y logros (v0.10.0)**
- "Jugar" abre una lista: Partida normal, Modo logros, Modo Survivor, Boss Rush, Logros, Exportar perfil e Importar perfil. Cambiar de modo
  empieza una partida nueva; volver al mismo modo la continúa.
- Modo logros: permite guardar y cargar estado, con ranura propia (decisión del usuario: en consola es así, facilita las pruebas y
  habrá logro de Survivor por no usarlos). Una partida guardada en modo normal no se carga aquí. Bloquea rebobinar, acelerar, trucos, doble salto, dificultad y reaparición "No" (también
  "Subarmas sin límite" e invencibilidad del menú oculto del juego). 16:9, sin ralentizaciones, imagen, música y
  OST son libres. Las partidas que no empiezan en la primera fase no cuentan.
- 36 logros propios (`src/app/logros.c`), ordenados por tipo (v0.12.0): historia (rondas 1-4, terminar), partida (sin continuar, sin perder
  vida), fases y rondas (sin daño, sin subarma), jefes (sin daño, cuchillo, hacha), colección (objeto en la pared, 4 armas, 4 subarmas,
  50 máscaras, 100 y 1000 enemigos, Miniyo por una vida extra; armas, subarmas, máscaras y enemigos se acumulan entre partidas), Survivor
  (rondas 1-4, terminar en Fácil, Normal y Difícil, cada uno da también los más fáciles, y terminar sin repetir fase) y Boss Rush (jefes 1-4 y
  terminar en Fácil, Normal y Difícil).
- **Boss Rush (v0.13.0):** los cinco jefes seguidos contra el reloj, con una vida y sin guardar ni cargar. Se elige Vida (tras cada jefe: Fácil toda la
  energía, Normal media barra, Difícil nada) y Armas (Fácil estoque o hacha y bombas o estacas; Normal espada o estoque y pistola o bumerán;
  Difícil cuchillo y pistola o bumerán); la dificultad de la partida sale de las dos. **Jefes desbloqueados (v0.13.1):** cada jefe se desbloquea la primera vez que el
  jugador entra en su sala en cualquier otra partida (vale partida normal con "Empezar en cualquier ronda" e invencible): ese momento se guarda en
  el aparato (`jefe_N.sav`) y el Boss Rush lo carga. Hacen falta los cinco.
  Récord por dificultad, guardado en el perfil. Pantalla de resultado con tiempo y récord.
- **Modo Survivor (v0.12.0):** todo el juego con una sola vida (marcador V=00; las vidas extra no suman). Mismos bloqueos que el modo logros y
  los logros cuentan. Se elige dificultad: Fácil, la energía se recupera como siempre; Normal, solo al empezar la ronda siguiente a cada jefe;
  Difícil, nunca. Donde no cura, la poción no hace nada (solo suena) y cada fase empieza con la energía que se tenía. Sin guardar ni cargar a mano (v0.12.1):
  se guarda solo al empezar cada fase. Al perder la vida sale "Has perdido tu única vida" con la ronda alcanzada: A repetir fase, X reiniciar,
  B pantalla de inicio. Repetir fase quita el logro "Sin red".
- **TEMPORAL (quitar antes de la versión final):** "Invencible (pruebas)" en el menú del modo logros y de Survivor, para que el usuario pueda
  comprobar los logros difíciles. R2 acelera y los logros van a un perfil de pruebas aparte (`logros_pruebas.txt`); al apagarlo vuelve el suyo.
- Perfil `logros.txt` con comprobación: exportar a un archivo; importar suma lo que falte, nunca quita nada.

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

- **v0.13.1 sin probar en consola:** Boss Rush con jefes desbloqueados (desbloquear los cinco en una partida y jugarlo), Miniyo.
- **v0.13.0 descartada:** el atajo sacaba al jefe en el pasillo (2-3) y la ronda no terminaba.
- **Información del usuario (4-11-2026):** con el invencible de pruebas, al caer a un precipicio o al agua se sale por arriba. Survivor completo con el
  truco y todos los logros salvo 1000 enemigos. Decía que al cambiar el traje se perdía el invencible: no se encontró relación; ahora el invencible
  de pruebas se aplica en todos los fotogramas.
- **v0.12.1 sin probar en consola:** bandas Ambiente en 8:7, guardado de Survivor al empezar fase y repetir fase, modo pruebas.
- **v0.12.0 sin probar en consola:** Survivor (sobre todo Normal y Difícil en las fases y con pociones), letras sueltas del jefe 4-3 (en el
  emulador ya no salen, y los acentos siguen en intro, historia y final), franja del final (no se llegó a esa pantalla en el emulador: el arreglo
  vale para cualquier fondo liso), visor con botones abajo, 8:7 con Estirar.
- **Boss Rush (decisión del usuario):** sin guardar ni cargar. Logros como Survivor: uno por jefe y tres por dificultad.
- **Survivor, por decidir con el usuario:** qué hacer con las pociones que no curan (ahora no hacen nada; podrían dar puntos).
- **Logros (4-11-2026):** el usuario ha terminado el juego en la Classic y tiene todos menos 1000 enemigos (el del bloque oculto lo sacó enseguida).
- **Logros:** falta probar exportar e importar en un aparato.
- **Sin probar en consola (v0.11.0):** Galería, Manual, ticket y los 4 logros nuevos. La pared secreta se probó en el emulador simulando el golpe
  (`C858`..`C85E` con `-p`): sale la poción y salta el logro. Con un bot normal no se llega a golpearla.
- **Confirmado por el usuario (4-11-2026):** traducción perfecta, vibración bien (en la Classic va más suave, se sube al máximo), icono y ajustes
  remasterizados bien, bucles de los OST bien.
- **Confirmado por el usuario (2-11-2026):** OST 2 pista 89, y los enemigos mayores ya no reaparecen. Los menores
  (murciélagos, perros) sí, y le gusta así: no tocar. Traducción revisada y correcta salvo lo de los acentos.
- **Traducción:** el texto se corrige en `tools/traduccion/es.txt` y se monta con `construir.py`.
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

- **Logros para disfrutar:** el usuario quiere logros fáciles además de los difíciles; no le gustan los sets puñeteros.
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
- `MOD_VIEWER=1|2|3` (galería, manual, ticket), `MOD_VIEWER_IDX`, `MOD_VIEWER_ZOOM`.
- `MOD_EVENTS`: pulsaciones de prueba (`l r u d a b`, mando `A B L R X`, toque `t0.5,0.3;`); `MOD_EVENTS_LOG=1` imprime cómo queda todo.
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
   - Bucles confirmados por el usuario.
4. **Selector de OST en la app: hecho** (v0.9.7).
5. **Acentos que se adelantaban al texto: arreglado** (v0.9.10) y confirmado.
6. **Modos extra (diseño acordado con el usuario, con las ventajas bloqueadas):**
   - **Logros: iconos propios pendientes.** Las ideas pueden salir de RetroAchievements; sus insignias no (derivan de la ROM y
     sus normas no aceptan reutilizarlas). Añadir logros a los modos extra.
   - **Survivor: hecho en v0.12.0** (ver Modos y logros).
   - **Boss Rush: hecho en v0.13.0.** Diseño original: todos los jefes seguidos, con cronómetro y récord por dificultad. Vida: Fácil recupera todo tras cada
     jefe, Normal media barra, Difícil nada. Armas: Fácil elige estoque o hacha y bombas o estacas; Normal espada o estoque y
     pistola o bumerán; Difícil cuchillo y pistola o bumerán. La subarma se recarga tras cada jefe con una carga normal
     (16 pistola y bumerán, 8 bombas y estacas). Dificultad: Fácil 0, Normal 1, Difícil 2 en cada eje; suma 0 = Fácil,
     1-2 = Normal, 3-4 = Difícil. Los jefes no dan objetos. Probar antes si se puede saltar de jefe en jefe; no usar estados
     guardados en el APK (llevan datos de la ROM). Oleadas descartadas.
   - **Narración de voz aparcada:** el usuario teme desvirtuar el juego y no encaja con la OST. Si acaso, algún extra en la galería.
7. **Subir y bajar de las escaleras al saltar** (salto + arriba para agarrarse, salto + abajo para soltarse): petición del usuario, para más
   adelante. Debe ser opcional o quedar claro cómo afecta a logros y modos.
8. **Hecho en v0.11.0:** descripciones del menú "Jugar", 4 logros nuevos, pantalla de inicio con Galería y Manual, huevo de pascua.
9. **Hecho en v0.12.0:** letras sueltas en diálogos de jefe, franja del final, visor, 8:7 con Estirar, logros ordenados, modo Survivor.
   Siguiente: probar Survivor en consola, luego iconos de logros, Boss Rush (y sus logros) y escaleras.
11. **Hecho en v0.13.1:** Boss Rush con jefes desbloqueados y sus logros, logro Miniyo. Siguiente: probar Boss Rush en consola, iconos de logros, escaleras.
12. **Plan del usuario para el final (4-11-2026):** la versión pública se queda con los jefes desbloqueables. Al terminar el proyecto, una
    **versión completa privada** solo para su uso y recuerdo, en un repositorio privado de GitHub: APK con todo dentro (su ROM, packs de música,
    galería, manual y las salas de los jefes ya desbloqueadas). Ahí también, si se quiere, que exportar/importar perfil lleve las salas de los
    jefes. Nada de eso va en el repositorio público.
10. **Antes de la versión final:** quitar "Invencible (pruebas)" (`test_inv` en `main.c`).
