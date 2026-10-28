"""Compare native template arguments/substitution with the fixed Pi source."""
from pathlib import Path
import argparse
import json
import random
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'references/pi/packages/coding-agent/src/core/prompt-templates.ts'
ORACLE = r'''
import {readFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
const source=readFileSync(process.argv[1],'utf8');
const begin=source.indexOf('export function parseCommandArgs(');
const end=source.indexOf('\nfunction loadTemplateFromFile(');
if(begin<0||end<=begin) throw Error('upstream template boundary changed');
const code=stripTypeScriptTypes(source.slice(begin,end),{mode:'strip'});
const pi=await import('data:text/javascript;base64,'+Buffer.from(code).toString('base64'));
const cases=JSON.parse(readFileSync(0,'utf8'));
console.log(JSON.stringify(cases.map(item=>{
 const args=pi.parseCommandArgs(item.arguments);
 return {arguments:args,result:pi.substituteArgs(item.body,args),status:0};
})));
'''
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',required=True,type=Path)
    args=parser.parse_args()
    bodies=['$1|$2|$3|$@|$ARGUMENTS', '${1:-one}|${2:-two}|${@:-none}',
            '${@:0}|${@:1:0}|${@:2:2}|${@:999999999999999999999999}',
            '$0|${0:-zero}|$100|${1:-$2}|${ARGUMENTS:-$1}',
            '${@:01:02}|$01|${1:-}|${2:-a\nb}|${3:-a{b}',
            'literal ${bad} $ARGUMENTSX $@@ $$1 ${1:-x} ${1:-x}',
            '$99999999999999999999999 ${99999999999999999999999:-missing}',
            '原样 $1 café ${@:2} 🎉', '', '${@:2:99999999999999999999999}']
    arguments=['', 'a b c', "''", "'' ''", "'a b' \"\" c", '"unclosed',
               "'$2' '$ARGUMENTS' '${1:-x}'", 'one\n二\t🎉', 'one\u3000two\u00a0three',
               "a' b 'c d", '\\a \\"b', '\ufeffa\u2028b\u200bc', '  ""  x  ']
    cases=[{'body':body,'arguments':text} for body in bodies for text in arguments]
    rng=random.Random(20261028)
    for _ in range(120):
        cases.append({'body':' '.join(rng.choices(bodies,k=3)), 'arguments':' '.join(rng.choices(arguments,k=2))})
    expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',ORACLE,str(SOURCE)],input=json.dumps(cases,ensure_ascii=False).encode(),stderr=subprocess.DEVNULL))
    raw=''.join(json.dumps(case,ensure_ascii=False)+'\n' for case in cases).encode()
    actual=[json.loads(line) for line in subprocess.check_output([str(args.binary),'--cases'],input=raw).splitlines()]
    assert len(actual)==len(expected)
    for case,got,want in zip(cases,actual,expected):
        assert got==want,(case,got,want)
    print(f'PASS: {len(cases)} argument/substitution cases against actual fixed Pi prompt-templates.ts')
if __name__=='__main__':
    main()
