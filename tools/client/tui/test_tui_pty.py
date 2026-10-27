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
        d.until(b'[tool] read_file (#call-1)', 5)
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


def main():
    if not DEMO.exists():
        subprocess.run(['bash', str(ROOT / 'tools/client/tui/build.sh')], check=True)
    case_startup_and_exit_paths()
    case_multiline_edit_submit()
    case_paste_safety()
    case_unfinished_paste_recovery()
    case_capacity_limit()
    case_signal_exit_restores_terminal()
    case_streaming_keeps_draft()
    case_resize_keeps_draft()
    case_narrow_autowrap()
    case_poll_error_failure_path()
    print('test_tui_pty: all passed')


if __name__ == '__main__':
    main()
