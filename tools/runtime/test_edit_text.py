"""Compare native edit bytes with the fixed Pi Durable functions, without a model."""
from pathlib import Path
import argparse
import hashlib
import json
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'references/pi/packages/durable/src/tools/edit-diff.ts'
COMMIT = '38ff99177fba21e9c57a670f4e95adcf407c99c7'
ORACLE = r'''
import {readFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
const source=readFileSync(process.argv[1],'utf8');
const dependency='import * as Diff from "diff";';
if(source.split(dependency).length!==2) throw new Error('unexpected import boundary');
// The diff display dependency is unused by these actual upstream transformations.
const code=stripTypeScriptTypes(source.replace(dependency,''),{mode:'strip'});
const pi=await import('data:text/javascript;base64,'+Buffer.from(code).toString('base64'));
const cases=JSON.parse(readFileSync(0,'utf8'));
const results=cases.map(item=>{
 try {
  const {bom,text}=pi.stripBom(item.content);
  const ending=pi.detectLineEnding(text);
  const {newContent}=pi.applyEditsToNormalizedContent(pi.normalizeToLF(text),item.edits,'case.txt');
  return {output:bom+pi.restoreLineEndings(newContent,ending)};
 } catch(error) {return {error:String(error.message)};}
});
process.stdout.write(JSON.stringify(results));
'''

def corpus():
    cases = []
    def add(content, *pairs):
        cases.append({'content': content, 'edits': [{'oldText': a, 'newText': b} for a, b in pairs]})
    add('alpha\nbeta\ngamma\ndelta\n', ('alpha\n', 'ALPHA\n'), ('gamma\n', 'GAMMA\n'))
    add('one\ntwo\nthree\n', ('one\ntwo\n', 'ONE\nTWO\n'), ('two\nthree\n', 'TWO\nTHREE\n'))
    add('abc', ('a', 'a'))
    add('abc', ('missing', 'x'))
    add('abc', ('', 'x'))
    add('abc', ('a', 'x'), ('x', 'y'))
    add('foo foo foo', ('foo', 'bar'))
    add('\ufeffone\r\ntwo\r\n', ('two', 'TWO\nnew'))
    add('unchanged  \nsay ‘hello’ — world \nend  ', ("say 'hello' - world", 'changed'))
    add('same “X”  \nsame "X"\n', ('same "X"', 'changed'))
    add('line  \nＡＢＣ\nlast  ', ('ABC', 'changed'))
    add('line\nＡＢＣ\ntrailing   ', ('ABC', 'changed'))
    add('Ａ “B” C  \nkeep  \n', ('A', 'first'), ('"B"', 'second'), ('C', 'third'))
    add('a', (' ', 'insert'))
    add('ab', (' ', 'insert'))
    add('abc', (' ', 'insert'))
    add('😀', (' ', 'insert'))
    add('', (' ', 'insert'))
    add('   ', (' ', 'insert'))
    add('ab', (' ', 'first'), ('\t', 'second'))
    add('😀', (' ', 'first'), ('\ufeff', 'second'))
    add('aaa', ('aa', 'x'))  # JS split counts nonoverlapping occurrences.
    add('aabb', ('aa', 'x'), ('bb', 'y'))
    add('aabb', ('bb', 'y'), ('aa', 'x'))
    variants = [
        ('ＡＢＣ', 'ABC'), ('e\u0301', 'é'), ('ﬁ', 'fi'), ('①', '1'), ('Å', 'Å'),
        ('‘x’', "'x'"), ('“x”', '"x"'), ('a—b', 'a-b'), ('a−b', 'a-b'),
        ('a\u00a0b', 'a b'), ('a\u202fb', 'a b'), ('a\u3000b', 'a b'),
        ('Ａ😀Ｂ', 'A😀B'), ('中 “文”', '中 "文"'), ('ｶﾞ', 'ガ'), ('ﬃ', 'ffi'),
    ]
    for raw, normalized in variants:
        for ending in ('\n', '\r\n', '\r'):
            for whitespace in ('', '  ', '\t', '\ufeff', '\u1680', '\u2028'):
                add('keep  ' + ending + raw + whitespace + ending + 'end  ' + ending,
                    (normalized, 'changed\nnext'))
                add(raw + ending + normalized, (normalized, 'changed'))
    rng = random.Random(7612)
    alphabet = ['one', 'two', 'three', 'é', 'e\u0301', '中', '😀', 'Ａ', 'A', '“x”', '"x"', 'a—b', 'a-b', '  ', '\t']
    for _ in range(700):
        lines = [rng.choice(alphabet) for _ in range(rng.randint(1, 6))]
        ending = rng.choice(['\n', '\r\n', '\r'])
        text = ending.join(lines) + rng.choice(['', ending])
        edits = [(rng.choice(alphabet), rng.choice(['new', '', 'x\ny', '中'])) for _ in range(rng.randint(1, 4))]
        add(text, *edits)
    return cases

def compare(binary):
    frozen = subprocess.check_output(['git', '-C', str(ROOT / 'references/pi'), 'show',
                                      COMMIT + ':packages/durable/src/tools/edit-diff.ts'])
    assert hashlib.sha256(REFERENCE.read_bytes()).digest() == hashlib.sha256(frozen).digest(), 'reference drift'
    cases = corpus()
    encoded = json.dumps(cases, ensure_ascii=False).encode()
    native = subprocess.run([str(binary), '--json'], input=encoded, capture_output=True, check=True, timeout=30)
    oracle = subprocess.run(['node', '--input-type=module', '-e', ORACLE, str(REFERENCE)],
                            input=encoded, capture_output=True, check=True, timeout=30)
    actual, expected = json.loads(native.stdout), json.loads(oracle.stdout)
    assert len(actual) == len(expected) == len(cases)
    for i, (left, right) in enumerate(zip(actual, expected)):
        assert (left['status'] == 0) == ('output' in right), (i, cases[i], left, right)
        if not left['status']:
            assert left['output'] == right['output'], (i, cases[i], left, right)
    print(f'PASS: {len(cases)} edit cases against actual Pi Durable {COMMIT[:12]} (exact output or rejection)')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path)
    args = parser.parse_args()
    if args.binary:
        compare(args.binary)
        return
    flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c', 'libutf8proc'], text=True).split()
    with tempfile.TemporaryDirectory(prefix='pangu-edit-') as directory:
        for mode in ('-O0', '-O2'):
            binary = Path(directory) / ('edit' + mode)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', str(ROOT / 'src/runtime/edit_text.S'),
                            str(ROOT / 'tests/runtime/test_edit_text.c'), '-Wl,--wrap=calloc,--wrap=utf8proc_map', *flags, '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, timeout=10)
            compare(binary)

if __name__ == '__main__':
    main()
