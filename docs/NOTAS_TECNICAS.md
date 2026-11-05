# Master of Darkness (SMS) — notas técnicas (fase 1)

ROM: Master of Darkness (Europe, Brazil) (En).sms — 262144 bytes
CRC32 96FB4D4B · SHA1 ed3569be5d5a49ff5a09b2b04ec0101d4edfa81e
Cabecera TMR SEGA válida, checksum 6FE6 correcto, región SMS Export, mapper Sega estándar (16 bancos de 16 KB).

## Herramientas (carpeta tools/)
- `sms.c/.h`: núcleo SMS mínimo (Z80 de superzazu/z80, MIT; VDP modo 4, mapper Sega). Sin sonido.
- `trace.c`: ejecuta con entrada scriptada; capturas (-s / -e), estados (-S frame:fichero, -l), log de VDP/VRAM/bancos (-v frame), log de escrituras RAM (-w C680-C693). Vuelca ram/vram/cram/cobertura.
- `dis.py`: desensamblador por dirección (z80dis).
- `render_map.py`: reconstruye el tramo actual del nivel desde RAM a PNG.
- Compilar: `gcc -O2 -w -o trace trace.c sms.c ../third_party/z80/z80.c`
- Entrada para llegar a R1-1: pulsar botón 1 cada 40 frames desde el 200 (partida jugable ~frame 1300).

## Vídeo / scroll
- Pantalla: HUD en filas 0-3 (y 0-31), zona de juego filas 4-23 (y 32-191, 160 px).
- Interrupción de línea: tabla de valores de registros en C6C0/C6D0 (rutina 2F6A-2FB6); en L30 se fija el hscroll de la zona de juego.
- Reg0=36 (columna izquierda de 8 px en negro, bloqueo hscroll de las 2 primeras filas).
- Cámara X 16 bits en C6A0/C6A1 (píxeles de mundo). Reg8 = -C6A0.
- Solo scroll horizontal en el mapa; reg9 solo lo toca una rutina de objeto en banco 1 (~7276), probablemente efecto/jefe.

## Formato de mapa (clave para 16:9)
- El tramo actual está descomprimido en RAM D000: 10 filas × 192 metatiles (stride 0xC0), = 12 pantallas de 256 px.
  Tabla 50B2: pantalla n -> D000 + n*0x10.
- Metatile 16×16 = 4 bytes [TL, TR, BL, BR] en tabla C6AC (p.ej. 9800) del banco C6A2 (p.ej. 4) en slot 2.
- Tile VRAM = 256 + byte; prioridad si byte >= C6A6 (umbral). Siempre paleta de fondo.
- 2D3F: construye columna de 20 tiles en buffer C680; 2F2B (VBlank) la escribe en VRAM. Sin margen fuera de pantalla.
- 4C5E / 4EB0: colisión con el mapa = (X pantalla + cámara) → índice en D000.

## Objetos / enemigos
- Coordenadas de objetos en pantalla, 8 bits (jugador X en CA24: 0x11..0x88; la cámara le sigue a partir de 0x88).
- Aparición por listas por pantalla: 3C9C (puntero C907, banco C904) y 52B5 (puntero C90C, banco C909).
  Indexadas por C6A1; entradas de 8 bytes [disparo X bajo][puntero flag 2 bytes][5 bytes]; disparan cuando C6A0 == valor exacto.
- Implicación: 16:9 "decorativo" (fondos laterales desde D000) es directo; 16:9 "completo" con enemigos visibles en los laterales
  requiere pasar coordenadas de objetos a 16 bits y adelantar disparos de aparición/desaparición → hacerlo en la versión recompilada.

## Toolchain
- dl.google.com / maven / gradle bloqueados en el entorno → APK vía GitHub Actions. GitHub (git clone) accesible. Java 21 presente.

## Fase 2 — recompilación estática (26-10-2026)
- `tools/recomp/recomp.py ROM cobertura.bin generated/` traduce el código Z80 a C.
  Cada instrucción se genera especializando las plantillas de `src/core/z80_impl.h`
  (las mismas que usa el intérprete) con los operandos leídos de la ROM.
- Regiones: `fixed` (0x0000-0x7FFF, bancos 0/1 fijos) y una por banco con código en
  0x8000-0xBFFF (bancos 2, 3 y 9). Despachador por `switch (pc)`; cualquier dirección
  desconocida cae al intérprete (red de seguridad).
- Descubrimiento: cobertura dinámica (`headless -c`, bot aleatorio `-r`, saltos de ronda con
  `-p 900-1300:dfc0=R -p 900-1300:dfc1=S`) + desensamblado recursivo + tablas de saltos
  `LD HL,tabla ; JP/CALL 0006`. Resultado: 13.894 instrucciones.
- Variables: DFC0 = ronda (0-4), DFC1 = sección.
- Interrupciones: se atienden en el mismo límite de instrucción que el intérprete
  (comprobación tras accesos a puertos, tras la instrucción siguiente a EI y al agotarse
  los ciclos de la línea). La sombra de EI se respeta en ambos caminos.
- Verificación:
  - `headless_recomp -V`: bloque a bloque contra el intérprete (registros, flags, RAM, VDP,
    PSG). ~200 millones de bloques en las 5 rondas: 0 diferencias.
  - `-H`: hash por fotograma (imagen + RAM + audio) de recompilado vs intérprete:
    105.000 fotogramas idénticos (5 rondas × 3 secciones × 7000).
- El código generado (`generated/`) es una traducción del programa del juego: no se sube
  al repositorio ni se incluye en el APK. El APK ejecuta el código del juego desde la ROM
  del usuario y las mejoras propias se enganchan por dirección.

## Fase 3 — enemigos en 16:9 (v0.2.0)
- Objetos: slots de 0x20 bytes desde CA40 (grupo 1: 36 slots hasta CEC0; grupo 2: 10 slots desde CEC0).
  +0 activo, +2/+3 Y (16 bits), +4/+5 X de pantalla (16 bits), +6/+7 puntero a la lista de piezas.
- Pasada de sprites 51B6-5228: elimina objetos del grupo 1 con X < -17 o X > 255 (51D5-51E3 → 5238).
  Constructor de sprites 0DD1: piezas [dy, dx, tile]; solo envía X 0..255; Y SAT = Y16 - 0x11.
- Apariciones: listas por pantalla (3C9C con C905/C907, 52B5 con C90C); se disparan cuando el byte
  bajo de la cámara es igual al de la entrada; el objeto nace con X de pantalla 0..255.
- Mejoras (src/game/mod_enhance.c, código propio enganchado por dirección, solo en 16:9):
  - 51D5: rango de vida ampliado a [-17-43-8, 255+43]; los objetos fuera de 0..255 no se mandan al VDP
    pero sus piezas se capturan para los laterales.
  - 0DD1 / 520E: captura de piezas (observación, sin cambiar nada).
  - Listas de aparición: cámara virtual +43 (3CA6/3CB3/3CD6 y 52BD/52CA/52ED) y X de nacimiento +desplazamiento
    (3CEE / 5305). En cambios de tramo se recorren los 44 valores pendientes. El puntero de la tabla se restaura
    en cada repetición del escaneo.
  - El renderizador elige, de las últimas 4 pasadas, la que coincide con la tabla de sprites que el VDP mostró.
- Se comprueban los bytes de la ROM en todas las direcciones enganchadas: si no coinciden, las mejoras no se activan.
- En 4:3 los ganchos no actúan: 5000 fotogramas idénticos al juego original.

## v0.2.2
- La cámara también retrocede (2C06: cam--, listas C907/C90C que hacen nacer enemigos en x=1).
  C905/C90A = listas al avanzar. La aparición adelantada distingue la dirección por el puntero de la lista:
  al retroceder se comprueba cam-43 y el objeto nace 43 px más a la izquierda.
- Ralentizaciones: `sms_t.cpu_mult = 2` duplica los ciclos de CPU por línea. Solo se aplica mientras
  `mod_is_gameplay()` (la intro y los menús miden el tiempo con bucles de CPU y se acelerarían).
  Con tres murciélagos en R1-1: 252 de 300 fotogramas de lógica en el original, 300 de 300 con la opción.

## v0.2.3
- "Decapitación": con el encuadre panorámico detenido (inicio/final de tramo) la zona de juego se desplaza
  respecto al marcador, y los píxeles de sprite en las líneas del marcador se dibujaban con el desplazamiento
  del marcador. El núcleo guarda ahora fondo (`fb_bg`) y máscara de sprites (`fb_spr`) por línea; en las líneas
  del marcador el fondo va centrado y los sprites siguen a la zona de juego.

## v0.3.0 (revisión completa del usuario)
- Piezas de sprite fuera de los límites del tramo (zona de al lado) ya no se dibujan: eran las
  "máscaras dentro de los muros".
- (v0.3.1) Sin oscurecido: rompía la continuidad. Fuera del tramo se repite, fila a fila de metatiles, el patrón
  que se repite junto al borde (periodo exacto más corto de 16 a 128 px; si no hay, el que más coincide).
- Celdas que difieren del mapa (bloques rotos, pasadizos abiertos, cuadros de diálogo) se recuerdan por tramo
  (`ovr`) y se usan también en los laterales.
- Si el juego muestra algo que el mapa no explica (efectos de jefe, diálogos) sin mover la cámara, se reutilizan
  los laterales del último fotograma bueno (`cache`, clave cámara+ronda+sección).
- Objetos cuyo origen está fuera de 0..255: sus piezas nunca llegan al VDP (`in_sat` falso) → se dibujan
  también en la zona central (barcas cortadas).
- Margen de aparición/eliminación dinámico (`mod_visible_margins`): lo que realmente se ve de zona jugable a
  cada lado (0..86 px) según el encuadre detenido en los extremos del tramo.

## v0.3.3 (segunda revisión completa)
- Colisión de metatiles: tabla en RAM D900 indexada por metatile (bits 0-1: 0 paso libre, 2 muro...).
  El patrón de 32 px en los bordes solo se usa si los dos metatiles son del mismo tipo (4-3: ya no salen
  huecos de cueva dentro del muro).
- Bordes elegidos a mano (`edge_rules` en mod_wide.c, por ronda/sección/columna de borde): 1-3 la hilera de
  casas sigue (periodo 7 metatiles), 3-1 campanario pared izquierda = derecha, 3-3 callejón sin salida = muro de
  detrás, 4-3 jefe pared izquierda = derecha.
- Caché de laterales: guarda índices de paleta (los destellos se aplican también a los laterales) y solo se usa
  si el marcador sigue siendo el mismo (fin del juego / créditos ya no se descentran). Por cada lado: si el borde
  de la imagen no ha cambiado se mantiene lo guardado; si ha cambiado (sala del jefe 5-1 a oscuras) se continúa
  lo que hay ahora en el borde.
- Objetos que han estado dentro de la imagen original (`seen_in` por slot) pueden salir volando por los laterales
  aunque allí no haya zona jugable (murciélagos del campanario): se mantienen vivos y se dibujan 43 px más.
- Apariciones: cada lista guarda la ventana de valores ya comprobados (bordes de lo visible). Al cambiar de
  sentido ya no se vuelve a comprobar todo lo visible: un enemigo eliminado a la vista (gema de 4-2) solo vuelve
  cuando su punto sale de la vista y vuelve a entrar, como en el original.

## v0.3.4
- `edge_rules` admite `flip` (columna en espejo: 3-1, la línea oscura del ladrillo queda junto al hueco) y
  `cover` (se pinta también N columnas dentro de la sala, solo fondo; 4-3 jefe: muro simétrico). 1-3: la sala
  de las casas sigue también por la izquierda.
- Laterales sin mapa (efectos de jefe): solo se continúa lo que se repite cada 16 px en cada banda de 8 líneas
  (suelo, muro, color liso). Drácula (dibujado con tiles de fondo) ya no se duplica: allí va el color de fondo.
- Banco de pruebas de bordes: render de una sala desde volcados de RAM/VRAM con mod_render (fuera del repo).

## v0.3.5
- 3-1 campanario: el muro izquierdo cubre también la columna de cielo del borde de la sala (`cover` 1).
- 4-2 tras la gema: comprobado con vídeo en 4:3 (juego original): la dama y el cuadro reaparecen igual al
  retroceder. Es comportamiento del juego, no se toca.

## v0.4.0 — traducción al español
- Parche IPS de Traducciones Wave (v1.2, 2018, ROM base CRC 96FB4D4B) en `assets/es/` y embebido en
  `src/app/patch_es.h` (`tools/gen_patch.py`). El launcher lo aplica en memoria sobre una copia de la ROM del
  usuario solo si la ROM es la original; opción "Idioma" en el menú (reinicia el juego). Créditos en el pie del menú.
- El parche solo cambia texto (bancos 1, 3, 4 y 15, más la suma de control): no toca ninguna dirección
  enganchada, las mejoras 16:9 siguen activas con el juego en español.
- Botón de saltar: se elige pulsándolo (`jump_btn`, cualquiera de A/B/X/Y; los demás atacan). Sustituye a
  `swap_ab` (se migra al cargar la configuración). El menú acepta con A y vuelve con B.

## v0.5.0
- Parche de traducción elegido por el usuario (Opciones › Parche, selector del sistema como la ROM) y guardado
  como `patch.ips`; ya no va en el APK ni en el repositorio. Se reconoce el de Traducciones Wave por su CRC
  (0x1173EA42) para acreditarlo. Android: `SDL_AndroidSendMessage(0x8001)` → `GameActivity` abre el selector.
- Menú por páginas: principal, Imagen, Opciones, Trucos.
- Trucos (RAM del juego, los mismos que usa su menú de depuración oficial, título: Arriba+1+2):
  DFB4 ≠ 0 = sin daño ("HIT CHECK"), DFBA vidas, DFB7 arma (0 cuchillo, 1 pico, 2 estoque, 3 hacha),
  DFB8 subarma (0 pistola, 1 bumerán, 2 dardos, 3 bombas), DFB9 munición (0 = ninguna), DFC0/DFC1 ronda/escena.
  El marcador se redibuja llamando a las rutinas del propio juego (4A07 arma, 4A10 subarma, 4A2C vidas), metidas
  como llamada en el gancho 51B6 (`mod_request_hud`). Empezar en una ronda: reinicio y DFC0/DFC1 fijados hasta
  que empieza la partida.
- Rebobinar (L2) y acelerar ×4 (R2): instantánea cada 2 fotogramas, 20 s (≈15 MB), con el estado de apariciones.
- Color del traje: el núcleo guarda el tile de sprite de cada píxel (`fb_sprtile`); los tiles 0x01-0x1F son del
  protagonista. Se cambian las entradas 10 (claro) y 11 (oscuro) de la paleta de sprites solo en sus píxeles.

## v0.5.1
- Arma/subarma: se pueden cambiar varias veces seguidas en el menú (el redibujado del marcador se acumula y se
  hace al volver al juego).
- Nombres de botones sin dirección (A/B/X/Y).
- Botón de subarma configurable (`sub_btn`, por defecto Y): envía arriba + ataque, como pide el juego.
  El estado del mando se guarda como máscara de botones pulsados y la entrada se calcula cada fotograma.

## v0.6.0 — página "Juego"
- Nombres: armas Cuchillo, Estoque, Espada, Hacha; subarmas Pistola, Bumerán, Estacas, Bombas.
- Reaparición de enemigos (`respawn`): cada entrada de las listas de aparición nombra el slot que usa
  (3CC9/52E0: DE = slot, se crea si el slot está libre). Se anota qué entrada creó cada slot (3CDD/52F4) y, al
  empezar cada pasada de sprites, los slots que se han vaciado: si no fue la limpieza fuera de pantalla
  (5238/523D) y el objeto estaba a la vista, cuenta como muerto. En 3CD0/52E7 (OR A tras leer el slot) una
  entrada muerta ve el slot "ocupado". "No": hasta cambiar de tramo o perder una vida. "Al volver a la zona":
  hasta que la cámara se aleja más de 342 px del punto de aparición.
- Dificultad: daño al jugador en 46F1 (B), a jefes en 6198 (B), a enemigos en 3DD6 (SUB (IY+1), se cambia el
  byte solo para esa instrucción y se restaura en 3DD9). Mitad con acumulador (un golpe de 1 alterna 0/1).
- Doble salto: estados del protagonista CA31 (0 suelo, 3 subiendo, 2 cayendo), CA2B fotograma del arco,
  C701 botones pulsados en el fotograma. Al pulsar saltar en el aire: estado 3, arco a 0, velocidad 0 e Y
  alineada a 8 (la comprobación de suelo necesita la misma alineación que un salto desde el suelo).

## v0.7.0 — pack de música
- Controlador de sonido en el banco 9 (se ejecuta en la interrupción). C104/C105/C106 son peticiones; 8093 elige
  por prioridad y deja la ganadora en C103; 811C la arranca: 0x81-0x8F música, 0x90-0xBD efectos, C0 fundido,
  C1 parar todo (847E borra C103-C28F), C108 ≠ 0 = pausa.
- Gancho en el banco conmutable (nuevo en el núcleo: `hook2_pc`/`hook2_bank`) en 9:811C: si la pista tiene
  sustituto, C103 pasa a C1 (silencio, como antes de cualquier pista nueva) y la app reproduce el .ogg.
- `src/app/music.c`: zip con miniz, Ogg con stb_vorbis decodificado sobre la marcha, remuestreo lineal a 44100 Hz
  mono, intro + bucle, fundido de 1 s. Formato documentado en docs/PACK_MUSICA.md.
- Pistas que no se repiten (medido con la salida del PSG): 81, 85, 86, 8A.

## v0.8.0 — pantalla de inicio y controles táctiles
- Arte del usuario (fondo y botones) en `android/app/src/main/assets/launcher/`, recortado del PNG original
  (fondo transparente por segmentación). Se carga con stb_image desde los assets del APK.
- Pantalla de inicio: JUGAR (empieza o continúa), OPCIONES, CONTROLES, CRÉDITOS, SALIR; mando, teclado y táctil.
  Menú del juego con "Pantalla de inicio". Página nueva "Controles" (saltar, subarma, gatillos, táctiles).
- Controles táctiles (No / Sí / Solo sin mando, tres tamaños): cruceta de 8 direcciones, 1 ataque, 2 salto,
  START abre el menú, PAUSE pausa el juego. Los menús también responden a toques.
- Corrección del pack de música: sustituir la petición por C1 (parar todo) dejaba al juego esperando en el inicio
  de ronda (la espera de fotograma no terminaba). Ahora la petición pasa a 0x80 (nada) y se apagan los cuatro
  canales de música (C140/C170/C1A0/C1D0) y sus volúmenes en el PSG.

## v0.8.2
- Música original exportada para el usuario (grabación del PSG en una partida quieta con los canales de efectos
  apagados; `psg_mute` en el núcleo para separar los 4 canales). Medido: 87 (introducción, 1:38) y 8D (historia y
  final, 2:20) no se repiten; el pack las toca una vez.

## v0.9.0 — inglés/español, trucos de armas, traje Camaleón
- Idioma de la interfaz (launcher, menús, avisos, pantalla de la ROM): inglés por defecto, español si el sistema
  está en español (`SDL_GetPreferredLocales`; en Java `Locale.getDefault()`). Se cambia en Sonido e idioma →
  "Idioma de los menús" (`ui_lang` en config.ini). Los botones del launcher en inglés son `launcher/en_*.png`
  (opcionales: si faltan se usan los españoles). La opción del parche ahora se llama "Texto del juego".
- Reaparición "No": al cambiar de sección ya no se borra `slot_entry` (los objetos creados antes de la primera
  pasada de sprites perdían su entrada y volvían a salir).
- Trucos nuevos: "Munición infinita" (DFB9 se mantiene en 99 si se lleva subarma) y "Bloquear armas"
  (DFB7/DFB8 vuelven al arma fijada si un objeto las cambia; cambiar el arma en el menú mueve el bloqueo).
  Se hace desde la app cada fotograma y se pide el redibujado del marcador con `mod_request_hud`.
- Traje "Camaleón": recorre 12 colores (entradas 10/11 de la paleta del héroe) cada medio segundo.
- Cruceta táctil: solo se iluminan los brazos pulsados (copia del trozo de la textura con mezcla aditiva).
- "Salir" (launcher y menú de partida) cierra la app del todo: mensaje 0x8003 a `GameActivity`, que hace
  `finishAndRemoveTask()` y `System.exit(0)` en `onDestroy`.
- Icono de la app: el logo del fondo del launcher.

## v0.9.1 — subarmas sin límite
- El juego solo tiene dos huecos para los proyectiles del héroe (CA40 y CA60) y no deja lanzar mientras los dos
  están ocupados. El truco "Subarmas sin límite" añade 6 huecos más (8 en total) en DC00-DCEF, una zona de RAM que
  el juego no usa nunca (comprobado escribiendo un mapa de RAM en las 13 salidas de ronda con un bot, 45 partidas;
  la zona libre es DBA0-DFB2) y sus cajas de golpe en DCC0.
- Los huecos extra los mueve, dibuja y hace chocar el código original: en cada bucle que recorre los proyectiles
  se manda a la CPU una vez más por el cuerpo del bucle con los huecos nuevos (2354/2379 movimiento, 43C7/43D5
  cajas de golpe, 43DC/43E8 golpes a enemigos, 4448/446D paredes, 51B9/51EC pasada de sprites, 50D8/50EE empujes;
  5179/5195 desplazamiento de cámara y 50CA/61D2 borrados se hacen en C). Las paredes se comprueban copiando la
  caja de golpe en C860 un momento, porque ese código tiene la dirección fija.
- Con el truco apagado y sin proyectiles extra los ganchos no tocan nada (huella idéntica a v0.8.2/v0.9.0).
- Comprobado: los proyectiles extra dañan a los enemigos (con IY en DCC0..), chocan con las paredes y se borran solos.

## v0.9.3 — columna izquierda, START en el título, margen de pantalla
- (La v0.9.2 se descartó y se revirtió: el problema de mandos de la Retroid era de la consola.)
- 4:3 y 8:7: el juego tapa la columna de 8 píxeles de la izquierda (registro 0, bit 5, para ocultar el scroll);
  en una pantalla 8:7 (Retroid Pocket Classic) se veía como franja negra. `mod_render_full43` dibuja también la
  imagen con 8 píxeles de lateral y copia solo esa columna en las líneas del escenario: el resto es la imagen
  original sin tocar.
- START (mando, Intro y START táctil) hace de botón 1 en las pantallas de título, introducción, historia y
  continuar (se reconocen por la música pedida: 83, 87, 8D, 82); en la partida sigue siendo la pausa.
- Pantalla de inicio: en pantallas más anchas que el fondo solo se recorta por abajo (el logo queda entero).
- Imagen → "Margen de pantalla" (no / pequeño / mediano): aparta la imagen de los bordes en móviles con esquinas
  redondeadas o cámara en la pantalla. El marcador del juego empieza en la línea 0 de la imagen original.

## v0.9.4 — botones C y Z, botón táctil de subarma
- Mandos de seis botones (Retroid Pocket Classic): Android manda KEYCODE_BUTTON_C/Z y SDL los da como botones 17 y
  18 del joystick, sin botón equivalente en "game controller". Se leen como SDL_JOYBUTTON (solo en Android) y se
  les asigna una acción en Controles: nada, atacar, saltar, subarma, pausa, abrir menú, rebobinar, acelerar,
  guardar estado, cargar estado (por defecto C = subarma, Z = nada). Rebobinar/acelerar desde C/Z funcionan
  aunque la opción de los gatillos esté apagada.
- Tercer botón táctil (subarma, arriba + ataque) encima del 1. Su dibujo, `launcher/b3.png`, es provisional:
  sale del botón 1 con la gema en azul y un puñal dorado en lugar del número; se puede sustituir por otro.

## v0.9.5 — C y Z como botones normales, icono de subarma, volver al juego original
- C y Z (botones 17 y 18 del joystick en Android) se tratan como dos botones frontales más (ids 24 y 25): atacan,
  y se pueden elegir como botón de saltar o de subarma con "pulsa el botón que quieras". Se quitan las opciones
  "Botón C/Z" de la v0.9.4.
- Botón táctil de subarma: muestra el dibujo de la subarma que se lleva (`b3_pistola/bumeran/estacas/bombas.png`,
  arte del usuario), algo más grande que la zona táctil porque el arma sobresale del aro; apagado sin munición.
  `b3.png` queda para ROMs no reconocidas.
- Juego → "Volver al juego original" (pide confirmación pulsando dos veces): apaga los trucos y deja dificultad
  normal, reaparición original, sin doble salto, ralentizaciones originales, traje original y música original.
  Imagen, controles, volumen e idiomas se mantienen.

## v0.9.6 — filtros CRT y paletas
- Filtros "CRT" y "CRT curvo" (por GPU): la imagen se dibuja a 3 columnas x k líneas por píxel en una textura,
  se multiplica por una máscara (perfil del haz en cada línea y franjas de fósforo rojo/verde/azul) y se le suma
  una copia borrosa a media resolución (resplandor, devuelve el brillo). El curvo dibuja el resultado con
  `SDL_RenderGeometry` sobre una rejilla 32x24 abombada, con las esquinas más oscuras.
- Imagen → "Colores": paletas Original, Tele antigua, Vivos, Nocturna, Sepia y Pantalla verde. Solo cambian el
  RGB de cada uno de los 64 colores de la consola (`sms_set_palette` en el núcleo); el dibujo no se toca. Los
  laterales vacíos del 16:9 usan el negro de la paleta.

## v0.9.7 — enemigos muertos de verdad, tres packs de música
- Reaparición rehecha. Antes contaba como "muerto" cualquier objeto que desaparecía a la vista; al morir el
  jugador, continuar o limpiarse la pantalla, todos los enemigos visibles se apuntaban como muertos (y con vidas
  infinitas la lista nunca se vaciaba). Ahora solo cuentan las muertes reales, enganchadas en el código del
  juego que destruye un enemigo: 3DFA (golpe sin energía, suena A0 y da puntos) y 41AE (destruido de un golpe,
  da puntos). En ese momento se apunta la entrada de la lista de aparición que creó ese slot (IX).
- Identificador de entrada: tramo (DFC1) + banco + dirección. La lista dura toda la ronda (DFC0); se vacía al
  cambiar de ronda o al pedir el juego la música del título (83) o de la intro (87), es decir, partida nueva.
  Perder una vida o continuar ya no la vacía. Hasta 256 entradas (se olvida la más antigua).
- "Al volver a la zona": igual, pero la entrada se borra cuando la cámara se aleja más de 342 px.
- Comprobado con el bot (`BOTFLIP=2500` invierte izquierda/derecha cada 2500 fotogramas): con "Sí" vuelven las
  entradas muertas, con "No" ninguna, con "Al volver" sí al alejarse. Con opciones por defecto la huella `-H`
  es idéntica a v0.9.6.
- headless: `KILLLOG=1` (cambios de la lista de muertos) y `BOTFLIP=N`.
- Música: `music_pack` pasa a ser 0 (original) o 1-3 (ranura OST 1-3). Archivos `musica.zip`, `musica2.zip`,
  `musica3.zip`; solo se abre el elegido. En Android el selector recibe la ranura como parámetro del mensaje
  0x8002. Un `music_pack=1` antiguo sigue siendo OST 1.
- Intro de una pista: el nombre debe **terminar** en "intro" (antes bastaba con contenerlo y
  "87 - Introduccion.ogg" se tomaba como intro sin pista principal).
- Pistas identificadas en la tabla de música por ronda (BCE4: 8B 89 8F 8C 84, luego 88 88 88 88 8E): 88 =
  jefes de las rondas 1-4, 8E = jefe de la ronda 5 (también en 73F9).
- Duraciones originales medidas en el emulador: 81 2,6 s; 85 3,05 s; 86 4,8 s; 8A 1,8 s (el juego hace fundido
  a los 2,0 s); 87 suena 97,7 s y la escena dura 131 s. Música original ≈ -21,5 LUFS; el pack se suma con
  ganancia 20000/32768 (-4,3 dB), así que -17 LUFS en el .ogg queda igual de fuerte que la original.

## v0.9.8 — traducción propia, reaparición por posición, vibración, versión remasterizada
- **Reaparición:** cada enemigo está en dos listas de aparición (una para cuando la cámara avanza, C905/C90A, y otra
  para cuando retrocede, C907/C90C) con entradas distintas. Antes se bloqueaba la entrada por su dirección en la
  ROM, así que al volver hacia atrás el enemigo salía por la otra lista. Ahora un muerto se recuerda por tramo
  (DFC1), grupo de listas (3C9C o 52B5), slot, Y y posición en el mapa (cámara de disparo + X de nacimiento), con
  margen de 112 px (las dos entradas de un enemigo lo colocan hasta 98 px aparte en 1-1). Comprobado con
  `FAKEKILL=1` (headless mata lo que entra en pantalla) yendo y viniendo: con "Sí" vuelven 23 de 55, con "No" 0,
  en 16:9 y 4:3 y en las 5 rondas. El rebobinado guarda también la información de los slots; cargar un estado la
  olvida (los objetos en pantalla ya no se conocen).
- **Vibración:** el núcleo apunta momentos (`MOD_EV_*`, `mod_events_take`): ataque (flanco del bit 0x10 de C701),
  golpe a enemigo (3DD6), enemigo destruido (3DFA/41AE), daño (46F1, B > 0), vida perdida (música 8A), aterrizaje
  (CA31 de 2/3 a 0), aparece jefe (música 88/8E), golpe a jefe (6198) y jefe sin energía (6198 con B ≥ CB3A).
  La app vibra el mando si tiene vibración (`SDL_GameControllerRumble`) o el aparato (mensaje 0x8004 a
  `GameActivity`: fuerza << 16 | ms, `VibrationEffect` con `USAGE_GAME`). Los toques en la pantalla vibran siempre el
  aparato. Seis fuerzas (No/Suave/Media/Fuerte) en Controles › Vibración; por fotograma solo se siente el momento más
  fuerte y uno más débil no corta el que está sonando.
- **Versión remasterizada:** Juego › "Versión remasterizada (recomendada)" (pide confirmación) y valores de una
  instalación nueva: dificultad normal, reaparición original, sin doble salto, sin ralentizaciones, sin trucos,
  16:9 ajustado a pantalla, scanlines, colores y traje originales, sin margen, música original y controles táctiles
  solo sin mando.
- **Traducción propia** (`tools/traduccion/`): `segmentos.py` describe dónde está cada texto y su formato (páginas
  `[x][texto](FF x texto)* FF FF`, cuadros de jefe `F1 x ... FF [F4 nn] x ...`, final `([n][texto])* 00`, rótulos de
  largo fijo). `construir.py` monta `es.txt` en el mismo sitio y número de bytes (hay punteros a cada página y a cada
  diálogo), rellena con espacios donde no tapa el dibujo y cubre lo que escribió la página anterior (el juego no
  borra la pantalla entre páginas del mismo capítulo; las cadenas se sacaron viendo qué páginas originales tapan a
  la anterior). Las palabras del tablero ouija no pueden ser más largas que las originales (se deletrean en el
  tiempo de la original). Salida: `src/app/text_es.h` (solo nuestro texto), aplicado en memoria sobre la ROM original.
- **Letras extra:** códigos 0xB0-0xB8 (Á É Í Ó Ú Ñ ¿ ¡ Ü) = tiles 0x1B0-0x1B8, que ninguna pantalla de texto usa
  (comprobado en intro, ouija, historia, rótulos y final). `mod_text.c` las crea cada fotograma desde la fuente del
  juego en VRAM (la letra tal cual; ¿ ¡ son ? y ! girados) y el núcleo las dibuja en su lugar (`sms_t.glyphs`). La
  tilde va en la fila vacía de encima (los renglones van a doble espacio): el núcleo pinta la segunda mitad de la
  tabla en los píxeles vacíos del tile de arriba. Apagado en la partida (`mod_is_gameplay`), donde esos tiles son del
  nivel: los diálogos de jefes, marcador y rótulos van sin tildes (otra fuente o tiles ocupados).
- Ya no se usa el parche IPS del usuario ni se acredita a Traducciones Wave.
- headless: `GLYPHS=1` (letras extra), `EVLOG=1` (momentos de vibración), `FAKEKILL=1`, `RESPDBG=1`, `KILLPC=1`,
  `BOTATK=1|2`, `JUMP=frame:banco:pc` (salta a una rutina; `JUMP=900:3:AC35` enseña el final).
- Con las opciones por defecto del núcleo la huella `-H` es idéntica a v0.9.7 (16:9 y 4:3).

## v0.9.9 — icono adaptable y ajustes remasterizados
- **Icono:** el anterior era un PNG cuadrado sin versión adaptable; Android 8+ lo encoge dentro de un plato blanco.
  Ahora `mipmap-anydpi-v26/ic_launcher.xml` con fondo de color y `ic_launcher_foreground.png` (108 dp, logo de 68 dp
  en los 72 dp visibles). `tools/gen_icon.py` recorta el logo de `fondo.jpg` y quita el cielo pintado alrededor de
  las volutas. Los PNG `ic_launcher.png` quedan para Android 7 o menos. El `banner` pasa a `drawable-xhdpi/banner.png`
  (320x180), porque un banner debe ser una imagen apaisada.
- **Versión remasterizada:** añade `multi_shot = 1` y `cfg.vib = vib_recommended`, también en una instalación nueva.
  Un `config.ini` existente conserva sus valores hasta pulsar la opción.

## v0.9.10 — acentos que se adelantaban al texto
- El texto de las páginas usa la paleta 1 (`0x0941`), que el juego funde a negro y vuelve a encender, y escribe cada
  página mientras esa paleta está a negro. Las tildes se pintaban en el tile vacío de arriba, cuya entrada usa la
  paleta 0, donde el color 15 es blanco fijo: se veían antes del fundido. Ahora la tilde toma la paleta de su letra
  (`src/core/sms.c`). ¿ y ¡ no tenían el fallo: son tiles propios con la paleta correcta.
- Comprobado con la intro en castellano (`GLYPHS=1`, 16:9), fotogramas 2216-2344 y 3120. Con la ROM original y las
  letras extra apagadas, la huella `-H` es idéntica a v0.9.9 (3000 fotogramas con bot, 16:9).
- Confirmado en la ROM que el tablero ouija deletrea KILLER, VAMPIRE, GO TO THAMES, CAUTION, CAUTION, IN THE WAKE OF, DRACULA.

## v0.10.0 — menú de modos, modo logros y perfil
- `game_mode` y `ADVANTAGES_BLOCKED` en `main.c`: un único bloqueo para este modo y los futuros. Con él `apply_game_options` fuerza
  dificultad normal, reaparición sí, sin doble salto ni subarmas sin límite, `cheats_frame` pone `DFB4` a 0 en cada fotograma
  (también tapa el menú oculto del juego) y no corre nada más, y no hay rebobinado, acelerar ni estados. El menú en partida es
  `PAGE_MAIN_LOCKED`.
- Lógica en `src/app/logros.c`, sin SDL, alimentada por `MOD_EV_*`, RAM y música. Partida nueva: música 83/87. Continuar: 82.
  Solo cuenta una partida que empieza en ronda 0, fase 0 y avanza de una en una. Ronda superada: `DFC0` sube.
  Final: `AC35` (banco 3) pide 8D, igual que las páginas de historia, y no hay enganche posible ahí (`hook_map` solo cubre hasta
  7FFF), así que se reconoce como 8D en la última ronda después de vencer a su jefe; esa ronda no tiene páginas de historia.
- Perfil `logros.txt`: texto con comprobación FNV al final; se escribe por archivo temporal y `rename`. Un archivo dañado o
  editado se rechaza. Importar une logros y coge el mayor número de enemigos. Android: mensajes 0x8005/0x8006 a `GameActivity`; lo
  importado queda como `perfil_importado.txt` y se revisa al volver a primer plano.
- Pruebas: `tools/test/test_logros.c` (partidas simuladas); headless `LOGROS=archivo` con `MUSHOOK=1` mete el juego real en los
  logros; `MOD_LOGROS_LIST=página` en el modo de prueba enseña la lista. Con las opciones normales la huella `-H` es idéntica a v0.9.10.

## v0.10.1 — guardar y cargar en el modo logros
- El bloqueo ya no incluye los estados. El modo logros usa `estado_logros.sav` (el normal sigue con `estado1.sav`): así no se puede
  cargar una partida con trucos o empezada en cualquier ronda. El archivo lleva detrás el seguimiento de la partida (`logros_run_*`): al
  cargar vuelven también las muertes y golpes de ese momento, y `state_loaded` queda marcado para el logro de Survivor sin estados.

## v0.10.2 — logros de ronda y música al cargar
- **Logros:** la música 83 no es solo la del título: también suena entre rondas (81 o 86, recuento, 83, 85, música de la ronda). Se tomaba
  por partida nueva, y la partida quedaba como no empezada en la primera fase: ni ronda superada, ni ninguno de los logros de partida
  posteriores. Partida nueva es solo la 87 (arranque, o responder NO en continuar; SÍ no la pide y deja la ronda, y pone fase 0 y armas a 0).
- **Música al cargar:** el estado guarda la pista pedida y si venía de un pack. Al cargar, una pista de pack se vuelve a empezar; la original
  vuelve con el estado. Si la fuente cambió desde que se guardó, se le pide la pista al juego. Los estados anteriores se siguen cargando.
- **Para probar jefes:** `-p F-F:DFC1=02 -p F-F:C90E=03` lleva al jefe de la ronda, con su diálogo (hay que pulsar botón). Estados de `C90E`: 1 fase
  superada, 3 jefe. Tabla de ronda y fase a `B9BE` (fase+1), `B9CB` (ronda+1) y `B9DC` (final).

## v0.10.3 — los logros de ronda saltan al vencer al jefe
- Antes saltaban al empezar la ronda siguiente, tras el recuento y las páginas de historia. Ahora, con la música 86 (ronda superada) en la última
  fase de la ronda. Una ronda son sus 3 fases (`R1-1` a `R1-3`); la 81 es fase superada y no da nada.

## v0.10.4 — logros de fase
- `stageclean` y `stagenosub`: con la música 81 (fase superada) o 86, sin daño o sin gastar subarma desde que empezó la fase. `run` lleva 32 bytes libres
  para añadir campos sin invalidar los estados guardados del modo logros.

## v0.10.5 — subarma y avisos de logros
- Se cuenta como gastada solo si baja una unidad con la misma subarma: coger otra podía bajar el contador y anular "A cuerpo limpio" y "Ahorrando munición".
- Los logros conseguidos a la vez salen en cola, uno cada 3 s: antes cada aviso tapaba al anterior.

## v0.11.0 (4-11-2026)
- **Máscaras y objetos:** 15 slots desde CC60, cargados al empezar cada fase desde la tabla del banco 6 (índice en A889 por ronda y fase, 9 bytes por objeto:
  bandera, Y, X, clase en +09, puntos en +1B, sprite del objeto en +1E). Bandera FF = máscara visible; 00 = objeto escondido en una pared.
  Clase: 0-3 armas, 4-7 subarmas, 8 poción, 9 vida.
- **41AE no destruye enemigos: es RECOGER un objeto** (salta por clase en IX+9). Las notas de arriba estaban mal. Ahora solo 3DFA cuenta como muerte y 41AE da
  `MOD_EV_PICKUP`, que vibra igual que antes. El contador de enemigos ya no suma objetos recogidos.
- **Romper una máscara:** 441A, con IX+0E/0F = 4320 (animación de las máscaras) → `MOD_EV_MASK`.
- **Objeto secreto:** el arma toca un metatile rompible (propiedad en D900 con bit 0x20, 448D → 44DA). Se busca el mapa en la tabla de C902 y, si tiene objeto, 4506 lo saca →
  `MOD_EV_SECRET`. En R1-1: x=384, 848 y 2096, y=160.
- **Armas:** C701 son los botones de este fotograma y C700 los mantenidos; ataque sin arriba mantenido = `MOD_EV_MELEE`.
- **Perfil:** líneas nuevas `masks=`, `weapons=` y `subs=` (bits). Un perfil antiguo se lee igual. Al importar: el mayor de las máscaras y la unión de los bits.
- **Packs de imágenes:** `tools/packs/montar_manual.py` gira las hojas, endereza, parte por el pliegue, limpia el papel (divide por el fondo y sube niveles) y deja cada página a 1600 px;
  `montar_galeria.py` ordena, pone los nombres y limita a 2160 px. Se copian como `galeria.zip` y `manual.zip` (mensaje Android 0x8007).
- Huellas `-H` idénticas a v0.10.5 con y sin mejoras: los ganchos nuevos solo anotan.


## v0.12.0 (4-11-2026)
- **Letras sueltas en la sala del jefe de 4-3 (y "JEEE" en el marcador):** durante los diálogos de jefe `mod_is_gameplay` falla (el cuadro tapa el
  escenario) y las letras extra se dibujaban sobre los tiles 1B0-1B8, que en esa sala son del nivel (UNUN, ÚÑ); la tilde caía en la F de JEFE.
  Ahora `mod_text_frame` también se apaga si el marcador está en pantalla (la etiqueta de puntos, ROM 10024, en las filas 0-3 de la tabla de nombres)
  o si alguno de los tiles 1B0-1B8 no está vacío: en intro, historia y final están vacíos (comprobado con volcados de VRAM).
- **Columna izquierda fuera de la partida (4:3 y 8:7):** en las líneas tapadas que no son de escenario, si los 8 píxeles siguientes son de un solo
  color, la columna se pinta de ese color (pantallas de fondo liso, como la del final). Las imágenes con dibujo quedan como en la consola.
- **Visor:** botones y pie en el borde inferior; la capa de interfaz se pega abajo de la pantalla mientras se ve una imagen (`ui_top`). Se ocultan
  a los 2 s y solo responden si se veían al tocar.
- **8:7** pone el escalado en Estirar al elegirlo.
- **Logros:** la lista se ordena por tipo (el perfil guarda los nombres, así que el orden no afecta a perfiles ni estados). 8 nuevos de Survivor.
- **Modo Survivor** (`GM_SURVIVOR`, mismo bloqueo que el modo logros y los logros cuentan): vidas a 1 cada fotograma (`DFBA`); al perder la vida
  (`MOD_EV_DEATH`) o con la música de continuar (82) sale la pantalla de fin. Energía del héroe `CA3A`, llena = 20. Ganchos nuevos con
  `energy_lock`: 13D3 (tras `LD (IX+1A),20` al preparar al héroe en cada fase) deja `energy_keep`, y 4233 (poción, sube 16 de uno en uno con 494F)
  vuelve sin curar (solo su sonido). Fácil: sin bloqueo. Normal: se suelta con la 86 en la fase 3 y vuelve en cuanto se juega la ronda siguiente.
  Difícil: siempre. Estados propios por dificultad (`estado_survivor_N.sav`).
- headless: `SVLOCK=1` (bloqueo de Difícil), `GPLOG=1` (cambios de partida/energía). Modo de prueba: `MOD_SVOVER=dificultad`.
- Huellas `-H` idénticas a v0.11.0 con y sin mejoras.

## v0.12.1 (4-11-2026)
- **8:7 sin deformar:** vuelve a Ajustar al elegirlo. Llenar una pantalla casi cuadrada sin deformar no se puede: la imagen 8:7 es más ancha que la
  pantalla y, al contrario que a los lados en 16:9, encima y debajo del escenario no hay nada en el mapa (el tramo tiene 10 filas de metatiles, la
  altura exacta de la pantalla). Opción nueva **Imagen › Bandas vacías: Negras / Ambiente** (`cfg.bands`, al elegir 8:7 se pone Ambiente): la imagen
  se reduce a 16x12 bloques de color medio (`draw_ambient`, en CPU para que no parpadee) y se estira oscurecida detrás. Vale para cualquier formato.
- **Survivor sin guardar ni cargar a mano** (menú `PAGE_MAIN_SURVIVOR`). Al empezar cada fase se guarda solo (`survivor_fase_N.sav`, primer fotograma
  de partida con otra ronda/fase). Al perder la vida: A repetir fase (carga ese guardado; cuenta como estado cargado y quita "Sin red"), X reiniciar,
  B inicio. Boss Rush: sin guardados (decisión del usuario).
- **TEMPORAL, quitar antes de la versión final:** `test_inv` / "Invencible (pruebas)" en los menús de modo logros y Survivor: `DFB4` a FF en partida,
  R2 acelera, y los logros van a `logros_pruebas.txt` (`logros_switch_profile`) para no mezclarse con el perfil real.

## v0.13.0 (4-11-2026) — Boss Rush
- **Saltar de jefe en jefe sin estados guardados:** se mantiene `DFC1` en la fase del jefe (2; 0 en la ronda 5, que solo tiene una) y, en la primera
  partida, también `DFC0`. Al superar una ronda el juego sube `DFC0` y carga la última fase de la siguiente. `DFC1` solo se escribe cuando `DFC0` ya es la
  ronda buscada: antes, el juego aún decide con él si la ronda ha terminado. Con la fase en marcha 10 fotogramas y la energía dada, `C90E = 3` lleva al
  jefe; si en 180 fotogramas no suena su música (`MOD_EV_BOSS_START`), se repite (en el primer fotograma de partida el juego lo deshacía: pasó en 4-3).
- El camino (título, intro, recuento y páginas de historia) va a 8 fotogramas por vuelta, con el botón 1 pulsado 4 de cada 40, sin sonido y tapado por
  "Jefe N de 5". Al llegar, arma, subarma y carga normal (16 pistola y bumerán, 8 bombas y estacas), vidas a 1.
- Jefe vencido: `MOD_EV_BOSS_DOWN`, o la música 86 (8D en el último) si el evento se pierde. Energía para el siguiente con `energy_lock`/`energy_keep`
  (gancho 13D3): Fácil llena, Normal +10 (media barra), Difícil la que quede. Pociones bloqueadas igual que en Survivor.
- Reloj: fotogramas desde llegar al jefe hasta que cae (los diálogos cuentan; el camino no). Récord por dificultad en el perfil (`bossrushN=`, al
  importar se queda el menor). Dificultad = Vida + Armas (0 Fácil, 1-2 Normal, 3-4 Difícil).
- Sin guardar ni cargar (decisión del usuario). Al morir o al final, pantalla de resultado (A otra vez, B inicio).
- Logro **Miniyo** (vida extra): gancho 4256 (clase 9 de objeto, sube `DFBA`) → `MOD_EV_LIFE`.
- Invencible de pruebas: ahora `DFB4` se escribe en cada fotograma, no solo cuando se reconoce la partida.
- **Prueba del bucle real** (escritorio): `MOD_LOOPTEST="fotogramas:dir"` y `MOD_LOOPMODE=br`: bucle a toda velocidad, bot que ataca, invencible, jefes con 1
  de energía; a Drácula el bot no llega y se da por vencido a los ~9600 fotogramas, saltando al final (AC35). Una partida completa salió bien.
- Huellas `-H` idénticas a v0.11.0.

## v0.13.1 (4-11-2026) — Boss Rush con jefes desbloqueados
- **El salto de v0.13.0 no valía:** `C90E = 3` saca al jefe donde está el héroe, no en su sala. En 1-3, 4-3 y 5 la sala es el principio de la fase y
  funcionaba; en 2-3 (y 3-3) el jefe salía en el pasillo y, tras vencerlo, la ronda no terminaba (vídeo del usuario).
- El juego llama al jefe en 1377: `CALL 5098` mira el metatile bajo el héroe y, si es `FD` (casilla de jefe de su sala), pone `C90E = 3`.
- Ahora, en cualquier otro modo, el fotograma en que `C90E` pasa a 3 en la fase del jefe (2; 0 en la ronda 5) se guarda como `jefe_N.sav` (estado +
  música, sin datos de logros), una vez por jefe, con el aviso "Boss Rush: jefe N desbloqueado". El Boss Rush pide los cinco y los carga seguidos.
  Al entrar: arma, subarma y carga, una vida, invencible a 0 y la energía de la partida (`energy_keep`), con la barra redibujada por el propio
  juego (`MOD_HUD_ENERGY` → 494F). Ronda superada (86) → 2 s → siguiente sala. Último: 8D → resultado.
- Ya no hay camino rápido ni tapado: se quitan `br_transit` a 8 fotogramas, el botón automático y el silencio.
- Prueba en el bucle real con estados hechos a mano (los de v0.13.0): cinco jefes, cambio entre ellos, final, récord y logros correctos.

## v0.14.0 (5-11-2026) — escaleras con salto, formato Game Gear, botones del inicio
- **Logro del jefe 2 antes de tiempo:** el jefe de la ronda 2 vacía su barra (`CB3A`, gancho 6198 → `MOD_EV_BOSS_DOWN`) al final de su primera
  forma y luego la vuelve a llenar (`A0`, y `RES 7` → `20`). Ahora un jefe solo cuenta como vencido con la música de fase/ronda superada (81/86; 8D
  el último del Boss Rush). En el Boss Rush el reloj se para con la barra vacía y vuelve a correr si el jefe recupera energía. En los logros normales
  (`bossclean`, `bossknife`, `bossaxe`) se guarda el arma del último golpe y se dan con la 81/86; Drácula sigue con `MOD_EV_BOSS_DOWN` (su final no
  tiene música de fase). Pruebas de `tools/test/test_logros.c` puestas al día (los logros de ronda salen ahora con la 86, como en el juego).
- **Estados del héroe** (`CA31`, tabla en 1456): 0 quieto, 1 andando, 2 cayendo, 3 subiendo en el salto, 4 escalera "/", 5 escalera "\", 6-9 otros.
  En escalera: `C80B` fotogramas que quedan del peldaño (7..0), pasos de 8 px en X e Y (lógica a 30 Hz, 1 px por fotograma alternando ejes),
  `CA30` mirando (0 derecha); en la 4 la cara decide el sentido (0 sube a la derecha, 1 baja a la izquierda). Con `C841 = (cámara + X) & 15 = 8`
  se comprueba si la escalera termina. Rejilla de sondas `C820..C82E` (5 filas × 3 columnas de metatiles alrededor del héroe, propiedades de D900:
  bit 04 "/", bit 08 "\", bits 01/02 suelo). `CA22` = pies del héroe.
- **Geometría medida (1-1):** en un tile "/" (fila R, columna C del mapa D000) los pies van por mapX + Y = 16(C+R) + 32, del pie en mapX = 16C-8 a la
  cima en 16C+8; en uno "\" por mapX − Y = 16(C−R) − 32 (pie en 16C+8, cima en 16C−8). Comprobado subiendo y bajando las dos de 1-1.
- **Escaleras: Con salto** (Controles, `cfg.stairs`, `E.stairs_jump`; remasterizada Sí, original No; vale en todos los modos porque no es una ventaja):
  - Agarrarse: tras cada fotograma, en el aire (2/3) con arriba (sin abajo ni ataque), se busca el punto de peldaño más cercano (X múltiplo de 8) en
    una línea de escalera a ≤6 px en X y ≤10 en Y; entre dos peldaños los dos tiles deben ser de escalera (los extremos no, para no cortar un salto
    junto a una escalera). Se coloca al héroe ahí, estado 4/5, `C80B = 0`, velocidades a 0 y la animación de quieto en escalera (2A/3B, 3A/2B).
    Si se salta desde una escalera, no se vuelve a agarrar hasta que empieza a caer (subir en diagonal y saltar sigue funcionando).
  - Soltarse: ganchos 1B15 y 1C14 (la comprobación de salto entre peldaños de cada tipo): con salto + abajo se va a 199F (caída, como al salir de un
    borde) con la velocidad horizontal a 0.
  - Huellas `-H` idénticas a v0.13.1 con y sin mejoras (opción apagada en headless; `STAIRS=1` la enciende). headless: `STAIRLOG`, `POSLOG`.
- **Formato Game Gear (8:7)** (Imagen › Formato, cuarto valor; `cfg.gg`): ventana de 160×144 sobre la imagen 4:3 (`mod_render_full43` a `gg_full`),
  que sigue al héroe (X centrada en `CA24`+8, Y en `CA22`−20, solo entre las líneas 32 y 48 para no enseñar el marcador) con un retardo de 3 px/2 px
  por fotograma. Se muestra a 8:7 (aspecto de píxel 1,029). Título, historia, continuar y diálogos de jefe (más de 20 fotogramas sin
  `mod_is_gameplay`) se ven enteros. En la pausa (`C108`) se dibuja encima el marcador (líneas 0-31) a todo el ancho. `game_h` sustituye a `SMS_H`
  en texturas, scanlines, CRT, bandas y escalado.
- **Botones del inicio:** esquinas de cada trío (Opciones, Galería, Manual, Salir) con la placa de pizarra de Galería/Manual; los del medio
  (Controles, Créditos) con la placa de cobre de Controles. Placas limpias reconstruidas de los botones existentes (texto quitado y relleno con la
  textura de la propia fila) y letras originales del usuario recortadas por color, a la misma altura de mayúsculas. Salir pasa de 250 a 236 de ancho.
