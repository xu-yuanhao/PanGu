"""Build the first native interactive composition, with output outside the checkout."""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile
import sys
ROOT = Path(__file__).resolve().parents[2]
# The approved inventory is the single product input list. No .c default/fallback.
SOURCES = list(json.loads((ROOT / 'governance/assembly-first.json').read_text())['sources'])
def implementation(stem):
    matches = [name for name in SOURCES if str(Path(name).with_suffix('')) == stem]
    if len(matches) != 1:
        raise ValueError('exactly one registered implementation required for ' + stem)
    return str(ROOT / matches[0])

def build(output):
    sys.path.insert(0, str(ROOT / 'tools/gates'))
    from assembly_first import inspect
    violations = inspect(ROOT, SOURCES)
    if violations:
        raise ValueError('Assembly-first admission: ' + '; '.join(violations))
    output = Path(output).resolve()
    if output.is_relative_to(ROOT):
        raise ValueError('build output must be outside the checkout')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Spawn attributes are opaque to ASM; reject a mismatching host layout before linking.
    subprocess.run(['/usr/bin/clang-23', '-D_GNU_SOURCE', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                    '-x', 'c', '-fsyntax-only', '-include', str(ROOT / 'src/runtime/process_layout.h'),
                    '/dev/null'], check=True)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c', 'libutf8proc', 'sqlite3'], text=True).split()
    subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                    '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                    '-Wl,--export-dynamic-symbol=pg_host_*',
                    *[str(ROOT / p) for p in SOURCES], *flags, '-ldl', '-o', str(output)], check=True)
    return output
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    output = args.output or Path(tempfile.mkdtemp(prefix='pangu-app-')) / 'pangu'
    print(build(output))
