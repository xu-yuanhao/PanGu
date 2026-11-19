"""Opt-in real model compaction only; synthetic history, no raw output saved.
Run with credentials loaded into environment via examples/agent/with-private.py.
"""
import os
from pathlib import Path
import sys
import tempfile
from session_pty import Terminal as BaseTerminal

class Terminal(BaseTerminal):
    def until(self, marker):
        try:
            return super().until(marker)
        except Exception:
            raise RuntimeError("live UI check failed (output withheld)") from None

exe = str(Path(sys.argv[1]).resolve())
if not os.environ.get('DEEPSEEK_API_KEY'):
    print('live compaction skipped: DEEPSEEK_API_KEY unavailable')
    sys.exit(77)
with tempfile.TemporaryDirectory() as d:
    path = Path(d)/'session.jsonl'
    t = Terminal(path, 'create', executable=exe)
    try:
        t.until(b'Current leaf: 0')
        for text in (b'Synthetic project: retain blue widget requirements.',
                     b'Synthetic next step: add widget tests.'):
            t.send(text+b'\r'); t.until(b'Tokens:')
        t.close()
    finally:
        t.dispose()
    before = path.read_bytes()
    t = Terminal(path, 'open', ('live',), executable=exe)
    try:
        t.until(b'Current leaf: 4')
        t.send(b'/compact 0\r'); t.until(b'Compacting context')
        t.send(b'\x03/usage\r')
        t.until(b'Compaction cancelled; context unchanged')
        t.until(b'Archived usage:')
        assert path.read_bytes() == before, 'cancelled summary mutated session'
        t.send(b'/compact 0\r')
        # Only the real summary request needs a longer network allowance.
        import time
        deadline = time.monotonic() + 90
        while b'Compaction complete' not in t.output[t.cursor:]:
            assert time.monotonic() < deadline, 'live compaction timeout (output withheld)'
            assert t.proc.poll() is None, 'live compaction process failed'
            import select
            if select.select([t.master], [], [], .05)[0]:
                t.output.extend(os.read(t.master, 65536))
        t.cursor = t.output.index(b'Compaction complete', t.cursor) + len(b'Compaction complete')
        t.until(b'Current leaf: 5')
        t.send(b'/usage\r'); t.until(b'Archived usage:')
        t.close()
    finally:
        t.dispose()
    assert path.read_bytes() != before
    key = os.environ['DEEPSEEK_API_KEY'].encode()
    assert key not in path.read_bytes(), 'credential persisted'
    t = Terminal(path, 'open', executable=exe)
    try:
        t.until(b'Current leaf: 5')
        t.send(b'/tree\r'); t.until(b'* id=5 parent=4 compaction')
        t.close()
    finally:
        t.dispose()
print('live compaction: owner-thread cancel/reuse, real summary commit/reopen, terminal restore, credential exclusion passed')
