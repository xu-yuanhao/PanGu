"""Exclusive driver for the tools/read handwritten-assembly batch.

Builds src/runtime/tools.S and src/runtime/read_tool.S plus the C layout
headers (compile-time assertions) and runs the original C tests and the
assembly ABI/boundary suite at O0 and O2 (UBSan on the C test code where
it links). All artifacts go to a TemporaryDirectory under /tmp and are
removed on exit. This is a task-local driver, not the product build.
"""
from pathlib import Path
import base64
import ctypes as C
import json
import random
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = '/usr/bin/clang-23'
SRC = [str(ROOT / 'src/runtime/tools.S'), str(ROOT / 'src/runtime/read_tool.S'), str(ROOT / 'src/runtime/tool_path.S'), str(ROOT / 'src/runtime/tool_path_posix.c')]
TESTS = [
    ('test_tools', str(ROOT / 'tests/runtime/test_tools.c')),
    ('test_read_tool', str(ROOT / 'tests/runtime/test_read_tool.c')),
    ('test_tools_asm', str(ROOT / 'tests/runtime/test_tools_asm.c')),
]

READ_ORACLE = r'''
import {readFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
import {pathToFileURL} from 'node:url';
const root=process.argv[1];
const chord=await import(pathToFileURL(root+'/packages/chord/src/output.ts'));
const source=readFileSync(root+'/packages/durable/src/truncate.ts','utf8');
const code=stripTypeScriptTypes(source.replace(/^import[^\n]+\n/,''),{mode:'strip'});
const truncate=await import('data:text/javascript;base64,'+Buffer.from('const utf8ByteLength=s=>Buffer.byteLength(s);\n'+code).toString('base64'));
const test=readFileSync(root+'/packages/durable/test/tools-read-differential.test.ts','utf8');
const body=test.slice(test.indexOf('function referenceRead('),test.indexOf('function random('));
if(!body.startsWith('function referenceRead('))throw new Error('read oracle boundary');
const reference=new Function('detectSupportedImageMimeType','characterEnd','DEFAULT_MAX_BYTES','formatSize','truncateHead',stripTypeScriptTypes(body,{mode:'strip'})+';return referenceRead')(()=>undefined,chord.characterEnd,truncate.DEFAULT_MAX_BYTES,truncate.formatSize,truncate.truncateHead);
const cases=JSON.parse(readFileSync(0,'utf8'));
process.stdout.write(JSON.stringify(cases.map(c=>{try{return {text:reference(Buffer.from(c.bytes,'base64'),'f.txt',c.offset,c.limit).output.map(x=>x.text??'').join('')}}catch(e){return {error:String(e.message)}}})));
'''

def read_differential():
    """Native text selection against Pi's actual full-file reference and truncation."""
    class Slice(C.Structure):
        _fields_=[('data',C.c_void_p),('size',C.c_size_t)]
    class Definition(C.Structure):
        _fields_=[(name,C.c_void_p) for name in ('name','description','schema','context','validate','authorize','execute')]+[('safe',C.c_uint),('async_',C.c_void_p),('limits',C.c_void_p)]
    rng=random.Random(1029)
    pieces=[b'\n',b'a',b'bcde',b'\xef\xbb\xbf','é'.encode(),'€'.encode(),'😀'.encode(),b'\xe2\x82',b'\xff',b'\r\n',b'\0']
    offsets=[None,0,1,2,3,2.5,-4,50,2001,3000,1e20]
    limits=[None,0,1,2,-3,-1e20,1.5,7,2000,2500,1e20]
    cases=[]
    for seed in range(400):
        if seed%10==0: data=b'\xef\xbb\xbf'+b'x\n'*2100
        elif seed%10==1: data=b'a'*51199+b'\xc3'+b'a'*300+b'\nb'
        elif seed%10==2: data=(b'0123456789'*30+b'\n')*200
        else: data=b''.join(rng.choice(pieces) for _ in range(rng.randrange(80)))
        for _ in range(4):
            case={'bytes':base64.b64encode(data).decode()}
            for key,values in [('offset',offsets),('limit',limits)]:
                value=rng.choice(values)
                if value is not None: case[key]=value
            cases.append(case)
    expected=json.loads(subprocess.check_output(['node','--experimental-strip-types','--input-type=module','-e',READ_ORACLE,str(ROOT/'references/pi')],input=json.dumps(cases).encode()))
    with tempfile.TemporaryDirectory(prefix='pangu-read-parity-') as tmp:
        library=Path(tmp)/'read.so'
        subprocess.run([CC,'-shared','-fPIC','-O2',*SRC,*subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl','libutf8proc'],text=True).split(),'-o',str(library)],check=True)
        native=C.CDLL(str(library));reader=C.create_string_buffer(48)
        native.pg_read_tool_open.argtypes=[C.c_void_p,C.c_char_p]
        native.pg_read_tool_definition.argtypes=[C.c_void_p];native.pg_read_tool_definition.restype=Definition
        native.pg_read_tool_close.argtypes=[C.c_void_p]
        assert native.pg_read_tool_open(reader,tmp.encode())==0
        definition=native.pg_read_tool_definition(reader)
        execute=C.CFUNCTYPE(C.c_int,C.c_void_p,Slice,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t))(definition.execute)
        try:
            for i,(case,wanted) in enumerate(zip(cases,expected)):
                (Path(tmp)/'f.txt').write_bytes(base64.b64decode(case['bytes']))
                args=C.create_string_buffer(json.dumps({'path':'f.txt',**{k:v for k,v in case.items() if k!='bytes'}}).encode())
                out=C.create_string_buffer(51201);size=C.c_size_t()
                rc=execute(definition.context,Slice(C.addressof(args),len(args.value)),out,len(out),C.byref(size))
                if 'error' in wanted:
                    assert rc==1,(i,case,wanted,rc)
                else:
                    text=out.raw[:size.value]
                    assert rc in (0,4) and text==wanted['text'].encode(),(i,{k:v for k,v in case.items() if k!='bytes'},rc,len(text),len(wanted['text'].encode()),text[:80],wanted['text'][:80])
        finally: native.pg_read_tool_close(reader)
    print(f'PASS: {len(cases)} read text selections match actual fixed Pi referenceRead/truncateHead')


def flags(opt, extra=()):
    cflags = subprocess.check_output(['pkg-config', '--cflags', 'json-c', 'libcurl', 'libutf8proc'], text=True).split()
    libs = subprocess.check_output(['pkg-config', '--libs', 'json-c', 'libcurl', 'libutf8proc'], text=True).split()
    return ['-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra',
            '-Wno-cast-function-type', opt, '-I' + str(ROOT / 'src/runtime'),
            '-I' + str(ROOT), *cflags, *extra, *libs]


def run(cmd, cwd=None):
    print('$', ' '.join(cmd))
    r = subprocess.run(cmd, cwd=cwd or str(ROOT))
    if r.returncode != 0:
        print(f'FAIL rc={r.returncode}: {" ".join(cmd)}')
        sys.exit(r.returncode)


def main():
    read_differential()
    failures = 0
    with tempfile.TemporaryDirectory(prefix='pangu-tools-asm-') as tmp:
        for opt in ['-O0', '-O2']:
            for extra, label in [([], opt), (['-fsanitize=undefined'], opt + '+ubsan')]:
                objects = []
                for src in SRC:
                    obj = str(Path(tmp) / (Path(src).stem + label.replace('+', '_') + '.o'))
                    run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11', opt,
                         '-I' + str(ROOT / 'src/runtime'), '-c', src, '-o', obj])
                    objects.append(obj)
                for name, test in TESTS:
                    out = str(Path(tmp) / (name + '-' + label.replace('+', '_')))
                    run([CC, *flags(opt, extra), test, *objects, *(['-Wl,--wrap=fstat'] if name=='test_read_tool' else []), '-o', out])
                    r = subprocess.run([out])
                    print(f'== {name} {label}: rc={r.returncode}')
                    if r.returncode != 0:
                        failures += 1
        # 对象导出/导入清单：无未定义的旧 C 核心符号
        obj = str(Path(tmp) / 'tools.o')
        run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-I' + str(ROOT / 'src/runtime'),
             '-c', SRC[0], '-o', obj])
        subprocess.run(['nm', '--undefined-only', obj])
        print('== tools.S undefined imports ^')
        subprocess.run(['nm', '-g', '--defined-only', obj])
        print('== tools.S exports ^')
    if failures:
        print(f'{failures} test runs failed')
        return 1
    print('ALL TOOLS ASM RUNS PASSED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
