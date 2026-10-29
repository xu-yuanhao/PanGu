"""Compare native output budgets with fixed Pi Chord's actual boundOutput."""
import ctypes as C
import json
from pathlib import Path
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
class Slice(C.Structure):
    _fields_=[('data',C.c_void_p),('size',C.c_size_t)]
class Window(C.Structure):
    _fields_=[('offset',C.c_size_t),('size',C.c_size_t),('dropped_bytes',C.c_size_t),('dropped_lines',C.c_size_t)]
ORACLE="""
import {readFileSync} from 'node:fs';import {pathToFileURL} from 'node:url';
const {boundOutput}=await import(pathToFileURL(process.argv[1]+'/packages/chord/src/output.ts'));
console.log(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(c=>boundOutput(c.text,c.limits))));
"""
def main():
    rng=random.Random(104)
    texts=['','a','a\n','a\nb\nc\n','a\nb\nc\n\n','aa\nbb\ncc\n','é'*9,'😀'*8,'x\n' * 2002,'a'*4094+'😀'+'b'*20]
    texts += [''.join(rng.choice(['a','é','😀','\n','\t','\ufeff']) for _ in range(rng.randrange(1,90))) for _ in range(35)]
    cases=[]
    for text in texts:
        n=len(text.encode())
        for byte_limit in sorted({0,1,3,7,n//2,n,n+1}):
            for line_limit in (0,1,2,2000):
                for retain in ('head','tail'):
                    cases.append(dict(text=text,limits=dict(maxBytes=byte_limit,maxLines=line_limit,retain=retain)))
    expected=json.loads(subprocess.check_output(['node','--experimental-strip-types','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(cases).encode()))
    with tempfile.TemporaryDirectory(prefix='pangu-output-window-') as directory:
        library=Path(directory)/'window.so'
        subprocess.run(['/usr/bin/clang-23','-fPIC','-shared',str(ROOT/'src/runtime/output_window.S'),'-o',str(library)],check=True)
        native=C.CDLL(str(library));native.pg_output_bound.argtypes=[Slice,C.c_size_t,C.c_size_t,C.c_uint,C.POINTER(Window)];native.pg_output_bound.restype=C.c_int
        for case,wanted in zip(cases,expected):
            data=case['text'].encode();buffer=C.create_string_buffer(data);out=Window();limits=case['limits']
            assert not native.pg_output_bound(Slice(C.cast(buffer,C.c_void_p),len(data)),limits['maxBytes'],limits['maxLines'],limits['retain']=='tail',C.byref(out))
            actual=dict(text=data[out.offset:out.offset+out.size].decode(),bytes=out.size,droppedBytes=out.dropped_bytes,droppedLines=out.dropped_lines)
            assert actual==wanted,(case,actual,wanted)
    print(f'PASS: {len(cases)} native head/tail budgets match fixed Pi Chord output.ts')
if __name__=='__main__':
    main()
