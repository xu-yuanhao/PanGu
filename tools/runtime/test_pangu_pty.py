"""PTY tests of the actual composition. --live uses MIMO_API_KEY without logging it."""
from pathlib import Path
import argparse
import json
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
        assert 56 <= length <= (38328 if version == 5 else 8592) and cursor + length <= len(data)
        if version in (2,5):
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
            terminal.until(b'\r\n> ', 5)
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
    # Valid native fixture with a prior idle composition prompt, same frame size.
    data = bytearray(original)
    cursor = 0
    while cursor < len(data):
        version, length = struct.unpack_from('<II', data, cursor + 8)
        kind = struct.unpack_from('<I', data, cursor + 24)[0]
        if version == 1 and kind == 1:
            obsolete = b'OLD_READ_ONLY_CONFIG'
            data[cursor + 40:cursor + length - 16] = obsolete.ljust(length - 56, b' ')
            checksum = 0xcbf29ce484222325
            for byte in data[cursor:cursor + length - 16]:
                checksum = ((checksum ^ byte) * 0x100000001b3) & 0xffffffffffffffff
            struct.pack_into('<Q', data, cursor + length - 16, checksum)
            break
        cursor += length
    else:
        raise AssertionError('expected initial system frame')
    (storage / 'session.pgj').write_bytes(data)
    for attempt in range(2):
        terminal = Terminal(binary, root, env)
        try:
            terminal.until(b'\r\n> ', 5)
            terminal.send(b'\x04')
            terminal.finish()
        finally:
            terminal.close()
        refreshed = (storage / 'session.pgj').read_bytes()
        if not attempt:
            assert refreshed.startswith(data) and len(refreshed) > len(data)
            assert b'Use read to inspect workspace files and write to create' in refreshed[len(data):]
            original = refreshed
        else:
            assert refreshed == original, 'configured prompt refreshed more than once'
    print('PASS: persistent idle prompt refresh preserves earlier frames and is not repeated', flush=True)
    print('PASS: persistent idle create/reopen without network, credentials absent and no redundant commits', flush=True)

def persistent_live(binary, root):
    storage = root / 'live-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    log = storage / 'session.pgj'
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
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
        assert terminal.output.count(b'[history] user:') == 1
        assert b'[history] assistant:' not in terminal.output, 'uncommitted partial appeared as history'
        terminal.until(b'[run] finished: success')
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert checkpoints(log)[-1][1] == 3
    before_history = log.read_bytes()
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        assert terminal.output.count(b'[history] user:') == 1
        assert terminal.output.count(b'[history] assistant:') == 1
        assert log.read_bytes() == before_history, 'history rendering changed journal'
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

def write_live(binary, root):
    storage = root / 'write-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    expected = b'Hello from PanGu.\n'
    target = root / 'generated' / 'answer.txt'
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.send(b'Use write to create generated/answer.txt. Its content must be exactly Hello from PanGu. followed by one newline. Then briefly confirm.\r')
        terminal.until(b'[run] finished: success')
        assert b'[tool] write' in terminal.output and b'finished ok:' in terminal.output, repr(terminal.output[-3000:].replace(terminal.secret,b'[REDACTED]'))
        assert target.read_bytes() == expected, 'actual write content differs'
        terminal.output = b''
        terminal.send(b'Call read for generated/answer.txt and quote its content.\r')
        terminal.until(b'[run] finished: success')
        assert b'[tool] read' in terminal.output and b'finished ok:' in terminal.output
        plain = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
        after_tool = plain.split(b'finished ok:', 1)[1].split(b'\n', 1)[1]
        assert b'HellofromPanGu.' in b''.join(after_tool.split())
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    log = storage / 'session.pgj'
    original = log.read_bytes()
    assert env['MIMO_API_KEY'].encode() not in original
    assert any(checkpoint[1] == 5 for checkpoint in checkpoints(log)), 'write intent was not durably unsafe'
    assert checkpoints(log)[-1][1] == 3
    before = target.stat().st_mtime_ns
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        assert b'[history] call' in terminal.output
        assert b'[run] started' not in terminal.output
        assert log.read_bytes() == original and target.stat().st_mtime_ns == before
        assert target.read_bytes() == expected
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual persistent TUI / MiMo write and read-back, unsafe intent, settled reopen without rewrite and termios restoration', flush=True)

def edit_live(binary, root):
    storage = root / 'edit-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    target = root / 'settings.txt'
    target.write_bytes(b'\xef\xbb\xbf# PanGu settings  \r\nenabled = false\r\nattempts = 2\r\n')
    expected = b'\xef\xbb\xbf# PanGu settings  \r\nenabled = true\r\nattempts = 3\r\n'
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.send(b'Use edit once on settings.txt with two edits: replace enabled = false with enabled = true; replace attempts = 2 with attempts = 3. Preserve all other content. Then confirm briefly.\r')
        terminal.until(b'[run] finished: success')
        assert b'[tool] edit' in terminal.output and b'finished ok:' in terminal.output, repr(terminal.output[-3000:].replace(terminal.secret,b'[REDACTED]'))
        assert b'[tool] write' not in terminal.output
        assert target.read_bytes() == expected, 'edit altered unexpected bytes or line endings'
        terminal.output = b''
        terminal.send(b'Call read for settings.txt and state the enabled and attempts values.\r')
        terminal.until(b'[run] finished: success')
        assert b'[tool] read' in terminal.output and b'finished ok:' in terminal.output
        plain = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
        after_tool = plain.split(b'finished ok:', 1)[1].split(b'\n', 1)[1]
        assert b'true' in after_tool and b'3' in after_tool
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    log = storage / 'session.pgj'
    original = log.read_bytes()
    assert env['MIMO_API_KEY'].encode() not in original
    assert any(checkpoint[1] == 5 for checkpoint in checkpoints(log))
    assert checkpoints(log)[-1][1] == 3
    before = target.stat().st_mtime_ns
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        assert b'[history] call' in terminal.output and b'[run] started' not in terminal.output
        assert target.read_bytes() == expected and target.stat().st_mtime_ns == before
        assert log.read_bytes() == original
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual MiMo/TUI edit and read-back, BOM/CRLF/unchanged bytes, unsafe intent and settled reopen without rewrite', flush=True)

def bash_live(binary, root):
    storage = root / 'bash-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        command = "printf PANGU_BASH_PROOF > shell-proof.txt; printf stdout-proof; printf stderr-proof >&2; if [ -z \"$MIMO_API_KEY\" ]; then printf clear > shell-env.txt; else exit 91; fi"
        terminal.send(('Call bash exactly once with arguments: '+json.dumps({'command':command})+' . Do not use other tools. Then report the result briefly.\r').encode())
        terminal.until(b'[run] finished: success')
        assert b'[tool] bash' in terminal.output and b'finished ok:' in terminal.output
        assert (root / 'shell-proof.txt').read_bytes() == b'PANGU_BASH_PROOF'
        assert (root / 'shell-env.txt').read_bytes() == b'clear'
        terminal.output = b''
        terminal.send(b'Use bash exactly once: printf PANGU_BASH_FAILED; exit 7 . Report its nonzero exit without retrying.\r')
        terminal.until(b'[run] finished: success')
        assert b'[tool] bash' in terminal.output and b'finished error:' in terminal.output and b'exit=7' in terminal.output
        terminal.output = b''
        terminal.send(b'Use bash once to execute: printf PANGU_BASH_WAITING; sleep 30; printf late > should-not-exist.txt . Wait for its result.\r')
        terminal.until(b') started')
        terminal.output = terminal.output.split(b') started', 1)[1]
        terminal.until(b'PANGU_BASH_WAITING')
        terminal.send(b'\x03')
        terminal.until(b'[run] finished: cancelled', 10)
        assert not (root / 'should-not-exist.txt').exists()
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    log = storage / 'session.pgj'
    original = log.read_bytes()
    assert env['MIMO_API_KEY'].encode() not in original
    assert any(cp[1] == 5 for cp in checkpoints(log)) and checkpoints(log)[-1][1] == 3
    before = (root / 'shell-proof.txt').stat().st_mtime_ns
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        assert b'[history] call' in terminal.output and b'[run] started' not in terminal.output
        assert log.read_bytes() == original and (root / 'shell-proof.txt').stat().st_mtime_ns == before
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual MiMo/TUI bash side effect, explicit environment, nonzero result, cancellation, unsafe intent and settled reopen', flush=True)

def bash_recovery_live(binary, root):
    storage = root / 'bash-recovery-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    terminal = Terminal(binary, root, env)
    artifact = None
    try:
        terminal.until(b'\r\n> ', 5)
        command = "printf '%06000d' 0"
        terminal.send(('Call bash exactly once with arguments: '+json.dumps({'command':command})+' . Do not repeat or read its full output; report briefly.\r').encode())
        terminal.until(b'[run] finished: success')
        found = re.search(rb'full raw output: (/tmp/pangu-output-[A-Za-z0-9]+)', (storage / 'session.pgj').read_bytes())
        assert found, 'missing committed spill artifact path; terminal: '+repr(terminal.output[-5000:].replace(terminal.secret,b'[REDACTED]'))
        artifact = Path(found[1].decode())
        assert b'[tool] bash' in terminal.output and b'earlier output omitted' in terminal.output
        assert found[1] in terminal.output, 'spill path was hidden by the visible summary'
        assert artifact.read_bytes() == b'0'*6000 and artifact.stat().st_mode & 0o777 == 0o600
        terminal.output = b''
        command = 'printf once >> bash-effects.txt; printf PANGU_BASH_PARKED; sleep 30; printf late >> bash-effects.txt'
        terminal.send(('Call bash exactly once with arguments: '+json.dumps({'command':command})+' . Never repeat this command if it is interrupted. Report its result briefly.\r').encode())
        terminal.until(b') started')
        terminal.output = terminal.output.split(b') started', 1)[1]
        terminal.until(b'PANGU_BASH_PARKED')
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
        if artifact is not None:
            artifact.unlink()
    target = root / 'bash-effects.txt'
    assert target.read_bytes() == b'once'
    log = storage / 'session.pgj'
    assert checkpoints(log)[-1][1] == 5, 'detach must preserve unsafe checkpoint, not abort or settle'
    before = target.stat().st_mtime_ns
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'[run] finished: success')
        assert b'interrupted' in terminal.output
        assert target.read_bytes() == b'once' and target.stat().st_mtime_ns == before
        assert checkpoints(log)[-1][1] == 3 and env['MIMO_API_KEY'].encode() not in log.read_bytes()
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual MiMo/TUI bash raw spill and cooperative detach, unsafe restart result without repeating command effects', flush=True)

def workflow_live(binary, root):
    storage = root / 'workflow-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    original = b'.intel_syntax noprefix\n.section .rodata\nmessage: .ascii "OLD_WORKFLOW\\n"\nmessage_end:\n.set message_length,message_end-message\n.text\n.globl _start\n_start:\n mov eax,1\n mov edi,1\n lea rsi,[rip+message]\n mov edx,OFFSET message_length\n syscall\n mov eax,60\n xor edi,edi\n syscall\n.section .note.GNU-stack,"",@progbits\n'
    expected = original.replace(b'OLD_WORKFLOW', b'PANGU_WORKFLOW_OK')
    (root / 'example.S').write_bytes(original)
    script = 'set -eu\nclang-23 -nostdlib -no-pie example.S -o example-run\noutput=$(./example-run)\ntest "$output" = PANGU_WORKFLOW_OK\nprintf "PANGU_WORKFLOW_VERIFIED\\n"\n'
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        request = ('Read example.S with read. Use edit to replace only OLD_WORKFLOW with PANGU_WORKFLOW_OK. '
                   'Use write to create verify.sh with exactly this content: '+json.dumps(script)+
                   ' . Then use bash to run sh verify.sh. Do not perform these changes using bash. Report the actual test result briefly.\r')
        terminal.send(request.encode())
        terminal.until(b'[run] finished: success', 180)
        for name in (b'read', b'edit', b'write', b'bash'):
            assert b'[tool] '+name in terminal.output, 'workflow omitted '+repr(name)
        assert (root / 'example.S').read_bytes() == expected
        assert (root / 'verify.sh').read_text() == script
        artifact = root / 'example-run'
        assert artifact.read_bytes().startswith(b'\x7fELF')
        check = subprocess.run([str(artifact)],env={'PATH':'/usr/bin:/bin'},capture_output=True,check=True,timeout=3)
        assert check.stdout == b'PANGU_WORKFLOW_OK\n'
        assert b'PANGU_WORKFLOW_VERIFIED' in terminal.output and b'finished error:' not in terminal.output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert env['MIMO_API_KEY'].encode() not in (storage / 'session.pgj').read_bytes()
    print('PASS: actual MiMo read/edit/write/bash workflow built and tested handwritten Assembly with exact source preservation', flush=True)

def batch_live(binary, root):
    storage = root / 'batch-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    markers = (b'PANGU_BATCH_FIRST', b'PANGU_BATCH_SECOND')
    for index, name in enumerate(('first-batch.txt', 'second-batch.txt')):
        (root / name).write_bytes(markers[index] + b'\n')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.send(b'Use read for first-batch.txt and read for second-batch.txt in the same assistant response with two tool calls. After both results, report both file markers briefly.\r')
        terminal.until(b'[run] finished: success', 150)
        assert terminal.output.count(b'finished ok:') == 2
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    path = storage / 'session.pgj'
    saved = path.read_bytes()
    assert env['MIMO_API_KEY'].encode() not in saved
    assert checkpoints(path)[-1][1] == 3
    cursor, group, results = 0, None, {}
    while cursor < len(saved):
        version, length = struct.unpack_from('<II', saved, cursor + 8)
        kind, count = struct.unpack_from('<II', saved, cursor + 24)
        rows, offset = [], cursor + 40
        if kind == 0:
            for _ in range(count):
                role, error, text_len, id_len, name_len = struct.unpack_from('<IIIHH', saved, offset)
                offset += 16
                text = saved[offset:offset + text_len]
                offset += text_len
                call_id = saved[offset:offset + id_len]
                offset += id_len
                name = saved[offset:offset + name_len]
                offset += name_len
                rows.append((role, error, text, call_id, name))
                if role == 3:
                    assert call_id not in results and not error
                    results[call_id] = text
            if version == 5:
                assert group is None and count == 3 and [row[0] for row in rows] == [1, 2, 2]
                group = rows[1:]
        cursor += length
    assert group is not None and len(results) == 2
    assert {json.loads(row[2])['path'] for row in group} == {'first-batch.txt', 'second-batch.txt'}
    for row in group:
        assert row[4] == b'read' and row[3] in results
    assert all(any(marker in result for result in results.values()) for marker in markers)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        assert terminal.output.count(b'[history] call read') == 2
        assert terminal.output.count(b'[history] result read') == 2
        assert all(row[3] in terminal.output for row in group)
        assert b'[run] started' not in terminal.output and path.read_bytes() == saved
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert path.read_bytes() == saved
    print('PASS: actual MiMo/TUI two-call group, v5 atomic intent, ordered file results and settled reopen history without model restart', flush=True)

def read_pages_live(binary, root):
    storage = root / 'read-pages-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    marker = 'PANGU_READ_PAGES_' + os.urandom(6).hex()
    content = ''.join(f'row-{i:03d}: abcdefghijklmnopqrstuvwxyz0123456789 repeated source content\n' for i in range(1, 121))
    content += 'END_OF_PAGED_MARKER=' + marker + '\n'
    (root / 'paged.txt').write_text(content)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.send(b'Read paged.txt using read, starting from the beginning. Follow the returned offset continuation until you find END_OF_PAGED_MARKER, then report its exact full value. Use only read, never bash/write/edit.\r')
        terminal.until(b'[run] finished: success', 180)
        assert marker.encode() in re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
        assert terminal.output.count(b'finished ok:') >= 2 and b'finished error:' not in terminal.output
        assert b'[tool] bash' not in terminal.output and b'[tool] edit' not in terminal.output and b'[tool] write' not in terminal.output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    data = (storage / 'session.pgj').read_bytes()
    assert env['MIMO_API_KEY'].encode() not in data and (root / 'paged.txt').read_text() == content
    assert b'[read: more lines; use offset=' in data and marker.encode() in data
    assert checkpoints(storage / 'session.pgj')[-1][1] == 3
    print('PASS: actual MiMo/TUI followed read pagination to an undisclosed late-file marker, without shell or file mutation', flush=True)

def snapshot_edit_live(binary, root):
    storage = root / 'snapshot-edit-storage'
    storage.mkdir()
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage))
    original = b'\n' * 2000 + b'UNIQUE_OLD\n'
    expected = original.replace(b'UNIQUE_OLD', b'UNIQUE_NEW')
    path = root / 'many-lines.txt'
    path.write_bytes(original)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.send(b'Use edit exactly once on many-lines.txt to replace UNIQUE_OLD with UNIQUE_NEW. Preserve every other byte. Do not use bash or write. Then use read with offset 2001 and limit 1 to verify the changed line. Report the result briefly.\r')
        terminal.until(b'[run] finished: success', 150)
        assert b'[tool] edit' in terminal.output and b'[tool] read' in terminal.output
        assert b'finished error:' not in terminal.output and b'[tool] bash' not in terminal.output and b'[tool] write' not in terminal.output
        assert path.read_bytes() == expected
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    data = (storage / 'session.pgj').read_bytes()
    assert b'pangu.native.read-write-edit-bash-v2' in data and env['MIMO_API_KEY'].encode() not in data
    assert checkpoints(storage / 'session.pgj')[-1][1] == 3
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        assert b'[history] result edit' in terminal.output and b'[run] started' not in terminal.output
        assert path.read_bytes() == expected and (storage / 'session.pgj').read_bytes() == data
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual MiMo/TUI edit beyond the model read page preserved all 2000 leading newlines, verified with ranged read and settled reopen', flush=True)

def prompt_file_live(binary, root):
    storage = root / 'prompt-source-storage'
    storage.mkdir()
    path = root / 'rules.md'
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage), PANGU_SYSTEM_PROMPT_FILE='rules.md')
    for attempt in range(2):
        marker = 'PANGU_PROMPT_' + os.urandom(6).hex()
        instructions = 'Reply with exactly ' + marker + ' and no other text. Do not use any tools.'
        path.write_text(instructions)
        terminal = Terminal(binary, root, env)
        try:
            terminal.until(b'\r\n> ', 5)
            terminal.output = b''
            terminal.send(b'Say hello.\r')
            terminal.until(b'[run] finished: success', 120)
            clean = re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
            if marker.encode() not in clean or b'[tool]' in clean:
                data = (storage / 'session.pgj').read_bytes()
                cursor, system, assistant = 0, b'', b''
                while cursor < len(data):
                    version, length = struct.unpack_from('<II', data, cursor + 8)
                    kind, count = struct.unpack_from('<II', data, cursor + 24)
                    if kind == 1:
                        system = data[cursor + 40:cursor + length - 16]
                    if kind == 0:
                        offset = cursor + 40
                        for _ in range(count):
                            role, error, text_len, id_len, name_len = struct.unpack_from('<IIIHH', data, offset)
                            offset += 16
                            if role == 1:
                                assistant = data[offset:offset + text_len]
                            offset += text_len + id_len + name_len
                    cursor += length
                expected_hash = 0xcbf29ce484222325
                for byte in instructions.encode():
                    expected_hash = ((expected_hash ^ byte) * 0x100000001b3) & 0xffffffffffffffff
                print('prompt diagnostic: attempt=', attempt, 'expected hash=', hex(expected_hash), 'latest system matches=', system == instructions.encode(),
                      'last assistant=', repr(assistant[:300].replace(env['MIMO_API_KEY'].encode(), b'[REDACTED]')),
                      'terminal tail=', repr(clean[-300:].replace(env['MIMO_API_KEY'].encode(), b'[REDACTED]')), flush=True)
            assert marker.encode() in clean and b'[tool]' not in clean
            terminal.send(b'\x04')
            terminal.finish()
        finally:
            terminal.close()
        data = (storage / 'session.pgj').read_bytes()
        assert instructions.encode() in data and env['MIMO_API_KEY'].encode() not in data
        assert checkpoints(storage / 'session.pgj')[-1][1] == 3
    marker = 'PANGU_FROZEN_' + os.urandom(6).hex()
    path.write_text('Start with ' + marker + ' then give 12 numbered facts about C pointers, each at most six words. Do not use tools.')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.output = b''
        terminal.send(b'Proceed.\r')
        terminal.until(marker.encode())
        terminal.send(b'\x04')
        terminal.finish()
        assert b'[run] finished:' not in terminal.output
    finally:
        terminal.close()
    assert checkpoints(storage / 'session.pgj')[-1][1] == 1
    path.unlink()
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'[run] finished: success', 150)
        assert marker.encode() in re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', terminal.output)
        assert not path.exists()
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert checkpoints(storage / 'session.pgj')[-1][1] == 3
    terminal = Terminal(binary, root, env)
    try:
        terminal.finish(expected=2)
    finally:
        terminal.close()
    print('PASS: actual MiMo used the explicit system file, idle reopen reloaded it, active resume used its stored prompt after source deletion, and missing idle source failed without fallback', flush=True)

def prompt_snapshot_live(binary, root):
    """Check real persistent source lifetime separately from model obedience."""
    storage = root / 'prompt-snapshot-storage'
    storage.mkdir()
    source = root / 'snapshot-rules.md'
    instructions = b'Answer concisely. Use only the supplied conversation. Do not use tools.'
    source.write_bytes(instructions)
    env = dict(os.environ, PANGU_SESSION_DIR=str(storage), PANGU_SYSTEM_PROMPT_FILE=source.name)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'\r\n> ', 5)
        terminal.output = b''
        terminal.send(b'Give twelve short numbered facts about C pointers.\r')
        terminal.until(b'[run] started', 20)
        terminal.send(b'\x04')
        terminal.finish()
        assert b'[run] finished:' not in terminal.output, 'run settled before suspension'
    finally:
        terminal.close()
    journal = storage / 'session.pgj'
    before = journal.read_bytes()
    assert checkpoints(journal)[-1][1] == 1
    assert instructions in before and env['MIMO_API_KEY'].encode() not in before
    source.unlink()
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'[run] finished: success', 150)
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    after = journal.read_bytes()
    assert after.startswith(before) and not source.exists()
    assert checkpoints(journal)[-1][1] == 3
    cursor, systems, assistants = 0, [], []
    while cursor < len(after):
        version, length = struct.unpack_from('<II', after, cursor + 8)
        kind, count = struct.unpack_from('<II', after, cursor + 24)
        if kind == 1:
            systems.append(after[cursor + 40:cursor + length - 16])
        if kind == 0:
            offset = cursor + 40
            for _ in range(count):
                role, error, text_len, id_len, name_len = struct.unpack_from('<IIIHH', after, offset)
                offset += 16
                if role == 1:
                    assistants.append(after[offset:offset + text_len])
                offset += text_len + id_len + name_len
        cursor += length
    assert cursor == len(after) and systems == [instructions]
    assert len(assistants) == 1 and assistants[0], 'resume must commit one complete assistant'
    assert env['MIMO_API_KEY'].encode() not in after
    terminal = Terminal(binary, root, env)
    try:
        terminal.finish(expected=2)
    finally:
        terminal.close()
    assert journal.read_bytes() == after, 'failed idle prompt load changed saved history'
    print('PASS: real MiMo resume after prompt source deletion preserved the committed system and prior journal, settled once, and missing idle source failed without history changes; model obedience is tested separately', flush=True)


def prompt_protocol_live():
    """Direct provider probe: same instructions, only stream/tools vary; no tool executes."""
    import urllib.request
    key = os.environ.get('MIMO_API_KEY')
    assert key and '\r' not in key and '\n' not in key, 'valid MIMO_API_KEY required'
    marker = 'PANGU_PROTOCOL_' + os.urandom(6).hex()
    failures = []
    for streaming, with_tools in ((False, False), (True, False), (False, True), (True, True)):
        body = {
            'model': os.environ.get('MIMO_MODEL', 'mimo-v2.6-pro'),
            'messages': [
                {'role': 'system', 'content': 'Reply with exactly ' + marker + ' and no other text. Do not use any tools.'},
                {'role': 'user', 'content': 'Say hello.'},
            ],
            'max_tokens': 512, 'thinking': {'type': 'disabled'}, 'stream': streaming,
        }
        if with_tools:
            body['tools'] = [{'type': 'function', 'function': {
                'name': 'read', 'description': 'Read a file',
                'parameters': {'type': 'object', 'properties': {'path': {'type': 'string'}}, 'required': ['path']},
            }}]
        request = urllib.request.Request(
            'https://token-plan-cn.xiaomimimo.com/v1/chat/completions',
            data=json.dumps(body).encode(),
            headers={'Content-Type': 'application/json', 'Authorization': 'Bearer ' + key},
        )
        content, calls, done, finished, received = '', False, False, False, 0
        with urllib.request.urlopen(request, timeout=55) as response:
            if streaming:
                for line in response:
                    received += len(line)
                    assert received <= 1048576, 'provider probe response exceeds bound'
                    if not line.startswith(b'data: '):
                        continue
                    if line.strip() == b'data: [DONE]':
                        done = True
                        break
                    event = json.loads(line[6:])
                    assert 'error' not in event, 'provider returned an SSE error'
                    for choice in event.get('choices', []):
                        delta = choice.get('delta', {})
                        content += delta.get('content') or ''
                        calls |= bool(delta.get('tool_calls'))
                        finished |= choice.get('finish_reason') == 'stop'
                assert done and finished, 'provider probe did not finish cleanly'
            else:
                raw = response.read(1048577)
                assert len(raw) <= 1048576, 'provider probe response exceeds bound'
                choice = json.loads(raw)['choices'][0]
                content = choice['message'].get('content') or ''
                calls = bool(choice['message'].get('tool_calls'))
                assert choice.get('finish_reason') == 'stop', 'provider probe did not stop normally'
        matched = content.strip() == marker and not calls
        print('prompt protocol: stream=' + str(streaming) + ' tools=' + str(with_tools) +
              ' exact_instruction=' + str(matched) + ' tool_call=' + str(calls), flush=True)
        if not matched:
            failures.append((streaming, with_tools))
    assert not failures, 'direct provider instruction mismatches (stream, tools): ' + repr(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live', action='store_true')
    parser.add_argument('--live-persistent', action='store_true')
    parser.add_argument('--live-write', action='store_true')
    parser.add_argument('--live-edit', action='store_true')
    parser.add_argument('--live-bash', action='store_true')
    parser.add_argument('--live-bash-recovery', action='store_true')
    parser.add_argument('--live-workflow', action='store_true')
    parser.add_argument('--live-batch', action='store_true')
    parser.add_argument('--live-read-pages', action='store_true')
    parser.add_argument('--live-snapshot-edit', action='store_true')
    parser.add_argument('--live-prompt-file', action='store_true')
    parser.add_argument('--live-prompt-snapshot', action='store_true')
    parser.add_argument('--live-prompt-protocol', action='store_true', help='direct real-provider stream/tools instruction matrix; no TUI build')
    args = parser.parse_args()
    if args.live_prompt_protocol:
        prompt_protocol_live()
        return
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
                terminal.until(b'\r\n> ', 5)
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
        terminal = Terminal(binary, root / 'missing-workspace', env)
        try:
            terminal.until(b'Cannot activate native plugin workspace', 5)
            terminal.finish(expected=2)
        finally:
            terminal.close()
        print('PASS: missing workspace fails native plugin activation and preserves terminal attributes', flush=True)
        for name in ('missing-storage', 'corrupt-storage'):
            location = root / name
            if name == 'corrupt-storage':
                location.mkdir()
                (location / 'session.pgj').write_bytes(b'invalid-complete-journal-frame' * 4)
            terminal = Terminal(binary, root, dict(env, PANGU_SESSION_DIR=str(location)))
            try:
                terminal.until(b'Cannot open persistent session', 5)
                terminal.finish(expected=2)
            finally:
                terminal.close()
        print('PASS: storage source open/replay failure closes composition and preserves terminal attributes', flush=True)
        persistent_idle(binary, root, env)
        if args.live_persistent:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-persistent'
            persistent_live(binary, root)
        if args.live_write:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-write'
            write_live(binary, root)
        if args.live_edit:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-edit'
            edit_live(binary, root)
        if args.live_prompt_snapshot:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-prompt-snapshot'
            prompt_snapshot_live(binary, root)
        if args.live_prompt_file:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-prompt-file'
            prompt_file_live(binary, root)
        if args.live_snapshot_edit:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-snapshot-edit'
            snapshot_edit_live(binary, root)
        if args.live_read_pages:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-read-pages'
            read_pages_live(binary, root)
        if args.live_batch:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-batch'
            batch_live(binary, root)
        if args.live_workflow:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-workflow'
            workflow_live(binary, root)
        if args.live_bash_recovery:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-bash-recovery'
            bash_recovery_live(binary, root)
        if args.live_bash:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live-bash'
            bash_live(binary, root)
        if args.live:
            assert os.environ.get('MIMO_API_KEY'), 'MIMO_API_KEY required for --live'
            live_env = os.environ.copy()
            live_env.pop('PANGU_SESSION_DIR', None)
            terminal = Terminal(binary, root, live_env)
            try:
                terminal.until(b'\r\n> ', 5)
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
