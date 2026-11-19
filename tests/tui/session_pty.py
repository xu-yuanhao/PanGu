import os
import pty
import select
import subprocess
import termios
import time

class Terminal:
    def __init__(self, path, mode, extra=(), executable=None):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.proc = subprocess.Popen([executable, str(path), mode, *extra], stdin=self.slave,
                                    stdout=self.slave, stderr=self.slave,
                                    env={**os.environ, 'TERM':'xterm-256color'})
        self.output = bytearray()
        self.cursor = 0
    def send(self, s):
        os.write(self.master, s)
    def until(self, needle):
        deadline = time.monotonic() + 8
        start = self.cursor
        while needle not in self.output[start:]:
            assert time.monotonic() < deadline, (needle, bytes(self.output[-1800:]))
            assert self.proc.poll() is None, (self.proc.returncode, bytes(self.output))
            if select.select([self.master], [], [], .05)[0]:
                self.output.extend(os.read(self.master, 65536))
        self.cursor = self.output.index(needle, start) + len(needle)
        return bytes(self.output[start:self.cursor])
    def close(self):
        self.send(b'/quit\r')
        self.proc.wait(timeout=5)
        assert self.proc.returncode == 0
        assert termios.tcgetattr(self.slave) == self.saved
    def dispose(self):
        if self.proc.poll() is None:
            self.proc.kill(); self.proc.wait()
        os.close(self.master); os.close(self.slave)

