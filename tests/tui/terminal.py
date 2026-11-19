"""Real PTY regression: Core streaming/tool result, cancellation, editing,
fragmented paste, SIGWINCH, signal/normal shutdown and terminal restoration."""
import errno
import os
import pty
import select
import signal
import subprocess
import sys
import termios
import time

exe = os.path.abspath(sys.argv[1])

class Terminal:
    def __init__(self):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.proc = subprocess.Popen([exe], stdin=self.slave, stdout=self.slave,
                                     stderr=self.slave, env={**os.environ, 'TERM': 'xterm-256color'})
        self.output = bytearray()
        self.cursor = 0
    def send(self, data):
        os.write(self.master, data)
    def until(self, needle, timeout=8):
        deadline = time.monotonic() + timeout
        start = self.cursor
        while needle not in self.output[start:]:
            if time.monotonic() > deadline:
                raise AssertionError(f'terminal did not emit {needle!r}; tail={bytes(self.output[-1200:])!r}')
            ready, _, _ = select.select([self.master], [], [], .05)
            if ready:
                try:
                    data = os.read(self.master, 65536)
                except OSError as e:
                    if e.errno != errno.EIO:
                        raise
                    data = b''
                if not data:
                    raise AssertionError(f'early terminal EOF, rc={self.proc.poll()}')
                self.output.extend(data)
        self.cursor = self.output.index(needle, start) + len(needle)
    def close(self, sig=None):
        if sig:
            self.proc.send_signal(sig)
        else:
            self.send(b'\x04')
        self.proc.wait(timeout=5)
        assert self.proc.returncode == 0, self.proc.returncode
        assert termios.tcgetattr(self.slave) == self.saved, 'raw mode leaked'
        os.close(self.master)
        os.close(self.slave)
    def kill(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()

term = Terminal()
try:
    term.until(b'Deterministic demo')
    # Fragmented paste marker and payload: a single submission including CJK.
    term.send(b'\x1b[20')
    term.send(b'0~' + '中文é\nsecond'.encode())
    term.send(b'\x1b[20')
    term.send(b'1~\r')
    term.until(b'Assistant:')
    term.send(b'\x03')
    term.until(b'[cancelled]')
    # Cancel must settle and allow another model-tool-model roundtrip.
    term.send(b'/tool\r')
    term.until(b'Tool started: read_readme')
    term.until(b'Tool result:')
    term.until(b'Tokens:')
    # Busy Enter and Alt+Enter must traverse the real Core queues, not only
    # the UI mailbox. Each of three assistant turns commits one usage line.
    term.send(b'queue test\r')
    term.until(b'Assistant:')
    term.send(b'steering message\r')
    term.send(b'follow-up message\x1b\r')
    term.until(b'Tokens:')
    term.until(b'Tokens:')
    term.until(b'Tokens:')
    term.proc.send_signal(signal.SIGWINCH)
    term.close()
finally:
    term.kill()
for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
    term = Terminal()
    try:
        term.until(b'Deterministic demo')
        term.close(sig)
    finally:
        term.kill()
result = subprocess.run([exe], stdin=subprocess.DEVNULL, capture_output=True)
assert result.returncode == 2
assert b'interactive terminal required' in result.stderr
assert b'\x1b' not in result.stdout, 'redirected output polluted by restore sequences'
print('PTY: stream, tool, cancel/continue, split paste, resize, EOF, 3 signals, restore, redirection passed')
