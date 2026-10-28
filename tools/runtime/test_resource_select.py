"""Compare native glob decisions with Pi's pinned minimatch dependency."""
from pathlib import Path
import argparse
import json
import subprocess

ORACLE=r'''
import {readFileSync} from 'node:fs';
import {pathToFileURL} from 'node:url';
const {minimatch}=await import(pathToFileURL(process.argv[1]));
const cases=JSON.parse(readFileSync(0,'utf8'));
console.log(JSON.stringify(cases.map(c=>minimatch(c.path,c.pattern)?1:0)));
'''
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    parser.add_argument('--oracle',type=Path,required=True,help='minimatch 10.2.6 dist/esm/index.js')
    args=parser.parse_args()
    package=args.oracle.resolve().parents[2]
    assert json.loads((package/'package.json').read_text())['version']=='10.2.6'
    dependency=package.parent/'brace-expansion'/'package.json'
    assert json.loads(dependency.read_text())['version']=='5.0.12'
    patterns=['','*','?','??','*.md','a/*','a/**','**','**/*.md','**/x/**','a/**/file.md',
              'a//b/*.md','/a/**','.*','**/.*','[ab].md','[!a].md','[a-c].md',
              '{a,b}.md','{a,{b,c}}.md','{#foo,bar}','a/{,b/}file.md',
              r'\{a,b\}.md',r'file\?.md','!foo','!!foo','#foo','!#foo',
              '+(foo|bar)','@(foo|bar)','abc!(x|y)','?(foo|bar)','*(foo|bar)',
              '!(foo)','{,a}','{1..3}.md','{03..01}.md','{-2..2..2}.md','{a..e..2}.md','x{1..4}y','中?','?中','a/**/**/b','a/**/../b','**/..','a/']
    paths=['','foo','bar','(foo)','#foo','a','a/','a.md','b.md','c.md','d.md',
           '{a,b}.md','1.md','2.md','3.md','01.md','02.md','03.md','-2.md','0.md','e.md','x3y','file?.md','file.md','a/file.md','a/b/file.md','a/.hidden/file.md',
           '.hidden.md','a/.hidden','a/b/x/file.md','/a/b/file.md','/file.md','a/b',
           'a/../b','..','abcx','abcz','foobar','中a','a中','中中','🎉','x']
    cases=[{'path':path,'pattern':pattern} for pattern in patterns for path in paths]
    expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',ORACLE,str(args.oracle.resolve())],input=json.dumps(cases,ensure_ascii=False).encode()))
    data=''.join(json.dumps(case,ensure_ascii=False)+'\n' for case in cases).encode()
    got=[int(line) for line in subprocess.check_output([str(args.binary),'--cases'],input=data).splitlines()]
    assert len(got)==len(expected)
    failures=[(case,want,actual) for case,want,actual in zip(cases,expected,got) if want!=actual]
    if failures:
        for failure in failures[:25]:print(failure)
        raise AssertionError(str(len(failures))+' native/upstream glob differences')
    print(f'PASS: {len(cases)} native glob decisions against minimatch 10.2.6')
if __name__=='__main__':main()
