#ifndef PANGU_TUI_KEYS_H
#define PANGU_TUI_KEYS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Key input parsing ported from Pi packages/tui/src/keys.ts, snapshot
 * d5b3de22. Input is a UTF-8 byte sequence exactly as delivered by the
 * terminal for one input event. All functions are pure and thread-safe;
 * the Kitty-protocol mode is the only shared state and is owned by the
 * terminal layer. */

/* Bitmask of key modifiers. */
enum {
    PANGU_KMOD_SHIFT = 1,
    PANGU_KMOD_ALT = 2,
    PANGU_KMOD_CTRL = 4,
    PANGU_KMOD_SUPER = 8
};

/* Press/repeat/release event types (Kitty protocol flag 2). */
typedef enum {
    PANGU_KEY_PRESS = 1,
    PANGU_KEY_REPEAT = 2,
    PANGU_KEY_RELEASE = 3
} pangu_key_event_type;

typedef struct {
    /* Normalized base key: ASCII printable, one of pangu_tui_key_name(), or
     * PANGU_KEY_NONE. The struct is zeroed when a sequence is not
     * recognized; data and data_len then carry the raw input. */
    uint32_t codepoint;
    /* Modifier bitmask (PANGU_KMOD_*), lock bits stripped. */
    unsigned modifiers;
    pangu_key_event_type event_type;
    char data[32];
    uint8_t data_len;
} pangu_tui_key;

#define PANGU_KEY_NONE 0

/* Global Kitty-protocol state: while active, ambiguous legacy sequences are
 * interpreted as custom terminal mappings. Set by the terminal layer after
 * protocol negotiation. */
void pangu_tui_keys_set_kitty(int active);
int pangu_tui_keys_kitty(void);

/* Windows Terminal heuristic for the ambiguous raw 0x08 backspace byte. */
void pangu_tui_keys_set_windows_terminal(int is_windows_terminal);

/* Parse one input event. Returns 1 when the sequence was recognized as a
 * key event (including printable text), 0 when it should be dropped or is
 * handled by another layer (mouse/paste/replies are filtered upstream).
 * data_len must be > 0; data need not be NUL-terminated. */
int pangu_tui_parse_key(const uint8_t *data, size_t data_len, pangu_tui_key *out);

/* Match raw input against a key identifier ("escape", "ctrl+c",
 * "shift+enter", "alt+left", "+", "1", ...). Empty or malformed key ids
 * never match. Uses the global Kitty state. */
int pangu_tui_matches_key(const uint8_t *data, size_t data_len, const char *key_id);

/* Decode a Kitty CSI-u or modifyOtherKeys sequence into a printable
 * character (plain or Shift only). Returns the UTF-8 length written to
 * out (max 4 bytes), or 0 when the input is not a printable key event. */
size_t pangu_tui_decode_printable(const uint8_t *data, size_t data_len, char out[4]);

#ifdef __cplusplus
}
#endif
#endif
