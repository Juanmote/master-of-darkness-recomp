// Letters the game's font does not have (Spanish translation): Á É Í Ó Ú Ñ ¿ ¡ Ü.
// They are made while the game runs from its own font in VRAM (an accent added on top of the
// letter, the question and exclamation marks turned upside down) and drawn in place of the
// tiles MOD_GLYPH_FIRST.. that no text screen uses. Not during gameplay: the level uses them.
#pragma once
#include "../core/sms.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOD_GLYPH_CODE 0xB0                    // text byte of the first extra letter
#define MOD_GLYPH_FIRST (0x100 + MOD_GLYPH_CODE)   // background tile it is drawn with
#define MOD_GLYPH_COUNT 9                      // Á É Í Ó Ú Ñ ¿ ¡ Ü

// Call once per frame (before running it): on = the Spanish text is in use.
void mod_text_frame(sms_t* s, bool on);

#ifdef __cplusplus
}
#endif
