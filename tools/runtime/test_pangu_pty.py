"""Actual Durable product PTY: terminal lifetime, coding tools and crash recovery."""
from pathlib import Path
import argparse
import json
import os
import pty
import re
import select
import signal
import subprocess
import tempfile
import termios
import time
from build_pangu import build

class Terminal:
    def __init__(self, binary, workspace, env):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.output = b''
        self.secret = env.get('PANGU_MODEL_API_KEY', env.get('MIMO_API_KEY', '')).encode()
        self.process = subprocess.Popen([str(binary), str(workspace)], stdin=self.slave,
                                        stdout=self.slave, stderr=self.slave, env=env)
    def until(self, marker, timeout=80):
        deadline = time.monotonic() + timeout
        while marker not in self.output:
            if time.monotonic() >= deadline:
                tail = self.output[-1000:]
                if self.secret:
                    tail = tail.replace(self.secret, b'[REDACTED]')
                raise AssertionError('PTY output marker not received within deadline: ' + repr(tail))
            readable, _, _ = select.select([self.master], [], [], 0.1)
            if readable:
                data = os.read(self.master, 8192)
                if not data:
                    raise AssertionError('PTY closed before expected marker')
                self.output += data
                if len(self.output) > 262144:
                    raise AssertionError('PTY transcript limit exceeded')
            if marker == b'[run] finished: success' and b'[run] finished: error' in self.output:
                raise AssertionError('actual run ended with error: ' + repr(self.output[-500:].replace(self.secret, b'[REDACTED]') if self.secret else self.output[-500:]))
            if self.process.poll() is not None and not readable:
                raise AssertionError('process exited ('+str(self.process.returncode)+') before expected marker: '+repr(self.output[-2000:].replace(self.secret,b'[REDACTED]') if self.secret else self.output[-2000:]))
    def send(self, data):
        os.write(self.master, data)
    def finish(self, expected=0):
        code = self.process.wait(timeout=8)
        assert code == expected, code
        assert termios.tcgetattr(self.slave) == self.saved, 'terminal attributes were not restored'
    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=5)
        os.close(self.master)
        os.close(self.slave)

def stored_bytes(directory):
    return b''.join(p.read_bytes() for p in sorted(directory.iterdir()) if p.is_file())

def terminal_exit(binary, root, env):
    for mode in ('ctrl-d', 'signal', 'resize'):
        terminal = Terminal(binary, root, env)
        try:
            terminal.until(b'> ', 5)
            assert termios.tcgetattr(terminal.slave) != terminal.saved
            if mode == 'signal':
                terminal.process.send_signal(signal.SIGTERM)
            else:
                if mode == 'resize':
                    terminal.process.send_signal(signal.SIGWINCH)
                terminal.send(b'\x04')
            terminal.finish()
        finally:
            terminal.close()
    directory = root / '.pangu'
    original = stored_bytes(directory)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert stored_bytes(directory) == original, 'idle reopen changed committed facts'
    assert b'offline-placeholder' not in original
    for name, content in [('corrupt', b'not-json\n'), ('torn', b'{')]:
        location = root / name
        location.mkdir()
        (location / 'main.jsonl').write_bytes(content)
        terminal = Terminal(binary, root, dict(env, PANGU_SESSION_DIR=str(location)))
        try:
            if name == 'corrupt':
                terminal.until(b'Cannot activate native plugin', 5)
                terminal.finish(2)
            else:
                terminal.until(b'> ', 5)
                terminal.send(b'\x04')
                terminal.finish()
        finally:
            terminal.close()
    print('PASS: Durable default .pangu create/reopen, idle stability, corruption/torn tail and terminal restoration', flush=True)

def run_input(terminal, text):
    terminal.output = b''
    terminal.send(text.encode() + b'\r')
    terminal.until(b'[run] finished: success', 160)
    return terminal.output

def workflow(binary, root, env):
    marker = 'PANGUPTY' + os.urandom(6).hex()
    (root / 'source.txt').write_text(marker + '\n')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Read source.txt using read. Write its exact content to target.txt using write. Then use edit to append the literal suffix _EDIT to its first line. Run bash command cat target.txt to verify the result. Finish by replying with the final file content. Use all four tools.')
        assert (root / 'target.txt').read_text() == marker + '_EDIT\n'
        for tool in (b'read', b'write', b'edit', b'bash'):
            assert b'[tool] ' + tool in re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', output), ('missing actual tool', tool)
        assert marker.encode() in output and b'_EDIT' in output
        output = run_input(terminal, 'Use bash to run exactly this command: `printf PANGU_EXIT_CHECK; exit 7`. Then report the exit code. Do not repeat the command.')
        assert b'7' in output and b'PANGU_EXIT_CHECK' in output
        output = run_input(terminal, 'What product are you? Answer only its name.')
        assert b'PanGu' in output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    directory = Path(env.get('PANGU_SESSION_DIR', str(root / '.pangu')))
    saved = stored_bytes(directory)
    secret = env.get('PANGU_MODEL_API_KEY', env.get('MIMO_API_KEY', '')).encode()
    assert secret and secret not in saved
    def records(value):
        if isinstance(value, dict):
            if value.get('kind') == 'pangu.tool-result':
                yield value
            for child in value.values():
                yield from records(child)
        elif isinstance(value, list):
            for child in value:
                yield from records(child)
    edits = []
    failed_bash = []
    for path in directory.iterdir():
        if path.is_file():
            for line in path.read_bytes().splitlines():
                try:
                    value = json.loads(line)
                except (ValueError, UnicodeDecodeError):
                    continue
                for entry in records(value):
                    message = entry['model'][0]
                    if message['toolName'] == 'edit' and not message['isError']:
                        edits.append(entry)
                    if message['toolName'] == 'bash' and message['isError']:
                        failed_bash.append(entry)
    assert edits, 'missing committed actual edit receipt'
    assert any(e['data']['structuredOutput']['exitCode'] == 7
               and any(d['code'] == 'exit_code' for d in e['data']['diagnostics'])
               and any('[error] Command exited with code 7' in b.get('text', '') for b in e['model'][0]['content'])
               for e in failed_bash), 'missing actual nonzero bash diagnostics'
    assert all(e['data']['status'] == 'completed' and e['data']['arguments']['path'] == 'target.txt'
               and e['model'][0]['details']['firstChangedLine'] == 1 for e in edits)
    assert all(e['model'][0]['details']['diff'] and e['model'][0]['details']['patch'] for e in edits)
    subprocess.run(['git', 'apply', '--reverse', '--check', '-'], cwd=root,
                   input=edits[-1]['model'][0]['details']['patch'].encode(), check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        assert marker.encode() in terminal.output and b'[history]' in terminal.output
        assert b'[run] started' not in terminal.output
        assert stored_bytes(directory) == saved
        output = run_input(terminal, 'Without tools, quote the first line of source.txt that you read earlier.')
        assert marker.encode() in output, repr(output[-4000:].replace(secret,b'[REDACTED]'))
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual model/TUI read-write-edit-bash, PanGu identity, committed history and resumed model context', flush=True)

def recovery(binary, root, env, crash=False):
    directory = root / ('crash-session' if crash else 'detach-session')
    env = dict(env, PANGU_SESSION_DIR=str(directory))
    target = root / ('crash-effects.txt' if crash else 'detach-effects.txt')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        child_pid = root / 'crash-child.pid'
        command = (f'printf "%s" "$$" > {child_pid.name}; ' if crash else '') + f'printf once >> {target.name}; printf PANGU_EFFECT_STARTED; sleep 30; printf late >> {target.name}'
        terminal.send(('Use bash exactly once to run: '+json.dumps(command)+'. If interrupted, never repeat it. Report the result briefly.').encode()+b'\r')
        # Tool previews may buffer a source line until LF; observe the actual
        # side effect while draining the PTY instead of synchronizing on text.
        deadline = time.monotonic()+60
        while not target.exists() or target.read_bytes()!=b'once':
            assert time.monotonic()<deadline, 'command effect was not observed: '+repr(target.read_bytes() if target.exists() else None)
            if target.exists():
                assert target.read_bytes() in (b'',b'once'), 'unexpected command effect before exit: '+repr(target.read_bytes())
            readable, _, _ = select.select([terminal.master], [], [], .1)
            if readable:
                terminal.output += os.read(terminal.master,8192)
        if crash:
            terminal.process.kill()
            terminal.process.wait(timeout=5)
            # External shell effects outlive SIGKILL; reap this test's process group.
            if child_pid.exists():
                try:
                    os.killpg(int(child_pid.read_text()), signal.SIGKILL)
                except ProcessLookupError:
                    pass
        else:
            terminal.send(b'\x04')
            terminal.finish()
    finally:
        terminal.close()
    assert target.read_bytes()==b'once', ('effect after exit',target.read_bytes())
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'[run] finished: success',160)
        assert b'interrupted' in terminal.output
        assert target.read_bytes()==b'once', 'unsafe command was replayed'
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: Durable '+('process death' if crash else 'cooperative exit')+' and unsafe-tool recovery without repeating effects',flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,help='already built product')
    parser.add_argument('--live',action='store_true',help='real coding workflow and committed context reopen')
    parser.add_argument('--live-persistent',action='store_true',help='cooperative exit during an unsafe tool and reopen')
    parser.add_argument('--live-crash',action='store_true',help='process death during an unsafe tool and reopen')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pangu-pty-') as directory:
        root=Path(directory);binary=args.binary or build(root/'pangu')
        offline={k:v for k,v in os.environ.items() if not k.startswith(('PANGU_', 'MIMO_'))}
        missing=subprocess.run([str(binary)],env=offline,capture_output=True,timeout=5)
        assert missing.returncode==2 and b'Model API key is required' in missing.stderr
        offline.update(PANGU_MODEL_ENDPOINT='http://127.0.0.1:1/chat/completions',PANGU_MODEL_NAME='offline',PANGU_MODEL_API_KEY='offline-placeholder')
        terminal_exit(binary,root,offline)
        if args.live or args.live_persistent or args.live_crash:
            assert os.environ.get('PANGU_MODEL_API_KEY') or os.environ.get('MIMO_API_KEY'), 'model credential required'
            env=dict(os.environ);env.pop('PANGU_SESSION_DIR',None)
            if args.live:
                workflow(binary,root,env)
            if args.live_persistent:
                recovery(binary,root,env)
            if args.live_crash:
                recovery(binary,root,env,True)

if __name__=='__main__':
    main()
