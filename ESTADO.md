# Estado del proyecto — Master of Darkness Re-Master-Ed

Última actualización: 11 de noviembre de 2026. Versión publicada: **v1.0.5** (en `main`; lanzamiento público el 12-11-2026).

Launcher para Android (y escritorio) que ejecuta *Master of Darkness* (Master System) desde la ROM del
usuario, con 16:9 real, trucos, opciones de juego, filtros, paletas, pack de música y controles táctiles.
Detalles técnicos de cada versión: [`docs/NOTAS_TECNICAS.md`](docs/NOTAS_TECNICAS.md). Pack de música:
[`docs/PACK_MUSICA.md`](docs/PACK_MUSICA.md). Guion de narración: [`docs/GUION_NARRACION.md`](docs/GUION_NARRACION.md).

## Para seguir en un chat nuevo (11-11-2026)

- **1.0.5: el filtro también en las bandas** (detalles en `docs/NOTAS_TECNICAS.md`, 1.0.5). Con Scanlines / CRT / CRT curvo y colores que no
  son negro puro, las bandas vacías llevan el mismo filtro que la imagen (antes salían lisas y se notaba el corte). **Falta:** que lo pruebe.

- **1.0.4: bandas y clave de firma** (detalles en `docs/NOTAS_TECNICAS.md`, 1.0.4).
  - Game Gear empieza con bandas negras (antes forzaba "Ambiente"); las bandas negras y la línea del marcador toman el negro de los colores
    elegidos (Sepia, Pantalla verde...), también en la portada.
  - Clave de firma nueva fuera del repositorio, en los Secrets `MOD_KEYSTORE_B64` y `MOD_KEYSTORE_PASSWORD` (los pone el usuario a mano). Sin
    ellos el flujo no publica. Quien tenga una versión anterior desinstala una vez (exportando antes el perfil).
  - **Edición completa:** pasada también a la clave nueva (Completa 1.0.5, build 7, con los mismos dos Secrets en su repositorio; huella
    comprobada en la APK publicada). También hay que desinstalar una vez la Completa anterior.

- **1.0.3: auditoría y optimización antes del lanzamiento** (detalles en `docs/NOTAS_TECNICAS.md`, 1.0.3). El juego se comporta igual
  (comprobado fotograma a fotograma contra la 1.0.2); cambia el trabajo que cuesta cada fotograma:
  - Núcleo: el fondo de cada línea se dibuja por celdas de 8 píxeles en vez de píxel a píxel (emular un fotograma cuesta la mitad).
  - 16:9: el centro de cada línea se copia de golpe; la paleta solo se recalcula cuando cambia. 4:3 y Game Gear también ganan.
  - App: la capa de menús (640×360) ya no se limpia, se sube a la GPU ni se mezcla cuando está vacía (casi siempre al jugar).
  - Si la pantalla no frena el bucle (sincronía vertical apagada), ya no gira en vacío.
  - Fallo encontrado y corregido: lectura fuera de tabla en el gancho del driver de sonido (`ms_hook_map[0x811C]`); funcionaba por suerte.
  - Estados guardados dañados: el chip de sonido ya no puede leer fuera de sus tablas.
  - README nuevo para el lanzamiento; texto de la Release pensado para el público.
  - `version-1.0` pasada a `main` (publica el APK 1.0.3) y copiada a la Completa.
  La clave de firma que estaba en el repositorio se cambió en la 1.0.4.


- **1.0.2:** el usuario confirma petardeo y aviso de logros perfectos. Game Gear: no le gusta que la ventana baje; prefiere que el héroe no se vea
  al pasar el borde de arriba. Quitado `gg_y`: el escenario empieza bajo la línea y lo que sube más allá no se ve. La "imagen duplicada" era el
  sprite del héroe que se copiaba con el marcador (está encima de la barra de la imagen original): `gg_hud_part` pone el fondo (`fb_bg` con
  `line_cram`) donde hay sprite. La Completa también se llama Re-Master-ed of Darkness. **Falta:** que lo pruebe.

- **1.0.1 (notas del usuario, 9-11-2026), rama `version-1.0`:**
  1. La app instalada se llama **Re-Master-ed of Darkness** (`strings.xml`; también la ventana de escritorio). En la 1.0.2 también la Completa.
  2. Game Gear: ventana que bajaba con el héroe en saltos altos (descartado en la 1.0.2).
  3. Logros: aviso tipo PlayStation arriba a la izquierda (icono con marco dorado, "Logro desbloqueado" y el nombre; entra y sale deslizando, 4 s).
  4. "Petardeo" al abrir/cerrar el menú, empezar o salir: la salida de la consola está muy por debajo de 0 (silencio ≈ -16000) y cada corte
     saltaba a 0. Ahora filtro de continua en el sonido del juego, rampa de 10 ms a silencio en cada cambio (menú, launcher, rebobinar,
     acelerar, salir) y fundido de entrada de 10 ms. Ya no se vacía la cola de audio de golpe.
  **Falta:** que lo pruebe en consola.

- **Ideas de los testers (7-11-2026): hechas** en la rama `version-1.0` (sin pasar a `main`), y llevadas a la edición completa para probarlas:
  1. Música del menú: pieza original sintetizada en la app (Sonido › Música del menú, Sí por defecto).
  2. Prueba de pistas también en el launcher (Opciones › Sonido e idioma › Probar música): suena ahí y se para al salir del menú.
  3. Cambiar de OST y volver a Original: se pide otra vez la pista de la fase (también si se cambia desde el launcher).
  4. Controles: "Definir botones" (atacar, saltar, subarma seguidos) y "Atacar" (un botón, o todos los libres como antes).
  5. Doble salto arriba del todo: el segundo salto sube menos si no cabe (o no hay), y ya no se muere por salir por arriba.
  6. Juego › Pausa al coger objetos: Sí (original) / No. También quita la espera de la poción.
  Detalles en `docs/NOTAS_TECNICAS.md` (1.0.0). **Falta:** que el usuario lo pruebe en consola (en la Completa).
- **Edición completa privada:** build 2 en `Juanmote/master-of-darkness-completa` (con la ROM, sin perfil). Al terminar la 1.0 hay que
  llevar los cambios también allí (copiar el código de `version-1.0` encima, sin tocar `assets/completa`, `applicationId`, `strings.xml`,
  el flujo de compilación ni `.gitignore`).

- **v1.0.0 (rama `version-1.0`, aún no en `main`):** el usuario da el juego por terminado. Plan: probar la edición completa privada y, si va bien,
  pasar esta rama a `main` como 1.0 pública (sin archivos extra) y cerrar el proyecto.
- **Edición completa privada (6-11-2026):** `MoD_Completa-1.0.0.apk`, id `com.juanmote.masterofdarkness.completa` (se instala junto a la pública),
  nombre "MoD Completa", firmada con la misma clave. Lleva en `assets/completa/` su ROM, los tres packs de música (OST 1 → `musica.zip`, OST 2 →
  `musica2.zip`, OST YT → `musica3.zip`), galería, manual y los cinco `jefe_N.sav` (sin perfil: los logros empiezan a cero, solo el
  Boss Rush desbloqueado). Repositorio privado `Juanmote/master-of-darkness-completa`: su `.gitignore` deja pasar solo `assets/completa/rom.sms`
  (en el build 1 faltaba y pedía la ROM); cada push a `main` publica la APK en sus Releases. La primera vez se copian dentro de la app si no están
  (`completa_install` en C; la ROM la copia `RomPickerActivity.romFromAssets` antes de pedirla). La APK pública no lleva esa carpeta.
  Los `jefe_N.sav` se generaron aquí (no son los del usuario): `headless -i entrada_R1-1 -p 1000-1290:DFC0=0R -p 1000-1290:DFC1=02 (00 en la ronda 5)
  -p 1330-1330:C90E=03 -S 1331:raw.sav` (en 1310 la ronda 1 no llega al jefe) + 8 bytes de música (id -1, 0); comprobados cargándolos.
  Se compiló a mano en la sesión (Gradle no puede bajar de Maven Central aquí): NDK r27c + cmake por ABI, aapt2 link con manifiesto con
  `package`, javac `--release 8` + d8, zipalign, apksigner. No se pudo crear el repositorio privado desde la sesión (permiso denegado):
  si el usuario lo crea vacío, se sube ahí la copia con los archivos.
- **v0.16.1 (prueba del usuario en la Classic, Game Gear con jefes):** el diálogo del jefe se pinta sobre el escenario y luego la zona queda en
  blanco un rato, así que el mapa no cuadra y el Game Gear pasaba a la imagen entera y volvía (el "efecto raro" y el cambio de proporción).
  Ahora sigue en Game Gear mientras el marcador del nombre de tabla sea el de la fase; con texto de diálogo en pantalla la ventana se va al
  centro (se lee entero) y luego vuelve deslizándose al héroe. En pelea de jefe (`C90E`=3) la línea lleva la vida del héroe, la del jefe, munición
  y tiempo (sin la palabra VIDA). **v0.16.2:** la pelea se reconoce por la música de jefe (88, 8E): `C90E` no vale 3 en una pelea de verdad
  (solo en la prueba con `-p`), y en la v0.16.1 la barra no salía. Los sprites que suben al marcador (la cabeza al saltar) se pintan sobre la banda negra.
- **v0.16.0 (candidata a final):** arreglados los dos fallos de 3-1 en 4:3/8:7 con los estados del usuario: la regla de borde de la torre del
  campanario tapaba 16 px de cielo en 16:9 pero en 4:3 solo se copiaba la columna oculta de 8 px (salía la columna azul y la muesca negra); y los
  sprites en la columna oculta de la izquierda (el murciélago) los borraba el núcleo: ahora se guardan (`fb_left`) y se pintan encima.
  Quitados del menú "Invencible (pruebas)" (modo logros, Survivor, Boss Rush) y "Jugar › Exportar estado". Prueba de escritorio nueva:
  `MOD_LOADSTATE=archivo.sav` arranca desde un estado guardado.
- **Lo que quedaba:** que pruebe la v0.16.1 en consola (el usuario prefiere corregir lo del Game Gear antes de seguir probando); luego la APK privada (ver punto 12 del plan).
  Lo demás de la v0.16.0 lo ve bien.
- **v0.15.2 (notas del usuario sobre Game Gear):** el héroe va en el medio de la ventana (se recorta de la imagen 16:9, con el motor del 16:9
  encendido); la línea de marcador va encima del escenario y ya no tapa sus 10 primeras líneas (lámparas, etc.); el aviso del formato sale en dos líneas.
- **v0.15.1:** nueva opción **Jugar › Exportar estado**: copia el estado guardado de la partida normal (`estado1.sav`) a un archivo que
  elige el usuario (`MasterOfDarkness_estado.sav`), para que pueda mandarlo. Se carga aquí con `headless -l archivo` o copiándolo como
  `estado1.sav` en la carpeta de configuración del ejecutable de escritorio.
- **Dónde estamos:** v0.15.0 publicada. El usuario probó la v0.14.0 en la Classic: logros todos bien (también el jefe 2), escaleras perfectas.
- **Qué tiene que probar:** Game Gear nuevo (ventana con toda la altura, sin temblor, línea de marcador), iconos de logros, "La cagaste",
  Opciones con gárgolas, el menú oculto bloqueado en modo logros y Survivor, escaleras ahora en Juego.
- **APK privada:** su perfil está guardado en el proyecto de claude.ai (`claude/perfil_usuario_apk_privada.md`). El Boss Rush no depende del perfil:
  hacen falta sus cinco `jefe_N.sav` (o generarlos de nuevo).
- **Logros:** le quedan Miniyo y 1000 enemigos. El de "Suena a hueco" salta con cualquier objeto escondido en una pared, de cualquier fase.
- **Siguiente:** lo que salga de la prueba de la v0.16.0; APK privada.
- **Para trabajar:** clonar `Juanmote/master-of-darkness-recomp`, pedir la ROM al usuario (no está en el repositorio) y compilar como en
  "Cómo compilar y probar". En escritorio hace falta `libsdl2-dev`.

ROM necesaria: *Master of Darkness (Europe, Brazil) (En).sms*, CRC32 `96FB4D4B`. Con otra ROM el juego
arranca, pero las mejoras se desactivan solas (se comprueban los bytes de cada punto enganchado).

---

## Lo que funciona

**Pantalla de inicio y app**
- Pantalla de inicio con 7 secciones y Jugar en el medio: Opciones, Controles, Galería, Jugar, Manual, Créditos, Salir.
  **v0.15.0:** Opciones y Salir (las esquinas) con la placa de gárgolas; Galería y Manual con la de pizarra; Controles y Créditos con la de cobre, con las letras originales del usuario (Galería y Manual siguen con la fuente Almendra). Se cambian
  sustituyendo los `.png` y `en_*.png`.
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
- **Formato Game Gear (8:7, v0.15.2):** una ventana de 194×160 (toda la altura del escenario, el suelo siempre a la vista) que lleva al héroe en el medio,
  solo en horizontal y sin temblor, con una línea de marcador como la de Game Gear (vida, munición, tiempo) encima, sin tapar nada; la pausa enseña el marcador entero.
  Título, historia y diálogos de jefe se ven enteros.
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
- Doble salto (1.0: cerca del techo el segundo salto sube menos o no sale, para no salir por arriba y morir).
- **Pausa al coger objetos** (1.0): Sí (original) o No (el juego sigue y la poción llena de golpe). Vale en todos los modos.
- **Escaleras: Con salto** (Juego, junto al doble salto; v0.14.0): salto + arriba en el aire te agarra a una escalera; salto + abajo en una escalera te suelta sin
  saltar. Remasterizada: Con salto; juego original: Clásicas. Vale en todos los modos (no es una ventaja).
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
- Atacar, saltar y subarma configurables "pulsando el botón" (A, B, X, Y, y también C y Z de mandos de 6 botones como
  Retroid Pocket Classic o Arc). 1.0: "Definir botones" pide los tres seguidos; atacar puede ser un botón o todos los libres.
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
- 37 logros propios (v0.15.0: "La cagaste", primera vida perdida; iconos propios de 11×11 en `src/app/logro_icons.h`) (`src/app/logros.c`), ordenados por tipo (v0.12.0): historia (rondas 1-4, terminar), partida (sin continuar, sin perder
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
- "Invencible (pruebas)" quitado del menú en v0.16.0 (el código de `test_inv` sigue para los bots de prueba, `MOD_LOOPMODE`).
- Perfil `logros.txt` con comprobación: exportar a un archivo; importar suma lo que falte, nunca quita nada.

**Sonido**
- Volumen.
- **Texto del juego: inglés o español con nuestra propia traducción**, incluida en la app (sin parches externos).
  La primera vez sigue el idioma de los menús. Con tildes, Ñ, ¿ y ¡ en introducción, historia y final; en los
  diálogos de jefes, marcador y rótulos van sin tildes (ahí la consola no tiene sitio para letras nuevas).
- Packs de música opcionales: `.zip` de `.ogg` nombrados por número de pista, con intro opcional. Tres ranuras:
  «Música: Original / OST 1 / OST 2 / OST 3» (v0.9.7).
- Prueba de pistas, en la partida y (1.0) también en el launcher.
- **Música del menú** (1.0): original, sintetizada en la app; se quita en Sonido.

**Guardar**
- Guardar y cargar estado (1 ranura).
- Exportar estado (v0.15.1, en Jugar): copia el de la partida normal a un archivo, para mandar fallos.

Probado por el usuario y compañeros en Odin 3 Max, Retroid Pocket Classic y móvil. Confirmado en consola
(v0.9.6): C y Z, filtros, START en el título, y sin parpadeos con subarmas sin límite.

---

## Errores y puntos pendientes de comprobar

- **v0.15.0 sin probar en consola:** todo lo de la v0.15.0 (ver arriba).
- **Probado por el usuario (5-11-2026):** v0.14.0, logros (jefe 2 incluido) y escaleras con salto bien.
- **Fallos de 3-1 en 8:7 (columna azul, murciélago cortado): arreglados en v0.16.0** con los estados del usuario; falta verlos en consola.
- **Antes, v0.14.0 sin probar en consola:** logro y reloj del jefe 2 en el Boss Rush, escaleras con salto, formato Game Gear, botones del inicio.
- **Jefe 2 (5-11-2026):** el usuario vio saltar el logro del jefe 2 antes de vencerlo. Ese jefe vacía su barra al acabar su primera forma; ahora
  solo cuenta la música de ronda superada (también para Intocable, cuchillo y hacha en las partidas normales).
- **Boss Rush (v0.13.1) probado por el usuario (5-11-2026):** jugado entero y todos sus logros.
- **v0.13.1 sin probar en consola:** Miniyo.
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
   - **Logros: iconos propios hechos en v0.15.0.** Las ideas pueden salir de RetroAchievements; sus insignias no (derivan de la ROM y
     sus normas no aceptan reutilizarlas). Añadir logros a los modos extra.
   - **Survivor: hecho en v0.12.0** (ver Modos y logros).
   - **Boss Rush: hecho en v0.13.0.** Diseño original: todos los jefes seguidos, con cronómetro y récord por dificultad. Vida: Fácil recupera todo tras cada
     jefe, Normal media barra, Difícil nada. Armas: Fácil elige estoque o hacha y bombas o estacas; Normal espada o estoque y
     pistola o bumerán; Difícil cuchillo y pistola o bumerán. La subarma se recarga tras cada jefe con una carga normal
     (16 pistola y bumerán, 8 bombas y estacas). Dificultad: Fácil 0, Normal 1, Difícil 2 en cada eje; suma 0 = Fácil,
     1-2 = Normal, 3-4 = Difícil. Los jefes no dan objetos. Probar antes si se puede saltar de jefe en jefe; no usar estados
     guardados en el APK (llevan datos de la ROM). Oleadas descartadas.
   - **Narración de voz aparcada:** el usuario teme desvirtuar el juego y no encaja con la OST. Si acaso, algún extra en la galería.
7. **Subir y bajar de las escaleras al saltar: hecho en v0.14.0** (opción "Escaleras: Con salto" en Controles).
8. **Hecho en v0.11.0:** descripciones del menú "Jugar", 4 logros nuevos, pantalla de inicio con Galería y Manual, huevo de pascua.
9. **Hecho en v0.12.0:** letras sueltas en diálogos de jefe, franja del final, visor, 8:7 con Estirar, logros ordenados, modo Survivor.
   Siguiente: probar Survivor en consola, luego iconos de logros, Boss Rush (y sus logros) y escaleras.
10. **Hecho en v0.16.0:** quitado "Invencible (pruebas)" del menú.
11. **Hecho en v0.13.1:** Boss Rush con jefes desbloqueados y sus logros, logro Miniyo. Siguiente: probar Boss Rush en consola, iconos de logros, escaleras.
12. **Plan del usuario para el final (4-11-2026):** la versión pública se queda con los jefes desbloqueables. Al terminar el proyecto, una
    **versión completa privada** solo para su uso y recuerdo, en un repositorio privado de GitHub: APK con todo dentro (su ROM, packs de música,
    galería, manual y las salas de los jefes ya desbloqueadas). Ahí también, si se quiere, que exportar/importar perfil lleve las salas de los
    jefes. Nada de eso va en el repositorio público.
13. **Hecho en v0.14.0:** jefe 2, escaleras con salto, formato Game Gear, botones del inicio igualados.
14. **Hecho en v0.15.0:** Game Gear afinado, iconos de logros, "La cagaste", menú oculto bloqueado, Opciones con gárgolas.
