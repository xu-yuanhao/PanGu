"""Real presentation plugin PTY: tree, branch replay, reopen, model summary cancel/reuse."""
import os
from pathlib import Path
import pty
import select
import subprocess
import sys
import tempfile
import termios
import time

exe = str(Path(sys.argv[1]).resolve())
from session_pty import Terminal as BaseTerminal

def Terminal(path, mode, extra=()):
    return BaseTerminal(path, mode, extra, executable=exe)

with tempfile.TemporaryDirectory() as d:
    path = Path(d)/'tree.jsonl'
    t = Terminal(path, 'create')
    try:
        t.until(b'Current leaf: 0')
        for text in (b'alpha', b'old-branch'):
            t.send(text+b'\r'); t.until(b'Tokens:')
        t.send(b'/tree\r'); t.until(b'* id=4 parent=3')
        t.send(b'/branch 2\r'); t.until(b'Current leaf: 2')
        replay = t.until(b'Assistant: alpha')
        assert b'old-branch' not in replay
        t.send(b'new-branch\r'); t.until(b'Tokens:')
        t.send(b'/switch 4\r'); t.until(b'Current leaf: 4')
        replay = t.until(b'Assistant: old-branch')
        assert b'new-branch' not in replay
        t.send(b'/switch 6\r'); t.until(b'Current leaf: 6')
        t.until(b'Assistant: new-branch')
        t.send(b'/compact 0\r'); t.until(b'Compacting context')
        # Cancel + immediate input must wait for compaction return, then run.
        t.send(b'\x03after-cancel\r')
        t.until(b'Compaction cancelled; context unchanged')
        t.until(b'Assistant: after-cancel'); t.until(b'Tokens:')
        t.send(b'/compact 0\r'); t.until(b'Compaction complete')
        t.until(b'fixture model summary')
        t.send(b'slow\r')
        # Provider polls while busy: mutation must be diagnosed, never enqueued.
        time.sleep(.08)
        t.send(b'/switch 0\r')
        t.until(b'Session command requires idle')
        t.send(b'\x03')
        t.send(b'/usage\r')
        t.until(b'Archived usage:')
        t.send(b'/usage\r'); t.until(b'Archived usage:')
        t.send(b'/handoff transfer\r'); t.until(b'You: transfer')
        t.send(b'/reset\r'); t.until(b'Current leaf:')
        t.send(b'/switch 4\r'); t.until(b'Current leaf: 4'); t.until(b'Assistant: old-branch')
        t.close()
    finally:
        t.dispose()
    t = Terminal(path, 'open')
    try:
        t.until(b'Current leaf: 4')
        replay = t.until(b'Assistant: old-branch')
        assert b'new-branch' not in replay and b'fixture model summary' not in replay
        t.send(b'/tree\r'); t.until(b'compaction')
        t.close()
    finally:
        t.dispose()
    # Direct plugin install exercises its own model lease fallback, then
    # missing-provider and shorter ABI1 tables without app pre-resolution.
    for mode in ('direct', 'missing', 'legacy', 'legacy-config', 'oldprefix', 'history-prefix'):
        t = Terminal(path, 'open', (mode,))
        try:
            t.until(b'Assistant: old-branch')
            t.send(b'/compact 0\r')
            if mode in ('direct', 'legacy-config', 'oldprefix', 'history-prefix'):
                t.until(b'Compaction complete')
                t.until(b'fixture model summary')
                t.send(b'/switch 4\r'); t.until(b'Assistant: old-branch')
            elif mode == 'missing':
                t.until(b'Compaction requires an available pangu.model provider')
            else:
                t.until(b'Session service does not provide this command capability')
                t.send(b'/tree\r'); t.until(b'Session service does not provide this command capability')
            t.send(b'/reload\r'); t.until(b'Resource reload is unavailable')
            t.close()
        finally:
            t.dispose()
    # Commands are UI operations, never user messages sent to the model.
    saved = path.read_text()
    for cmd in ('/tree', '/switch', '/branch', '/compact', '/usage', '/reset', '/handoff'):
        assert cmd not in saved, cmd
print('PTY session: tree/switch/branch/reset/handoff/usage, reopen, isolated history, model compact cancel/reuse passed')
