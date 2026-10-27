"""Compare independent native SQLite/JSONL reopen results to fixed Pi behavior."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
import sys
ROOT = Path(__file__).resolve().parents[2]
COMMIT = 'd5b3de22d0e24a3e9c38a9410d3329b099717283'
ORACLE = r'''
import {registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';import {readFileSync} from 'node:fs';
const root=pathToFileURL(process.argv[1]+'/');
registerHooks({resolve(s,c,next){const p={'@earendil-works/chord':'json.ts','@earendil-works/chord/delta':'delta/index.ts','@earendil-works/chord/sqlite':'sqlite/index.ts','@earendil-works/chord/sqlite/node':'sqlite/node.ts'};if(p[s])return {url:new URL('packages/chord/src/'+p[s],root).href,shortCircuit:true};return next(s,c);}});
const {openNodeSqliteStorage}=await import(new URL('packages/durable/src/storage/sqlite/node.ts',root));
let s=await openNodeSqliteStorage(process.argv[2]);
for(const batch of JSON.parse(readFileSync(0,'utf8')))await s.commit(batch,()=>{},{});
await s.close({});s=await openNodeSqliteStorage(process.argv[2]);
const out=[await s.conversation(1,{}),(await s.entry(2,{})).entry,await s.task(3,{}),await s.submission(4,{})];
const entry=await s.entry(6,{});out.push(entry.entry,entry.commitSeq,await s.document(5,1,{}),await s.document(5,"current",{}),await s.mintId());
await s.close({});console.log(JSON.stringify(out));
'''
def main():
    jsonl='--jsonl' in sys.argv
    provider='jsonl' if jsonl else 'sqlite'
    case=ROOT/f'tests/runtime/test_record_{provider}.c'
    text=case.read_text()
    oracle=ORACLE
    if jsonl:
        oracle=oracle.replace('openNodeSqliteStorage','openNodeJsonlStorage').replace('storage/sqlite/node.ts','storage/jsonl/node.ts').replace('openNodeJsonlStorage(process.argv[2])','openNodeJsonlStorage(process.argv[2],{})')
    batches=[json.loads(json.loads('"'+re.search(r'static const char \*'+name+r'="(.*?)";',text).group(1)+'"')) for name in ['first','second']]
    with tempfile.TemporaryDirectory(prefix=f'pangu-{provider}-diff-') as directory:
        work=Path(directory);frozen=work/'pi';frozen.mkdir()
        archive=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'archive',COMMIT])
        import io,tarfile
        with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:bundle.extractall(frozen,filter='data')
        flags=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libutf8proc','sqlite3'],text=True).split()
        binary=work/'native'
        extra=[str(ROOT/'src/runtime/record_jsonl_posix.c'),'-Wl,--wrap=write'] if jsonl else []
        subprocess.run(['clang-23','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2',*[str(ROOT/'src/runtime'/f'{name}.S') for name in ['documents','records','record_memory','delta',f'record_{provider}']],str(case),*extra,*flags,'-o',str(binary)],check=True)
        native=json.loads(subprocess.check_output([str(binary),'--trace'],text=True))
        upstream=json.loads(subprocess.check_output(['node','--input-type=module','-e',oracle,str(frozen),str(work/f'upstream.{provider}')],input=json.dumps(batches),text=True))
        assert native==upstream,(native,upstream)
        print(f'PASS: independent Pi and PanGu {provider} stores: mixed commit/reopen records, Entry sequence, document history/current, next ID')
if __name__=='__main__':main()
