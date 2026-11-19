"""Certified direct-session input -> startup editor history, JSONL and SQLite.

The fixture exclusively writes raw USER inputs through session.run. Ordinary
app/resource snapshots do not provide that provenance and are tested separately.
"""
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
from session_pty import Terminal

exe = str(Path(sys.argv[1]).resolve())
def snapshot(path, backend):
    if backend == 'jsonl':
        return json.loads(path.read_text().splitlines()[-1])
    with sqlite3.connect(path) as db:
        return json.loads(db.execute('SELECT snapshot FROM pangu_session WHERE id=1').fetchone()[0])
def users(saved):
    return [''.join(c['data'] for c in m['content'] if c['kind'] == 0)
            for m in saved['messages'] if m['role'] == 1]
def paste(s):
    return b'\x1b[200~' + s.encode() + b'\x1b[201~'
def ready(t):
    t.until(b'/tree /switch ID /branch ID /reset /handoff TEXT /compact [keep tokens] /usage')
def no_match(t, query):
    t.send(b'\x12' + query.encode())
    t.until(b'No match: ' + query.encode())
    t.send(b'\x1b[27u')

a = '  first 中文\nline é Straße  '
with tempfile.TemporaryDirectory(prefix='pangu-history 中文 ') as directory:
    for backend in ('jsonl', 'sqlite'):
        path = Path(directory) / (backend + '.session')
        extra = ('history-' + backend,)
        t = Terminal(path, 'create', extra, executable=exe)
        try:
            ready(t)
            initial = snapshot(path, backend)
            # Startup history is filled from two concatenated USER text blocks,
            # while transcript replay neither executes nor resubmits them.
            t.send(b'\x12seeded')
            t.until(b'Search: seeded')
            t.until('> seeded-中文'.encode())
            t.send(b'\x03')
            no_match(t, 'tool-only-sentinel')
            no_match(t, 'system-only-sentinel')
            assert snapshot(path, backend) == initial
            assert b'Model request ' not in t.output
            for i, text in enumerate((a, 'repeat', 'repeat'), 1):
                t.send(paste(text) + b'\r')
                t.until(f'Model request {i}'.encode())
                t.until(b'Tokens:')
            t.close()
        finally:
            t.dispose()
        saved = snapshot(path, backend)
        assert users(saved) == ['  seeded-中文  ', a, 'repeat', 'repeat'], users(saved)
        # The same legacy file has no recorded original provenance. Default
        # app must decline import rather than interpreting USER bodies as raw.
        t = Terminal(path, 'open', ('app-legacy-' + backend,), executable=exe)
        try:
            ready(t)
            no_match(t, 'seeded')
            no_match(t, 'STRASSE')
            t.close()
        finally:
            t.dispose()
        assert snapshot(path, backend) == saved
        # A fresh process reconstructs history, never the assistant/tool text.
        t = Terminal(path, 'open', extra, executable=exe)
        try:
            ready(t)
            t.send(paste('draft\n草稿é') + b'\x1b[1;2D\x10')
            t.until(b'> repeat')
            t.send(b'\x10')
            t.until('> first 中文'.encode())  # adjacent duplicate did not consume a slot
            t.send(b'\x10')
            t.until('> seeded-中文'.encode())
            t.send(b'\x0e\x0e\x0e')  # restore draft and grapheme selection
            no_match(t, 'assistant-only-sentinel')
            no_match(t, 'tool-only-sentinel')
            # Search accepts to edit; cancel returns to the complete draft.
            t.send(b'\x12STRASSE')
            t.until(b'Search: STRASSE')
            t.send(b'\r')
            # No implicit Enter submission, including no-match Enter.
            t.send(b'\x12not-in-history\r')
            t.until(b'No match: not-in-history')
            t.send(b'\x03')
            assert snapshot(path, backend) == saved
            assert b'Model request ' not in t.output
            # Clear accepted editable draft before exiting; no model mutation.
            t.send(b'\x01\x0b\x02\x01\x0b\x0b')
            t.close()
        finally:
            t.dispose()
        assert snapshot(path, backend) == saved
        # Independently verify cursor/selection survives nav + cancelled search,
        # then send exactly one newly edited input through the real model driver.
        t = Terminal(path, 'open', extra, executable=exe)
        try:
            ready(t)
            t.send(paste('draft\n草稿é') + b'\x1b[1;2D\x10\x10\x0e\x0e\x12missing')
            t.until(b'No match: missing')
            t.send(b'\x03!\r')
            t.until(b'Model request 1')
            t.until(b'Tokens:')
            now = snapshot(path, backend)
            assert users(now) == users(saved) + ['draft\n草稿!'], users(now)
            assert b'Model request 2' not in t.output
            # A recalled old input reaches the model only after explicit
            # accept, edit and a second Enter; it is a newly submitted prompt.
            t.send(b'\x12STRASSE\r!\r')
            t.until(b'Model request 2')
            t.until(b'Tokens:')
            expected = users(saved) + ['draft\n草稿!', 'first 中文\nline é Straße!']
            assert users(snapshot(path, backend)) == expected
            t.close()
        finally:
            t.dispose()
print('PTY persisted history: JSONL/SQLite create/open, raw USER text blocks, Unicode/multiline, draft/selection, dedup, role filtering, no replay execution passed')
