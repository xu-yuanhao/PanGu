"""Exclusive driver for the tools/read handwritten-assembly batch.

Builds src/runtime/tools.S and src/runtime/read_tool.S plus the C layout
headers (compile-time assertions) and runs the original C tests and the
assembly ABI/boundary suite at O0 and O2 (UBSan on the C test code where
it links). All artifacts go to a TemporaryDirectory under /tmp and are
removed on exit. This is a task-local driver, not the product build.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = '/usr/bin/clang-23'
SRC = [str(ROOT / 'src/runtime/tools.S'), str(ROOT / 'src/runtime/read_tool.S')]
TESTS = [
    ('test_tools', str(ROOT / 'tests/runtime/test_tools.c')),
    ('test_read_tool', str(ROOT / 'tests/runtime/test_read_tool.c')),
    ('test_tools_asm', str(ROOT / 'tests/runtime/test_tools_asm.c')),
]


def flags(opt, extra=()):
    cflags = subprocess.check_output(['pkg-config', '--cflags', 'json-c'], text=True).split()
    libs = subprocess.check_output(['pkg-config', '--libs', 'json-c'], text=True).split()
    return ['-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra',
            '-Wno-cast-function-type', opt, '-I' + str(ROOT / 'src/runtime'),
            '-I' + str(ROOT), *cflags, *extra, *libs]


def run(cmd, cwd=None):
    print('$', ' '.join(cmd))
    r = subprocess.run(cmd, cwd=cwd or str(ROOT))
    if r.returncode != 0:
        print(f'FAIL rc={r.returncode}: {" ".join(cmd)}')
        sys.exit(r.returncode)


def main():
    failures = 0
    with tempfile.TemporaryDirectory(prefix='pangu-tools-asm-') as tmp:
        for opt in ['-O0', '-O2']:
            for extra, label in [([], opt), (['-fsanitize=undefined'], opt + '+ubsan')]:
                objects = []
                for src in SRC:
                    obj = str(Path(tmp) / (Path(src).stem + label.replace('+', '_') + '.o'))
                    run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11', opt,
                         '-I' + str(ROOT / 'src/runtime'), '-c', src, '-o', obj])
                    objects.append(obj)
                for name, test in TESTS:
                    out = str(Path(tmp) / (name + '-' + label.replace('+', '_')))
                    run([CC, *flags(opt, extra), test, *objects, '-o', out])
                    r = subprocess.run([out])
                    print(f'== {name} {label}: rc={r.returncode}')
                    if r.returncode != 0:
                        failures += 1
        # 对象导出/导入清单：无未定义的旧 C 核心符号
        obj = str(Path(tmp) / 'tools.o')
        run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-I' + str(ROOT / 'src/runtime'),
             '-c', SRC[0], '-o', obj])
        subprocess.run(['nm', '--undefined-only', obj])
        print('== tools.S undefined imports ^')
        subprocess.run(['nm', '-g', '--defined-only', obj])
        print('== tools.S exports ^')
    if failures:
        print(f'{failures} test runs failed')
        return 1
    print('ALL TOOLS ASM RUNS PASSED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
