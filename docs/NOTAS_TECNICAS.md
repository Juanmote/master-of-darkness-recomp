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
