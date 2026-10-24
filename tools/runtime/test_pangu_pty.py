"""PTY tests of the actual composition. --live uses MIMO_API_KEY without logging it."""
from pathlib import Path
import argparse
import os
import pty
import select
import signal
import subprocess
import tempfile
import termios
import time
import struct
import re
from build_pangu import build

class Terminal:
    def __init__(self, binary, workspace, env):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.output = b''
        self.secret = env.get('MIMO_API_KEY', '').encode()
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
                raise AssertionError('process exited before expected marker')
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

def checkpoints(path):
    data = path.read_bytes()
    cursor = 0
    result = []
    while cursor < len(data):
        version, length = struct.unpack_from('<II', data, cursor + 8)
        assert 56 <= length <= 8592 and cursor + length <= len(data)
        if version == 2:
            result.append(struct.unpack_from('<QIIQIIQ', data, cursor + length - 56))
        cursor += length
    assert cursor == len(data)
    return result

def persistent_idle(binary, root, env):
    storage = root / 'idle-storage'
    storage.mkdir()
    env = dict(env, PANGU_SESSION_DIR=str(storage))
    original = None
    for attempt in range(2):
        terminal = Terminal(binary, root, env)
        try:
            terminal.until(b'> ', 5)
            terminal.send(b'\x04')
            terminal.finish()
        finally:
            terminal.close()
        data = (storage / 'session.pgj').read_bytes()
        assert env['MIMO_API_KEY'].encode() not in data
        if attempt:
            assert original == data, 'idle reopen wrote unnecessary profile/system records'
        original = data
    assert not checkpoints(storage / 'session.pgj')
    print('PASS: persistent idle create/reopen without network, credentials absent and no redundant commits', flush=True)

def persistent_live(binary, root):
    storage = root / 'live-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    log = storage / 'session.pgj'
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        terminal.send(b'Remember PANGU_DURABLE_8731. Start with PERSIST_STREAM then write 12 numbered facts about C pointers, each at most six words. Do not use tools.\r')
        terminal.until(b'[run] started')
        terminal.output = terminal.output.split(b'[run] started', 1)[1]
        terminal.until(b'PERSIST_STREAM')
        terminal.send(b'\x04')
        terminal.finish()
        assert b'[run] finished:' not in terminal.output, 'detach fabricated a terminal'
    finally:
        terminal.close()
    saved = log.read_bytes()
    assert checkpoints(log)[-1][1] == 1, 'interrupted request must remain MODEL'
    assert env['MIMO_API_KEY'].encode() not in saved
    other = root / 'different-workspace'
    other.mkdir()
    for test_env, workspace in ((dict(env, MIMO_MODEL='incompatible-model'), root), (env, other)):
        terminal = Terminal(binary, workspace, test_env)
        try:
            terminal.until(b'profile mismatch', 5)
            terminal.finish(2)
            assert terminal.output.find(b'[run] started') < 0
        finally:
            terminal.close()
        assert log.read_bytes() == saved, 'mismatch changed durable state'
    print('PASS: actual stream detach preserves pending run; changed model/workspace refused without journal mutation', flush=True)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'[run] started', 5)
        terminal.until(b'[run] finished: success')
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert checkpoints(log)[-1][1] == 3
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        assert b'[run] started' not in terminal.output, 'terminal run restarted'
        terminal.output = b''
        terminal.send(b'Return only the remembered PANGU token. Do not use tools.\r')
        terminal.until(b'[run] finished: success')
        plain = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
        assert b'PANGU_DURABLE_8731' in b''.join(plain.split())
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual persistent TUI auto-resume without resubmit, terminal does not replay, new-process recall and termios restoration', flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live', action='store_true')
    parser.add_argument('--live-persistent', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pangu-pty-') as directory:
        root = Path(directory)
        binary = build(root / 'pangu')
        (root / 'sample.c').write_text('const char *marker = "PANGU_PTY_READ_5208";\n')
        env = os.environ.copy()
        env.pop('MIMO_API_KEY', None)
        env.pop('PANGU_SESSION_DIR', None)
        missing = subprocess.run([str(binary)], env=env, capture_output=True, timeout=5)
        assert missing.returncode == 2 and b'MIMO_API_KEY is required' in missing.stderr
        # No network: enter real terminal with an unused placeholder credential.
        env['MIMO_API_KEY'] = 'local-pty-no-network-placeholder'
        for exit_mode in ('ctrl-d', 'signal', 'resize'):
            terminal = Terminal(binary, root, env)
            try:
                terminal.until(b'> ', 5)
                assert termios.tcgetattr(terminal.slave) != terminal.saved
                if exit_mode == 'signal':
                    terminal.process.send_signal(signal.SIGTERM)
                else:
                    if exit_mode == 'resize':
                        terminal.process.send_signal(signal.SIGWINCH)
                        time.sleep(0.1)
                    terminal.send(b'\x04')
                terminal.finish()
                print('PASS: native PTY', exit_mode, 'and exact termios restoration', flush=True)
            finally:
                terminal.close()
        persistent_idle(binary, root, env)
        if args.live_persistent:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-persistent'
            persistent_live(binary, root)
        if args.live:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live'
            live_env = os.environ.copy()
            live_env.pop('PANGU_SESSION_DIR', None)
            terminal = Terminal(binary, root, live_env)
            try:
                terminal.until(b'> ', 5)
                terminal.send(b'Call read exactly once for sample.c and return just the marker inside it.\r')
                terminal.until(b'[run] finished: success')
                assert b'[tool] read' in terminal.output and b'finished ok:' in terminal.output
                # ANSI editor redraw separates chunks, so remove terminal controls for comparison.
                import re
                plain = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
                plain = plain.replace(b'\r', b'').replace(b'> ', b'')
                after_tool = plain.split(b'finished ok:', 1)[1].split(b'\n', 1)[1]
                assert b'PANGU_PTY_READ_5208' in b''.join(after_tool.split())
                print('PASS: actual TUI / MiMo SSE / workspace read / model continuation', flush=True)
                terminal.output = b''
                terminal.send(b'Start with STREAM_BEGIN then write 60 numbered short facts about C pointers. Do not use tools.\r')
                terminal.until(b'[run] started')
                terminal.output = terminal.output.split(b'[run] started', 1)[1]
                terminal.until(b'STREAM_BEGIN')
                terminal.send(b'\x03')
                terminal.until(b'[run] finished: cancelled')
                terminal.send(b'\x04')
                terminal.finish()
                print('PASS: actual TUI cancellation and exact termios restoration', flush=True)
            finally:
                terminal.close()
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
