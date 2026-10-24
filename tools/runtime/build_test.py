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
        if any(flag in sys.argv for flag in ('--live', '--live-session', '--live-journal', '--live-resume')):
            if not os.environ.get('MIMO_API_KEY'):
                raise SystemExit('MIMO_API_KEY is required; never put credentials in source or arguments')
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            output = str(Path(directory) / 'live-mimo')
            live_source = 'live_journal.c' if '--live-journal' in sys.argv or '--live-resume' in sys.argv else ('live_session.c' if '--live-session' in sys.argv else 'live_mimo.c')
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                            implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/mimo'),
                            implementation('src/runtime/tools'), implementation('src/runtime/read_tool'), implementation('src/runtime/journal'), implementation('src/runtime/journal_posix'), str(ROOT / 'tests/runtime' / live_source), *flags, '-o', output], check=True)
            if '--live-journal' in sys.argv or '--live-resume' in sys.argv:
                storage_dir = Path(directory) / 'storage'
                storage_dir.mkdir()
                phases = ('interrupt', 'resume', 'recall') if '--live-resume' in sys.argv else ('store', 'recall')
                for phase in phases:
                    subprocess.run([output, phase, str(storage_dir)], check=True, timeout=350)
            else:
                subprocess.run([output], check=True, timeout=350)
            return 0
        for mode in ('-O0', '-O2'):
            profile_test = str(Path(directory) / 'test-profile')
            profile_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/profile'),
                            implementation('src/runtime/agent'), implementation('src/runtime/session'),
                            implementation('src/runtime/journal'), implementation('src/runtime/journal_posix'),
                            implementation('src/runtime/tools'), str(ROOT / 'tests/runtime/test_profile.c'),
                            *profile_flags, '-o', profile_test], check=True)
            subprocess.run([profile_test], check=True, timeout=30)
            resume_test = str(Path(directory) / 'test-resume')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'),
                            implementation('src/runtime/session'), implementation('src/runtime/journal'),
                            implementation('src/runtime/journal_posix'), str(ROOT / 'tests/runtime/test_agent_resume.c'),
                            '-Wl,--wrap=pwrite', '-o', resume_test], check=True)
            subprocess.run([resume_test], check=True, timeout=30)
            journal_test = str(Path(directory) / 'test-journal')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/journal'),
                            implementation('src/runtime/journal_posix'), implementation('src/runtime/session'),
                            str(ROOT / 'tests/runtime/test_journal.c'), str(ROOT / 'tests/runtime/journal_abi.S'),
                            '-Wl,--wrap=pwrite,--wrap=pread,--wrap=fsync,--wrap=ftruncate', '-o', journal_test], check=True)
            subprocess.run([journal_test], check=True, timeout=30)
            session_test = str(Path(directory) / 'test-session')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/session'),
                            implementation('src/runtime/agent'), str(ROOT / 'tests/runtime/test_session_commit.c'), '-o', session_test], check=True)
            subprocess.run([session_test], check=True, timeout=10)
            tools_asm_test = str(Path(directory) / 'test-tools-asm')
            tool_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-pedantic', '-Wno-cast-function-type', mode, '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            '-I' + str(ROOT / 'src/runtime'), implementation('src/runtime/tools'), implementation('src/runtime/read_tool'),
                            str(ROOT / 'tests/runtime/test_tools_asm.c'), *tool_flags, '-o', tools_asm_test], check=True)
            subprocess.run([tools_asm_test], check=True, timeout=30)
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
                            str(ROOT / 'tests/runtime/runtime_bridge_abi.S'),
                            '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'), implementation('src/runtime/session'),
                            implementation('src/runtime/profile'), str(ROOT / 'tests/runtime/agent_abi.S'), str(ROOT / 'tests/runtime/test_agent_asm.c'),
                            '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
