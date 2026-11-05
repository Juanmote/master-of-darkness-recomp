// Achievement icons: our own 11x11 pixel art (nothing from the game), drawn at 2x in the list.
// Each achievement has a motif, a metal for its tier and optionally 1-4 dots (round or boss number).
// Palette: . clear, k black, w white, s silver, g grey, r red, d dark red, y gold, o orange, b brown,
// n blue, c light blue, G green, p skin.
#pragma once
#include <stdint.h>
#include <string.h>

enum { IC_HAT, IC_CANDLE, IC_TOMB, IC_TOWER, IC_CROSS, IC_HEART, IC_CROWN, IC_SKULL, IC_SHIELD, IC_BAG,
       IC_FIST, IC_STAR, IC_KNIFE, IC_AXE, IC_WALL, IC_MINI, IC_SWORD, IC_BOMB, IC_MASK, IC_BAT, IC_BONES,
       IC_MOON, IC_LOCK, IC_DEMON, IC_WATCH, IC_COUNT };

static const char* const icon_art[IC_COUNT][11] = {
  {"...........", "...ggggg...", "..gsssssg..", "..gsggggg..", "..gsggggg..", "..rrrrrrr..", "..ggggggg..", "ggggggggggg", ".ggggggggg.", "...........", "..........."},  // top hat
  {".....o.....", "....oyo....", "....oyo....", ".....k.....", "....www....", "....wsw....", "....wsw....", "....wsw....", "....wsw....", "...ggggg...", "..ggggggg.."},  // candle
  {"...........", "...sssss...", "..sgggggs..", "..sgsgsgs..", "..sgsssgs..", "..sgsgsgs..", "..sgggggs..", "..sgggggs..", "..sgggggs..", ".GGGGGGGGG.", "GGGGGGGGGGG"},  // tombstone
  {"..g.g.g.g..", "..ggggggg..", "...ggggg...", "...gnggg...", "...ggggg...", "...gggng...", "...ggggg...", "...ggkgg...", "..ggkkkgg..", "..ggkkkgg..", ".ggggggggg."},  // tower
  {".....y.....", "....yyy....", ".....y.....", "..y..y..y..", ".yyyyyyyyy.", "..y..y..y..", ".....y.....", ".....y.....", ".....y.....", "....yyy....", "...yyyyy..."},  // cross
  {"...........", ".rr.....rr.", "rrrr...rrrr", "rrwrr.rrrrr", "rrrrrrrrrrr", "rrrrrrrrrrr", ".rrrrrrrrr.", "..rrrrrrr..", "...rrrrr...", "....rrr....", ".....r....."},  // heart
  {"...........", "y....y....y", "yy..yyy..yy", "yyy.yry.yyy", "yyyyyyyyyyy", "yyyyyyyyyyy", "yryyyryyyry", "yyyyyyyyyyy", "yyyyyyyyyyy", "...........", "..........."},  // crown
  {"...wwwww...", "..wwwwwww..", ".wwwwwwwww.", ".wkkwwwkkw.", ".wkkwwwkkw.", ".wwwwkwwww.", "..wwwkwww..", "...wwwww...", "...wkwkw...", "...wwwww...", "..........."},  // skull
  {"yyyyyyyyyyy", "yrrrrrrrrry", "yrrrryrrrry", "yrrrryrrrry", "yryyyyyyyry", "yrrrryrrrry", ".yrrryrrry.", ".yrrryrrry.", "..yrryrry..", "...yrrry...", "....yyy...."},  // shield
  {"....bbb....", "...b...b...", "....bbb....", "...bbbbb...", "..bbbbbbb..", ".bbbbybbbb.", ".bbbyyybbb.", ".bbbbybbbb.", ".bbbbbbbbb.", "..bbbbbbb..", "..........."},  // money bag
  {"...........", "...pp.pp...", "..pppppppp.", "..pkpkpkpp.", "..pppppppp.", "..pppppppp.", "..ppppppp..", "...ppppp...", "...nnnnn...", "...nnnnn...", "..........."},  // fist
  {".....y.....", ".....y.....", "....yyy....", "yyyyyyyyyyy", ".yyyyyyyyy.", "..yyyyyyy..", "...yyyyy...", "..yyy.yyy..", "..yy...yy..", ".yy.....yy.", "..........."},  // star
  {"..........s", ".........ss", "........ss.", ".......ss..", "......ss...", ".....ss....", "..k.ss.....", "...kk......", "..bkk......", ".bb..k.....", "bb........."},  // knife
  {"...ssss....", "..sssssb...", ".ssssssb...", ".sssssbb...", "..sss.b....", "......b....", ".....b.....", ".....b.....", "....b......", "....b......", "...b......."},  // axe
  {"bbbbbkbbbbb", "bbbbbkbbbbb", "kkkkkkkkkkk", "bbkbbbbkbbb", "bbkbbkbkbbb", "kkkkkkkkkkk", "bbbbbkbbybb", "bbbbbkbyyyb", "kkkkkkkkykk", "bbkbbbbbbkb", "bbkbbbbbbkb"},  // wall with a hidden object
  {"...........", "....bbb....", "....ppp....", "....ppp....", "...nnnnn...", "..nnnnnnn..", "..p.nnn.p..", "....nnn....", "....n.n....", "....n.n....", "...kk.kk..."},  // mini-me
  {"..........s", ".........ss", "........sws", ".......sws.", "......sws..", ".....sws...", "..y.sws....", "...yys.....", "...byy.....", "..bb..y....", ".bb........"},  // sword
  {"........o..", ".......o.y.", "......kk...", "....kkkk...", "...kkkkkk..", "..kkwkkkkk.", "..kwkkkkkk.", "..kkkkkkkk.", "..kkkkkkkk.", "...kkkkkk..", "....kkkk..."},  // bomb
  {"...wwwww...", "..wwwwwww..", ".wwwwwwwww.", ".wkkkwkkkw.", ".wwkwwwkww.", ".wwwwwwwww.", ".wwwwkwwww.", "..wwwwwww..", "..wwkkkww..", "...wwwww...", "....www...."},  // mask
  {"...........", "k.........k", "kk..k.k..kk", "kkk.kkk.kkk", "kkkkkrkkkkk", ".kkkkkkkkk.", "..kkkkkkk..", "..k.kkk.k..", ".....k.....", "...........", "..........."},  // bat
  {"w.........w", "ww.wwwww.ww", "..wwwwwwww.", "..wkkwkkww.", "..wwwkwwww.", "...wwwwww..", "...wkwkw...", "ww.......ww", "..ww...ww..", "....www....", "..ww...ww.."},  // skull and bones
  {"....yyy....", "..yyy......", ".yyy.......", ".yy........", "yyy........", "yyy........", "yyy........", ".yy........", ".yyy.......", "..yyy......", "....yyy...."},  // crescent moon
  {"...........", "....sss....", "...s...s...", "...s...s...", "..yyyyyyy..", "..yyyyyyy..", "..yyykyyy..", "..yyykyyy..", "..yyyyyyy..", "..yyyyyyy..", "..........."},  // padlock
  {"r.........r", "rr.......rr", ".rrrrrrrrr.", ".rrrrrrrrr.", ".ryyrrryyr.", ".rkyrrrykr.", ".rrrrrrrrr.", "..rrkkkrr..", "..rwkwkwr..", "...rrrrr...", "..........."},  // demon
  {"....yyy....", ".....y.....", "...yyyyy...", "..ywwwwwy..", ".ywwwkwwwy.", ".ywwwkwwwy.", ".ywwwkkkwy.", ".ywwwwwwwy.", "..ywwwwwy..", "...yyyyy...", "..........."},  // stopwatch
};

// metal of the tier: replaces the gold of the motif
enum { MT_GOLD, MT_SILVER, MT_BRONZE };

typedef struct { const char* id; uint8_t motif, metal, dots; } logro_icon_t;
static const logro_icon_t logro_icons[] = {
  {"round1", IC_HAT, MT_GOLD, 0}, {"round2", IC_CANDLE, MT_GOLD, 0}, {"round3", IC_TOMB, MT_GOLD, 0}, {"round4", IC_TOWER, MT_GOLD, 0},
  {"final", IC_CROSS, MT_GOLD, 0}, {"nocont", IC_HEART, MT_GOLD, 0}, {"nodeath", IC_CROWN, MT_GOLD, 0}, {"firstdeath", IC_SKULL, MT_GOLD, 0},
  {"stageclean", IC_SHIELD, MT_SILVER, 0}, {"stagenosub", IC_BAG, MT_GOLD, 0}, {"roundclean", IC_SHIELD, MT_GOLD, 0}, {"roundnosub", IC_FIST, MT_GOLD, 0},
  {"bossclean", IC_STAR, MT_GOLD, 0}, {"bossknife", IC_KNIFE, MT_GOLD, 0}, {"bossaxe", IC_AXE, MT_GOLD, 0},
  {"secret", IC_WALL, MT_GOLD, 0}, {"extralife", IC_MINI, MT_GOLD, 0}, {"weapons", IC_SWORD, MT_GOLD, 0}, {"subs", IC_BOMB, MT_GOLD, 0},
  {"mask50", IC_MASK, MT_GOLD, 0}, {"kill100", IC_BAT, MT_GOLD, 0}, {"kill1000", IC_BONES, MT_GOLD, 0},
  {"sv_round1", IC_MOON, MT_SILVER, 1}, {"sv_round2", IC_MOON, MT_SILVER, 2}, {"sv_round3", IC_MOON, MT_SILVER, 3}, {"sv_round4", IC_MOON, MT_SILVER, 4},
  {"sv_easy", IC_HEART, MT_BRONZE, 0}, {"sv_normal", IC_HEART, MT_SILVER, 0}, {"sv_hard", IC_CROWN, MT_GOLD, 0}, {"sv_nostate", IC_LOCK, MT_GOLD, 0},
  {"br_boss1", IC_DEMON, MT_GOLD, 1}, {"br_boss2", IC_DEMON, MT_GOLD, 2}, {"br_boss3", IC_DEMON, MT_GOLD, 3}, {"br_boss4", IC_DEMON, MT_GOLD, 4},
  {"br_easy", IC_WATCH, MT_BRONZE, 0}, {"br_normal", IC_WATCH, MT_SILVER, 0}, {"br_hard", IC_WATCH, MT_GOLD, 0},
};

static uint32_t icon_color(char c, int metal) {
  switch (c) {
  case 'k': return 0xFF141018; case 'w': return 0xFFF4F0E8; case 's': return 0xFFB8C0C8; case 'g': return 0xFF707880;
  case 'r': return 0xFFC8302A; case 'd': return 0xFF701818; case 'o': return 0xFFF08020; case 'b': return 0xFF7A4A24;
  case 'n': return 0xFF3050B0; case 'c': return 0xFF70A8E8; case 'G': return 0xFF3A7A3A; case 'p': return 0xFFE8B090;
  case 'y': return metal == MT_SILVER ? 0xFFD8E0E8 : metal == MT_BRONZE ? 0xFFC07838 : 0xFFF0C040;
  }
  return 0;
}

// the 11x11 icon of achievement `id` into out (ARGB, 0 = clear); false if it has none
static bool logro_icon(const char* id, uint32_t out[11 * 11], int* dots) {
  for (unsigned i = 0; i < sizeof logro_icons / sizeof logro_icons[0]; i++) {
    const logro_icon_t* li = &logro_icons[i];
    if (strcmp(li->id, id)) continue;
    for (int y = 0; y < 11; y++)
      for (int x = 0; x < 11; x++) out[y * 11 + x] = icon_color(icon_art[li->motif][y][x], li->metal);
    *dots = li->dots;   // which round or boss: drawn on the frame
    return true;
  }
  return false;
}
