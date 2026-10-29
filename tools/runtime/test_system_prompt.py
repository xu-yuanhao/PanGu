"""Compare native ordered prompt plans with the actual fixed Pi prompt module."""
import ctypes as C
import json
from pathlib import Path
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
ORACLE=r'''
import {registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';import {readFileSync} from 'node:fs';
const root=pathToFileURL(process.argv[1]+'/');registerHooks({resolve(s,c,n){if(s==='@earendil-works/pi-ai/utils/transcript')return {url:new URL('packages/ai/src/utils/transcript.ts',root).href,shortCircuit:true};return n(s,c);}});
const {planSystemEntries}=await import(new URL('packages/durable/src/harness/prompt.ts',root));
console.log(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(c=>planSystemEntries(c.view,new Map(Object.entries(c.sections)),c.tools,7))));
'''
def main():
    def tool(n,d=None):return dict(name=n,description=d or n,parameters={'type':'object','properties':{}})
    rng=random.Random(201)
    cases=[]
    for i in range(90):
        shown={k:str(rng.randrange(3)) for k in rng.sample(['a','b','c','d'],rng.randrange(5))}
        desired={k:str(rng.randrange(3)) for k in rng.sample(['a','b','c','d'],rng.randrange(5))}
        offered=[tool(k) for k in rng.sample(['x','y','z'],rng.randrange(4))]
        wanted=[tool(k, str(rng.randrange(2))) for k in rng.sample(['x','y','z'],rng.randrange(4))]
        msg=dict(role='system',content='',sections=shown,toolsAdded=offered,timestamp=1)
        view=dict(entries=[dict(id=1,kind='pangu.system',model=[msg])],messages=[msg])
        if i%3==0:view['head']=dict(id=2)
        if i%7==0:
            msg2=dict(role='system',content='',sections={'a':None,'c':'retained'},toolsRemoved=[dict(name='x')],timestamp=2)
            view['entries'].append(dict(id=3,kind='pangu.system',model=[msg2]));view['messages'].append(msg2)
        cases.append(dict(view=view,sections=desired,tools=wanted))
    upstream=json.loads(subprocess.check_output(['node','--experimental-strip-types','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(cases).replace('pangu.system','pi.system').encode()))
    with tempfile.TemporaryDirectory(prefix='pangu-prompt-') as directory:
        so=Path(directory)/'prompt.so'
        subprocess.run(['/usr/bin/clang-23','-fPIC','-shared',str(ROOT/'src/runtime/system_prompt.S'),str(ROOT/'src/runtime/request_context.S'),'-ljson-c','-o',str(so)],check=True)
        lib=C.CDLL(str(so));lib.json_tokener_parse.argtypes=[C.c_char_p];lib.json_tokener_parse.restype=C.c_void_p;lib.json_object_to_json_string.argtypes=[C.c_void_p];lib.json_object_to_json_string.restype=C.c_char_p;lib.json_object_put.argtypes=[C.c_void_p]
        lib.pg_prompt_plan.argtypes=[C.c_void_p,C.c_void_p,C.c_void_p,C.c_uint64,C.POINTER(C.c_void_p)]
        for index,(case,expected) in enumerate(zip(cases,upstream)):
            owned=[lib.json_tokener_parse(json.dumps(case[k]).encode()) for k in ('view','sections','tools')];out=C.c_void_p()
            assert lib.pg_prompt_plan(*owned,7,C.byref(out))==0,index
            actual=json.loads(lib.json_object_to_json_string(out))
            for draft in actual:draft.pop('kind')
            assert actual==expected,(index,actual,expected)
            lib.json_object_put(out)
            for value in owned:lib.json_object_put(value)
    print(f'PASS: {len(cases)} ordered sections/tools/head plans match actual fixed Pi prompt.ts')
if __name__=='__main__':main()
