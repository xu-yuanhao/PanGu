"""Build the first native interactive composition, with output outside the checkout."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
UI = ['tui_utf8.c', 'tui_term.c', 'tui_input.c', 'tui_view.c', 'tui_state.c', 'tui_app.c', 'runtime_bridge.c']
SOURCES = ['src/composition/pangu.c', 'src/runtime/agent.c', 'src/runtime/mimo.c',
           'src/runtime/tools.c', 'src/runtime/read_tool.c'] + ['src/client/tui/' + n for n in UI]
def build(output):
    output = Path(output).resolve()
    if output.is_relative_to(ROOT):
        raise ValueError('build output must be outside the checkout')
    output.parent.mkdir(parents=True, exist_ok=True)
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
    subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                    '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                    *[str(ROOT / p) for p in SOURCES], *flags, '-o', str(output)], check=True)
    return output
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    output = args.output or Path(tempfile.mkdtemp(prefix='pangu-app-')) / 'pangu'
    print(build(output))
