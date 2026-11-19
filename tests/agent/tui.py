"""Product PTY: install UI plugin, submit, stop, and reopen the saved session."""
import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import time

cli = str(Path(sys.argv[1]).resolve())
path = Path('agent-pty.jsonl').resolve()
path.unlink(missing_ok=True)
pid, master = pty.fork()
if pid == 0:
    os.environ['TERM'] = 'xterm-256color'
    os.execv(cli, [cli, 'create', str(path), '--tui-demo'])
fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', 24, 100, 0, 0))
output = bytearray()
finished = False
try:
    deadline = time.monotonic() + 12
    submitted = quit_sent = False
    start = time.monotonic()
    while time.monotonic() < deadline:
        if not submitted and time.monotonic() - start > .4:
            os.write(master, b'PTY saved input\r')
            submitted = True
        if submitted and not quit_sent and b'PanGu deterministic demo response' in output:
            os.write(master, b'/quit\r')
            quit_sent = True
        ready, _, _ = select.select([master], [], [], .05)
        if ready:
            try:
                chunk = os.read(master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                chunk = b''
            if chunk:
                output.extend(chunk)
        done, status = os.waitpid(pid, os.WNOHANG)
        if done:
            finished = True
            assert os.waitstatus_to_exitcode(status) == 0, 'product exit failed'
            break
    assert finished, 'TUI did not complete submit/quit in 12 seconds'
    assert b'PanGu deterministic demo response' in output, 'assistant result absent'
    reopened = subprocess.check_output([cli, 'open', str(path)], text=True)
    assert 'user: PTY saved input' in reopened
    assert 'assistant: PanGu deterministic demo response' in reopened
    print('PTY plugin start/input/Core/save/stop/reopen passed')
finally:
    if not finished:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
    os.close(master)
    path.unlink(missing_ok=True)

# Reuse the TUI owner's terminal reader and restoration checks with CLI argv.
import json
import sqlite3
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tui'))
from session_pty import Terminal

def cli_terminal(path, flags):
    t = Terminal.__new__(Terminal)
    t.master, t.slave = pty.openpty()
    t.saved = termios.tcgetattr(t.slave)
    t.proc = subprocess.Popen([cli, 'open', str(path), *flags, '--tui-demo'],
                             stdin=t.slave, stdout=t.slave, stderr=t.slave,
                             env={**os.environ, 'TERM': 'xterm-256color'})
    t.output, t.cursor = bytearray(), 0
    t.until(b'/tree /switch ID /branch ID /reset /handoff TEXT /compact [keep tokens] /usage')
    return t

def snapshot(path, backend):
    if backend == 'jsonl':
        return json.loads(path.read_text().splitlines()[-1])
    with sqlite3.connect(path) as db:
        return json.loads(db.execute('SELECT snapshot FROM pangu_session WHERE id=1').fetchone()[0])

with tempfile.TemporaryDirectory(prefix='pangu-app-history ') as directory:
    root = Path(directory)
    user = root / 'agent'
    (user / 'prompts').mkdir(parents=True)
    (user / 'skills' / 'example').mkdir(parents=True)
    (user / 'prompts' / 'hello.md').write_text('expanded-template $1')
    (user / 'skills' / 'example' / 'SKILL.md').write_text('---\nname: example\ndescription: history test\n---\nexpanded-skill-body\n')
    for backend in ('jsonl', 'sqlite'):
        path = root / (backend + '.session')
        flags = ['--agent-dir', str(user), '--cwd', str(root), '--no-tools']
        if backend == 'sqlite':
            flags += ['--sqlite']
        raws = ['plain 中文', '/hello 原始', '/skill:example argument']
        subprocess.run([cli, 'create', str(path), *flags, *[arg for raw in raws for arg in ('--demo', raw)]],
                       capture_output=True, check=True, timeout=10)
        saved = snapshot(path, backend)
        users = [m for m in saved['messages'] if m['role'] == 1]
        assert 'expanded-template' in users[1]['content'][0]['data']
        assert '<skill name="example"' in users[2]['content'][0]['data']
        # Resource changes after save cannot reconstruct historical raw text.
        (user / 'prompts' / 'hello.md').write_text('changed-template $1')
        t = cli_terminal(path, flags)
        try:
            for query in ('plain', '/hello', '/skill:example'):
                t.send(b'\x12' + query.encode())
                t.until(b'Search: ' + query.encode())
                t.send(b'\x03')
            t.send(b'\x12expanded-template')
            t.until(b'No match: expanded-template')
            t.send(b'\x03')
            assert snapshot(path, backend) == saved
            t.close()
        finally:
            t.dispose()
        assert snapshot(path, backend) == saved
        # Accepting a recall edits the draft; only the second Enter submits.
        t = cli_terminal(path, flags)
        try:
            t.send(b'\x12/hello\r')
            t.until(b'Search: /hello')
            assert snapshot(path, backend) == saved
            t.send(b'!\r')
            t.until(b'PanGu deterministic demo response')
            t.close()
        finally:
            t.dispose()
        now = snapshot(path, backend)
        nodes = [n for n in now['tree']['nodes'] if n.get('original_input')]
        assert nodes[-1]['original_input'] == '/hello 原始!'
        assert 'changed-template' in now['messages'][-2]['content'][0]['data']
        # Missing provenance explicitly disables import, without guessing.
        for n in now['tree']['nodes']:
            n.pop('original_input', None)
        if backend == 'jsonl':
            records = [json.loads(line) for line in path.read_text().splitlines()]
            for record in records:
                for node in record.get('tree', {}).get('nodes', []):
                    node.pop('original_input', None)
            path.write_text(''.join(json.dumps(record, ensure_ascii=False) + '\n' for record in records))
        else:
            with sqlite3.connect(path) as db:
                db.execute('UPDATE pangu_session SET snapshot=? WHERE id=1', (json.dumps(now) + '\n',))
        t = cli_terminal(path, flags)
        try:
            t.send(b'\x12/hello')
            t.until(b'No match: /hello')
            t.send(b'\x03')
            t.close()
        finally:
            t.dispose()
        # Next backend starts with the initial template body again.
        (user / 'prompts' / 'hello.md').write_text('expanded-template $1')
print('default app PTY raw history: plain/template/skill, JSONL/SQLite reopen, no replay, explicit edited submission, legacy unavailable passed')
