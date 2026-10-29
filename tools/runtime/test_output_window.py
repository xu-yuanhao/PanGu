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
class Stream(C.Structure):
    _fields_=[("data",C.c_void_p),("size",C.c_size_t),("total_bytes",C.c_size_t),("total_newlines",C.c_size_t),("max_bytes",C.c_size_t),("max_lines",C.c_size_t),("tail",C.c_uint),("ends",C.c_uint),("full",C.c_uint),("reserved",C.c_uint)]
class Skip(C.Structure):
    _fields_=[("bytes",C.c_size_t),("newlines",C.c_size_t),("ends",C.c_uint),("reserved",C.c_uint)]
class Snapshot(C.Structure):
    _fields_=[("text",C.c_void_p),("size",C.c_size_t),("dropped_bytes",C.c_size_t),("dropped_lines",C.c_size_t)]
class Window(C.Structure):
    _fields_=[('offset',C.c_size_t),('size',C.c_size_t),('dropped_bytes',C.c_size_t),('dropped_lines',C.c_size_t)]
ORACLE="""
import {readFileSync} from 'node:fs';import {pathToFileURL} from 'node:url';
const {boundOutput,sanitizeOutput,OutputBuffer}=await import(pathToFileURL(process.argv[1]+'/packages/chord/src/output.ts'));
console.log(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(c=>{if(c.chunks){const b=new OutputBuffer(c.limits);return c.chunks.map(x=>{if(typeof x==='string')b.push(x);else b.push(x.text,x.skipped);return b.snapshot()})}return {...boundOutput(c.text,c.limits),sanitized:sanitizeOutput(c.text)}})));
"""
def main():
    rng=random.Random(104)
    texts=['a\0b\tc\nd\re\x07f\ufff9g\ufffbh😀','','a','a\n','a\nb\nc\n','a\nb\nc\n\n','aa\nbb\ncc\n','é'*9,'😀'*8,'x\n' * 2002,'a'*4094+'😀'+'b'*20]
    texts += [''.join(rng.choice(['a','é','😀','\n','\t','\ufeff']) for _ in range(rng.randrange(1,90))) for _ in range(35)]
    cases=[]
    for text in texts:
        n=len(text.encode())
        for byte_limit in sorted({0,1,3,7,n//2,n,n+1}):
            for line_limit in (0,1,2,2000):
                for retain in ('head','tail'):
                    cases.append(dict(text=text,limits=dict(maxBytes=byte_limit,maxLines=line_limit,retain=retain)))
    expected=json.loads(subprocess.check_output(['node','--experimental-strip-types','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(cases).encode()))
    streams=[]
    for _ in range(220):
        chunks=[''.join(rng.choice(['a','é','😀','\n','\t','\0','\r','\ufff9']) for _ in range(rng.randrange(0,40))) for _ in range(rng.randrange(2,16))]
        streams.append(dict(chunks=chunks,limits=dict(maxBytes=rng.choice([0,1,3,7,20,64]),maxLines=rng.choice([0,1,2,10]),retain=rng.choice(['head','tail']))))
    for _ in range(40):
        chunks=['before\n',dict(text=rng.choice(['tail\n','é\0x','', '😀\nlast']),skipped=dict(bytes=10000,newlines=100,endsWithNewline=bool(rng.randrange(2)))),'after\n']
        streams.append(dict(chunks=chunks,limits=dict(maxBytes=rng.choice([0,1,7,64]),maxLines=rng.choice([0,1,2,10]),retain='tail')))
    stream_expected=json.loads(subprocess.check_output(['node','--experimental-strip-types','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(streams).encode()))
    with tempfile.TemporaryDirectory(prefix='pangu-output-window-') as directory:
        library=Path(directory)/'window.so'
        subprocess.run(['/usr/bin/clang-23','-fPIC','-shared',str(ROOT/'src/runtime/output_window.S'),'-o',str(library)],check=True)
        native=C.CDLL(str(library));native.pg_output_sanitize.argtypes=[C.c_void_p,C.c_size_t];native.pg_output_sanitize.restype=C.c_size_t;native.pg_output_bound.argtypes=[Slice,C.c_size_t,C.c_size_t,C.c_uint,C.POINTER(Window)];native.pg_output_bound.restype=C.c_int
        native.pg_output_buffer_push.argtypes=[C.POINTER(Stream),Slice];native.pg_output_buffer_skip.argtypes=[C.POINTER(Stream),C.POINTER(Skip)];native.pg_output_buffer_snapshot.argtypes=[C.POINTER(Stream),C.POINTER(Snapshot)];native.pg_output_buffer_close.argtypes=[C.POINTER(Stream)];native.free.argtypes=[C.c_void_p]
        for case,wanted_steps in zip(streams,stream_expected):
            limits=case['limits'];state=Stream(max_bytes=limits['maxBytes'],max_lines=limits['maxLines'],tail=limits['retain']=='tail')
            for chunk,wanted in zip(case['chunks'],wanted_steps):
                if isinstance(chunk,dict):
                    omitted=chunk['skipped'];skip=Skip(omitted['bytes'],omitted['newlines'],omitted['endsWithNewline'])
                    assert not native.pg_output_buffer_skip(C.byref(state),C.byref(skip))
                    chunk=chunk['text']
                data=chunk.encode();buffer=C.create_string_buffer(data);out=Snapshot()
                assert not native.pg_output_buffer_push(C.byref(state),Slice(C.cast(buffer,C.c_void_p),len(data)))
                assert not native.pg_output_buffer_snapshot(C.byref(state),C.byref(out))
                actual=dict(text=C.string_at(out.text,out.size).decode(),droppedBytes=out.dropped_bytes,droppedLines=out.dropped_lines);native.free(out.text)
                assert actual==wanted,(case,chunk,actual,wanted)
            native.pg_output_buffer_close(C.byref(state))
        for case,wanted in zip(cases,expected):
            data=case['text'].encode();buffer=C.create_string_buffer(data);out=Window();limits=case['limits']
            assert not native.pg_output_bound(Slice(C.cast(buffer,C.c_void_p),len(data)),limits['maxBytes'],limits['maxLines'],limits['retain']=='tail',C.byref(out))
            actual=dict(text=data[out.offset:out.offset+out.size].decode(),bytes=out.size,droppedBytes=out.dropped_bytes,droppedLines=out.dropped_lines)
            sanitized=wanted.pop("sanitized");assert actual==wanted,(case,actual,wanted)
            kept=native.pg_output_sanitize(buffer,len(data));assert buffer.raw[:kept].decode()==sanitized
    print(f'PASS: {len(cases)} budgets and {len(streams)} multi-chunk streams match fixed Pi Chord output.ts')
if __name__=='__main__':
    main()
