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
