"""Build the exact first Runtime subset; artifacts stay outside the repository."""
from pathlib import Path
import subprocess
import tempfile
import sys
import os
ROOT = Path(__file__).resolve().parents[2]
SOURCES = ['src/runtime/agent.c', 'tests/runtime/test_agent.c']

def main():
    with tempfile.TemporaryDirectory(prefix='pangu-agent-') as directory:
        if '--live' in sys.argv or '--live-session' in sys.argv:
            if not os.environ.get('MIMO_API_KEY'):
                raise SystemExit('MIMO_API_KEY is required; never put credentials in source or arguments')
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            output = str(Path(directory) / 'live-mimo')
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                            str(ROOT / 'src/runtime/agent.c'), str(ROOT / 'src/runtime/mimo.c'),
                            str(ROOT / 'src/runtime/tools.c'), str(ROOT / 'src/runtime/read_tool.c'), str(ROOT / ('tests/runtime/live_session.c' if '--live-session' in sys.argv else 'tests/runtime/live_mimo.c')), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=350)
            return 0
        for mode in ('-O0', '-O2'):
            output = str(Path(directory) / 'test-agent')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-pedantic', mode, '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[str(ROOT / p) for p in SOURCES], '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(ROOT / 'src/runtime/mimo.c'), str(ROOT / 'tests/runtime/test_mimo_stream.c'),
                            *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(ROOT / 'src/runtime/tools.c'), str(ROOT / 'tests/runtime/test_tools.c'),
                            *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            str(ROOT / 'src/runtime/tools.c'), str(ROOT / 'src/runtime/read_tool.c'),
                            str(ROOT / 'tests/runtime/test_read_tool.c'), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', str(ROOT / 'src/runtime/mimo.c'),
                            str(ROOT / 'tests/runtime/test_projection.c'), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
