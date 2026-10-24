"""Build the exact first Runtime subset; artifacts stay outside the repository."""
from pathlib import Path
import subprocess
import tempfile
import sys
import os
ROOT = Path(__file__).resolve().parents[2]
from build_pangu import implementation
SOURCES = [implementation('src/runtime/agent'), implementation('src/runtime/session'), str(ROOT / 'tests/runtime/test_agent.c')]

def main():
    with tempfile.TemporaryDirectory(prefix='pangu-agent-') as directory:
        if '--live' in sys.argv or '--live-session' in sys.argv:
            if not os.environ.get('MIMO_API_KEY'):
                raise SystemExit('MIMO_API_KEY is required; never put credentials in source or arguments')
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            output = str(Path(directory) / 'live-mimo')
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                            implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/mimo'),
                            implementation('src/runtime/tools'), implementation('src/runtime/read_tool'), str(ROOT / ('tests/runtime/live_session.c' if '--live-session' in sys.argv else 'tests/runtime/live_mimo.c')), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=350)
            return 0
        for mode in ('-O0', '-O2'):
            session_test = str(Path(directory) / 'test-session')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/session'),
                            implementation('src/runtime/agent'), str(ROOT / 'tests/runtime/test_session_commit.c'), '-o', session_test], check=True)
            subprocess.run([session_test], check=True, timeout=10)
            output = str(Path(directory) / 'test-agent')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-pedantic', mode, '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *SOURCES, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/mimo'), str(ROOT / 'tests/runtime/test_mimo_stream.c'),
                            *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/tools'), str(ROOT / 'tests/runtime/test_tools.c'),
                            *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/tools'), implementation('src/runtime/read_tool'),
                            str(ROOT / 'tests/runtime/test_read_tool.c'), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/mimo'),
                            str(ROOT / 'tests/runtime/test_projection.c'), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'), implementation('src/runtime/session'),
                            implementation('src/client/tui/runtime_bridge'), str(ROOT / 'tests/client/tui/test_runtime_bridge.c'),
                            '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'), implementation('src/runtime/session'),
                            str(ROOT / 'tests/runtime/agent_abi.S'), str(ROOT / 'tests/runtime/test_agent_asm.c'),
                            '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
