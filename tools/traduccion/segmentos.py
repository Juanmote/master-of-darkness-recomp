"""Text segments of Master of Darkness (Europe/Brazil, CRC 96FB4D4B) and how they are stored.

Every segment keeps its place and size in the ROM (other code points at them), so a translation
only has to fit the same number of bytes. Formats:
  page   [x][text]([FF][x][text])*[FF][FF]        story, intro and ouija pages (x = column)
  box    [F1][x][text]([FF][ctl..][x][text])*      boss dialogue boxes, until the next F1/F5/FE
  scroll ([len][text])*[00]                        ending text that scrolls up
  fixed  text of a fixed length                    titles, status bar, menus
"""
# story / intro pages: address of the x byte of the first line (narration code)
PAGES = [
    ('N01', 0xDD1C), ('N02', 0xDD48), ('N03', 0xDD73), ('N04', 0xDD9A),
    ('N05', 0xE299), ('N06', 0xE317), ('N07', 0xE3BC), ('N08', 0xE434),
    ('N10', 0xD081), ('N11', 0xD100), ('N12', 0xD1B5), ('N13', 0xD25B),
    ('N14', 0xD329), ('N15', 0xD3BB), ('N16', 0xD46C),
    ('N17', 0xD535), ('N18', 0xD5DA), ('N19', 0xD68B), ('N20', 0xD719),
    ('N21', 0xD7AC), ('N22', 0xD80C), ('N23', 0xD8A9), ('N24', 0xD946),
    ('N25', 0xD9EB), ('N26', 0xDA87), ('N27', 0xDB37),
    ('OUIJA', 0xE68B),
]
# pages shown one after another without clearing the screen (each one must cover the previous;
# found by checking that the original pages do cover the previous ones)
_CHAINS = [['N05', 'N06'], ['N07', 'N08'], ['N10', 'N11', 'N12', 'N13'],
           ['N14', 'N15', 'N16'], ['N17', 'N18', 'N19', 'N20'], ['N21', 'N22', 'N23', 'N24'], ['N25', 'N26', 'N27']]
CHAIN = {c[i]: c[i + 1] for c in _CHAINS for i in range(len(c) - 1)}
SCROLL = 0xEEDC          # ending (bank 3, CPU address AEDC)
BOXES = (0x58C5, 0x5C5F)  # boss dialogue boxes (bank 1), last one ends with FF F5 FE
FIXED = [  # (name, offset, length)
    ('PAUSE', 0x4C0E, 7), ('BOSS', 0x625A, 4),
    ('HUD_HISCORE', 0x10001, 8), ('HUD_SCORE', 0x10024, 5), ('HUD_TIME_LIFE', 0x1003A, 15), ('HUD_P', 0x1007B, 1),
    ('R1', 0xF80D, 40), ('R2', 0xF835, 40), ('R3', 0xF85D, 40), ('R4', 0xF885, 40), ('R5', 0xF8AD, 40),
    ('CLEAR', 0xFBA1, 21), ('TIME_BONUS', 0xFBE6, 11), ('LIFE_BONUS', 0xFC4F, 11), ('TOTAL', 0xFCB8, 5), ('SCORE', 0xFCF7, 5),
    ('DBG_MODE', 0xC299, 11), ('DBG_ROUND', 0xC2D2, 12), ('DBG_STAGE', 0xC2FA, 13), ('DBG_PLAYER', 0xC322, 13),
    ('DBG_HIT', 0xC34A, 12), ('DBG_SOUND', 0xC372, 6), ('DBG_START', 0xC3AE, 10),
]
TEXT_W = 24   # characters per line in the story pages (column 0)

def parse_page(rom, a):
    """-> list of [x, bytes] lines and the end offset (after FF FF)"""
    lines = [[rom[a], bytearray()]]
    i = a + 1
    while True:
        b = rom[i]
        if b == 0xFF:
            if rom[i + 1] == 0xFF: return lines, i + 2
            lines.append([rom[i + 1], bytearray()]); i += 2
        else:
            lines[-1][1].append(b); i += 1

def parse_scroll(rom, a):
    lines = []
    i = a
    while rom[i]:
        n = rom[i]; lines.append(bytes(rom[i + 1:i + 1 + n])); i += 1 + n
    return lines, i + 1

def parse_boxes(rom, a, end):
    """-> list of boxes; box = list of (prefix bytes, text bytes)"""
    boxes = []
    i = a
    while i < end:
        b = rom[i]
        if b == 0xF1:
            boxes.append({'start': i, 'lines': [[bytes([0xF1, rom[i + 1]]), bytearray()]]}); i += 2
        elif b == 0xFF:
            j = i + 1; pre = bytearray([0xFF])
            while rom[j] in (0xF4, 0xF3):
                pre += rom[j:j + 2]; j += 2
            if rom[j] in (0xF5, 0xFE, 0xF1):   # end of the box: what follows is control
                boxes[-1]['tail'] = bytearray([0xFF]); i = j; continue
            pre.append(rom[j]); boxes[-1]['lines'].append([bytes(pre), bytearray()]); i = j + 1
        elif b in (0xF5, 0xFE):
            boxes[-1].setdefault('tail', bytearray()).append(b); i += 1
        elif b == 0xF3:
            boxes[-1].setdefault('tail', bytearray()).extend(rom[i:i + 2]); i += 2
        else:
            boxes[-1]['lines'][-1][1].append(b); i += 1
    for k, bx in enumerate(boxes):
        bx['end'] = boxes[k + 1]['start'] if k + 1 < len(boxes) else end
    return boxes
