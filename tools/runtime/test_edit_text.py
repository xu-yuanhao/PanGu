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
let prepare;
if(process.argv[2]) {
 const tool=readFileSync(process.argv[2],'utf8');
 const begin=tool.indexOf('function isSingleEditInput('),end=tool.indexOf('export type EditToolDetails');
 if(begin<0||end<=begin)throw new Error('unexpected prepare boundary');
 const code=stripTypeScriptTypes(tool.slice(begin,end)+'\nexport {prepareEditArguments};',{mode:'strip'});
 prepare=(await import('data:text/javascript;base64,'+Buffer.from(code).toString('base64'))).prepareEditArguments;
}
const cases=JSON.parse(readFileSync(0,'utf8'));
const results=cases.map(item=>{
 try {
  const {bom,text}=pi.stripBom(item.content);
  const ending=pi.detectLineEnding(text);
  const edits=prepare?prepare(item.arguments).edits:item.edits;
  const {newContent}=pi.applyEditsToNormalizedContent(pi.normalizeToLF(text),edits,'case.txt');
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
    add('a\0b\nkeep\0end\n', ('a\0b', 'A\0B'))
    add('before\0tail\r\nnext\n', ('before', 'after\0new'))
    add('Ａ\0Ｂ\n', ('A\0B', 'changed\0result'))
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
    for count in (17,32,65,128):
        pairs=[(f'T{i:03d}',f'R{i:03d}') for i in range(count)]
        add('\ufeff'+'\r\n'.join(a for a,b in pairs)+'\r\n',*reversed(pairs))
    return cases

def tool_corpus():
    edit = {'oldText': 'alpha', 'newText': 'ALPHA'}
    legacy = {'oldText': 'beta', 'newText': 'BETA'}
    shapes = [[edit], edit, json.dumps([edit]), json.dumps(edit), [], {}, 7, None,
              'invalid', '[]', '7', [None], [{}], {'oldText': 7, 'newText': 'x'},
              {'oldText': '', 'newText': 'x'}]
    cases = []
    for value in shapes:
        for extra in ({}, legacy, {'oldText': 7, 'newText': 'x'}):
            cases.append({'content': 'alpha\nbeta\n', 'arguments': {'path': 'file.txt', 'edits': value, **extra}})
    for extra in (legacy, {'oldText': 'alpha'}, {'newText': 'ALPHA'}, {}):
        cases.append({'content': 'alpha\nbeta\n', 'arguments': {'path': 'file.txt', **extra}})
    for case in corpus()[:22] + corpus()[-20:]:
        cases.append({'content': case['content'], 'arguments': {'path': 'file.txt', 'edits': case['edits']}})
    for case in corpus()[-4:]:
        cases.append({'content':case['content'],'arguments':{'path':'file.txt','edits':json.dumps(case['edits'][:-1]),**case['edits'][-1]}})
    return cases

def compare(binary, tool=False):
    frozen = subprocess.check_output(['git', '-C', str(ROOT / 'references/pi'), 'show',
                                      COMMIT + ':packages/durable/src/tools/edit-diff.ts'])
    assert hashlib.sha256(REFERENCE.read_bytes()).digest() == hashlib.sha256(frozen).digest(), 'reference drift'
    cases = tool_corpus() if tool else corpus()
    encoded = json.dumps(cases, ensure_ascii=False).encode()
    native = subprocess.run([str(binary), '--json'], input=encoded, capture_output=True, check=True, timeout=30)
    command = ['node', '--input-type=module', '-e', ORACLE, str(REFERENCE)]
    if tool:
        path = REFERENCE.with_name('edit.ts')
        frozen = subprocess.check_output(['git', '-C', str(ROOT / 'references/pi'), 'show',
                                         COMMIT + ':packages/durable/src/tools/edit.ts'])
        assert path.read_bytes() == frozen, 'prepare reference drift'
        command.append(str(path))
    oracle = subprocess.run(command,
                            input=encoded, capture_output=True, check=True, timeout=30)
    actual, expected = json.loads(native.stdout), json.loads(oracle.stdout)
    assert len(actual) == len(expected) == len(cases)
    for i, (left, right) in enumerate(zip(actual, expected)):
        assert (left['status'] == 0) == ('output' in right), (i, cases[i], left, right)
        if not left['status']:
            assert left['output'] == right['output'], (i, cases[i], left, right)
        elif tool:
            assert left['output'] == cases[i]['content'], (i, cases[i], left)
    print(f'PASS: {len(cases)} edit {"file/prepare" if tool else "text"} cases against actual Pi Durable {COMMIT[:12]} (exact output or rejection)')

def compare_diff(binary, module):
    manifest = module.resolve().parents[1] / 'package.json'
    assert json.loads(manifest.read_text())['version'] == '8.0.4', 'jsdiff version mismatch'
    rng = random.Random(54)
    cases = [{'before': '', 'after': ''}, {'before': 'same', 'after': 'same'}, {'before':'a\0b\nkeep\0end\n','after':'A\0B\nkeep\0end\n'},
             {'before': '\ufeffone\r\ntwo\r\n', 'after': '\ufeffone\r\nTWO\r\n'},
             {'before': '\n' * 2048 + 'tail', 'after': '\n' * 2048 + 'changed'}]
    for _ in range(240):
        a = ''.join(rng.choice(['a\n', 'b\n', '\n', '中\n', '😀\n', 'a\0b\n', '\0\n']) for _ in range(rng.randrange(28)))
        b = ''.join(rng.choice(['a\n', 'b\n', '\n', '中\n', '😀\n', 'a\0b\n', '\0\n']) for _ in range(rng.randrange(28)))
        if rng.randrange(3) == 0: a = a.rstrip('\n')
        if rng.randrange(3) == 0: b = b.rstrip('\n')
        cases.append({'before': a, 'after': b})
    code = r'''import {readFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
import {pathToFileURL} from 'node:url';
const module=pathToFileURL(process.argv[2]).href;
const source=readFileSync(process.argv[1],'utf8').replace('from "diff"', 'from '+JSON.stringify(module));
const pi=await import('data:text/javascript;base64,'+Buffer.from(stripTypeScriptTypes(source,{mode:'strip'})).toString('base64'));
const cases=JSON.parse(readFileSync(0,'utf8'));
console.log(JSON.stringify(cases.map(({before,after})=>{before=before.replace(/^\uFEFF/,'').replace(/\r\n/g,'\n');after=after.replace(/^\uFEFF/,'').replace(/\r\n/g,'\n');return {...pi.generateDiffString(before,after),patch:pi.generateUnifiedPatch('case.txt',before,after)};})));
'''
    expected = json.loads(subprocess.check_output(['node', '--input-type=module', '-e', code,
                           str(REFERENCE), str(module.resolve())], input=json.dumps(cases).encode(), timeout=30))
    actual = [json.loads(line) for line in subprocess.check_output([str(binary), '--diff-json'],
              input=''.join(json.dumps(c)+'\n' for c in cases).encode(), timeout=30).splitlines()]
    assert actual == expected, next(((c, a, e) for c, a, e in zip(cases, actual, expected) if a != e), 'result count')
    print(f'PASS: {len(cases)} numbered diff/patch/firstChangedLine cases match actual Pi and jsdiff 8.0.4')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--tool-binary', type=Path)
    parser.add_argument('--diff-module', type=Path, help='local jsdiff 8.0.4 ESM entry for upstream diff comparison')
    args = parser.parse_args()
    if args.diff_module:
        if not args.tool_binary: parser.error("--diff-module requires --tool-binary")
        compare_diff(args.tool_binary, args.diff_module)
        return
    if args.tool_binary:
        compare(args.tool_binary, tool=True)
        return
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
