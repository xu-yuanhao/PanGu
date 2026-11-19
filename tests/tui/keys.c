/* Key matching/parsing tests ported from Pi packages/tui/test/keys.test.ts,
 * snapshot d5b3de22. */
#include <pangu/tui/keys.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static int match(const char *data, const char *key_id) {
    return pangu_tui_matches_key((const uint8_t *)data, (*data?strlen(data):1), key_id);
}

static int parse_is(const char *data, const char *expected_id) {
    pangu_tui_key key;
    int recognized = pangu_tui_parse_key((const uint8_t *)data, (*data?strlen(data):1), &key);
    if (!recognized) return expected_id == NULL;
    char id[48];
    unsigned mods = key.modifiers;
    size_t used = 0;
    if (mods & PANGU_KMOD_SHIFT) { memcpy(id + used, "shift+", 6); used += 6; }
    if (mods & PANGU_KMOD_CTRL) { memcpy(id + used, "ctrl+", 5); used += 5; }
    if (mods & PANGU_KMOD_ALT) { memcpy(id + used, "alt+", 4); used += 4; }
    if (mods & PANGU_KMOD_SUPER) { memcpy(id + used, "super+", 6); used += 6; }
    /* Codepoint to name. */
    const char *name = NULL;
    char single[2] = {0, 0};
    unsigned cp = key.codepoint;
    if (!cp) return expected_id == NULL;
    switch (cp) {
    case 0xFFFFF001u: name="up"; break;
    case 0xFFFFF002u: name="down"; break;
    case 0xFFFFF003u: name="right"; break;
    case 0xFFFFF004u: name="left"; break;
    case 0xFFFFF00Au: name="delete"; break;
    case 0xFFFFF00Bu: name="insert"; break;
    case 0xFFFFF00Cu: name="pageUp"; break;
    case 0xFFFFF00Du: name="pageDown"; break;
    case 0xFFFFF00Eu: name="home"; break;
    case 0xFFFFF00Fu: name="end"; break;
    case 27: name = "escape"; break;
    case 9: name = "tab"; break;
    case 13: name = "enter"; break;
    case 32: name = "space"; break;
    case 127: name = "backspace"; break;
    default:
        if (cp < 127) { single[0] = (char)cp; name = single; }
        else return expected_id == NULL;
    }
    size_t name_len = strlen(name);
    memcpy(id + used, name, name_len);
    id[used + name_len] = 0;
    return expected_id && strcmp(id, expected_id) == 0;
}

static void test_kitty_alternate_keys(void) {
    pangu_tui_keys_set_kitty(1);
    CHECK(match("\x1b[1089::99;5u", "ctrl+c"));
    CHECK(match("\x1b[1074::100;5u", "ctrl+d"));
    CHECK(match("\x1b[1103::122;5u", "ctrl+z"));
    CHECK(match("\x1b[1079::112;6u", "ctrl+shift+p"));
    CHECK(match("\x1b[99;5u", "ctrl+c"));
    CHECK(match("\x1b[1089::99;5u", "ctrl+d") == 0);
    CHECK(match("\x1b[1089::99;5u", "ctrl+shift+c") == 0);
    CHECK(match("\x1b[107;9u", "super+k"));
    CHECK(match("\x1b[13;9u", "super+enter"));
    CHECK(match("\x1b[107;13u", "ctrl+super+k"));
    CHECK(match("\x1b[107;14u", "ctrl+shift+super+k"));
    CHECK(match("\x1b[107;13u", "super+k") == 0);
    CHECK(match("\x1b[49u", "1"));
    CHECK(match("\x1b[49;5u", "ctrl+1"));
    CHECK(match("\x1b[49;5u", "ctrl+2") == 0);
    /* Keypad functional keys. */
    CHECK(match("\x1b[57400u", "1"));
    CHECK(match("\x1b[57410u", "/"));
    CHECK(match("\x1b[57417u", "left"));
    CHECK(match("\x1b[57426u", "delete"));
    /* Shifted symbols by produced character. */
    CHECK(match("\x1b[61:43;2u", "+"));
    CHECK(match("\x1b[61:43;2u", "shift+="));
    CHECK(match("\x1b[61:43;6u", "ctrl++"));
    /* Shifted key format and event types. */
    CHECK(match("\x1b[99:67:99;2u", "shift+c"));
    CHECK(match("\x1b[1089::99;5:3u", "ctrl+c"));
    CHECK(match("\x1b[1089:1057:99;6:2u", "ctrl+shift+c"));
    /* Dvorak-style layouts: codepoint is authoritative. */
    CHECK(match("\x1b[107::118;5u", "ctrl+k"));
    CHECK(match("\x1b[107::118;5u", "ctrl+v") == 0);
    CHECK(match("\x1b[47::91;5u", "ctrl+/"));
    CHECK(match("\x1b[47::91;5u", "ctrl+[") == 0);
    pangu_tui_keys_set_kitty(0);
}

static void test_modify_other_keys(void) {
    pangu_tui_keys_set_kitty(0);
    CHECK(match("\x1b[27;5;99~", "ctrl+c"));
    CHECK(match("\x1b[27;5;100~", "ctrl+d"));
    CHECK(match("\x1b[27;5;122~", "ctrl+z"));
    CHECK(match("\x1b[27;5;13~", "ctrl+enter"));
    CHECK(match("\x1b[27;2;13~", "shift+enter"));
    CHECK(match("\x1b[27;3;13~", "alt+enter"));
    CHECK(match("\x1b[27;2;9~", "shift+tab"));
    CHECK(match("\x1b[27;5;9~", "ctrl+tab"));
    CHECK(match("\x1b[27;3;9~", "alt+tab"));
    CHECK(match("\x1b[27;1;127~", "backspace"));
    CHECK(match("\x1b[27;5;127~", "ctrl+backspace"));
    CHECK(match("\x1b[27;3;127~", "alt+backspace"));
    CHECK(match("\x1b[27;1;27~", "escape"));
    CHECK(match("\x1b[27;1;32~", "space"));
    CHECK(match("\x1b[27;5;32~", "ctrl+space"));
    CHECK(match("\x1b[27;5;47~", "ctrl+/"));
    CHECK(match("\x1b[27;6;43~", "ctrl++"));
    CHECK(match("\x1b[27;5;49~", "ctrl+1"));
    CHECK(match("\x1b[27;2;49~", "shift+1"));
    CHECK(match("\x1b[27;2;69~", "shift+e"));
    CHECK(match("\x1b[27;6;69~", "ctrl+shift+e"));
    CHECK(match("\x1b[104;7u", "ctrl+alt+h"));
    CHECK(match("\x1b[27;7;104~", "ctrl+alt+h"));
}

static void test_legacy_keys(void) {
    pangu_tui_keys_set_kitty(0);
    CHECK(match("\x03", "ctrl+c"));
    CHECK(match("\x04", "ctrl+d"));
    CHECK(match("\x1b", "escape"));
    CHECK(match("\n", "enter"));
    CHECK(match("\x00", "ctrl+space"));
    CHECK(match("\x1c", "ctrl+\\"));
    CHECK(match("\x1d", "ctrl+]"));
    CHECK(match("\x1f", "ctrl+_"));
    CHECK(match("\x1f", "ctrl+-"));
    CHECK(match("\x1b\x1b", "ctrl+alt+["));
    CHECK(match("\x1b\x1c", "ctrl+alt+\\"));
    CHECK(match("\x1b\x1d", "ctrl+alt+]"));
    CHECK(match("\x1b\x1f", "ctrl+alt+_"));
    CHECK(match("\x1b\x1f", "ctrl+alt+-"));

    /* Raw 0x08 ambiguity outside Windows Terminal. */
    pangu_tui_keys_set_windows_terminal(0);
    CHECK(match("\x7f", "backspace"));
    CHECK(match("\x7f", "ctrl+backspace") == 0);
    CHECK(match("\x08", "backspace"));
    CHECK(match("\x08", "ctrl+backspace") == 0);

    /* Windows Terminal local session. */
    pangu_tui_keys_set_windows_terminal(1);
    CHECK(match("\x08", "ctrl+backspace"));
    CHECK(match("\x08", "backspace") == 0);
    pangu_tui_keys_set_windows_terminal(0);

    /* Alt-prefixed input, kitty inactive. */
    CHECK(match("\x1b ", "alt+space"));
    CHECK(match("\x1b\b", "alt+backspace"));
    CHECK(match("\x1b\x03", "ctrl+alt+c"));
    CHECK(match("\x1b" "B", "alt+left"));
    CHECK(match("\x1b" "F", "alt+right"));
    CHECK(match("\x1b" "a", "alt+a"));
    CHECK(match("\x1b" "1", "alt+1"));
    CHECK(match("\x1b,", "alt+,"));
    CHECK(match("\x1by", "alt+y"));

    /* With kitty active, ambiguous legacy alt input is not recognized. */
    pangu_tui_keys_set_kitty(1);
    CHECK(match("\x1b ", "alt+space") == 0);
    CHECK(match("\x1b\b", "alt+backspace"));
    CHECK(match("\x1b\x03", "ctrl+alt+c") == 0);
    CHECK(match("\x1b" "B", "alt+left") == 0);
    CHECK(match("\x1b" "a", "alt+a") == 0);
    CHECK(match("\n", "shift+enter"));
    CHECK(match("\n", "enter") == 0);
    pangu_tui_keys_set_kitty(0);

    /* Arrows, SS3, function keys, rxvt modifier sequences. */
    CHECK(match("\x1b[A", "up"));
    CHECK(match("\x1b[B", "down"));
    CHECK(match("\x1b[C", "right"));
    CHECK(match("\x1b[D", "left"));
    CHECK(match("\x1bOA", "up"));
    CHECK(match("\x1bOB", "down"));
    CHECK(match("\x1bOC", "right"));
    CHECK(match("\x1bOD", "left"));
    CHECK(match("\x1bOH", "home"));
    CHECK(match("\x1bOF", "end"));
    CHECK(match("\x1b[1;5H", "ctrl+home"));
    CHECK(match("\x1b[1;5F", "ctrl+end"));
    CHECK(match("\x1b[5;5~", "ctrl+pageUp"));
    CHECK(match("\x1b[6;5~", "ctrl+pageDown"));
    CHECK(match("\x1bOP", "f1"));
    CHECK(match("\x1b[24~", "f12"));
    CHECK(match("\x1b[E", "clear"));
    CHECK(match("\x1bp", "alt+up"));
    CHECK(match("\x1bp", "up") == 0);
    CHECK(match("\x1b[a", "shift+up"));
    CHECK(match("\x1bOa", "ctrl+up"));
    CHECK(match("\x1b[2$", "shift+insert"));
    CHECK(match("\x1b[2^", "ctrl+insert"));
    CHECK(match("\x1b[7$", "shift+home"));
}

static void test_parse_key(void) {
    pangu_tui_keys_set_kitty(1);
    CHECK(parse_is("\x1b[1089::99;5u", "ctrl+c"));
    CHECK(parse_is("\x1b[107::118;5u", "ctrl+k"));
    CHECK(parse_is("\x1b[47::91;5u", "ctrl+/"));
    CHECK(parse_is("\x1b[99;5u", "ctrl+c"));
    CHECK(parse_is("\x1b[69;2u", "shift+e"));
    CHECK(parse_is("\x1b[107;9u", "super+k"));
    CHECK(parse_is("\x1b[107;13u", "ctrl+super+k"));
    CHECK(parse_is("\x1b[107;14u", "shift+ctrl+super+k"));
    CHECK(parse_is("\x1b[49u", "1"));
    CHECK(parse_is("\x1b[49;5u", "ctrl+1"));
    CHECK(parse_is("\x1b[61:43;2u", "+"));
    CHECK(parse_is("\x1b[61:43;6u", "ctrl++"));
    CHECK(parse_is("\x1b[57399u", "0"));
    CHECK(parse_is("\x1b[57409u", "."));
    CHECK(parse_is("\x1b[57413u", "+"));
    CHECK(parse_is("\x1b[57416u", ","));
    CHECK(parse_is("\x1b[57417u", "left"));
    CHECK(parse_is("\x1b[57418u", "right"));
    CHECK(parse_is("\x1b[57419u", "up"));
    CHECK(parse_is("\x1b[57420u", "down"));
    CHECK(parse_is("\x1b[57421u", "pageUp"));
    CHECK(parse_is("\x1b[57422u", "pageDown"));
    CHECK(parse_is("\x1b[57423u", "home"));
    CHECK(parse_is("\x1b[57424u", "end"));
    CHECK(parse_is("\x1b[57425u", "insert"));
    CHECK(parse_is("\x1b[57426u", "delete"));
    CHECK(parse_is("\x1b[99;17u", NULL)); /* unsupported modifier */
    pangu_tui_keys_set_kitty(0);

    CHECK(parse_is("\x03", "ctrl+c"));
    CHECK(parse_is("\x04", "ctrl+d"));
    CHECK(parse_is("\x1b", "escape"));
    CHECK(parse_is("\t", "tab"));
    CHECK(parse_is("\r", "enter"));
    CHECK(parse_is("\n", "enter"));
    CHECK(parse_is("\x00", "ctrl+space"));
    CHECK(parse_is(" ", "space"));
    CHECK(parse_is("1", "1"));
    CHECK(parse_is("+", "+"));
    CHECK(parse_is("\x1b[A", "up"));
    CHECK(parse_is("\x1b[B", "down"));
    CHECK(parse_is("\x1b[C", "right"));
    CHECK(parse_is("\x1b[D", "left"));
    CHECK(parse_is("\x1bOA", "up"));
    CHECK(parse_is("\x1bOH", "home"));
    CHECK(parse_is("\x1bOF", "end"));
    CHECK(parse_is("\x1b[2^", "ctrl+insert"));
    CHECK(parse_is("\x1bp", "alt+up"));
    CHECK(parse_is("\x1b[[5~", "pageUp"));
    CHECK(parse_is("\x1c", "ctrl+\\"));
    CHECK(parse_is("\x1d", "ctrl+]"));
    CHECK(parse_is("\x1f", "ctrl+-"));
    CHECK(parse_is("\x1b\x1b", "ctrl+alt+["));
    CHECK(parse_is("\x1b\x1c", "ctrl+alt+\\"));
    CHECK(parse_is("\x1b\x1d", "ctrl+alt+]"));
    CHECK(parse_is("\x1b\x1f", "ctrl+alt+-"));
    CHECK(parse_is("\x1b ", "alt+space"));
    CHECK(parse_is("\x1b\x03", "ctrl+alt+c"));
    CHECK(parse_is("\x1b" "B", "alt+left"));
    CHECK(parse_is("\x1b" "F", "alt+right"));
    CHECK(parse_is("\x1b" "a", "alt+a"));
    CHECK(parse_is("\x1b,", "alt+,"));
    CHECK(parse_is("\x1b[27;5;99~", "ctrl+c"));
    CHECK(parse_is("\x1b[27;2;69~", "shift+e"));
    CHECK(parse_is("\x1b[27;6;69~", "shift+ctrl+e"));
    CHECK(parse_is("\x1b[1;5H", "ctrl+home"));
    CHECK(parse_is("\x1b[5;5~", "ctrl+pageUp"));
}

static void test_decode_printable(void) {
    char out[4];
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57399u", 8, out) == 1 && out[0] == '0');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57409u", 8, out) == 1 && out[0] == '.');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57410u", 8, out) == 1 && out[0] == '/');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57411u", 8, out) == 1 && out[0] == '*');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57412u", 8, out) == 1 && out[0] == '-');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57413u", 8, out) == 1 && out[0] == '+');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57415u", 8, out) == 1 && out[0] == '=');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57416u", 8, out) == 1 && out[0] == ',');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[57417u", 8, out) == 0);
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[27;2;69~", 10, out) == 1 && out[0] == 'E');
    /* Non-ASCII codepoint (Ä = U+00C4). */
    size_t n = pangu_tui_decode_printable((const uint8_t *)"\x1b[27;2;196~", 11, out);
    CHECK(n == 2 && (unsigned char)out[0] == 0xC3 && (unsigned char)out[1] == 0x84);
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[27;2;32~", 10, out) == 1 && out[0] == ' ');
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[27;2;13~", 10, out) == 0);
    CHECK(pangu_tui_decode_printable((const uint8_t *)"\x1b[27;6;69~", 10, out) == 0);
}

int main(void) {
    test_kitty_alternate_keys();
    test_modify_other_keys();
    test_legacy_keys();
    test_parse_key();
    test_decode_printable();
    if (failures) {
        printf("keys: %d failures\n", failures);
        return 1;
    }
    printf("keys: all tests passed\n");
    return 0;
}
