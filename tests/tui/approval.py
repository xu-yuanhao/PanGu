"""Actual app/model/tool -> durable asked -> TUI reply -> exact effect gating."""
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
def ready(t):
    t.until(b'/tree /switch ID /branch ID /reset /handoff TEXT /compact [keep tokens] /usage')
def question(t):
    t.until(b'Approval requested')
    t.until(b'approval-fixture')
    t.until(b'approval-call')
    t.until(b'Tool execution')
    t.until(b'Approval: y allow once | n reject')

with tempfile.TemporaryDirectory(prefix='pangu-tui-approval ') as root:
    for backend in ('jsonl', 'sqlite'):
        path = Path(root)/(backend+'.session')
        extra = ('approval-'+backend,)
        t = Terminal(path, 'create', extra, executable=exe)
        try:
            ready(t)
            for prompt, keys, effects, outcome in (
                (b'allow', b'y', 1, 0),
                (b'reject', b'n', 1, 1),
                (b'cancel', b'\x03', 1, 2),
                (b'allow-again', b'y', 2, 0)):
                t.send(prompt+b'\r')
                question(t)
                asked = snapshot(path, backend)
                # Every invocation, even same callId, needs its own live answer.
                t.send(b'\r\t\x1b[200~y\x1b[201~')
                # Enter/Tab/pasted y cannot grant; only a dedicated y key can.
                # The editor unit test also checks this synchronously.
                t.send(keys)
                t.until(b'[cancelled]' if outcome == 2 else b'Tokens:')
                effect = Path(str(path)+'.effects')
                assert (effect.read_bytes() if effect.exists() else b'') == b'effect\n' * effects
                saved = snapshot(path, backend)
                assert saved != asked
                audit = saved['approval_events']
                assert audit[-2]['type'] == 'asked'
                assert audit[-1]['type'] == 'decided'
                assert audit[-2]['id'] == audit[-1]['id']
                assert audit[-1]['outcome'] == ('allowed-once','rejected','cancelled')[outcome]
            t.close()
        finally:
            t.dispose()
        saved = snapshot(path, backend)
        # Audit is separate from transcript/history, never model input.
        users = [m['content'][0]['data'] for m in saved['messages'] if m['role']==1]
        assert users == ['allow','reject','cancel','allow-again'], users
        t = Terminal(path, 'open', extra, executable=exe)
        try:
            ready(t)
            assert b'Approval requested' not in t.output
            for query in (b'Reason', b'allow once', b'approval-fixture'):
                t.send(b'\x12'+query+b'\r')
                t.until(b'No match: '+query)
                t.send(b'\x03')
            t.close()
        finally:
            t.dispose()
        assert snapshot(path, backend) == saved
        # Reopen never reuses a previous allow. Escape followed by a late y must
        # cancel; a subsequent request still needs an explicit fresh answer.
        t = Terminal(path, 'open', extra, executable=exe)
        try:
            ready(t)
            t.send(b'reopened\r')
            question(t)
            t.send(b'\x1b')
            t.until(b'[cancelled]')
            t.send(b'y')
            t.send(b'\x01\x0bfresh\r')
            question(t)
            t.send(b'n')
            t.until(b'Tokens:')
            t.close()
            assert Path(str(path)+'.effects').read_bytes() == b'effect\n' * 2
        finally:
            t.dispose()
        # Stop while asked must revoke, fail closed and restore termios.
        t = Terminal(path, 'open', extra, executable=exe)
        try:
            ready(t)
            t.send(b'stop\r')
            t.proc.wait(timeout=5)
            assert t.proc.returncode == 0
            import termios
            assert termios.tcgetattr(t.slave) == t.saved
            assert Path(str(path)+'.effects').read_bytes() == b'effect\n' * 2
        finally:
            t.dispose()
        final = snapshot(path, backend)
        audit = final['approval_events']
        assert len(audit) == 14, audit
        ids = [e['id'] for e in audit if e['type'] == 'asked']
        assert len(ids) == len(set(ids))
        assert [e['outcome'] for e in audit if e['type'] == 'decided'] == [
            'allowed-once','rejected','cancelled','allowed-once',
            'cancelled','rejected','cancelled']
        users = [m['content'][0]['data'] for m in final['messages'] if m['role'] == 1]
        assert users == ['allow','reject','cancel','allow-again','reopened','fresh','stop'], users
print('PTY approvals: JSONL/SQLite actual tools, allow-once/reject/cancel, fresh question, audit/history isolation, reopen, stop/restore passed')
