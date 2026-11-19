/* Key input parsing ported from Pi packages/tui/src/keys.ts, snapshot
 * d5b3de22. See third-party/Pi-LICENSE for upstream attribution. */
#include <pangu/tui/keys.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Shared state                                                        */
/* ------------------------------------------------------------------ */

static atomic_int g_kitty_active = 0;
static atomic_int g_windows_terminal = 0;

void pangu_tui_keys_set_kitty(int active) { g_kitty_active = active; }
int pangu_tui_keys_kitty(void) { return g_kitty_active; }
void pangu_tui_keys_set_windows_terminal(int is_windows_terminal) {
    g_windows_terminal = is_windows_terminal;
}

/* ------------------------------------------------------------------ */
/* Parsed sequence representation                                      */
/* ------------------------------------------------------------------ */

#define LOCK_MASK (64 + 128)

enum {
    CP_ESCAPE = 27,
    CP_TAB = 9,
    CP_ENTER = 13,
    CP_SPACE = 32,
    CP_BACKSPACE = 127,
    CP_KP_ENTER = 57414
};

/* Sentinel codepoints for keys without a Unicode codepoint; stored in the
 * unsigned codepoint field, far above the valid Unicode range. */
#define CP_UP       0xFFFFF001u
#define CP_DOWN     0xFFFFF002u
#define CP_RIGHT    0xFFFFF003u
#define CP_LEFT     0xFFFFF004u
#define CP_DELETE   0xFFFFF00Au
#define CP_INSERT   0xFFFFF00Bu
#define CP_PAGE_UP  0xFFFFF00Cu
#define CP_PAGE_DOWN 0xFFFFF00Du
#define CP_HOME     0xFFFFF00Eu
#define CP_END      0xFFFFF00Fu

static int is_functional_cp(unsigned cp) {
    return cp >= 0xFFFFF001u && cp <= 0xFFFFF00Fu;
}

/* Leaves sentinel values intact: they are far outside the map range. */
static unsigned normalize_functional(unsigned cp) {
    switch (cp) {
    case 57399u: return '0'; case 57400u: return '1'; case 57401u: return '2';
    case 57402u: return '3'; case 57403u: return '4'; case 57404u: return '5';
    case 57405u: return '6'; case 57406u: return '7'; case 57407u: return '8';
    case 57408u: return '9'; case 57409u: return '.'; case 57410u: return '/';
    case 57411u: return '*'; case 57412u: return '-'; case 57413u: return '+';
    case 57415u: return '='; case 57416u: return ',';
    case 57417u: return CP_LEFT; case 57418u: return CP_RIGHT; case 57419u: return CP_UP;
    case 57420u: return CP_DOWN; case 57421u: return CP_PAGE_UP;
    case 57422u: return CP_PAGE_DOWN; case 57423u: return CP_HOME;
    case 57424u: return CP_END; case 57425u: return CP_INSERT;
    case 57426u: return CP_DELETE;
    default: return cp;
    }
}

static int is_symbol_key(unsigned cp) {
    static const char symbols[] = "`-=[]\\;',./!@#$%^&*()_+|~{}:<>?";
    if (cp < 32 || cp > 126) return 0;
    return strchr(symbols, (char)cp) != NULL;
}

static unsigned normalize_shifted_letter(unsigned cp, unsigned modifier) {
    unsigned effective = modifier & ~(unsigned)LOCK_MASK;
    if ((effective & PANGU_KMOD_SHIFT) && cp >= 'A' && cp <= 'Z') return cp + 32;
    return cp;
}

typedef struct {
    unsigned codepoint;
    unsigned shifted_key; /* 0 = absent */
    unsigned base_layout_key; /* 0 = absent */
    unsigned modifier; /* bitmask, 0 = none */
    pangu_key_event_type event_type;
} kitty_sequence;

/* Parse CSI-u: <cp>[:<shifted>[:<base>]][;<mod>[:<event>]]u. Colons separate
 * key groups (codepoint, shifted, base); the first semicolon starts the
 * modifier group, where colons denote the event type. */
static int parse_csi_u(const uint8_t *d, size_t n, kitty_sequence *out) {
    if (n < 3 || n > 64 || d[0] != 0x1b || d[1] != '[' || d[n - 1] != 'u') return 0;
    unsigned values[5] = {0, 0, 0, 0, 0};
    int have[5] = {0, 0, 0, 0, 0};
    int idx = 0;
    int expect_digit = 0;
    for (size_t i = 2; i < n - 1; ++i) {
        uint8_t c = d[i];
        if (c >= '0' && c <= '9') {
            if (values[idx] > 0x10ffffu) return 0;
            values[idx] = values[idx] * 10 + (unsigned)(c - '0');
            expect_digit = 0;
        } else if (c == ':') {
            if (idx >= 3 && expect_digit) return 0;
            if (++idx > 4) return 0;
            have[idx] = 1;
            expect_digit = 1;
        } else if (c == ';') {
            if (expect_digit) return 0;
            /* Jump from the key groups (0-2) to the modifier group (3). */
            idx = 3;
            have[3] = 1;
            expect_digit = 1;
        } else {
            return 0;
        }
    }
    if (expect_digit || values[0] == 0) return 0;
    out->codepoint = values[0];
    out->shifted_key = have[1] ? values[1] : 0;
    out->base_layout_key = have[2] ? values[2] : 0;
    out->modifier = have[3] ? (values[3] ? values[3] - 1 : 0) : 0;
    out->event_type = have[4] ? (values[4] == 2 ? PANGU_KEY_REPEAT
                               : values[4] == 3 ? PANGU_KEY_RELEASE : PANGU_KEY_PRESS)
                              : PANGU_KEY_PRESS;
    return 1;
}

/* Parse ESC[<num>[;<mod>[:<event>]]<final> for arrows, home/end, and the
 * `~` functional keys. Arrows/home/end require first param 1. */
static int parse_csi_final(const uint8_t *d, size_t n, kitty_sequence *out) {
    if (n < 3 || n > 64 || d[0] != 0x1b || d[1] != '[') return 0;
    uint8_t final_byte = d[n - 1];
    unsigned first = 0;
    unsigned mod = 0;
    int have_mod = 0;
    pangu_key_event_type event = PANGU_KEY_PRESS;
    size_t i = 2;
    if (d[2] >= '0' && d[2] <= '9') {
        while (i < n - 1 && d[i] >= '0' && d[i] <= '9') {
            first = first * 10 + (unsigned)(d[i] - '0');
            ++i;
        }
        if (i < n - 1 && d[i] == ';') {
            ++i;
            have_mod = 1;
            while (i < n - 1 && d[i] >= '0' && d[i] <= '9') {
                mod = mod * 10 + (unsigned)(d[i] - '0');
                ++i;
            }
            if (i < n - 1 && d[i] == ':') {
                ++i;
                unsigned ev = 0;
                while (i < n - 1 && d[i] >= '0' && d[i] <= '9') {
                    ev = ev * 10 + (unsigned)(d[i] - '0');
                    ++i;
                }
                event = ev == 2 ? PANGU_KEY_REPEAT : ev == 3 ? PANGU_KEY_RELEASE : PANGU_KEY_PRESS;
            }
        }
        if (i != n - 1) return 0;
    } else if (n != 3) {
        return 0;
    }
    unsigned cp;
    switch (final_byte) {
    case 'A': cp = CP_UP; break;
    case 'B': cp = CP_DOWN; break;
    case 'C': cp = CP_RIGHT; break;
    case 'D': cp = CP_LEFT; break;
    case 'H': cp = CP_HOME; break;
    case 'F': cp = CP_END; break;
    case 'E': cp = 0; break; /* clear key: \x1b[E has no params */
    case '~':
        switch (first) {
        case 2: cp = CP_INSERT; break;
        case 3: cp = CP_DELETE; break;
        case 5: cp = CP_PAGE_UP; break;
        case 6: cp = CP_PAGE_DOWN; break;
        case 7: cp = CP_HOME; break;
        case 8: cp = CP_END; break;
        default: return 0;
        }
        break;
    default:
        return 0;
    }
    if (final_byte != '~' && (first != 1 || !have_mod)) return 0;
    out->codepoint = cp;
    out->shifted_key = 0;
    out->base_layout_key = 0;
    out->modifier = have_mod ? (mod ? mod - 1 : 0) : 0;
    out->event_type = event;
    return 1;
}

static int parse_kitty(const uint8_t *d, size_t n, kitty_sequence *out) {
    memset(out, 0, sizeof *out);
    if (parse_csi_u(d, n, out)) return 1;
    memset(out, 0, sizeof *out);
    if (parse_csi_final(d, n, out)) return 1;
    return 0;
}

/* xterm modifyOtherKeys: ESC[27;<mod>;<cp>~ */
static int parse_modify_other_keys(const uint8_t *d, size_t n, kitty_sequence *out) {
    if (n < 8 || d[0] != 0x1b || d[1] != '[' || memcmp(d, "\x1b[27;", 5) != 0) return 0;
    unsigned mod = 0, cp = 0;
    size_t i = 5;
    while (i < n && d[i] >= '0' && d[i] <= '9') { mod = mod * 10 + (unsigned)(d[i] - '0'); ++i; }
    if (i >= n || d[i] != ';') return 0;
    ++i;
    while (i < n && d[i] >= '0' && d[i] <= '9') { cp = cp * 10 + (unsigned)(d[i] - '0'); ++i; }
    if (i != n - 1 || d[i] != '~') return 0;
    if (!mod) return 0;
    memset(out, 0, sizeof *out);
    out->codepoint = cp;
    out->modifier = mod - 1;
    out->event_type = PANGU_KEY_PRESS;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Legacy sequences                                                    */
/* ------------------------------------------------------------------ */

typedef struct { const char *seq; const char *id; } legacy_entry;

static const legacy_entry LEGACY_IDS[] = {
    {"\x1bOA", "up"}, {"\x1bOB", "down"}, {"\x1bOC", "right"}, {"\x1bOD", "left"},
    {"\x1bOH", "home"}, {"\x1bOF", "end"},
    {"\x1b[A", "up"}, {"\x1b[B", "down"}, {"\x1b[C", "right"}, {"\x1b[D", "left"},
    {"\x1b[H", "home"}, {"\x1b[F", "end"},
    {"\x1b[E", "clear"}, {"\x1bOE", "clear"},
    {"\x1bOe", "ctrl+clear"}, {"\x1b[e", "shift+clear"},
    {"\x1b[2~", "insert"}, {"\x1b[2$", "shift+insert"}, {"\x1b[2^", "ctrl+insert"},
    {"\x1b[3~", "delete"},
    {"\x1b[3$", "shift+delete"}, {"\x1b[3^", "ctrl+delete"},
    {"\x1b[5~", "pageUp"}, {"\x1b[6~", "pageDown"},
    {"\x1b[[5~", "pageUp"}, {"\x1b[[6~", "pageDown"},
    {"\x1b[a", "shift+up"}, {"\x1b[b", "shift+down"},
    {"\x1b[c", "shift+right"}, {"\x1b[d", "shift+left"},
    {"\x1bOa", "ctrl+up"}, {"\x1bOb", "ctrl+down"},
    {"\x1bOc", "ctrl+right"}, {"\x1bOd", "ctrl+left"},
    {"\x1b[5$", "shift+pageUp"}, {"\x1b[6$", "shift+pageDown"},
    {"\x1b[7$", "shift+home"}, {"\x1b[8$", "shift+end"},
    {"\x1b[5^", "ctrl+pageUp"}, {"\x1b[6^", "ctrl+pageDown"},
    {"\x1b[7^", "ctrl+home"}, {"\x1b[8^", "ctrl+end"},
    {"\x1b[1;5H", "ctrl+home"}, {"\x1b[1;5F", "ctrl+end"},
    {"\x1bOP", "f1"}, {"\x1bOQ", "f2"}, {"\x1bOR", "f3"}, {"\x1bOS", "f4"},
    {"\x1b[11~", "f1"}, {"\x1b[12~", "f2"}, {"\x1b[13~", "f3"}, {"\x1b[14~", "f4"},
    {"\x1b[[A", "f1"}, {"\x1b[[B", "f2"}, {"\x1b[[C", "f3"}, {"\x1b[[D", "f4"},
    {"\x1b[[E", "f5"}, {"\x1b[15~", "f5"},
    {"\x1b[17~", "f6"}, {"\x1b[18~", "f7"}, {"\x1b[19~", "f8"},
    {"\x1b[20~", "f9"}, {"\x1b[21~", "f10"}, {"\x1b[23~", "f11"}, {"\x1b[24~", "f12"},
    {"\x1b" "b", "alt+left"}, {"\x1b" "f", "alt+right"},
    {"\x1b" "p", "alt+up"}, {"\x1b" "n", "alt+down"},
    {"\x1b" "B", "alt+left"}, {"\x1b" "F", "alt+right"},
};

static const char *legacy_id(const uint8_t *d, size_t n) {
    char buf[12];
    if (n == 0 || n >= sizeof buf) return NULL;
    memcpy(buf, d, n); buf[n] = 0;
    for (size_t i = 0; i < sizeof LEGACY_IDS / sizeof LEGACY_IDS[0]; ++i) {
        if (strcmp(LEGACY_IDS[i].seq, buf) == 0) return LEGACY_IDS[i].id;
    }
    return NULL;
}

/* Control character for ctrl+letter/symbol, or 0 when none. */
static int raw_ctrl_char(unsigned key_cp) {
    unsigned cp = (key_cp >= 'A' && key_cp <= 'Z') ? key_cp + 32 : key_cp;
    if ((cp >= 'a' && cp <= 'z') || cp == '[' || cp == '\\' || cp == ']' || cp == '_') {
        return (int)(cp & 0x1f);
    }
    if (cp == '-') return 31; /* same physical key as _ */
    return 0;
}

/* ------------------------------------------------------------------ */
/* Kitty / modifyOtherKeys matching helpers                            */
/* ------------------------------------------------------------------ */

static int matches_kitty_sequence(const uint8_t *data, size_t n, unsigned expected_cp,
                                  unsigned expected_mod) {
    kitty_sequence parsed;
    if (!parse_kitty(data, n, &parsed)) return 0;
    unsigned actual_mod = parsed.modifier & ~(unsigned)LOCK_MASK;
    unsigned expected_mod_clean = expected_mod & ~(unsigned)LOCK_MASK;

    unsigned actual_cp = normalize_shifted_letter(normalize_functional(parsed.codepoint),
                                                  parsed.modifier);
    unsigned expected_cp_clean = normalize_shifted_letter(normalize_functional(expected_cp),
                                                          expected_mod);

    if (actual_mod == expected_mod_clean && actual_cp == expected_cp_clean) return 1;

    /* Logical match: symbols like "+" arrive as shift plus the physical "=" key. */
    if ((actual_mod & PANGU_KMOD_SHIFT) && parsed.shifted_key
        && is_symbol_key(normalize_functional(parsed.shifted_key))) {
        unsigned logical_mod = actual_mod & ~(unsigned)PANGU_KMOD_SHIFT;
        if (logical_mod == expected_mod_clean
            && normalize_functional(parsed.shifted_key) == expected_cp) return 1;
    }
    if (actual_mod != expected_mod_clean) return 0;

    /* Alternate match: base layout key for non-Latin layouts, only when the
     * codepoint is not already a recognized Latin letter or symbol. */
    if (parsed.base_layout_key && parsed.base_layout_key == expected_cp) {
        unsigned cp = actual_cp;
        int is_latin = cp >= 'a' && cp <= 'z';
        if (!is_latin && !is_symbol_key(cp)) return 1;
    }
    return 0;
}

static int matches_modify_other_keys(const uint8_t *data, size_t n, unsigned expected_cp,
                                     unsigned expected_mod) {
    kitty_sequence parsed;
    if (!parse_modify_other_keys(data, n, &parsed)) return 0;
    return parsed.codepoint == expected_cp && parsed.modifier == expected_mod;
}

static int matches_printable_modify_other_keys(const uint8_t *data, size_t n, unsigned expected_cp,
                                               unsigned expected_mod) {
    kitty_sequence parsed;
    if (!parse_modify_other_keys(data, n, &parsed)) return 0;
    unsigned actual = normalize_shifted_letter(parsed.codepoint, parsed.modifier);
    unsigned expected = normalize_shifted_letter(expected_cp, expected_mod);
    if (parsed.modifier == expected_mod && actual == expected) return 1;
    /* Logical shifted symbol: e.g. ctrl+"+" reported as shift ctrl "=". */
    if ((parsed.modifier & PANGU_KMOD_SHIFT) && is_symbol_key(parsed.codepoint)) {
        unsigned logical_mod = parsed.modifier & ~(unsigned)PANGU_KMOD_SHIFT;
        return logical_mod == expected_mod && parsed.codepoint == expected_cp;
    }
    return 0;
}

/* Raw 0x08 ambiguity: Windows Terminal means ctrl+backspace, otherwise plain. */
static int matches_raw_backspace(const uint8_t *data, size_t n, unsigned expected_mod) {
    if (n == 1 && data[0] == 0x7f) return expected_mod == 0;
    if (n != 1 || data[0] != 0x08) return 0;
    return g_windows_terminal ? expected_mod == PANGU_KMOD_CTRL : expected_mod == 0;
}

/* ------------------------------------------------------------------ */
/* Key id parsing                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    char key[12];
    unsigned modifiers;
} key_id_parts;

static int parse_key_id(const char *key_id, key_id_parts *out) {
    if (!key_id) return 0;
    size_t len = strlen(key_id);
    char lower[64];
    if (len == 0 || len >= sizeof lower) return 0;
    for (size_t i = 0; i <= len; ++i) {
        char c = key_id[i];
        lower[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    const char *p = lower;
    unsigned mods = 0;
    for (;;) {
        const char *plus = strchr(p, '+');
        size_t name_len = plus ? (size_t)(plus - p) : strlen(p);
        if (name_len == 5 && memcmp(p, "shift", 5) == 0) {
            if (mods & PANGU_KMOD_SHIFT) return 0;
            mods |= PANGU_KMOD_SHIFT;
        } else if (name_len == 4 && memcmp(p, "ctrl", 4) == 0) {
            if (mods & PANGU_KMOD_CTRL) return 0;
            mods |= PANGU_KMOD_CTRL;
        } else if (name_len == 3 && memcmp(p, "alt", 3) == 0) {
            if (mods & PANGU_KMOD_ALT) return 0;
            mods |= PANGU_KMOD_ALT;
        } else if (name_len == 5 && memcmp(p, "super", 5) == 0) {
            if (mods & PANGU_KMOD_SUPER) return 0;
            mods |= PANGU_KMOD_SUPER;
        } else {
            break; /* rest is the key name */
        }
        if (!plus) return 0; /* modifier without a key */
        p = plus + 1;
    }
    size_t key_len = strlen(p);
    if (key_len == 0 || key_len >= sizeof out->key) return 0;
    if (key_len > 1 && strchr(p, '+')) return 0; /* stray modifier */
    memcpy(out->key, p, key_len + 1);
    out->modifiers = mods;
    return 1;
}

/* ------------------------------------------------------------------ */
/* UTF-8 helpers                                                       */
/* ------------------------------------------------------------------ */

static size_t encode_utf8(char *out, unsigned cp) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static size_t decode_utf8(const uint8_t *d, size_t n, unsigned *cp) {
    if (n == 0) return 0;
    uint8_t c = d[0];
    if (c < 0x80) { *cp = c; return 1; }
    if (c < 0xC0) return 0;
    size_t len = c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    unsigned value = c & (len == 2 ? 0x1Fu : len == 3 ? 0x0Fu : 0x07u);
    if (n < len) return 0;
    for (size_t i = 1; i < len; ++i) {
        if ((d[i] & 0xC0) != 0x80) return 0;
        value = (value << 6) | (d[i] & 0x3Fu);
    }
    *cp = value;
    return len;
}

/* ------------------------------------------------------------------ */
/* Key formatting                                                      */
/* ------------------------------------------------------------------ */

/* Write "ctrl+shift+<key>" style identifier into out. Returns the length,
 * or 0 when the key/combination cannot be named (unsupported modifiers). */
static size_t format_key(char *out, size_t capacity, unsigned codepoint, unsigned modifier,
                         unsigned base_layout_key) {
    unsigned normalized = normalize_functional(codepoint);
    unsigned identity = normalize_shifted_letter(normalized, modifier);
    const char *key_name = NULL;
    char single[4];
    switch (identity) {
    case CP_ESCAPE: key_name = "escape"; break;
    case CP_TAB: key_name = "tab"; break;
    case CP_ENTER: case CP_KP_ENTER: key_name = "enter"; break;
    case CP_SPACE: key_name = "space"; break;
    case CP_BACKSPACE: key_name = "backspace"; break;
    case CP_DELETE: key_name = "delete"; break;
    case CP_INSERT: key_name = "insert"; break;
    case CP_HOME: key_name = "home"; break;
    case CP_END: key_name = "end"; break;
    case CP_PAGE_UP: key_name = "pageUp"; break;
    case CP_PAGE_DOWN: key_name = "pageDown"; break;
    case CP_UP: key_name = "up"; break;
    case CP_DOWN: key_name = "down"; break;
    case CP_LEFT: key_name = "left"; break;
    case CP_RIGHT: key_name = "right"; break;
    default:
        if (identity >= '0' && identity <= '9') {
            key_name = single; single[0] = (char)identity; single[1] = 0;
        } else if (identity >= 'a' && identity <= 'z') {
            key_name = single; single[0] = (char)identity; single[1] = 0;
        } else if (is_symbol_key(identity)) {
            key_name = single; single[0] = (char)identity; single[1] = 0;
        } else if (base_layout_key && !is_functional_cp(identity)
                   && base_layout_key != identity) {
            /* Non-Latin layout: fall back to the PC-101 base key. */
            unsigned base = normalize_functional(base_layout_key);
            if (base >= 'a' && base <= 'z') {
                key_name = single; single[0] = (char)base; single[1] = 0;
                identity = base;
            }
        }
        break;
    }
    if (!key_name) return 0;

    unsigned effective = modifier & ~(unsigned)LOCK_MASK;
    unsigned supported = PANGU_KMOD_SHIFT | PANGU_KMOD_CTRL | PANGU_KMOD_ALT | PANGU_KMOD_SUPER;
    if (effective & ~supported) return 0;

    size_t used = 0;
    struct { unsigned bit; const char *name; } mods[] = {
        {PANGU_KMOD_SHIFT, "shift+"}, {PANGU_KMOD_CTRL, "ctrl+"},
        {PANGU_KMOD_ALT, "alt+"}, {PANGU_KMOD_SUPER, "super+"}
    };
    for (size_t m = 0; m < 4; ++m) {
        if (effective & mods[m].bit) {
            size_t l = strlen(mods[m].name);
            if (used + l + 1 > capacity) return 0;
            memcpy(out + used, mods[m].name, l); used += l;
        }
    }
    size_t key_len = key_name == single ? 1 : strlen(key_name);
    if (used + key_len + 1 > capacity) return 0;
    memcpy(out + used, key_name, key_len); out[used + key_len] = 0;
    return used + key_len;
}

/* ------------------------------------------------------------------ */
/* Key name resolution for named keys                                  */
/* ------------------------------------------------------------------ */

static int key_name_codepoint(const char *name, unsigned *cp) {
    struct { const char *name; unsigned cp; } table[] = {
        {"escape", CP_ESCAPE}, {"esc", CP_ESCAPE},
        {"enter", CP_ENTER}, {"return", CP_ENTER},
        {"tab", CP_TAB}, {"space", CP_SPACE}, {"backspace", CP_BACKSPACE},
        {"delete", CP_DELETE}, {"insert", CP_INSERT},
        {"home", CP_HOME}, {"end", CP_END},
        {"pageup", CP_PAGE_UP}, {"pagedown", CP_PAGE_DOWN},
        {"up", CP_UP}, {"down", CP_DOWN}, {"left", CP_LEFT}, {"right", CP_RIGHT},
        {"clear", 0},
    };
    for (size_t i = 0; i < sizeof table / sizeof table[0]; ++i) {
        if (strcmp(table[i].name, name) == 0) { *cp = table[i].cp; return 1; }
    }
    if (name[0] == 'f' && name[1] >= '1' && name[1] <= '9' && name[2] == 0) {
        *cp = 0; return 1; /* f-keys handled by legacy tables only */
    }
    if (name[0] == 'f' && (strcmp(name, "f10") == 0 || strcmp(name, "f11") == 0
                           || strcmp(name, "f12") == 0)) {
        *cp = 0; return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public matching                                                     */
/* ------------------------------------------------------------------ */

int pangu_tui_matches_key(const uint8_t *data, size_t data_len, const char *key_id) {
    if (!data || data_len == 0 || !key_id) return 0;
    key_id_parts parts;
    if (!parse_key_id(key_id, &parts)) return 0;
    const char *key = parts.key;
    unsigned modifier = parts.modifiers;

    if (strcmp(key, "escape") == 0 || strcmp(key, "esc") == 0) {
        if (modifier != 0) return 0;
        return (data_len == 1 && data[0] == 0x1b)
            || matches_kitty_sequence(data, data_len, CP_ESCAPE, 0)
            || matches_modify_other_keys(data, data_len, CP_ESCAPE, 0);
    }
    if (strcmp(key, "space") == 0) {
        if (!g_kitty_active) {
            if (modifier == PANGU_KMOD_CTRL && data_len == 1 && data[0] == 0) return 1;
            if (modifier == PANGU_KMOD_ALT && data_len == 2 && data[0] == 0x1b && data[1] == ' ') return 1;
        }
        if (modifier == 0) {
            return (data_len == 1 && data[0] == ' ')
                || matches_kitty_sequence(data, data_len, CP_SPACE, 0)
                || matches_modify_other_keys(data, data_len, CP_SPACE, 0);
        }
        return matches_kitty_sequence(data, data_len, CP_SPACE, modifier)
            || matches_modify_other_keys(data, data_len, CP_SPACE, modifier);
    }
    if (strcmp(key, "tab") == 0) {
        if (modifier == PANGU_KMOD_SHIFT) {
            return (data_len == 3 && memcmp(data, "\x1b[Z", 3) == 0)
                || matches_kitty_sequence(data, data_len, CP_TAB, PANGU_KMOD_SHIFT)
                || matches_modify_other_keys(data, data_len, CP_TAB, PANGU_KMOD_SHIFT);
        }
        if (modifier == 0) {
            return (data_len == 1 && data[0] == '\t')
                || matches_kitty_sequence(data, data_len, CP_TAB, 0);
        }
        return matches_kitty_sequence(data, data_len, CP_TAB, modifier)
            || matches_modify_other_keys(data, data_len, CP_TAB, modifier);
    }
    if (strcmp(key, "enter") == 0 || strcmp(key, "return") == 0) {
        if (modifier == PANGU_KMOD_SHIFT) {
            if (matches_kitty_sequence(data, data_len, CP_ENTER, PANGU_KMOD_SHIFT)
                || matches_kitty_sequence(data, data_len, CP_KP_ENTER, PANGU_KMOD_SHIFT)) return 1;
            if (matches_modify_other_keys(data, data_len, CP_ENTER, PANGU_KMOD_SHIFT)) return 1;
            if (g_kitty_active) {
                return (data_len == 2 && data[0] == 0x1b && data[1] == '\r')
                    || (data_len == 1 && data[0] == '\n');
            }
            return 0;
        }
        if (modifier == PANGU_KMOD_ALT) {
            if (matches_kitty_sequence(data, data_len, CP_ENTER, PANGU_KMOD_ALT)
                || matches_kitty_sequence(data, data_len, CP_KP_ENTER, PANGU_KMOD_ALT)) return 1;
            if (matches_modify_other_keys(data, data_len, CP_ENTER, PANGU_KMOD_ALT)) return 1;
            if (!g_kitty_active) return data_len == 2 && data[0] == 0x1b && data[1] == '\r';
            return 0;
        }
        if (modifier == 0) {
            return (data_len == 1 && data[0] == '\r')
                || (!g_kitty_active && data_len == 1 && data[0] == '\n')
                || (data_len == 3 && memcmp(data, "\x1bOM", 3) == 0)
                || matches_kitty_sequence(data, data_len, CP_ENTER, 0)
                || matches_kitty_sequence(data, data_len, CP_KP_ENTER, 0);
        }
        return matches_kitty_sequence(data, data_len, CP_ENTER, modifier)
            || matches_kitty_sequence(data, data_len, CP_KP_ENTER, modifier)
            || matches_modify_other_keys(data, data_len, CP_ENTER, modifier);
    }
    if (strcmp(key, "backspace") == 0) {
        if (modifier == PANGU_KMOD_ALT) {
            if ((data_len == 2 && data[0] == 0x1b && (data[1] == 0x7f || data[1] == 0x08))) return 1;
            return matches_kitty_sequence(data, data_len, CP_BACKSPACE, PANGU_KMOD_ALT)
                || matches_modify_other_keys(data, data_len, CP_BACKSPACE, PANGU_KMOD_ALT);
        }
        if (modifier == PANGU_KMOD_CTRL) {
            if (matches_raw_backspace(data, data_len, PANGU_KMOD_CTRL)) return 1;
            return matches_kitty_sequence(data, data_len, CP_BACKSPACE, PANGU_KMOD_CTRL)
                || matches_modify_other_keys(data, data_len, CP_BACKSPACE, PANGU_KMOD_CTRL);
        }
        if (modifier == 0) {
            return matches_raw_backspace(data, data_len, 0)
                || matches_kitty_sequence(data, data_len, CP_BACKSPACE, 0)
                || matches_modify_other_keys(data, data_len, CP_BACKSPACE, 0);
        }
        return matches_kitty_sequence(data, data_len, CP_BACKSPACE, modifier)
            || matches_modify_other_keys(data, data_len, CP_BACKSPACE, modifier);
    }

    struct { const char *name; unsigned cp; } functional[] = {
        {"insert", CP_INSERT}, {"delete", CP_DELETE},
        {"home", CP_HOME}, {"end", CP_END},
        {"pageup", CP_PAGE_UP}, {"pagedown", CP_PAGE_DOWN},
        {"up", CP_UP}, {"down", CP_DOWN}, {"left", CP_LEFT}, {"right", CP_RIGHT},
    };
    for (size_t i = 0; i < sizeof functional / sizeof functional[0]; ++i) {
        if (strcmp(key, functional[i].name) != 0) continue;
        const char *id = legacy_id(data, data_len);
        if(g_kitty_active && data_len==2 && data[0]==27) id=NULL;
        char expected[32];
        if (modifier == 0) {
            if (id && strlen(id)==strlen(functional[i].name) && (strcmp(id, functional[i].name) == 0 || (strcmp(id,"pageUp")==0 && strcmp(key,"pageup")==0) || (strcmp(id,"pageDown")==0 && strcmp(key,"pagedown")==0))) return 1;
            return matches_kitty_sequence(data, data_len, functional[i].cp, 0);
        }
        size_t len = format_key(expected, sizeof expected, functional[i].cp, modifier, 0);
        if (len && id && strcmp(id, expected) == 0) return 1;
        /* xterm modifier forms like \x1b[1;5D */
        if (strcmp(functional[i].name, "left") == 0 && modifier == PANGU_KMOD_CTRL
            && data_len == 6 && memcmp(data, "\x1b[1;5D", 6) == 0) return 1;
        if (strcmp(functional[i].name, "right") == 0 && modifier == PANGU_KMOD_CTRL
            && data_len == 6 && memcmp(data, "\x1b[1;5C", 6) == 0) return 1;
        if (strcmp(functional[i].name, "left") == 0 && modifier == PANGU_KMOD_ALT
            && data_len == 6 && memcmp(data, "\x1b[1;3D", 6) == 0) return 1;
        if (strcmp(functional[i].name, "right") == 0 && modifier == PANGU_KMOD_ALT
            && data_len == 6 && memcmp(data, "\x1b[1;3C", 6) == 0) return 1;
        if ((strcmp(functional[i].name, "up") == 0 || strcmp(functional[i].name, "down") == 0)
            && modifier == PANGU_KMOD_ALT && !g_kitty_active) {
            char alt_key = strcmp(functional[i].name, "up") == 0 ? 'p' : 'n';
            if (data_len == 2 && data[0] == 0x1b && data[1] == (uint8_t)alt_key) return 1;
        }
        if ((strcmp(functional[i].name, "left") == 0 || strcmp(functional[i].name, "right") == 0)
            && modifier == PANGU_KMOD_ALT && !g_kitty_active) {
            char alt_key = strcmp(functional[i].name, "left") == 0 ? 'b' : 'f';
            if (data_len == 2 && data[0] == 0x1b && data[1] == (uint8_t)alt_key) return 1;
        }
        return matches_kitty_sequence(data, data_len, functional[i].cp, modifier);
    }
    if (strcmp(key, "clear") == 0) {
        const char *id = legacy_id(data, data_len);
        if (modifier == 0) return id && strcmp(id, "clear") == 0;
        char expected[32];
        size_t len = format_key(expected, sizeof expected, ' ', modifier, 0);
        (void)len;
        if (modifier == PANGU_KMOD_CTRL) return id && strcmp(id, "ctrl+clear") == 0;
        if (modifier == PANGU_KMOD_SHIFT) return id && strcmp(id, "shift+clear") == 0;
        return 0;
    }
    if (key[0] == 'f' && (strcmp(key, "f1") == 0 || strcmp(key, "f2") == 0
        || strcmp(key, "f3") == 0 || strcmp(key, "f4") == 0 || strcmp(key, "f5") == 0
        || strcmp(key, "f6") == 0 || strcmp(key, "f7") == 0 || strcmp(key, "f8") == 0
        || strcmp(key, "f9") == 0 || strcmp(key, "f10") == 0 || strcmp(key, "f11") == 0
        || strcmp(key, "f12") == 0)) {
        if (modifier != 0) return 0;
        char expected[16];
        snprintf(expected, sizeof expected, "%s", key);
        char alt_form[16];
        snprintf(alt_form, sizeof alt_form, "f%s", key + 1);
        const char *id = legacy_id(data, data_len);
        return id && strcmp(id, expected) == 0;
    }

    /* Single letter/digit/symbol keys. */
    if (key[1] == 0) {
        unsigned cp = (unsigned)(unsigned char)key[0];
        int is_letter = cp >= 'a' && cp <= 'z';
        int is_digit = cp >= '0' && cp <= '9';
        int is_symbol = is_symbol_key(cp);
        if (!is_letter && !is_digit && !is_symbol) return 0;
        int raw_ctrl = raw_ctrl_char(cp);

        if (modifier == (PANGU_KMOD_CTRL | PANGU_KMOD_ALT) && !g_kitty_active && raw_ctrl) {
            if (data_len == 2 && data[0] == 0x1b && data[1] == (uint8_t)raw_ctrl) return 1;
        }
        if (modifier == PANGU_KMOD_ALT && !g_kitty_active) {
            if (data_len == 2 && data[0] == 0x1b && data[1] == (uint8_t)cp) return 1;
        }
        if (modifier == PANGU_KMOD_CTRL) {
            if (raw_ctrl && data_len == 1 && data[0] == (uint8_t)raw_ctrl) return 1;
            return matches_kitty_sequence(data, data_len, cp, PANGU_KMOD_CTRL)
                || matches_printable_modify_other_keys(data, data_len, cp, PANGU_KMOD_CTRL);
        }
        if (modifier == (PANGU_KMOD_SHIFT | PANGU_KMOD_CTRL)) {
            return matches_kitty_sequence(data, data_len, cp, PANGU_KMOD_SHIFT | PANGU_KMOD_CTRL)
                || matches_printable_modify_other_keys(data, data_len, cp,
                                                       PANGU_KMOD_SHIFT | PANGU_KMOD_CTRL);
        }
        if (modifier == PANGU_KMOD_SHIFT) {
            if (is_letter && data_len == 1 && data[0] == (uint8_t)(cp - 32)) return 1;
            return matches_kitty_sequence(data, data_len, cp, PANGU_KMOD_SHIFT)
                || matches_printable_modify_other_keys(data, data_len, cp, PANGU_KMOD_SHIFT);
        }
        if (modifier != 0) {
            return matches_kitty_sequence(data, data_len, cp, modifier)
                || matches_printable_modify_other_keys(data, data_len, cp, modifier);
        }
        return (data_len == 1 && data[0] == (uint8_t)cp)
            || matches_kitty_sequence(data, data_len, cp, 0)
            || matches_printable_modify_other_keys(data, data_len, cp, 0);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public parsing                                                      */
/* ------------------------------------------------------------------ */

static void set_raw(pangu_tui_key *out, const uint8_t *data, size_t n) {
    memset(out, 0, sizeof *out);
    if (n > sizeof out->data) n = sizeof out->data;
    memcpy(out->data, data, n);
    out->data_len = (uint8_t)n;
}

int pangu_tui_parse_key(const uint8_t *data, size_t data_len, pangu_tui_key *out) {
    if (!data || data_len == 0 || !out) return 0;
    memset(out, 0, sizeof *out);
    set_raw(out, data, data_len);

    kitty_sequence kitty;
    if (parse_kitty(data, data_len, &kitty)) {
        unsigned cp = kitty.codepoint;
        unsigned mod = kitty.modifier;
        /* Logical shifted symbol: the produced character names the key. */
        if ((mod & PANGU_KMOD_SHIFT) && kitty.shifted_key
            && is_symbol_key(normalize_functional(kitty.shifted_key))) {
            cp = kitty.shifted_key;
            mod &= ~(unsigned)PANGU_KMOD_SHIFT;
        }
        char id[32];
        size_t len = format_key(id, sizeof id, cp, mod, kitty.base_layout_key);
        if (len) {
            cp = normalize_shifted_letter(normalize_functional(cp), mod);
            if (cp > 127 && cp < 0xFFFFF000u && kitty.base_layout_key) cp = kitty.base_layout_key;
            out->codepoint = cp;
            out->modifiers = mod & ~(unsigned)LOCK_MASK;
            out->event_type = kitty.event_type;
            return 1;
        }
        return 0;
    }

    kitty_sequence modify;
    if (parse_modify_other_keys(data, data_len, &modify)) {
        unsigned cp = modify.codepoint;
        unsigned mod = modify.modifier;
        if ((mod & PANGU_KMOD_SHIFT) && is_symbol_key(cp)) {
            mod &= ~(unsigned)PANGU_KMOD_SHIFT;
        }
        char id[32];
        size_t len = format_key(id, sizeof id, cp, mod, 0);
        if (len) {
            cp = normalize_shifted_letter(normalize_functional(cp), mod);
            if (cp > 127 && cp < 0xFFFFF000u && kitty.base_layout_key) cp = kitty.base_layout_key;
            out->codepoint = cp;
            out->modifiers = mod & ~(unsigned)LOCK_MASK;
            out->event_type = PANGU_KEY_PRESS;
            return 1;
        }
        return 0;
    }

    /* Mode-aware legacy: with Kitty active, \x1b\r and \n are shift+enter. */
    if (g_kitty_active && data_len == 1 && data[0] == '\n') {
        out->codepoint = CP_ENTER;
        out->modifiers = PANGU_KMOD_SHIFT;
        return 1;
    }
    if (g_kitty_active && data_len == 2 && data[0] == 0x1b && data[1] == '\r') {
        out->codepoint = CP_ENTER;
        out->modifiers = PANGU_KMOD_SHIFT;
        return 1;
    }

    const char *id = legacy_id(data, data_len);
    if (id) {
        /* Resolve the id back into codepoint + modifiers. */
        key_id_parts parts;
        if (!parse_key_id(id, &parts)) return 0;
        unsigned cp = 0;
        if (!key_name_codepoint(parts.key, &cp)) {
            if (parts.key[1] == 0) cp = (unsigned char)parts.key[0];
            else return 0;
        }
        if (cp) {
            out->codepoint = cp;
            out->modifiers = parts.modifiers;
            out->event_type = PANGU_KEY_PRESS;
            return 1;
        }
        /* f-keys: only matchable, not parseable to a codepoint. */
        out->modifiers = parts.modifiers;
        return 1;
    }

    if (data_len == 1) {
        uint8_t c = data[0];
        if (c == 0x1b) { out->codepoint = CP_ESCAPE; return 1; }
        if (c == 0x1c) { out->codepoint = '\\'; out->modifiers = PANGU_KMOD_CTRL; return 1; }
        if (c == 0x1d) { out->codepoint = ']'; out->modifiers = PANGU_KMOD_CTRL; return 1; }
        if (c == 0x1f) { out->codepoint = '-'; out->modifiers = PANGU_KMOD_CTRL; return 1; }
        if (c == '\t') { out->codepoint = CP_TAB; return 1; }
        if (c == '\r' || (!g_kitty_active && c == '\n')) { out->codepoint = CP_ENTER; return 1; }
        if (c == 0) { out->codepoint = CP_SPACE; out->modifiers = PANGU_KMOD_CTRL; return 1; }
        if (c == ' ') { out->codepoint = CP_SPACE; return 1; }
        if (c == 0x7f) { out->codepoint = CP_BACKSPACE; return 1; }
        if (c == 0x08) {
            out->codepoint = CP_BACKSPACE;
            if (g_windows_terminal) out->modifiers = PANGU_KMOD_CTRL;
            return 1;
        }
        if (c >= 1 && c <= 26) {
            out->codepoint = (unsigned)(c + 96); /* a..z */
            out->modifiers = PANGU_KMOD_CTRL;
            return 1;
        }
        if (c >= 32 && c <= 126) { out->codepoint = c; return 1; }
        return 0;
    }

    /* ESC + single char: legacy alt-prefixed input (kitty inactive). */
    if (!g_kitty_active && data_len == 2 && data[0] == 0x1b) {
        uint8_t c = data[1];
        if (c == ' ') {out->codepoint=CP_SPACE;out->modifiers=PANGU_KMOD_ALT;return 1;}
        if (c >= 28 && c <= 31) {out->codepoint=c==28?'\\':c==29?']':'-';out->modifiers=PANGU_KMOD_CTRL|PANGU_KMOD_ALT;return 1;}
        if (c >= 1 && c <= 26) {
            out->codepoint = (unsigned)(c + 96);
            out->modifiers = PANGU_KMOD_CTRL | PANGU_KMOD_ALT;
            return 1;
        }
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || is_symbol_key(c)) {
            out->codepoint = c;
            out->modifiers = PANGU_KMOD_ALT;
            return 1;
        }
    }
    if (data_len == 2 && data[0] == 0x1b && (data[1] == 0x7f || data[1] == 0x08)) {
        out->codepoint = CP_BACKSPACE;
        out->modifiers = PANGU_KMOD_ALT;
        return 1;
    }
    if (data_len == 2 && data[0] == 0x1b && data[1] == 0x1b) {
        out->codepoint = '[';
        out->modifiers = PANGU_KMOD_CTRL | PANGU_KMOD_ALT;
        return 1;
    }

    /* Multi-byte printable UTF-8 (one grapheme per event). */
    if (data[0] != 0x1b) {
        unsigned cp = 0;
        size_t used = decode_utf8(data, data_len, &cp);
        if (used == data_len && cp >= 32) {
            out->codepoint = cp;
            return 1;
        }
    }
    return 0;
}

size_t pangu_tui_decode_printable(const uint8_t *data, size_t data_len, char out[4]) {
    if (!data || data_len == 0) return 0;
    kitty_sequence parsed;
    if (parse_csi_u(data, data_len, &parsed)) {
        unsigned effective = parsed.modifier & ~(unsigned)LOCK_MASK;
        unsigned allowed = PANGU_KMOD_SHIFT | (unsigned)LOCK_MASK;
        if (effective & ~allowed) return 0;
        if (effective & (PANGU_KMOD_ALT | PANGU_KMOD_CTRL)) return 0;
        unsigned cp = parsed.codepoint;
        if ((effective & PANGU_KMOD_SHIFT) && parsed.shifted_key) cp = parsed.shifted_key;
        cp = normalize_functional(cp);
        if (cp < 32 || cp > 0x10ffff || cp == 127) return 0;
        return encode_utf8(out, cp);
    }
    if (parse_modify_other_keys(data, data_len, &parsed)) {
        unsigned effective = parsed.modifier & ~(unsigned)LOCK_MASK;
        if (effective & ~(unsigned)PANGU_KMOD_SHIFT) return 0;
        if (parsed.codepoint < 32) return 0;
        return encode_utf8(out, parsed.codepoint);
    }
    return 0;
}
