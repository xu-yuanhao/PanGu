"""Real PTY: history/search keys and completion FIFO reach chat submission."""
import os
import sys
from session_pty import Terminal

t = Terminal('unused', 'unused', executable=os.path.abspath(sys.argv[1]))
def accepted(number, text):
    t.until(f'Accepted[{number}]: {text}\r\n'.encode())
def paste(text):
    return b'\x1b[200~' + text.encode() + b'\x1b[201~'
try:
    t.until(b'PanGu |')
    t.send(paste('  中文\né Straße  ') + b'\r')
    accepted(1, '  中文|é Straße  ')
    t.send(b'newer\rnewer\r')
    accepted(2, 'newer')
    accepted(3, 'newer')
    # Dedicated navigation skips adjacent duplicates and restores multiline draft.
    t.send(paste('draft\n草稿') + b'\x10\x10\x0e\x0e!\r')
    accepted(4, 'draft|草稿!')
    # Empty query is idle; query and Unicode-folded preview are visible.
    t.send(b'kept\x12STRASSE')
    t.until(b'Search: STRASSE')
    # Alt+Enter/Tab cannot submit the preview; Ctrl+C restores draft without quit.
    t.send(b'\x1b\r\t\x03\r')
    accepted(5, 'kept')
    # Accept then edit. No hidden submission is allowed (sequence numbers assert).
    t.send(b'\x12STRASSE\r!\r')
    accepted(6, '中文|é Straße!')
    # No-match Enter stays in search; Escape restores saved draft.
    t.send(b'safe\x12missing\r')
    t.until(b'No match: missing')
    t.send(b'\x1b[27u\r')
    accepted(7, 'safe')
    # Unicode grapheme backspace, pasted multiline query, older/newer traversal.
    t.send(b'\x12' + paste('中文\né') + b'\x7f')
    t.until('Search: 中文'.encode())
    t.send(b'\x12\x13\r?\r')
    accepted(8, '中文|é Straße!?')
    # Both Tabs, Enter, search/accept/edit arrive in one terminal write.
    t.send(b'/rel\t\t\r\x12/reload\r!\r')
    accepted(9, '/reload')
    accepted(10, '/reload!')
    t.send(b'\x04')
    t.proc.wait(timeout=5)
    assert t.proc.returncode == 0
    import termios
    assert termios.tcgetattr(t.slave) == t.saved
finally:
    t.dispose()
print('PTY history/search: draft, duplicates, multiline, Unicode, accept/cancel, FIFO, restore passed')
