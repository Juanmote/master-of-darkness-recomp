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
