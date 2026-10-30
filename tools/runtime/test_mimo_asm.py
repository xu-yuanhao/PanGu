"""Exclusive driver for the MiMo handwritten-assembly batch.

Builds src/runtime/mimo.S + mimo_projection.S + mimo_stream.S with the C
layout header (compile-time assertions) and runs, at O0 and O2 (UBSan on the
C test code where it links):

- the existing protocol suites plus explicit bounded batch-contract cases;
- test_mimo_asm (ABI/boundary suite) linked with mimo_abi.S and --wrap
  interposition that trashes caller-saved registers after every wrapped
  external call and asserts stack alignment at each call site;
- test_mimo_transport (scripted curl stand-in via --wrap, no network);
- test_mimo_alloc (constructor/insertion/serialization rejection ownership).

All artifacts go to a TemporaryDirectory under /tmp and are removed on exit.
This is a task-local driver, not the product build.
"""
from pathlib import Path
import subprocess
import sys
import ctypes as C
import json
import random
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = '/usr/bin/clang-23'
SRC = [str(ROOT / 'src/runtime/mimo.S'),
       str(ROOT / 'src/runtime/mimo_projection.S'),
       str(ROOT / 'src/runtime/mimo_stream.S'),
       str(ROOT / 'src/runtime/request_context.S')]
ABI_WRAPS = ['-Wl,--wrap=' + n for n in (
    'strlen', 'strcmp', 'strncmp', 'memchr', 'memcpy', 'malloc', 'realloc', 'strchr', 'free',
    'getenv', 'strdup', 'json_object_object_add', 'json_object_array_add',
    'json_object_new_string', 'json_object_new_string_len', 'json_object_put',
    'json_tokener_parse_ex', 'json_tokener_get_error', 'json_object_get_string', 'json_object_is_type',
    'json_object_object_get_ex', 'json_object_get_int64', 'memset',
    'json_object_array_length', 'json_object_array_get_idx', 'json_object_get_string_len',
    'json_tokener_get_parse_end', 'json_tokener_free')]
CURL_WRAPS = ['-Wl,--wrap=' + n for n in (
    'curl_easy_init', 'curl_easy_setopt', 'curl_easy_getinfo',
    'curl_easy_cleanup', 'curl_multi_init', 'curl_multi_perform',
    'curl_multi_info_read', 'curl_multi_add_handle', 'curl_multi_cleanup',
    'curl_slist_free_all', 'curl_slist_append', 'json_tokener_new')]

ALLOC_WRAPS = ['-Wl,--wrap=' + name for name in ('json_object_deep_copy', 'json_object_new_object', 'json_object_new_array', 'json_object_new_string', 'json_object_new_string_len', 'json_object_new_boolean', 'json_object_new_int', 'json_object_object_add', 'json_object_array_add', 'json_tokener_parse', 'json_object_to_json_string_ext', 'strdup')]


def flags(opt, extra=()):
    cflags = subprocess.check_output(['pkg-config', '--cflags', 'json-c', 'libcurl'], text=True).split()
    libs = subprocess.check_output(['pkg-config', '--libs', 'json-c', 'libcurl'], text=True).split()
    return ['-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra',
            '-Wno-cast-function-type', opt, '-I' + str(ROOT / 'src/runtime'),
            '-I' + str(ROOT), *cflags, *extra, *libs]


def run(cmd):
    print('$', ' '.join(cmd))
    r = subprocess.run(cmd, cwd=str(ROOT))
    if r.returncode != 0:
        print(f'FAIL rc={r.returncode}: {" ".join(cmd)}')
        sys.exit(r.returncode)


def arguments_differential():
    """Compare finished argument values/errors with the fixed Pi implementation."""
    cases=['', ' ', '\ufeff', '{}', '[]', 'null', 'true', '1', '{"x":.5}', '{"x":1,}', '{"x":1} junk', '\ufeff{}']
    cases += ['{"s":"a'+chr(c)+'b"}' for c in range(32)]
    cases += ['{"s":"a'+chr(92)+escape+'b"}' for escape in ['q','d','x','u12','uZZZZ','u0000','u0041','uD83D'+chr(92)+'uDE00','n','t','"',chr(92)]]
    rng=random.Random(311)
    pieces=['a','中','🙂',chr(92),'"','\n','\r','\t',chr(0)]
    for _ in range(120):
        value={'text':''.join(rng.choice(pieces) for _ in range(rng.randrange(30))), 'items':[1,True,None,{'key':'value'}]}
        encoded=json.dumps(value,ensure_ascii=False)
        cases.extend([encoded,encoded.replace('\\n','\n'),encoded[:-1]])
    oracle=r"""
import {readFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
const source=readFileSync(process.argv[1],'utf8').replace(/^import.*$/mg,'');
const native=await import('data:text/javascript;base64,'+Buffer.from(stripTypeScriptTypes(source,{mode:'strip'})).toString('base64'));
process.stdout.write(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(s=>{const x=native.parseToolCallArguments(s);return {arguments:x.arguments,error:!!x.argumentsError}})));
"""
    expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',oracle,str(ROOT/'references/pi/packages/ai/src/utils/json-parse.ts')],input=json.dumps(cases).encode()))
    class Slice(C.Structure):
        _fields_=[('data',C.c_void_p),('size',C.c_size_t)]
    with tempfile.TemporaryDirectory(prefix='pangu-arguments-parity-') as directory:
        output=Path(directory)/'arguments.so'
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl'],text=True).split()
        subprocess.run([CC,'-shared','-fPIC','-O2',*SRC,*libs,'-o',str(output)],check=True)
        native=C.CDLL(str(output));native.pg_mimo_arguments_parse.argtypes=[Slice,C.POINTER(C.c_void_p)];native.pg_mimo_arguments_parse.restype=C.c_int
        native.json_object_to_json_string_ext.argtypes=[C.c_void_p,C.c_int];native.json_object_to_json_string_ext.restype=C.c_char_p
        native.json_object_put.argtypes=[C.c_void_p]
        for i,(source,wanted) in enumerate(zip(cases,expected)):
            raw=source.encode();buffer=C.create_string_buffer(raw);result=C.c_void_p()
            assert native.pg_mimo_arguments_parse(Slice(C.addressof(buffer),len(raw)),C.byref(result))==0,(i,source)
            actual=json.loads(native.json_object_to_json_string_ext(result,0));native.json_object_put(result)
            assert actual['arguments']==wanted['arguments'] and bool(actual.get('argumentsError'))==wanted['error'],(i,source,wanted,actual)
            if wanted['error']: assert actual['argumentsError'].endswith('Received arguments:\n'+source)
    print(f'PASS: {len(cases)} finished tool arguments match fixed Pi values/error classification')


def strict_schema_differential():
    """Exercise the actual encoder against fixed Pi constrained sampling."""
    cases=[{'type':'object'}, {'type':'object','properties':{}}, {'type':'string'}, True]
    leaves=[{'type':'string'},{'type':'number'},{'type':'boolean'},{'type':'null'}, {'type':['string','null']},{'enum':['a',None]},{'const':None},{'anyOf':[{'type':'string'},{'type':'null'}]}, {'type':'array','items':{'type':'string'}}, {'type':'object','properties':{'x':{'type':'string'}}}, {'type':'string','format':'uuid'},{'type':'string','format':'unsupported'}, {'type':'number','minimum':0}]
    cases += [{'type':'object','properties':{'value':x,'required':{'type':'string'}},'required':['required']} for x in leaves]
    for key in ['$ref','$defs','definitions','allOf','oneOf','patternProperties','dependentSchemas','dependencies','unevaluatedProperties','propertyNames','contains','prefixItems','not','if','then','else','minimum','maximum','exclusiveMinimum','exclusiveMaximum','multipleOf','maxItems','uniqueItems','minContains','maxContains','minProperties','maxProperties']:
        cases.append({'type':'object','properties':{'x':{'type':'string',key:None}}})
    cases += [{'type':'object','properties':{'x':{'type':'array','minItems':n,'items':{'type':'string'}}}} for n in [-1,0,1,2,1.0,True,None]]
    cases += [{'type':'object','properties':{'x':False}}, {'type':'object','required':['missing']}, {'type':'object','required':[1]}, {'type':'object','properties':[]}, {'type':'object','additionalProperties':True}, {'type':'object','additionalProperties':None}, {'type':'object','properties':{'x':{'anyOf':[]}}}, {'type':'object','properties':{'x':{'anyOf':[{'type':'object'}]}}}, {'type':'object','properties':{'x':{'type':'array','items':[]}}}]
    rng=random.Random(918)
    for _ in range(90):
        props={f'p{i}':rng.choice(leaves) for i in range(rng.randrange(1,6))}
        cases.append({'type':'object','properties':props,'required':rng.sample(list(props),rng.randrange(len(props)+1))})
    oracle=r"""
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';
const base=readFileSync(process.argv[1],'utf8').replace(/^import.*$/mg,'');
const provider=readFileSync(process.argv[2],'utf8');
const check=provider.slice(provider.indexOf('const ANTHROPIC_STRICT_UNSUPPORTED_KEYWORDS'),provider.indexOf('\nfunction convertTools('));
const google=readFileSync(process.argv[3],'utf8');
const rewrite=google.slice(google.indexOf('const JSON_SCHEMA_NAME_MAPS'),google.indexOf('/**\n * Convert tools to Gemini'));
const text=stripTypeScriptTypes(base+'\n'+check+'\n'+rewrite+'\nexport {isAnthropicStrictUnsupportedKeyword,rewriteConstAsEnum};',{mode:'strip'});
const lib=await import('data:text/javascript;base64,'+Buffer.from(text).toString('base64'));
const cases=JSON.parse(readFileSync(0,'utf8'));
const result=[false,true].map(a=>cases.map(s=>{try{return lib.makeStrictJsonSchema(s,a?lib.isAnthropicStrictUnsupportedKeyword:undefined)}catch{return null}}));
result.push(cases.map(s=>{try{return lib.rewriteConstAsEnum(lib.makeStrictJsonSchema(s))}catch{return null}}));result.push(cases.map(s=>lib.rewriteConstAsEnum(s)));console.log(JSON.stringify(result));
"""
    expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',oracle,str(ROOT/'references/pi/packages/ai/src/api/constrained-sampling.ts'),str(ROOT/'references/pi/packages/ai/src/api/anthropic-messages.ts'),str(ROOT/'references/pi/packages/ai/src/api/google-shared.ts')],input=json.dumps(cases).encode()))
    bridge=r"""
#include "src/runtime/mimo.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
int schema_request(const char *schema,unsigned protocol,unsigned required,char **out){
 pg_mimo *m=calloc(1,sizeof(*m));pg_session *s=calloc(1,sizeof(*s));if(!m||!s)abort();pg_model model=pg_mimo_model(m);
 json_object *cfg=json_tokener_parse(protocol==2?"{\"api\":\"anthropic-messages\",\"model\":\"test\",\"endpoint\":\"https://example.invalid/messages\",\"modelInfo\":{\"compat\":{\"supportsStrictTools\":true}}}":protocol==1?"{\"api\":\"openai-responses\",\"model\":\"test\",\"endpoint\":\"https://example.invalid/responses\",\"modelInfo\":{\"compat\":{\"supportsStrictMode\":true}}}":"{\"api\":\"openai-completions\",\"model\":\"test\",\"endpoint\":\"https://example.invalid/chat/completions\",\"modelInfo\":{\"compat\":{\"supportsStrictMode\":true}}}");
 if(protocol>=3){json_object_put(cfg);cfg=json_tokener_parse(protocol==3?"{\"api\":\"google-generative-ai\",\"model\":\"gemini-3-flash\",\"endpoint\":\"https://example.invalid/streamGenerateContent\"}":"{\"api\":\"google-vertex\",\"model\":\"gemini-3-flash\",\"endpoint\":\"https://example.invalid/streamGenerateContent\"}");}
 int rc=pg_mimo_configure(m,cfg);json_object_put(cfg);if(rc)abort();pg_tool_options options={.max_bytes=51200,.max_lines=2000,.constrained_sampling=required?"{\"type\":\"json_schema\",\"strict\":\"require\"}":"{\"type\":\"json_schema\",\"strict\":\"prefer\"}"};pg_tool_registry registry={.entries={{.name="check",.description="Check",.schema=schema,.options=&options}},.count=1};pg_mimo_bind_tools(m,&registry);s->count=1;s->messages[0].role=PG_USER;s->messages[0].size=2;memcpy(s->messages[0].text,"hi",2);rc=pg_mimo_encode_request(m,s);*out=rc?0:strdup(m->request);model.close(m);free(s);free(m);return rc;
}
"""
    with tempfile.TemporaryDirectory(prefix='pangu-strict-parity-') as directory:
        source=Path(directory)/'bridge.c';source.write_text(bridge);output=Path(directory)/'strict.so'
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl'],text=True).split()
        subprocess.run([CC,'-shared','-fPIC','-D_POSIX_C_SOURCE=200809L','-O2','-I'+str(ROOT),str(source),*SRC,*libs,'-o',str(output)],check=True)
        native=C.CDLL(str(output));native.schema_request.argtypes=[C.c_char_p,C.c_uint,C.c_uint,C.POINTER(C.c_void_p)];native.schema_request.restype=C.c_int;native.free.argtypes=[C.c_void_p]
        count=0
        for protocol in range(5):
            for index,schema in enumerate(cases):
                wanted=expected[2 if protocol>=3 else protocol==2][index]
                if protocol==2 and wanted is not None:wanted={**wanted,'properties':wanted.get('properties',{})}
                for required in range(2):
                    out=C.c_void_p();rc=native.schema_request(json.dumps(schema).encode(),protocol,required,C.byref(out));count+=1
                    if wanted is None and required:assert rc and not out.value,(protocol,index,schema)
                    else:
                        assert not rc and out.value,(protocol,index,schema)
                        wire=json.loads(C.string_at(out));native.free(out)
                        tool=wire['tools'][0];tool=tool['functionDeclarations'][0] if protocol>=3 else tool['function'] if protocol==0 else tool
                        strict=wire.get('toolConfig',{}).get('functionCallingConfig',{}).get('mode')=='VALIDATED' if protocol>=3 else tool.get('strict') is True
                        if wanted is not None:
                            assert strict,(protocol,index,tool)
                            actual=tool['parametersJsonSchema' if protocol>=3 else 'input_schema' if protocol==2 else 'parameters']
                            assert actual==wanted,(protocol,index,schema,actual,wanted)
                        else:
                            assert not strict,(protocol,index,tool)
                            if protocol>=3:assert tool['parametersJsonSchema']==expected[3][index],(protocol,index,tool)
    print(f'PASS: {count} strict/prefer native tool requests match fixed Pi schema conversion across Chat, Responses, Anthropic, Google and Vertex')

def main():
    failures = 0
    with tempfile.TemporaryDirectory(prefix='pangu-mimo-asm-') as tmp:
        for opt in ['-O0', '-O2']:
            for extra, label in [([], opt), (['-fsanitize=undefined', '-fno-sanitize-recover=all'], opt + '+ubsan')]:
                tag = label.replace('+', '_')
                objects = []
                for src in SRC:
                    obj = str(Path(tmp) / (Path(src).stem + '-' + tag + '.o'))
                    run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11', opt,
                         '-I' + str(ROOT / 'src/runtime'), '-c', src, '-o', obj])
                    objects.append(obj)
                suites = [
                    ('stream', [str(ROOT / 'tests/runtime/test_mimo_stream.c')], []),
                    ('projection', [str(ROOT / 'tests/runtime/test_projection.c')], []),
                    ('asm', [str(ROOT / 'tests/runtime/test_mimo_asm.c'),
                             str(ROOT / 'tests/runtime/mimo_abi.S')], ABI_WRAPS),
                    ('transport', [str(ROOT / 'tests/runtime/test_mimo_transport.c'), str(ROOT / 'tests/runtime/mimo_abi.S')], ABI_WRAPS + CURL_WRAPS),
                    ('alloc', [str(ROOT / 'tests/runtime/test_mimo_alloc.c')], ALLOC_WRAPS),
                ]
                for name, tests, wraps in suites:
                    out = str(Path(tmp) / ('test-mimo-' + name + '-' + tag))
                    run([CC, *flags(opt, extra), *tests, *objects, *wraps, '-o', out])
                    r = subprocess.run([out])
                    print(f'== mimo-{name} {label}: rc={r.returncode}')
                    if r.returncode != 0:
                        failures += 1
        # 核对模型模块导入/导出清单。
        for src in SRC:
            obj = str(Path(tmp) / (Path(src).stem + '-audit.o'))
            run([CC, '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                 '-I' + str(ROOT / 'src/runtime'), '-c', src, '-o', obj])
            subprocess.run(['nm', '-g', '--defined-only', obj])
            print(f'== {Path(src).name} exports ^')
            subprocess.run(['nm', '--undefined-only', obj])
            print(f'== {Path(src).name} undefined imports ^')
    if failures:
        print(f'{failures} test runs failed')
        return 1
    arguments_differential()
    strict_schema_differential()
    print('ALL MIMO ASM RUNS PASSED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
