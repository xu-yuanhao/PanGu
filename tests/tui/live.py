"""Opt-in real DeepSeek TUI acceptance. Credentials must already be in child
process environment. No raw transcript, API key, endpoint or model payload is
printed or recorded. All model-visible fixtures are synthetic. Not in CTest.
Usage: with-private.py PRIVATE -- python3 tests/tui/live.py EXECUTABLE BUILD_DIR
"""
import json
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import sys
import termios
import time
import uuid

exe = str(Path(sys.argv[1]).resolve())
base = Path(sys.argv[2]).resolve() / ('tui-live-' + uuid.uuid4().hex[:10])
base.mkdir(parents=True)
session = base / 'session.jsonl'
work = base / 'work'
work.mkdir()

class UI:
    def __init__(self, create):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.proc = subprocess.Popen([exe, 'create' if create else 'open',
                                     str(session), '--tui', '--cwd', str(work)],
                                    stdin=self.slave, stdout=self.slave, stderr=self.slave,
                                    env={**os.environ, 'TERM': 'xterm-256color'})
        self.data = bytearray()
        self.cursor = 0
    def send(self, text):
        os.write(self.master, text.encode() if isinstance(text, str) else text)
    def until(self, marker, seconds=90):
        deadline = time.monotonic() + seconds
        while marker not in self.data[self.cursor:]:
            if time.monotonic() >= deadline:
                raise RuntimeError('live UI marker timed out (transcript withheld)')
            ready, _, _ = select.select([self.master], [], [], .05)
            if ready:
                try:
                    chunk = os.read(self.master, 262144)
                except OSError:
                    chunk = b''
                if not chunk:
                    raise RuntimeError('live UI exited early (transcript withheld)')
                self.data.extend(chunk)
        self.cursor = self.data.index(marker, self.cursor) + len(marker)
    def quit(self):
        self.send('/quit\r')
        self.proc.wait(timeout=10)
        assert self.proc.returncode == 0, 'live UI process failed'
        assert termios.tcgetattr(self.slave) == self.saved, 'live UI terminal restore failed'
        os.close(self.master)
        os.close(self.slave)
    def cleanup(self):
        if self.proc.poll() is None:
            self.proc.kill()
            self.proc.wait()

ui = UI(True)
try:
    ui.until(b'/help shortcuts')
    ui.send('Use the write tool to write exactly "PANGU_SYNTHETIC_FILE" to proof.txt in the working directory. Then use read to read proof.txt. After both tools succeed, reply exactly PANGU_TUI_FILE_OK.\r')
    ui.until(b'Tool started: write')
    ui.until(b'Tool started: read')
    ui.until(b'Assistant: PANGU_TUI_FILE_OK')
    ui.until(b'Tokens:')
    assert (work / 'proof.txt').read_text().strip() == 'PANGU_SYNTHETIC_FILE'
    print('live UI: real write/read tool result and assistant response passed', flush=True)
    ui.send('Without tools, produce a very long numbered list of 10000 distinct tips, continuing until the entire list is complete.\r')
    ui.until(b'Assistant:')
    # Start event may precede the first response byte; allow network poll to run.
    time.sleep(.15)
    ui.send(b'\x03')
    ui.until(b'[cancelled]', 20)
    print('live UI: cooperative network cancel via owner-thread poll passed', flush=True)
    ui.send('Without tools reply exactly PANGU_TUI_CONTINUE_OK.\r')
    ui.until(b'Assistant: PANGU_TUI_CONTINUE_OK')
    ui.until(b'Tokens:')
    ui.quit()
finally:
    ui.cleanup()
ui = UI(False)
try:
    ui.until(b'Assistant: PANGU_TUI_FILE_OK', 20)
    ui.until(b'[cancelled]', 20)
    ui.until(b'Assistant: PANGU_TUI_CONTINUE_OK', 20)
    ui.quit()
finally:
    ui.cleanup()
# The store must contain artifacts, never configuration/credential strings.
raw = session.read_bytes()
key = os.environ.get('DEEPSEEK_API_KEY', '').encode()
assert not key or key not in raw
assert b'PANGU_TUI_CONTINUE_OK' in raw
print('live UI: continue, close/reopen replay, credential exclusion and terminal restore passed')
