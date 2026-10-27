"""TUI PTY 回放：真实 read/termios/SIGWINCH 路径验证 U01/U02 输入与粘贴行为。

驱动 build.sh 产出的 pangu-tui-demo（假 Runtime），在真 PTY 上核对：
  - 启动启用 bracketed paste（ESC[?2004h），退出（EOF/闲置^C/信号/poll错误）
    先关粘贴再还原 termios；
  - 中文/emoji 编辑（完整码点才重绘）、Alt+Enter 多行、Left 定位插入（光标列）、
    Backspace、窄终端自动折行、SIGWINCH resize 草稿保留；
  - 括号粘贴任意分片：CRLF 折叠、粘贴中 CR/^C/^D/ESC 不提交不退出、
    未完成标记可恢复、1023 字节上限只报一次并继续消费；
  - 文本与工具流式输出期间草稿保留、事件后可继续提交。

用法：python3 -B tools/client/tui/test_tui_pty.py [构建目录]
"""
from pathlib import Path
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import time


ROOT = Path(__file__).resolve().parents[3]
BUILD = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(
    os.environ.get('PANGU_TUI_BUILD_DIR',
                   f"{os.environ.get('TMPDIR', '/tmp')}/pangu-tui-build-{os.getuid()}"))
DEMO = BUILD / 'pangu-tui-demo'

PASTE_ON = b'\x1b[?2004h'
PASTE_OFF = b'\x1b[?2004l'
LIMIT_MSG = '[tui] 输入已达长度上限'.encode()
RUN_STARTED = b'[run] started'
RUN_FINISHED_OK = b'[run] finished: success'


class Demo:
    """一个场景一个 PTY；输出累积在 self.output，secret 机制不需要（假 Runtime）。"""

    def __init__(self, scenario, cols=80, rows=24, tick=5):
        assert DEMO.exists(), f'missing {DEMO}; run tools/client/tui/build.sh first'
        self.master, self.slave = pty.openpty()
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        self.saved = termios.tcgetattr(self.slave)
        self.output = b''
        self.process = subprocess.Popen(
            [str(DEMO), '--demo', scenario, '--tick', str(tick)],
            stdin=self.slave, stdout=self.slave, stderr=self.slave,
            cwd=str(ROOT))
        os.close(self.slave)

    def pump(self, timeout=0.1):
        readable, _, _ = select.select([self.master], [], [], timeout)
        if not readable:
            return False
        try:
            data = os.read(self.master, 65536)
        except OSError:
            return False  # 子进程退出后 master 读 EIO：视作 EOF
        if not data:
            return False
        self.output += data
        assert len(self.output) <= 262144, 'PTY transcript limit exceeded'
        return True

    def until(self, marker, timeout=15):
        deadline = time.monotonic() + timeout
        while marker not in self.output:
            if time.monotonic() >= deadline:
                raise AssertionError(
                    f'marker not received: {marker!r}; tail={self.output[-400:]!r}')
            if self.process.poll() is not None and not self.pump(0):
                raise AssertionError(
                    f'exited before {marker!r}; tail={self.output[-400:]!r}')
            self.pump(0.05)

    def send(self, data):
        os.write(self.master, data)

    def resize(self, cols, rows):
        fcntl.ioctl(self.master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))

    def finish(self, expected=0):
        code = self.process.wait(timeout=10)
        assert code == expected, f'exit {code}, expected {expected}'
        assert termios.tcgetattr(self.master) == self.saved, 'termios not restored'
        os.close(self.master)

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=5)
        try:
            os.close(self.master)
        except OSError:
            pass

    def alive(self):
        return self.process.poll() is None


def wait_idle(demo):
    """等假 Runtime 就绪（banner + 粘贴模式 + 首个提示符）。"""
    demo.until(PASTE_ON, 5)
    demo.until(b'[tui] ', 5)
    demo.until(b'> ', 5)
    time.sleep(0.1)
    demo.pump(0.1)


def case_startup_and_exit_paths():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'\x04')  # ^D：EOF 退出
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()

    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'\x03')  # 闲置 ^C：退出且不进 Runtime
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: startup_and_exit_paths')


def case_prompt_history_draft():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send('历史🙂'.encode() + b'\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'draft\x1b[H\x1b[A')
        d.until(b'> ' + '历史🙂'.encode(), 5)
        # Down at the end restores exact draft and its saved start cursor.
        d.send(b'\x1b[F\x1b[B')
        d.until(b'> draft', 5)
        d.send(b'!')
        d.until(b'> !draft', 5)
        d.send(b'\x15\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: prompt_history_draft')


def case_visual_vertical_edit():
    d = Demo('no_tool', cols=6)
    try:
        wait_idle(d)
        d.send(b'abcdefghij\x1b[A!')
        d.until(b'> abc!', 5)
        assert RUN_STARTED not in d.output
        d.send(b'\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: visual_vertical_edit')


def case_help_preserves_draft():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'draft\x1b[D\x1bOP')
        d.until(b'[help] PanGu', 5)
        d.send(b'!')
        d.until(b'> draf!t', 5)
        assert RUN_STARTED not in d.output
        d.send(b'\x15\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: help_preserves_draft')


def case_hotkeys_local_command():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'/hotkeys\r')
        d.until(b'[help] PanGu', 5)
        assert RUN_STARTED not in d.output
        # Command consumed its editor text. A normal prompt still submits.
        d.send(b'hello\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'/hotkeysX\r')
        d.until(RUN_FINISHED_OK, 15)
        assert b'[help] PanGu' not in d.output
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    d = Demo('no_tool')
    try:
        wait_idle(d)
        # A multiline prompt containing the command is model input.
        d.send(b'/hotkeys\x1b\rcomment\r')
        d.until(RUN_FINISHED_OK, 15)
        assert b'[help] PanGu' not in d.output
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    d = Demo('tool_roundtrip', tick=100)
    try:
        wait_idle(d)
        d.send(b'go\r')
        d.until(RUN_STARTED, 5)
        d.send(b'/hotkeys\r')
        d.until(b'[help] PanGu', 5)
        d.until(RUN_FINISHED_OK, 15)
        assert d.output.count(RUN_STARTED) == 1
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: hotkeys_local_command_and_exact_match')


def case_quit_command():
    for active in (False, True):
        d = Demo('tool_roundtrip' if active else 'no_tool', tick=100)
        try:
            wait_idle(d)
            if active:
                d.send(b'go\r')
                d.until(RUN_STARTED, 5)
            d.send(b'/quit\r')
            d.until('/quit 退出'.encode(), 5)
            d.until(PASTE_OFF, 5)
            d.finish(0)
            assert d.output.count(RUN_STARTED) == int(active)
            if active:
                assert RUN_FINISHED_OK not in d.output
        finally:
            d.close()
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'/quitX\r')
        d.until(RUN_FINISHED_OK, 15)
        assert d.alive()
        d.send(b'/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: quit_idle_active_and_exact_match')


def case_local_command_completion():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'/\t')
        d.until(b'[commands] /hotkeys  /quit', 5)
        d.send(b'h\t\r')
        d.until(b'[help] PanGu', 5)
        assert RUN_STARTED not in d.output
        d.send(b'/q\t\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: local_command_completion')


def case_command_choice_focus():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'/\t\x03!')
        d.until(b'> /!', 5)
        assert d.alive() and RUN_STARTED not in d.output
        d.send(b'\x15/\t\x1b[B\r')
        d.until(b'> /quit', 5)
        assert d.alive()  # Enter accepted the choice, did not execute it.
        d.send(b'\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    d = Demo('tool_roundtrip', tick=100)
    try:
        wait_idle(d)
        d.send(b'go\r')
        d.until(RUN_STARTED, 5)
        d.send(b'/\t\x03')
        d.until(b'[commands]', 5)
        d.until(RUN_FINISHED_OK, 15)
        assert b'[run] finished: cancelled' not in d.output
        d.send(b'\x15/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: command_choice_accept_cancel_focus')


def case_reverse_history_recall():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'older needle\r')
        d.until(RUN_FINISHED_OK, 15)
        d.output = b''
        d.send(b'newer NEEDLE\r')
        d.until(RUN_FINISHED_OK, 15)
        d.output = b''
        d.send(b'needle\x12needle')
        d.until(b'[match] newer NEEDLE', 5)
        d.output = b''
        d.send(b'\x12')
        d.until(b'[match] older needle', 5)
        d.output = b''
        d.send(b'\x03!')
        d.until(b'> needle!', 5)
        assert d.alive() and RUN_STARTED not in d.output
        d.send(b'\x15/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: reverse_history_recall_restore')


def case_escape_control_cancel():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'original\x12query')
        d.until(b'[history search]', 5)
        d.output = b''
        d.send(b'\x1b')
        deadline = time.monotonic() + 0.05
        while time.monotonic() < deadline:
            d.pump(min(0.01, deadline - time.monotonic()))
        d.until(b'> original', 5)
        d.send(b'!')
        d.until(b'> original!', 5)
        d.send(b'\x15/\t')
        d.until(b'[commands]', 5)
        d.output = b''
        d.send(b'\x1b')
        deadline = time.monotonic() + 0.05
        while time.monotonic() < deadline:
            d.pump(min(0.01, deadline - time.monotonic()))
        d.until(b'> /', 5)
        d.send(b'quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    d = Demo('tool_roundtrip', tick=100)
    try:
        wait_idle(d)
        d.send(b'go\r')
        d.until(RUN_STARTED, 5)
        d.send(b'original\x12query')
        d.until(b'[history search]', 5)
        fcntl.ioctl(d.master, termios.TIOCSWINSZ, struct.pack('HHHH', 20, 40, 0, 0))
        d.process.send_signal(signal.SIGWINCH)
        d.output = b''
        d.send(b'\x1b')
        deadline = time.monotonic() + 0.05
        while time.monotonic() < deadline:
            d.pump(min(0.01, deadline - time.monotonic()))
        d.until(b'> original', 5)
        d.until(RUN_FINISHED_OK, 15)
        assert b'[run] finished: cancelled' not in d.output
        d.send(b'\x15/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: standalone_escape_closes_controls')


def case_partial_utf8_never_submits():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'go\xe4\r')
        d.pump(0.1)
        assert RUN_STARTED not in d.output
        d.send(b'\x7f\r')
        d.until(RUN_FINISHED_OK, 15)
        assert b'\xe4\x1b[' not in d.output
        d.send(b'/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: partial_utf8_never_submits')


def case_stream_preserves_partial_utf8():
    d = Demo('tool_roundtrip', tick=100)
    try:
        wait_idle(d)
        d.send(b'go\r')
        d.until(RUN_STARTED, 5)
        d.send(b'ab\x1b[D\xe4')
        d.until(RUN_FINISHED_OK, 15)
        assert b'\xe4b' not in d.output
        d.send(b'\xb8\xad')
        d.until('a中b'.encode(), 5)
        d.send(b'\x15/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: stream_preserves_partial_utf8')


def case_reading_keeps_anchor_and_draft():
    d = Demo('tool_roundtrip', tick=100)
    try:
        wait_idle(d)
        d.send(b'go\r')
        d.until(RUN_STARTED, 5)
        d.send(b'draft\x1b[5~')
        d.until(b'\x1b[?1049h', 5)
        d.until(b'[history] PgUp/PgDn', 5)
        # Source events continue while the physical reading screen is frozen.
        deadline = time.monotonic()+1
        while time.monotonic()<deadline:
            d.pump(0.02)
        d.output = b''
        fcntl.ioctl(d.master, termios.TIOCSWINSZ, struct.pack('HHHH', 8, 30, 0, 0))
        d.process.send_signal(signal.SIGWINCH)
        d.until(b'[history] PgUp/PgDn', 5)
        d.send(b'\x04')  # return to latest; never backspace the nonempty draft
        d.until(b'\x1b[?1049l', 5)
        d.until(b'> draft', 5)
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'!')
        d.until(b'> draft!', 5)
        d.send(b'\x15/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'draft\x1b[5~')
        d.until(b'\x1b[?1049h', 5)
        d.process.send_signal(signal.SIGTERM)
        d.until(b'\x1b[?1049l', 5)
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: reading_keeps_anchor_and_draft')


def case_read_long_line_visual_pages():
    d = Demo('no_tool', cols=8, rows=6)
    try:
        wait_idle(d)
        d.send(b'0123456789abcdefghijklmnop\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x1b[5~' * 20)
        d.until(b'[user] 0', 5)
        deadline=time.monotonic()+0.1
        while time.monotonic()<deadline:
            d.pump(0.01)
        d.output=b''
        d.send(b'\x1b[6~')
        d.until(b'9abcdefg\r\nhijklmno\r\np\r\n', 5)
        d.send(b'\r/quit\r')
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: read_long_line_visual_pages')


def case_reader_one_cell_resize():
    d = Demo('no_tool', cols=8, rows=6)
    try:
        wait_idle(d)
        d.send(b'abcdefghijk\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x1b[5~')
        d.until(b'\x1b[?1049h', 5)
        deadline=time.monotonic()+0.1
        while time.monotonic()<deadline:
            d.pump(0.01)
        d.output=b''
        fcntl.ioctl(d.master, termios.TIOCSWINSZ, struct.pack('HHHH', 1, 1, 0, 0))
        d.process.send_signal(signal.SIGWINCH)
        d.until(b'\x1b[H\x1b[2J', 5)
        deadline=time.monotonic()+0.1
        while time.monotonic()<deadline:
            d.pump(0.01)
        assert b'\r\n' not in d.output, 'one-row reading page scrolled its only row'
        assert b'\x1b[?1049l' not in d.output
        d.send(b'\r/quit\r')
        d.until(b'\x1b[?1049l', 5)
        d.until(PASTE_OFF, 5)
        d.finish(0)
    finally:
        d.close()
    print('ok: reader_one_cell_resize_no_scroll')


def case_multiline_edit_submit():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send('你好🙂'.encode())          # 中文 + emoji
        d.until('你好🙂'.encode(), 5)
        # 多字节序列只在完整码点后重绘：不得出现半个序列紧接转义
        assert b'\xe4\x1b[' not in d.output and b'\xf0\x1b[' not in d.output
        d.send(b'\x1b[D')                   # Left：移到 🙂 前
        d.until(b'\x1b[7G', 5)              # 真实光标到第 7 列（提示2+你好4）
        d.send(b'!')
        d.until(b'\xe5\xa5\xbd!', 5)        # "好!"：中间插入生效
        d.send(b'\x1b\r')                   # Alt+Enter：换行
        d.send(b'cd')
        d.until(b'\r\ncd', 5)               # 多行草稿：真实换行渲染
        d.send(b'\x1b[D')                   # Left 到 'd' 前
        d.send(b'X')
        d.until(b'cXd', 5)
        d.send(b'\x7f')                     # Backspace 删 X
        d.send(b'Q')                        # 哨兵：退格成功则得 cQd 而非 cXQd
        d.until(b'cQd', 5)
        assert b'cXQd' not in d.output
        assert d.alive() and RUN_STARTED not in d.output
        d.send(b'\r')                       # Enter 提交
        d.until(RUN_STARTED, 5)
        assert d.output.rindex(b'cQd') < d.output.index(RUN_STARTED)
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: multiline_edit_submit')


def case_paste_safety():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        # 粘贴含 CRLF、^C、^D、孤立 ESC：不提交、不取消、不退出
        d.send(b'\x1b[200~x\r\n\x03\x04\x1b')
        time.sleep(0.15)
        d.pump(0.1)
        assert d.alive(), 'paste ^C/^D must not exit'
        assert RUN_STARTED not in d.output, 'CR inside paste must not submit'
        d.send(b'y\x1b[201~')
        d.until(b'x\r\ny', 5)               # CRLF 折叠为一个换行
        d.send(b'z')                        # 结束后正常编辑
        d.until(b'x\r\nyz', 5)
        d.send(b'\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: paste_safety')


def case_unfinished_paste_recovery():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'\x1b[200~abc')             # 未完成粘贴
        for b in (b'\r', b'\x03', b'\x04'):
            d.send(b)
            time.sleep(0.05)
            d.pump(0.02)
        assert d.alive() and RUN_STARTED not in d.output
        d.send(b'\x1b[201~')                # 结束标记收尾：尾部 CR 折叠为 LF
        d.until(b'abc\r\n', 5)
        d.send(b'd')                        # 恢复正常编辑
        d.until(b'abc\r\nd', 5)
        d.send(b'\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: unfinished_paste_recovery')


def case_capacity_limit():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'\x1b[200~')
        payload = b'a' * 1023 + b'b' * 40   # 1023 恰满，其余整字符拒绝
        for i in range(0, len(payload), 300):  # 强制跨 read() 分片
            d.send(payload[i:i + 300])
            time.sleep(0.02)
        d.send(b'\x1b[201~')
        d.until(LIMIT_MSG, 5)
        time.sleep(0.2)
        d.pump(0.2)
        assert d.output.count(LIMIT_MSG) == 1, 'overflow notice must be throttled'
        assert d.alive()
        d.send(b'\x7f')                     # 退一格后仍可编辑
        d.send(b'z')
        d.send(b'\r')
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: capacity_limit')


def case_resize_keeps_draft():
    d = Demo('no_tool')
    try:
        wait_idle(d)
        d.send(b'hi draft')
        d.until(b'hi draft', 5)
        d.resize(40, 12)                    # SIGWINCH：窄终端重绘
        time.sleep(0.2)
        d.pump(0.2)
        assert d.alive()
        assert b'hi draft' in d.output
        d.send(b'\r')                       # 草稿仍在，提交闭环
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: resize_keeps_draft')


def case_signal_exit_restores_terminal():
    for sig in ('SIGINT', 'SIGTERM', 'SIGHUP'):
        d = Demo('no_tool')
        try:
            wait_idle(d)
            d.process.send_signal(getattr(signal, sig))
            d.until(PASTE_OFF, 5)
            d.finish(0)                     # 信号路径同为恢复后的正常退出
        finally:
            d.close()
    print('ok: signal_exit_restores_terminal')


def case_streaming_keeps_draft():
    d = Demo('tool_roundtrip')
    try:
        wait_idle(d)
        d.send(b'go\r')                     # 触发一轮文本+工具流式输出
        d.until(RUN_STARTED, 5)
        d.send(b'draft')                    # 流式期间输入新草稿
        d.until(b'draft', 5)
        d.until(b'[tool] \x1b[1mread_file\x1b[22m (#call-1)', 5)
        d.until(RUN_FINISHED_OK, 15)
        # 流式/工具输出结束后草稿仍在输入区（事件挂起/恢复不丢草稿）
        d.until(b'> draft', 5)
        d.send(b'\r')                       # 草稿可继续提交
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: streaming_keeps_draft')


def case_narrow_autowrap():
    d = Demo('no_tool', cols=10, rows=24)
    try:
        wait_idle(d)
        d.send(b'abcdefghij')               # 提示符 2 列 + 8 字符恰满一行
        d.until(b'abcdefgh\r\nij', 5)       # 后续字符折到下一视觉行
        d.send(b'\r')                       # 提交字节不受折行影响
        d.until(RUN_FINISHED_OK, 15)
        d.send(b'\x04')
        d.finish(0)
    finally:
        d.close()
    print('ok: narrow_autowrap')


def case_poll_error_failure_path():
    d = Demo('poll_error')
    try:
        wait_idle(d)
        d.send(b'go\r')
        d.until(b'[run] started', 5)
        d.until(b'\xe5\x87\xba\xe9\x94\x99\xe5\x89\x8d', 5)  # "出错前"：先有输出
        # 脚本播完后 poll 返回错误：不补造终态，直接失败退出（码 2）
        code = d.process.wait(timeout=10)
        assert code == 2, f'exit {code}, expected 2'
        assert b'finished:' not in d.output.split(b'[run] started')[-1], \
            'poll error must not fabricate a terminal state'
        d.until(PASTE_OFF, 5)              # 失败路径同样先关粘贴再还原
        assert termios.tcgetattr(d.master) == d.saved, 'termios not restored'
        os.close(d.master)
    finally:
        d.close()
    print('ok: poll_error_failure_path')


def case_product_prompt_history(binary):
    """Real product/storage; a local refused connection produces a model error.

    The committed user receipt must remain editable after reopening regardless
    of the model outcome. No external service or credential is required.
    """
    import socket
    import tempfile
    sys.path.insert(0, str(ROOT / 'tools/runtime'))
    from test_pangu_pty import Terminal, stored_bytes
    with tempfile.TemporaryDirectory(prefix='pangu-tui-history-') as directory:
        root = Path(directory)
        # Reserve a non-listening port for deterministic local connection refusal.
        with socket.socket() as reserved:
            reserved.bind(('127.0.0.1', 0))
            port = reserved.getsockname()[1]
            env = {k:v for k,v in os.environ.items() if not k.startswith(('PANGU_', 'MIMO_'))}
            env.update(PANGU_MODEL_ENDPOINT=f'https://127.0.0.1:{port}/chat/completions',
                       PANGU_MODEL_NAME='offline-history', PANGU_MODEL_API_KEY='offline-placeholder')
            prompt = 'persist中🙂'.encode()
            terminal = Terminal(Path(binary), root, env)
            try:
                terminal.until(b'> ', 5)
                terminal.send(prompt + b'\r')
                terminal.until(b'[run] finished: error', 15)
                terminal.send(b'/quit\r')
                terminal.until(PASTE_OFF, 5)
                terminal.finish()
            finally:
                terminal.close()
            original = stored_bytes(root / '.pangu')
            terminal = Terminal(Path(binary), root, env)
            try:
                terminal.until(b'> ', 5)
                assert prompt in terminal.output and b'[history]' in terminal.output
                assert RUN_STARTED not in terminal.output
                terminal.output = b''
                terminal.send(b'\x1b[A')
                terminal.until(b'> ' + prompt, 5)
                terminal.output = b''
                terminal.send(b'\x15\x12PERSIST')
                terminal.until(b'[match] ' + prompt, 5)
                terminal.send(b'\x03/quit\r')
                terminal.until(PASTE_OFF, 5)
                terminal.finish()
                assert RUN_STARTED not in terminal.output
            finally:
                terminal.close()
            assert stored_bytes(root / '.pangu') == original, 'idle history browsing changed storage'
    print('ok: product_failed_run_reopen_prompt_history_no_execution')


def main():
    if not DEMO.exists():
        subprocess.run(['bash', str(ROOT / 'tools/client/tui/build.sh')], check=True)
    case_startup_and_exit_paths()
    case_prompt_history_draft()
    case_visual_vertical_edit()
    case_help_preserves_draft()
    case_hotkeys_local_command()
    case_quit_command()
    case_local_command_completion()
    case_command_choice_focus()
    case_reverse_history_recall()
    case_escape_control_cancel()
    case_partial_utf8_never_submits()
    case_stream_preserves_partial_utf8()
    case_reading_keeps_anchor_and_draft()
    case_read_long_line_visual_pages()
    case_reader_one_cell_resize()
    case_multiline_edit_submit()
    case_paste_safety()
    case_unfinished_paste_recovery()
    case_capacity_limit()
    case_signal_exit_restores_terminal()
    case_streaming_keeps_draft()
    case_resize_keeps_draft()
    case_narrow_autowrap()
    case_poll_error_failure_path()
    if os.environ.get('PANGU_TUI_PRODUCT'):
        case_product_prompt_history(os.environ['PANGU_TUI_PRODUCT'])
    print('test_tui_pty: all passed')


if __name__ == '__main__':
    main()
