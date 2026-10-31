"""Exclusive driver for the MiMo handwritten-assembly batch.

Builds src/runtime/mimo.S + mimo_projection.S + mimo_stream.S with the C
layout header (compile-time assertions) and runs, at O0 and O2 (UBSan on the
C test code where it links):

- the existing protocol suites plus explicit bounded batch-contract cases;
- test_mimo_asm (ABI/boundary suite) linked with mimo_abi.S and --wrap
  interposition that trashes caller-saved registers after every wrapped
  external call and asserts stack alignment at each call site;
- test_mimo_transport (scripted curl replay plus a local TLS SigV4 verification);
- test_mimo_alloc (constructor/insertion/serialization rejection ownership).

All artifacts go to a TemporaryDirectory under /tmp and are removed on exit.
This is a task-local driver, not the product build.
"""
from pathlib import Path
import subprocess
import sys
import ctypes as C
import json
import base64
import os
import ssl
import threading
import time
import math
import hashlib
import hmac
import re
from http.server import HTTPServer, ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlsplit
import random
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CC = '/usr/bin/clang-23'
SRC = [str(ROOT / 'src/runtime/mimo.S'),
       str(ROOT / 'src/runtime/mimo_projection.S'),
       str(ROOT / 'src/runtime/mimo_stream.S'),
       str(ROOT / 'src/runtime/request_context.S'),
       str(ROOT / 'src/runtime/context_tokens.S'),
       str(ROOT / 'src/runtime/base64.S'),
       str(ROOT / 'src/runtime/oauth.S'),
       str(ROOT / 'src/runtime/oauth_device.S'),
       str(ROOT / 'src/runtime/oauth_http.c')]
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
    'curl_slist_free_all', 'curl_slist_append', 'json_tokener_new', 'ZSTD_compress')]

ALLOC_WRAPS = ['-Wl,--wrap=' + name for name in ('json_object_deep_copy', 'json_object_new_object', 'json_object_new_array', 'json_object_new_string', 'json_object_new_string_len', 'json_object_new_boolean', 'json_object_new_int', 'json_object_object_add', 'json_object_array_add', 'json_tokener_parse', 'json_object_to_json_string_ext', 'strdup')]


def flags(opt, extra=()):
    cflags = subprocess.check_output(['pkg-config', '--cflags', 'json-c', 'libcurl', 'libcrypto', 'glib-2.0', 'libzstd'], text=True).split()
    libs = subprocess.check_output(['pkg-config', '--libs', 'json-c', 'libcurl', 'libcrypto', 'glib-2.0', 'libzstd'], text=True).split()
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
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl', 'libcrypto', 'glib-2.0', 'libzstd'],text=True).split()
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
 if(protocol==5){json_object_put(cfg);cfg=json_tokener_parse("{\"api\":\"bedrock-converse-stream\",\"model\":\"anthropic.claude-opus-4-8\",\"endpoint\":\"https://example.invalid/model/test/converse-stream\",\"modelInfo\":{\"compat\":{\"supportsStrictMode\":true}}}");}
 int rc=pg_mimo_configure(m,cfg);json_object_put(cfg);if(rc)abort();pg_tool_options options={.max_bytes=51200,.max_lines=2000,.constrained_sampling=required?"{\"type\":\"json_schema\",\"strict\":\"require\"}":"{\"type\":\"json_schema\",\"strict\":\"prefer\"}"};pg_tool_registry registry={.entries={{.name="check",.description="Check",.schema=schema,.options=&options}},.count=1};pg_mimo_bind_tools(m,&registry);s->count=1;s->messages[0].role=PG_USER;s->messages[0].size=2;memcpy(s->messages[0].text,"hi",2);rc=pg_mimo_encode_request(m,s);*out=rc?0:strdup(m->request);model.close(m);free(s);free(m);return rc;
}
"""
    with tempfile.TemporaryDirectory(prefix='pangu-strict-parity-') as directory:
        source=Path(directory)/'bridge.c';source.write_text(bridge);output=Path(directory)/'strict.so'
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl', 'libcrypto', 'glib-2.0', 'libzstd'],text=True).split()
        subprocess.run([CC,'-shared','-fPIC','-D_POSIX_C_SOURCE=200809L','-O2','-I'+str(ROOT),str(source),*SRC,*libs,'-o',str(output)],check=True)
        native=C.CDLL(str(output));native.schema_request.argtypes=[C.c_char_p,C.c_uint,C.c_uint,C.POINTER(C.c_void_p)];native.schema_request.restype=C.c_int;native.free.argtypes=[C.c_void_p]
        count=0
        for protocol in range(6):
            for index,schema in enumerate(cases):
                wanted=expected[2 if protocol in (3,4) else protocol==2][index]
                if protocol==2 and wanted is not None:wanted={**wanted,'properties':wanted.get('properties',{})}
                for required in range(2):
                    out=C.c_void_p();rc=native.schema_request(json.dumps(schema).encode(),protocol,required,C.byref(out));count+=1
                    if wanted is None and required:assert rc and not out.value,(protocol,index,schema)
                    else:
                        assert not rc and out.value,(protocol,index,schema)
                        wire=json.loads(C.string_at(out));native.free(out)
                        tool=wire['toolConfig']['tools'][0]['toolSpec'] if protocol==5 else wire['tools'][0];tool=tool['functionDeclarations'][0] if protocol in (3,4) else tool['function'] if protocol==0 else tool
                        strict=wire.get('toolConfig',{}).get('functionCallingConfig',{}).get('mode')=='VALIDATED' if protocol in (3,4) else tool.get('strict') is True
                        if wanted is not None:
                            assert strict,(protocol,index,tool)
                            actual=tool['inputSchema']['json'] if protocol==5 else tool['parametersJsonSchema' if protocol in (3,4) else 'input_schema' if protocol==2 else 'parameters']
                            assert actual==wanted,(protocol,index,schema,actual,wanted)
                        else:
                            assert not strict,(protocol,index,tool)
                            if protocol in (3,4):assert tool['parametersJsonSchema']==expected[3][index],(protocol,index,tool)
                            if protocol==5:assert tool['inputSchema']['json']==schema,(protocol,index,tool)
    print(f'PASS: {count} strict/prefer native tool requests match fixed Pi schema conversion across Chat, Responses, Anthropic, Google, Vertex and Bedrock')

def bedrock_thinking_differential():
    source=ROOT/'references/pi/packages/ai/src/api/bedrock-converse-stream.ts'
    models=[
        {'id':name,'name':name,'reasoning':True}
        for name in ('anthropic.claude-sonnet-4-5','us.anthropic.claude-opus-4-6','anthropic.claude-opus-4-7','anthropic.claude-opus-4-8','anthropic.claude-opus-5','anthropic.claude-sonnet-4-6','anthropic.claude-sonnet-5','anthropic.claude-haiku-5','anthropic.claude-fable-5','openai.gpt-oss-120b','openai.gpt-5.6','qwen.qwen3','us-gov.anthropic.claude-opus-4-8','arn:aws-us-gov:bedrock:us-gov-west-1:123:inference-profile/anthropic.claude-opus-4-8')
    ]
    models += [{'id':'arn:aws:bedrock:us-west-2:123:application-inference-profile/example','name':'Claude Opus_4.8','reasoning':True},{'id':'anthropic.claude-opus-4-8','reasoning':False}]
    cases=[]
    for model in models:
        for level in ('minimal','low','medium','high','xhigh','max'):
            for extra in ({},{'thinkingBudgets':{'low':3333,'high':5555},'interleavedThinking':False,'thinkingDisplay':'omitted'},{'region':'us-gov-west-1','thinkingDisplay':'omitted'}):
                for mapped in (False,True):
                    descriptor=dict(model)
                    if mapped: descriptor['thinkingLevelMap']={'minimal':'medium','xhigh':'low','max':'max'}
                    cases.append({'model':descriptor,'options':dict(extra,reasoning=level)})
    oracle=r"""
import {readFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
const getProviderEnvValue=(name,env)=>env?.[name];
const source=readFileSync(process.argv[1],'utf8').replace(/^import\b[\s\S]*?;\s*/gm,'').replace(/^export /gm,'');
const code=stripTypeScriptTypes(source,{mode:'strip'});
const prefix="const CachePointType={DEFAULT:'default'}, CacheTTL={ONE_HOUR:'1h'}; const sanitizeSurrogates=s=>s; const resolveJsonSchemaStrictSampling=()=>false; const getJsonSchemaToolParameters=t=>t.parameters;";
const evaluate=new Function('getProviderEnvValue',prefix+code+';return {thinking:buildAdditionalModelRequestFields,system:buildSystemPrompt,tools:convertToolConfig};');
const build=evaluate(getProviderEnvValue);
const cases=JSON.parse(readFileSync(0,'utf8'));
process.stdout.write(JSON.stringify(cases.map(x=>process.argv[2]==='policies'?{system:build.system('sys',x.model,x.options.cacheRetention,x.options.env),tools:build.tools([{name:'check',description:'Check',parameters:{type:'object',properties:{value:{type:'string'}},additionalProperties:false}}],x.options.toolChoice,false)??null}:build.thinking(x.model,x.options)??null)));
"""
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',oracle,str(source)],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    bridge=r"""
#define _POSIX_C_SOURCE 200809L
#include "src/runtime/mimo.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
int emit_configuration(const char *source,char **out){
 pg_mimo *m=calloc(1,sizeof(*m));pg_session *s=calloc(1,sizeof(*s));if(!m||!s)abort();pg_model model=pg_mimo_model(m);json_object *cfg=json_tokener_parse(source);*out=0;int rc=pg_mimo_configure(m,cfg);json_object_put(cfg);if(!rc){pg_tool_registry registry={.entries={{.name="check",.description="Check",.schema="{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\"}},\"additionalProperties\":false}"}},.count=1};pg_mimo_bind_tools(m,&registry);s->system_size=3;memcpy(s->system_prompt,"sys",3);s->count=1;s->messages[0].role=PG_USER;s->messages[0].size=2;memcpy(s->messages[0].text,"hi",2);rc=pg_mimo_encode_request(m,s);if(!rc)*out=strdup(m->request);}model.close(m);free(s);free(m);return rc;
}
void release_configuration(char *value){free(value);}
"""
    with tempfile.TemporaryDirectory(prefix='pangu-bedrock-thinking-') as directory:
        adapter=Path(directory)/'adapter.c';adapter.write_text(bridge)
        output=Path(directory)/'model.so'
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl', 'libcrypto', 'glib-2.0', 'libzstd'],text=True).split()
        subprocess.run([CC,'-shared','-fPIC','-O2','-I'+str(ROOT),str(adapter),*SRC,*libs,'-o',str(output)],check=True)
        native=C.CDLL(str(output));native.emit_configuration.argtypes=[C.c_char_p,C.POINTER(C.c_void_p)];native.emit_configuration.restype=C.c_int;native.release_configuration.argtypes=[C.c_void_p]
        for case,wanted in zip(cases,expected):
            model=case['model']
            alias=dict(case['options']);alias['reasoningEffort']=alias.pop('reasoning')
            for options in (case['options'],alias):
                cfg={'api':'bedrock-converse-stream','model':model['id'],'endpoint':'https://example.invalid/model/test/converse-stream','modelInfo':model,'options':options}
                result=C.c_void_p();status=native.emit_configuration(json.dumps(cfg).encode(),C.byref(result));assert not status,case
                actual=json.loads(C.string_at(result));native.release_configuration(result)
                assert actual.get('additionalModelRequestFields')==wanted,(case,wanted,actual.get('additionalModelRequestFields'))
                assert 'thinking' not in actual and 'output_config' not in actual
        policy_models=models+[{'id':name,'name':name,'reasoning':False} for name in ('anthropic.claude-3-7-sonnet','anthropic.claude-3-5-haiku','anthropic.claude-3-5-sonnet')]
        policies=[{'model':model,'options':{'cacheRetention':retention,'toolChoice':choice,'env':{'AWS_BEDROCK_FORCE_CACHE':'1'} if force else {}}} for model in policy_models for retention in ('short','long','none') for choice in (None,'auto','any','none',{'type':'tool','name':'check'}) for force in (False,True)]
        wanted_policies=json.loads(subprocess.run(['node','--input-type=module','-e',oracle,str(source),'policies'],input=json.dumps(policies),text=True,capture_output=True,check=True).stdout)
        previous_force=os.environ.get('AWS_BEDROCK_FORCE_CACHE')
        try:
            for case,wanted in zip(policies,wanted_policies):
                force=case['options']['env'].get('AWS_BEDROCK_FORCE_CACHE')
                if force: os.environ['AWS_BEDROCK_FORCE_CACHE']=force
                else: os.environ.pop('AWS_BEDROCK_FORCE_CACHE',None)
                cfg={'api':'bedrock-converse-stream','model':case['model']['id'],'modelInfo':case['model'],'endpoint':'https://example.invalid/model/test/converse-stream','sessionId':'pangu-cache-policy','options':dict(case['options'],requestMetadata={'project':'pangu'})}
                result=C.c_void_p();status=native.emit_configuration(json.dumps(cfg).encode(),C.byref(result));assert not status,case
                actual=json.loads(C.string_at(result));native.release_configuration(result)
                assert actual.get('system')==wanted['system'] and actual.get('toolConfig')==wanted['tools'],(case,wanted,actual)
                cache=[x for x in wanted['system'] if 'cachePoint' in x]
                assert actual['messages'][-1]['content']==[{'text':'hi'}]+cache,(case,actual)
                assert 'prompt_cache_key' not in actual and actual['requestMetadata']=={'project':'pangu'}
        finally:
            if previous_force is None: os.environ.pop('AWS_BEDROCK_FORCE_CACHE',None)
            else: os.environ['AWS_BEDROCK_FORCE_CACHE']=previous_force
        samples=['','AAE=','AAE','/w==','A','A=','A===','!!!!','YR==','YQ==',' YQ==\n','Y\tQ','Y\vQ','YWJj\x00','YQ==more','====','ĀAAA']
        samples += [base64.b64encode(bytes(range(n))).decode() for n in range(1,10)]
        samples += [base64.b64encode(bytes(range(256))*40).decode()]
        codec_oracle="import {readFileSync} from 'node:fs';process.stdout.write(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(s=>{try{return btoa(atob(s))}catch{return null}})));"
        normalized=json.loads(subprocess.check_output(['node','--input-type=module','-e',codec_oracle],input=json.dumps(samples),text=True))
        native.json_tokener_parse.argtypes=[C.c_char_p];native.json_tokener_parse.restype=C.c_void_p
        native.json_object_get_string.argtypes=[C.c_void_p];native.json_object_get_string.restype=C.c_char_p
        native.json_object_put.argtypes=[C.c_void_p]
        native.pg_mimo_base64_decode.argtypes=[C.c_void_p,C.POINTER(C.c_void_p),C.POINTER(C.c_size_t)];native.pg_mimo_base64_decode.restype=C.c_int
        native.pg_mimo_base64_encode.argtypes=[C.c_void_p,C.c_size_t];native.pg_mimo_base64_encode.restype=C.c_void_p
        for sample,wanted in zip(samples,normalized):
            value=native.json_tokener_parse(json.dumps(sample).encode());decoded=C.c_void_p();length=C.c_size_t()
            status=native.pg_mimo_base64_decode(value,C.byref(decoded),C.byref(length));native.json_object_put(value)
            if wanted is None: assert status==1 and not decoded.value
            else:
                assert not status
                encoded=native.pg_mimo_base64_encode(decoded,length);assert encoded and native.json_object_get_string(encoded).decode()==wanted
                native.json_object_put(encoded);native.release_configuration(decoded)
    print(f'PASS: {len(cases)} Bedrock thinking configurations match fixed Pi model-family, effort, budget, mapping and GovCloud behavior, including native thinking selection; {len(policies)} tool/cache policies match Pi; {len(samples)} opaque codec cases match atob/btoa')

def cost_differential():
    source=(ROOT/'references/pi/packages/ai/src/models.ts').read_text()
    start=source.index('export function calculateCost(')
    function=source[start:source.index('\nconst EXTENDED_THINKING_LEVELS',start)]
    prices=[{'input':3,'output':15,'cacheRead':0.3,'cacheWrite':3.75},{'input':0,'output':0,'cacheRead':0,'cacheWrite':0}]
    tiers=[{'inputTokensAbove':limit,'input':rate,'output':rate*5,'cacheRead':rate/10,'cacheWrite':rate*1.25} for limit,rate in ((200,9),(100,6),(100,99),(0,4))]
    prices += [dict(prices[0],tiers=tiers),dict(prices[0],tiers=list(reversed(tiers)))]
    cases=[]
    for price in prices:
        for total in (0,1,99,100,101,199,200,201,1000000):
            for cache in (0,total//3,total):
                for long_write in (0,cache//2,cache):
                    cases.append({'model':{'cost':price},'usage':{'input':total-cache,'output':17,'cacheRead':0,'cacheWrite':cache,'cacheWrite1h':long_write,'cost':{}}})
    oracle="import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const x=JSON.parse(readFileSync(0,'utf8'));const build=new Function(stripTypeScriptTypes(x.function.replace('export ',''),{mode:'strip'})+';return calculateCost')();process.stdout.write(JSON.stringify(x.cases.map(c=>build(c.model,c.usage))));"
    expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',oracle],input=json.dumps({'function':function,'cases':cases}),text=True))
    with tempfile.TemporaryDirectory(prefix='pangu-cost-parity-') as directory:
        src=Path(directory)/'mimo.S'
        src.write_text((ROOT/'src/runtime/mimo.S').read_text()+'\n.text\n.globl pg_cost_parity\n.set pg_cost_parity,.Lmc_calculate\n.globl pg_codex_tier_parity\n.set pg_codex_tier_parity,.Lcd_price\n')
        output=Path(directory)/'cost.so'
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl','libcrypto', 'glib-2.0', 'libzstd'],text=True).split()
        subprocess.run([CC,'-shared','-fPIC','-O2','-I'+str(ROOT/'src/runtime'),str(src),*SRC[1:],*libs,'-o',str(output)],check=True)
        native=C.CDLL(str(output));native.json_tokener_parse.argtypes=[C.c_char_p];native.json_tokener_parse.restype=C.c_void_p
        native.json_object_to_json_string_ext.argtypes=[C.c_void_p,C.c_int];native.json_object_to_json_string_ext.restype=C.c_char_p
        native.json_object_put.argtypes=[C.c_void_p];native.pg_cost_parity.argtypes=[C.c_void_p,C.c_void_p]
        for case,wanted in zip(cases,expected):
            configuration=native.json_tokener_parse(json.dumps({'modelInfo':case['model']}).encode());message=native.json_tokener_parse(json.dumps({'usage':case['usage']}).encode())
            try:
                assert not native.pg_cost_parity(configuration,message)
                actual=json.loads(native.json_object_to_json_string_ext(message,0))['usage']['cost']
                for key,value in wanted.items():
                    assert actual[key]==value if isinstance(value,bool) else math.isclose(actual[key],value,rel_tol=1e-13,abs_tol=1e-15),(case,key,value,actual)
            finally:native.json_object_put(configuration);native.json_object_put(message)
        codex_source=(ROOT/'references/pi/packages/ai/src/api/openai-codex-responses.ts').read_text()
        tier_functions=codex_source[codex_source.index('function getServiceTierCostMultiplier('):codex_source.index('function resolveCodexUrl(')]
        tier_cases=[{'model':{'id':model},'request':request,'response':response,'usage':{'cost':{'input':1.25,'output':2.5,'cacheRead':0.25,'cacheWrite':0.75,'total':4.75}}} for model in ('gpt-5.5','gpt-5.5-mini','gpt-6') for request in (None,'default','flex','priority','auto') for response in (None,'default','flex','priority','auto')]
        tier_oracle="import{readFileSync}from'node:fs';import{stripTypeScriptTypes}from'node:module';const x=JSON.parse(readFileSync(0,'utf8'));const f=new Function(stripTypeScriptTypes(x.function,{mode:'strip'})+';return{resolveCodexServiceTier,applyServiceTierPricing}')();process.stdout.write(JSON.stringify(x.cases.map(c=>{f.applyServiceTierPricing(c.usage,f.resolveCodexServiceTier(c.response??undefined,c.request??undefined),c.model);return c.usage.cost})));"
        tier_expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',tier_oracle],input=json.dumps({'function':tier_functions,'cases':tier_cases}),text=True))
        native.pg_codex_tier_parity.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p]
        for case,wanted in zip(tier_cases,tier_expected):
            cfg=native.json_tokener_parse(json.dumps({'api':'openai-codex-responses','model':case['model']['id'],'options':{'serviceTier':case['request']}}).encode())
            terminal=native.json_tokener_parse(json.dumps({'service_tier':case['response']}).encode())
            message=native.json_tokener_parse(json.dumps({'usage':case['usage']}).encode())
            try:
                assert not native.pg_codex_tier_parity(cfg,terminal,message)
                actual=json.loads(native.json_object_to_json_string_ext(message,0))['usage']['cost']
                assert actual==wanted,(case,actual,wanted)
            finally:
                native.json_object_put(cfg);native.json_object_put(terminal);native.json_object_put(message)
        print(f'PASS: {len(tier_cases)} Codex response/request tier resolution and native prices match fixed Pi functions')
    print(f'PASS: {len(cases)} native cost calculations match fixed Pi tiers, strict thresholds, duplicate ordering, free rates and cache durations')

def context_budget_differential():
    base=ROOT/'references/pi/packages/ai/src'
    estimate=(base/'utils/estimate.ts').read_text()
    text=(base/'utils/text.ts').read_text()
    simple=(base/'api/simple-options.ts').read_text()
    clamp=simple[simple.index('const CONTEXT_SAFETY_TOKENS'):]
    cases=[]
    for value in ('','hello','中文😀','a'*127):
        for content in (value,[{'type':'text','text':value}],[{'type':'text','text':value},{'type':'image','data':'','mimeType':'image/png'}]):
            cases.append([{'role':'user','content':content,'timestamp':1}])
    for total in (0,999):
        for stop in ('stop','error','aborted'):
            for prefix in (False,True):
                cases.append([{'role':'system','content':'sys','sections':{'empty':'','context':'上下文😀','removed':None},'toolsAdded':[{'name':'check','description':'Check / workspace','parameters':{'type':'object'}}],'timestamp':100 if prefix else 0},{'role':'assistant','content':[{'type':'thinking','thinking':'think'},{'type':'text','text':'answer'},{'type':'toolCall','name':'check','arguments':{'path':'中文/file'}}],'stopReason':stop,'usage':{'totalTokens':total,'input':200,'output':10,'cacheRead':3,'cacheWrite':2},'timestamp':2},{'role':'toolResult','content':[{'type':'text','text':'result'},{'type':'image','mimeType':'image/png','data':''}],'timestamp':3}])
    oracle="import{readFileSync}from'node:fs';import{stripTypeScriptTypes}from'node:module';const x=JSON.parse(readFileSync(0,'utf8'));const clean=s=>s.replace(/^import.*$/gm,'').replace(/^export /gm,'');const f=new Function(stripTypeScriptTypes(clean(x.text)+'\\n'+clean(x.estimate)+'\\n'+clean(x.clamp),{mode:'strip'})+';return {estimateContextTokens,clampMaxTokensToContext,adjustMaxTokensForThinking}')();process.stdout.write(JSON.stringify(x.cases.map(c=>f.estimateContextTokens(c).tokens)));"
    cases.append({'systemPrompt':'s'*350,'messages':[{'role':'user','content':'hello','timestamp':1}]})
    oracle=oracle.replace('f.estimateContextTokens(c).tokens',"f.estimateContextTokens(Array.isArray(c)?c:[{role:'system',content:c.systemPrompt,timestamp:0},...c.messages]).tokens")
    expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',oracle],input=json.dumps({'text':text,'estimate':estimate,'clamp':clamp,'cases':cases}),text=True))
    bridge=r"""
#include "src/runtime/mimo.h"
#include "src/runtime/context_tokens.h"
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
int emit_budget(const char *configuration,const char *messages,char **output){
 pg_mimo provider={0};pg_model model=pg_mimo_model(&provider);pg_session *session=calloc(1,sizeof(*session));struct json_object *cfg=json_tokener_parse(configuration),*context=json_object_new_object();*output=NULL;if(!session||!cfg||!context)abort();struct json_object *input=json_tokener_parse(messages);if(json_object_is_type(input,json_type_object)){json_object_put(context);context=input;}else json_object_object_add(context,"messages",input);session->count=1;session->messages[0].role=PG_CONTEXT_VIEW;session->messages[0].context_view=context;int status=pg_mimo_configure(&provider,cfg);if(!status)status=pg_mimo_encode_request(&provider,session);if(!status)*output=strdup(provider.request);model.close(model.context);json_object_put(context);json_object_put(cfg);free(session);return status;
}
void release_budget(char *text){free(text);}
"""
    with tempfile.TemporaryDirectory(prefix='pangu-context-budget-') as directory:
        adapter=Path(directory)/'adapter.c';adapter.write_text(bridge)
        output=Path(directory)/'model.so'
        libs=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libcurl','libcrypto','glib-2.0','libzstd'],text=True).split()
        subprocess.run([CC,'-shared','-fPIC','-O2','-D_POSIX_C_SOURCE=200809L','-I'+str(ROOT),str(adapter),*SRC,*libs,'-o',str(output)],check=True)
        native=C.CDLL(str(output));native.json_tokener_parse.argtypes=[C.c_char_p];native.json_tokener_parse.restype=C.c_void_p;native.json_object_put.argtypes=[C.c_void_p]
        native.pg_context_estimate.argtypes=[C.c_void_p];native.pg_context_estimate.restype=C.c_uint64
        for case,wanted in zip(cases,expected):
            value=native.json_tokener_parse(json.dumps(case).encode())
            try: assert native.pg_context_estimate(value)==wanted,(case,wanted,native.pg_context_estimate(value))
            finally: native.json_object_put(value)
        native.emit_budget.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];native.release_budget.argtypes=[C.c_void_p]
        import os,re
        native.pg_mimo_environment_key.argtypes=[C.c_void_p,C.POINTER(C.c_uint)];native.pg_mimo_environment_key.restype=C.c_char_p
        environment_cases=[]
        for source in sorted((base/'providers').glob('*.ts')):
            code=source.read_text();identity=re.search(r'id:\s*"([^"]+)"',code);auth=re.search(r'envApiKeyAuth\([^\n]+?\[([^]]+)\]',code)
            if identity and auth: environment_cases.extend((identity[1],key) for key in re.findall(r'"([^"]+)"',auth[1]))
        environment_cases.extend([('anthropic','ANTHROPIC_API_KEY'),('google-vertex','GOOGLE_CLOUD_API_KEY')])
        saved={key:os.environ.get(key) for key in {'PANGU_MODEL_API_KEY',*(key for _,key in environment_cases),'ANTHROPIC_AUTH_TOKEN'}}
        try:
            for key in saved: os.environ.pop(key,None)
            for provider,key in environment_cases:
                cfg=native.json_tokener_parse(json.dumps({'modelInfo':{'provider':provider}}).encode());mode=C.c_uint(99)
                try:
                    assert native.pg_mimo_environment_key(cfg,C.byref(mode)) is None and mode.value==0
                    os.environ[key]=' \t\r\n';assert native.pg_mimo_environment_key(cfg,C.byref(mode)) is None
                    os.environ[key]='unit-provider-secret';assert native.pg_mimo_environment_key(cfg,C.byref(mode))==b'unit-provider-secret' and mode.value==0
                    os.environ['PANGU_MODEL_API_KEY']='unit-explicit-secret';assert native.pg_mimo_environment_key(cfg,C.byref(mode))==b'unit-explicit-secret'
                finally: native.json_object_put(cfg);os.environ.pop(key,None);os.environ.pop('PANGU_MODEL_API_KEY',None)
        finally:
            for key,value in saved.items():
                if value is None: os.environ.pop(key,None)
                else: os.environ[key]=value
        print(f'PASS: {len(environment_cases)} fixed Pi provider environment mappings, whitespace and explicit priority')
        count=0
        for api in ('openai-completions','openai-responses','anthropic-messages','google-generative-ai','google-vertex','bedrock-converse-stream','azure-openai-responses','openai-codex-responses'):
            for window in (0,4096,4200,128000):
                for cap in (None,17,10000):
                    messages=[{'role':'user','content':[{'type':'text','text':'hello'}],'timestamp':1}]
                    cfg={'api':api,'model':'budget-test','endpoint':'https://provider.invalid/request','modelInfo':{'maxTokens':2048,'contextWindow':window},'options':{} if cap is None else {'maxTokens':cap}}
                    result=C.c_void_p();status=native.emit_budget(json.dumps(cfg).encode(),json.dumps(messages).encode(),C.byref(result));assert not status,(api,window,cap)
                    wire=json.loads(C.string_at(result));native.release_budget(result)
                    wanted=2048 if cap is None else cap
                    if window>0: wanted=min(wanted,max(1,window-4096-2))
                    if api=='openai-codex-responses': assert 'max_output_tokens' not in wire
                    else:
                        actual=wire.get('max_tokens',wire.get('max_completion_tokens',wire.get('max_output_tokens')))
                        if api.startswith('google-'): actual=wire['generationConfig']['maxOutputTokens']
                        if api=='bedrock-converse-stream': actual=wire['inferenceConfig']['maxTokens']
                        if api in ('openai-responses','azure-openai-responses'): wanted=max(16,wanted)
                        assert actual==wanted,(api,window,cap,wanted,actual)
                    count+=1
        cfg={'api':'openai-completions','model':'budget-prefix','endpoint':'https://provider.invalid/request','modelInfo':{'maxTokens':2048,'contextWindow':4200}}
        result=C.c_void_p();assert not native.emit_budget(json.dumps(cfg).encode(),json.dumps(cases[-1]).encode(),C.byref(result))
        wire=json.loads(C.string_at(result));native.release_budget(result);assert wire['max_tokens']==2,wire
        thinking_cases=[{'model':{'maxTokens':maximum,'contextWindow':window},'context':[{'role':'user','content':[{'type':'text','text':'hello'}],'timestamp':1}],'cap':cap,'budget':budget,'level':level} for maximum in (512,5000) for window in (0,4200,128000) for cap in (None,500) for budget in (None,3333,0) for level in ('low','max')]
        thinking_oracle=oracle.replace("x.cases.map(c=>f.estimateContextTokens(Array.isArray(c)?c:[{role:'system',content:c.systemPrompt,timestamp:0},...c.messages]).tokens)","x.cases.map(c=>{const base=f.clampMaxTokensToContext(c.model,c.context,c.cap??c.model.maxTokens);const a=f.adjustMaxTokensForThinking(base,c.model.maxTokens,c.level,c.budget===null?undefined:{low:c.budget,high:c.budget});const maxTokens=f.clampMaxTokensToContext(c.model,c.context,a.maxTokens);return {maxTokens,thinkingBudget:Math.min(a.thinkingBudget,Math.max(0,maxTokens-1024))}})")
        thinking_expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',thinking_oracle],input=json.dumps({'text':text,'estimate':estimate,'clamp':clamp,'cases':thinking_cases}),text=True))
        for api in ('anthropic-messages','bedrock-converse-stream'):
            for case,wanted in zip(thinking_cases,thinking_expected):
                descriptor=dict(case['model'],id='anthropic.claude-3-7-sonnet',name='Claude 3.7 Sonnet',reasoning=True)
                options={'reasoningEffort':case['level']}
                if case['budget'] is not None: options['thinkingBudgets']={'low':case['budget'],'high':case['budget']}
                if case['cap'] is not None: options['maxTokens']=case['cap']
                cfg={'api':api,'model':descriptor['id'],'endpoint':'https://provider.invalid/request','modelInfo':descriptor,'options':options}
                result=C.c_void_p();status=native.emit_budget(json.dumps(cfg).encode(),json.dumps(case['context']).encode(),C.byref(result));assert not status,(api,case)
                wire=json.loads(C.string_at(result));native.release_budget(result)
                actual={'maxTokens':wire['max_tokens'] if api=='anthropic-messages' else wire['inferenceConfig']['maxTokens'],'thinkingBudget':(wire['thinking'] if api=='anthropic-messages' else wire['additionalModelRequestFields']['thinking'])['budget_tokens']}
                assert actual==wanted,(api,case,wanted,actual)
        models=(base/'models.ts').read_text()
        level_source=models[models.index('const EXTENDED_THINKING_LEVELS:'):models.index('/**\n * Check if two models are equal')]
        sample_source=simple[simple.index('export function resolveSamplingParams('):simple.index('export function buildBaseOptions(')]
        sampling_cases=[]
        for reasoning in (False,True):
            for mapping in ({},{'low':None,'medium':None},{'off':None,'minimal':None,'low':None,'medium':None,'high':None},{'xhigh':'high','max':'high'}):
                for level in ('off','low','xhigh','max'):
                    sampling_cases.append({'model':{'reasoning':reasoning,'thinkingLevelMap':mapping,'samplingParams':{'temperature':0.25,'native_probe':{'base':True}},'samplingParamsByThinkingLevel':{k:{'native_probe':k,'top_p':0.8} for k in ('off','minimal','low','medium','high','xhigh','max')}},'level':level,'request':{'temperature':None,'model':'sampling-override','extra_probe':{'nested':[1,None]}}})
        sampling_oracle="import{readFileSync}from'node:fs';import{stripTypeScriptTypes}from'node:module';const x=JSON.parse(readFileSync(0,'utf8'));const f=new Function(stripTypeScriptTypes((x.level+x.sample).replace(/^export /gm,''),{mode:'strip'})+';return resolveSamplingParams')();process.stdout.write(JSON.stringify(x.cases.map(c=>f(c.model,c.level,c.request))));"
        sampling_expected=json.loads(subprocess.check_output(['node','--input-type=module','-e',sampling_oracle],input=json.dumps({'level':level_source,'sample':sample_source,'cases':sampling_cases}),text=True))
        for api in ('openai-completions','openai-responses','azure-openai-responses','anthropic-messages','google-generative-ai','google-vertex','bedrock-converse-stream','openai-codex-responses'):
            for case,wanted in zip(sampling_cases,sampling_expected):
                cfg={'api':api,'model':'sampling-test','endpoint':'https://provider.invalid/request','modelInfo':dict(case['model'],maxTokens=2048),'options':{'reasoningEffort':case['level'],'samplingParams':case['request']}}
                if api not in ('openai-completions','openai-responses','azure-openai-responses'): cfg['options']['reasoningEffort']='off'
                result=C.c_void_p();status=native.emit_budget(json.dumps(cfg).encode(),json.dumps([{'role':'user','content':'hello'}]).encode(),C.byref(result));assert not status,(api,case)
                wire=json.loads(C.string_at(result));native.release_budget(result)
                if api in ('openai-completions','openai-responses','azure-openai-responses'):
                    assert {k:wire[k] for k in wanted}==wanted,(api,case,wanted,wire)
                else: assert 'native_probe' not in wire and 'extra_probe' not in wire,(api,wire)
        print(f'PASS: {len(sampling_cases)*8} requests match Pi sampling merge, thinking selection and protocol boundaries')
        print(f'PASS: {len(thinking_cases)*2} Anthropic/Bedrock thinking ceilings match fixed Pi simple-options helpers')
        print(f'PASS: {len(cases)} shared context estimates match fixed Pi, and {count} protocol requests obey model/context output budgets')

def sigv4_wire(binary, directory):
    cert=Path(directory)/'sigv4-cert.pem'
    key=Path(directory)/'sigv4-key.pem'
    subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-keyout',str(key),'-out',str(cert),'-days','1','-subj','/CN=127.0.0.1','-addext','subjectAltName=IP:127.0.0.1'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    failures=[]
    received=[]
    class Receiver(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_POST(self):
            try:
                body=self.rfile.read(int(self.headers['Content-Length']))
                auth=self.headers['Authorization']
                match=re.search(r'Credential=([^, ]+).*SignedHeaders=([^, ]+).*Signature=([0-9a-f]+)',auth)
                assert auth.startswith('AWS4-HMAC-SHA256 ') and match
                credential,signed,signature=match.groups()
                access,date,region,service,terminator=credential.split('/')
                proxy_mode='proxy=1' in self.path
                assert (access,region,service,terminator)==('dummy-access-key' if proxy_mode else 'AKID_PANGU_WIRE','us-west-2','bedrock','aws4_request')
                assert self.headers['X-Amz-Security-Token']==(None if proxy_mode else 'pangu-wire-session')
                if proxy_mode: assert self.request_version=='HTTP/1.1'
                assert self.headers['X-Project']=='pangu-wire'
                headers=''.join(name+':'+ ' '.join(self.headers[name].split())+'\n' for name in signed.split(';'))
                target=urlsplit(self.path)
                canonical='\n'.join(('POST',target.path,target.query,headers,signed,hashlib.sha256(body).hexdigest()))
                scope='/'.join((date,region,service,terminator))
                to_sign='\n'.join(('AWS4-HMAC-SHA256',self.headers['X-Amz-Date'],scope,hashlib.sha256(canonical.encode()).hexdigest()))
                signing=b'AWS4dummy-secret-key' if proxy_mode else b'AWS4pangu-wire-secret'
                for part in (date,region,service,terminator): signing=hmac.new(signing,part.encode(),hashlib.sha256).digest()
                assert hmac.compare_digest(signature,hmac.new(signing,to_sign.encode(),hashlib.sha256).hexdigest())
                received.append(True)
                if 'deadline=1' in self.path: time.sleep(0.2)
                self.send_response(200)
            except Exception as error:
                failures.append(type(error).__name__)
                self.send_response(403)
            self.send_header('X-Amzn-RequestId','wire-public-request-id')
            self.send_header('Content-Length','0')
            self.end_headers()
    server=HTTPServer(('127.0.0.1',0),Receiver)
    tls=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);tls.load_cert_chain(cert,key)
    server.socket=tls.wrap_socket(server.socket,server_side=True)
    server.timeout=15
    worker=threading.Thread(target=lambda:[server.handle_request() for _ in range(3)],daemon=True);worker.start()
    env=os.environ.copy()
    for name in ('PANGU_MODEL_API_KEY','AWS_BEARER_TOKEN_BEDROCK','AWS_PROFILE','AWS_BEDROCK_SKIP_AUTH','AWS_BEDROCK_FORCE_HTTP1'): env.pop(name,None)
    env.update(NO_PROXY='127.0.0.1',no_proxy='127.0.0.1',AWS_ACCESS_KEY_ID='AKID_PANGU_WIRE',AWS_SECRET_ACCESS_KEY='pangu-wire-secret',AWS_SESSION_TOKEN='pangu-wire-session',PANGU_TEST_SIGV4_CA=str(cert),PANGU_TEST_SIGV4_ENDPOINT=f'https://127.0.0.1:{server.server_port}/model/anthropic.claude-test/converse-stream')
    try:
        subprocess.run([binary,'--sigv4-wire'],env=env,check=True,timeout=20)
        assert received==[True] and not failures,failures
        env.update(PANGU_TEST_DEADLINE='1',PANGU_TEST_SIGV4_ENDPOINT=env['PANGU_TEST_SIGV4_ENDPOINT']+'?deadline=1')
        subprocess.run([binary,'--sigv4-wire'],env=env,check=True,timeout=20)
        env.pop('PANGU_TEST_DEADLINE',None)
        env.update(AWS_BEDROCK_SKIP_AUTH='1',AWS_BEDROCK_FORCE_HTTP1='1',PANGU_MODEL_API_KEY='ignored-explicit-token',PANGU_TEST_SIGV4_ENDPOINT=env['PANGU_TEST_SIGV4_ENDPOINT'].split('?')[0]+'?proxy=1')
        subprocess.run([binary,'--sigv4-wire'],env=env,check=True,timeout=20)
        worker.join(2)
        assert received==[True,True,True] and not failures,failures
    finally: server.server_close()

def codex_compressed_wire(binary, directory):
    failures=[]
    received=[]
    failed_once=False
    codec=C.CDLL('libzstd.so.1')
    codec.ZSTD_getFrameContentSize.argtypes=[C.c_void_p,C.c_size_t];codec.ZSTD_getFrameContentSize.restype=C.c_ulonglong
    codec.ZSTD_decompress.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t];codec.ZSTD_decompress.restype=C.c_size_t
    class Receiver(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def do_POST(self):
            nonlocal failed_once
            try:
                raw=self.rfile.read(int(self.headers['Content-Length']))
                assert self.headers['Content-Encoding']=='zstd' and b'\x00' in raw
                size=codec.ZSTD_getFrameContentSize(raw,len(raw));assert size<1024*1024
                plain=C.create_string_buffer(size)
                assert codec.ZSTD_decompress(plain,size,raw,len(raw))==size
                body=json.loads(plain.raw)
                assert body['model']=='codex-wire' and body['store'] is False and body['stream'] is True
                assert body['instructions'] and body['input'] and 'max_output_tokens' not in body
                assert body['prompt_cache_key']=='wire-codex-session'
                assert self.headers['session-id']==self.headers['x-client-request-id']=='wire-codex-session'
                assert self.headers['chatgpt-account-id']=='pangu-test-account' and self.headers['originator']=='pangu'
                received.append(raw)
                if 'headers-timeout=1' in self.path or 'headers-disabled=1' in self.path: time.sleep(0.25)
                if 'retry=1' in self.path and not failed_once:
                    failed_once=True;payload=b'{"error":"overloaded"}';self.send_response(503);self.send_header('Retry-After-Ms','0')
                else:
                    payload=b'data: {"type":"response.done","response":{"status":"completed","end_turn":true}}\n\n';self.send_response(200)
            except Exception as error:
                failures.append(type(error).__name__);payload=b'';self.send_response(400)
            try:
                self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.flush()
                if 'slow-body=1' in self.path: time.sleep(0.25)
                self.wfile.write(payload)
            except OSError as error:
                if 'headers-timeout=1' not in self.path: failures.append(type(error).__name__)
    server=ThreadingHTTPServer(('127.0.0.1',0),Receiver)
    tls=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);tls.load_cert_chain(Path(directory)/'sigv4-cert.pem',Path(directory)/'sigv4-key.pem')
    server.socket=tls.wrap_socket(server.socket,server_side=True);server.timeout=15
    worker=threading.Thread(target=lambda:[server.handle_request() for _ in range(6)],daemon=True);worker.start()
    env=os.environ.copy()
    payload=base64.b64encode(json.dumps({'https://api.openai.com/auth':{'chatgpt_account_id':'pangu-test-account'}}).encode()).decode()
    env.update(NO_PROXY='127.0.0.1',no_proxy='127.0.0.1',PANGU_MODEL_API_KEY='e30.'+payload+'.signature',PANGU_TEST_SIGV4_CA=str(Path(directory)/'sigv4-cert.pem'),PANGU_TEST_CODEX_ENDPOINT=f'https://127.0.0.1:{server.server_port}/codex/responses')
    try:
        subprocess.run([binary,'--codex-wire'],env=env,check=True,timeout=20)
        env.update(PANGU_TEST_REAL_MULTI='1',PANGU_TEST_CODEX_ENDPOINT=env['PANGU_TEST_CODEX_ENDPOINT']+'?retry=1')
        subprocess.run([binary,'--codex-wire'],env=env,check=True,timeout=20)
        for mode,query in (('deadline','headers-timeout'),('slow','slow-body'),('zero','headers-disabled')):
            env.update(PANGU_TEST_HEADER_MODE=mode,PANGU_TEST_CODEX_ENDPOINT=env['PANGU_TEST_CODEX_ENDPOINT'].split('?')[0]+'?'+query+'=1')
            subprocess.run([binary,'--codex-wire'],env=env,check=True,timeout=20)
        worker.join(2)
        assert len(received)==6 and received[1]==received[2] and not failures,failures
    finally: server.server_close()

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
                    elif name=='transport' and label=='-O2':
                        sigv4_wire(out,tmp)
                        codex_compressed_wire(out,tmp)
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
    bedrock_thinking_differential()
    cost_differential()
    context_budget_differential()
    print('ALL MIMO ASM RUNS PASSED')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
