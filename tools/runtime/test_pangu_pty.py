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
from build_pangu import build

class Terminal:
    def __init__(self, binary, workspace, env):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.output = b''
        self.process = subprocess.Popen([str(binary), str(workspace)], stdin=self.slave,
                                        stdout=self.slave, stderr=self.slave, env=env)
    def until(self, marker, timeout=80):
        deadline = time.monotonic() + timeout
        while marker not in self.output:
            if time.monotonic() >= deadline:
                raise AssertionError('PTY output marker not received within deadline')
            readable, _, _ = select.select([self.master], [], [], 0.1)
            if readable:
                data = os.read(self.master, 8192)
                if not data:
                    raise AssertionError('PTY closed before expected marker')
                self.output += data
                if len(self.output) > 262144:
                    raise AssertionError('PTY transcript limit exceeded')
            if self.process.poll() is not None and not readable:
                raise AssertionError('process exited before expected marker')
    def send(self, data):
        os.write(self.master, data)
    def finish(self):
        code = self.process.wait(timeout=8)
        assert code == 0, code
        assert termios.tcgetattr(self.slave) == self.saved, 'terminal attributes were not restored'
    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=5)
        os.close(self.master)
        os.close(self.slave)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pangu-pty-') as directory:
        root = Path(directory)
        binary = build(root / 'pangu')
        (root / 'sample.c').write_text('const char *marker = "PANGU_PTY_READ_5208";\n')
        env = os.environ.copy()
        env.pop('MIMO_API_KEY', None)
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
        if args.live:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live'
            terminal = Terminal(binary, root, os.environ.copy())
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
