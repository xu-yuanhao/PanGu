"""Installed C SDK consumer: real input, rendering, quit and terminal restore."""
import errno
import os
import pty
import select
import subprocess
import sys
import termios
import time

master, slave = pty.openpty()
saved = termios.tcgetattr(slave)
process = subprocess.Popen(sys.argv[1:], stdin=slave, stdout=slave, stderr=slave,
                           env={**os.environ, 'TERM': 'xterm-256color'})
output = bytearray()

def until(needle):
    deadline = time.monotonic() + 8
    while needle not in output:
        if time.monotonic() >= deadline:
            raise AssertionError(f'missing {needle!r}, rc={process.poll()}, output={bytes(output[-1200:])!r}')
        ready, _, _ = select.select([master], [], [], .05)
        if not ready and process.poll() is not None:
            raise AssertionError(f'process exited before {needle!r}: rc={process.returncode}, output={bytes(output[-1200:])!r}')
        if ready:
            try:
                data = os.read(master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                data = b''
            assert data, f'early EOF: rc={process.poll()}, output={bytes(output[-1200:])!r}'
            output.extend(data)

try:
    until(b'/quit exit')
    os.write(master, b'installed input\r')
    until(b'installed response')
    os.write(master, b'/quit\r')
    process.wait(timeout=8)
    assert process.returncode == 0, (process.returncode, bytes(output[-1200:]))
    assert termios.tcgetattr(slave) == saved, 'terminal modes not restored'
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    os.close(master)
    os.close(slave)
