"""Exclusive driver for the MiMo handwritten-assembly batch.

Builds src/runtime/mimo.S + mimo_projection.S + mimo_stream.S with the C
layout header (compile-time assertions) and runs, at O0 and O2 (UBSan on the
C test code where it links):

- the existing protocol suites plus explicit bounded batch-contract cases;
- test_mimo_asm (ABI/boundary suite) linked with mimo_abi.S and --wrap
  interposition that trashes caller-saved registers after every wrapped
  external call and asserts stack alignment at each call site;
- test_mimo_transport (scripted curl stand-in via --wrap, no network);
- test_mimo_alloc (constructor/insertion/serialization rejection ownership).

All artifacts go to a TemporaryDirectory under /tmp and are removed on exit.
This is a task-local driver, not the product build.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = '/usr/bin/clang-23'
SRC = [str(ROOT / 'src/runtime/mimo.S'),
       str(ROOT / 'src/runtime/mimo_projection.S'),
       str(ROOT / 'src/runtime/mimo_stream.S'),
       str(ROOT / 'src/runtime/request_context.S')]
ABI_WRAPS = ['-Wl,--wrap=' + n for n in (
    'strlen', 'strcmp', 'strncmp', 'memchr', 'memcpy', 'strchr', 'free',
    'getenv', 'strdup', 'json_object_object_add', 'json_object_array_add',
    'json_object_new_string', 'json_object_new_string_len', 'json_object_put',
    'json_tokener_parse_ex', 'json_tokener_get_error', 'json_object_get_string', 'json_object_is_type',
    'json_object_object_get_ex', 'json_object_get_int64', 'memset',
    'json_object_array_length', 'json_object_array_get_idx', 'json_object_get_string_len',
    'json_tokener_get_parse_end', 'json_tokener_free')]
CURL_WRAPS = ['-Wl,--wrap=' + n for n in (
    'curl_easy_init', 'curl_easy_setopt', 'curl_easy_getinfo',
    'curl_easy_cleanup', 'curl_multi_init', 'curl_multi_perform',
    'curl_multi_info_read', 'curl_multi_add_handle', 'curl_multi_cleanup',
    'curl_slist_free_all', 'curl_slist_append', 'json_tokener_new')]

ALLOC_WRAPS = ['-Wl,--wrap=' + name for name in ('json_object_new_object', 'json_object_new_array', 'json_object_new_string', 'json_object_new_string_len', 'json_object_new_boolean', 'json_object_new_int', 'json_object_object_add', 'json_object_array_add', 'json_tokener_parse', 'json_object_to_json_string_ext', 'strdup')]


def flags(opt, extra=()):
    cflags = subprocess.check_output(['pkg-config', '--cflags', 'json-c', 'libcurl'], text=True).split()
    libs = subprocess.check_output(['pkg-config', '--libs', 'json-c', 'libcurl'], text=True).split()
    return ['-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra',
            '-Wno-cast-function-type', opt, '-I' + str(ROOT / 'src/runtime'),
            '-I' + str(ROOT), *cflags, *extra, *libs]


def run(cmd):
    print('$', ' '.join(cmd))
    r = subprocess.run(cmd, cwd=str(ROOT))
    if r.returncode != 0:
        print(f'FAIL rc={r.returncode}: {" ".join(cmd)}')
        sys.exit(r.returncode)


def main():
    failures = 0
    with tempfile.TemporaryDirectory(prefix='pangu-mimo-asm-') as tmp:
        for opt in ['-O0', '-O2']:
            for extra, label in [([], opt), (['-fsanitize=undefined', '-fno-sanitize-recover=all'], opt + '+ubsan')]:
                tag = label.replace('+', '_')
                objects = []
                for src in SRC:
                    obj = str(Path(tmp) / (Path(src).stem + '-' + tag + '.o'))
                    run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11', opt,
                         '-I' + str(ROOT / 'src/runtime'), '-c', src, '-o', obj])
                    objects.append(obj)
                suites = [
                    ('stream', [str(ROOT / 'tests/runtime/test_mimo_stream.c')], []),
                    ('projection', [str(ROOT / 'tests/runtime/test_projection.c')], []),
                    ('asm', [str(ROOT / 'tests/runtime/test_mimo_asm.c'),
                             str(ROOT / 'tests/runtime/mimo_abi.S')], ABI_WRAPS),
                    ('transport', [str(ROOT / 'tests/runtime/test_mimo_transport.c'), str(ROOT / 'tests/runtime/mimo_abi.S')], ABI_WRAPS + CURL_WRAPS),
                    ('alloc', [str(ROOT / 'tests/runtime/test_mimo_alloc.c')], ALLOC_WRAPS),
                ]
                for name, tests, wraps in suites:
                    out = str(Path(tmp) / ('test-mimo-' + name + '-' + tag))
                    run([CC, *flags(opt, extra), *tests, *objects, *wraps, '-o', out])
                    r = subprocess.run([out])
                    print(f'== mimo-{name} {label}: rc={r.returncode}')
                    if r.returncode != 0:
                        failures += 1
        # 对象导入/导出清单：导出恰为四个公开函数，导入仅 json-c/libcurl/libc
        for src in SRC:
            obj = str(Path(tmp) / (Path(src).stem + '-audit.o'))
            run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                 '-I' + str(ROOT / 'src/runtime'), '-c', src, '-o', obj])
            subprocess.run(['nm', '-g', '--defined-only', obj])
            print(f'== {Path(src).name} exports ^')
            subprocess.run(['nm', '--undefined-only', obj])
            print(f'== {Path(src).name} undefined imports ^')
    if failures:
        print(f'{failures} test runs failed')
        return 1
    print('ALL MIMO ASM RUNS PASSED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
