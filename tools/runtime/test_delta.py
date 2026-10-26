"""Compare decoded native delta replay with fixed actual Chord applyImmutable."""
import ctypes
import ctypes.util
import json
from pathlib import Path
import random
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
COMMIT='d5b3de22d0e24a3e9c38a9410d3329b099717283'
ORACLE=r'''
import {readFileSync} from 'node:fs';import {pathToFileURL} from 'node:url';
const {applyImmutable,applyImmutableBatches}=await import(pathToFileURL(process.argv[1]));
console.log(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(c=>{
 const original=JSON.stringify([c.value,c.ops,c.batches]);
 try{const value=c.batches===undefined?applyImmutable(c.value,c.ops):applyImmutableBatches(c.value,c.batches);if(original!==JSON.stringify([c.value,c.ops,c.batches]))throw Error('input changed');return {ok:true,value};}
 catch{return {ok:false};}
})));
'''
def corpus():
    cases=[]
    base=dict(a={'x':1},xs=[0,1,2,3],text='hello',**{'1':'one'})
    for path in (['a','x'],['a','new'],['xs',0],['xs',4],['xs',9],['missing','x'],[],['constructor'],['a','__proto__'],[1],['text','x']):
        for op in (['s',path,{'n':[1,None]}],['d',path],['a',path,'!'],['t',path,2]):cases.append(dict(value=base,ops=[op]))
    for path in ([],['xs'],['a'],['missing'],['text'],['xs',0]):
        for index in (0,1,4,100,1e100):
            for remove in (0,1,100):cases.append(dict(value=base if path else [0,1,2,3],ops=[['p',path,index,remove,[8,9]]]))
        for permutation in ([],[0,1,2,3],[3,2,1,0],[0,0,1,2],[4,1,2,3],[0.5,1,2,3],['0',1,2,3]):cases.append(dict(value=base if path else [0,1,2,3],ops=[['m',path,permutation]]))
    for text in ('','abc','中😀tail','a\0b','é中'):
        units=len(text.encode('utf-16-le'))//2
        for count in range(units+3):
            prefix=text.encode('utf-16-le')[:count*2]
            try:prefix.decode('utf-16-le')
            except UnicodeDecodeError:continue
            cases.append(dict(value={'text':text},ops=[['a',['text'],'结束\0'],['t',['text'],count]]))
    for key in (True,False,None,'0',0.5):cases.append(dict(value=['x','y'],ops=[['a',[0],'a'],['a',[key],'b']]))
    cases.append(dict(value={'foo':'x'},ops=[['a',['foo'],'a'],['a',['foo\0bad'],'b']]))
    cases.append(dict(value={'text':''},ops=[['a',['text'],'x'*64]]*10000))
    for value in (None,False,7,'text',[],{},[1,2],{'x':1}):cases.append(dict(value={'old':1},ops=[['r',value]]))
    for op in ([],['x'],['s'],['s',['x']],['d',[]],['p',[],0,-1,[]],['p',[],0,0,{}],['t',['text'],-1],['a',['text'],7],['r'],['r',1,2],['m',[],[True]],['s','path',1],['s',[True],1]):cases.append(dict(value=base,ops=[op]))
    rng=random.Random(1049)
    for _ in range(100):
        count=rng.randrange(8);ops=[]
        for i in range(count):
            choice=rng.randrange(4)
            if choice==0:ops.append(['s',['a','n'+str(i)],{'v':i}])
            elif choice==1:ops.append(['a',['text'],str(i)])
            elif choice==2:ops.append(['p',['xs'],rng.randrange(7),rng.randrange(3),[i]])
            else:ops.append(['d',['a','n'+str(rng.randrange(8))]])
        cases.append(dict(value=base,ops=ops))
    for case in list(cases)[::3]:
        ops=case['ops'];middle=len(ops)//2
        cases.append(dict(value=case['value'],batches=[ops[:middle],[],ops[middle:]]))
    cases.extend([dict(value=None,ops=[]),dict(value=[1],ops=[['r',None],['r',{'x':1}]]),dict(value={},batches=[[],{}])])
    cases.extend([
        dict(value={'text':'😀x'},ops=[['t',['text'],1]],native_boundary='UTF-16 scalar boundary'),
        dict(value={},ops=[['s',['a\0b'],1]],native_boundary='C-string property key'),
        dict(value={},ops=[['s',[18014398509481984],1]],native_boundary='safe numeric path segment')])
    return cases

REVISION_ORACLE=r'''
import {registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';
const base=pathToFileURL(process.argv[1]+'/');
registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord/delta')return {url:new URL('packages/chord/src/delta/index.ts',base).href,shortCircuit:true};return n(s,c);}});
const {MemoryStorage}=await import(new URL('packages/durable/src/storage/memory.ts',base));const output=[];
for(const scope of ['session','task','conversation'])for(const history of (scope==='conversation'?['latest','rewindable']:[undefined]))for(const retire of [false,true]){
 const storage=new MemoryStorage();const record={id:2,kind:'test',scope:{kind:scope,...(scope==='conversation'?{conversationId:1}:scope==='task'?{taskId:3}:{})},...(history?{history,fork:'current'}:{})};
 await storage.commit([{type:'document.create',record,content:{kind:'base',version:1,value:{text:'a',xs:[1]}}}],()=>{},{});
 for(let i=0;i<7;i++)await storage.commit([{type:'document.change',id:2,content:i===3?{kind:'base',version:2,value:{text:'new',xs:[]}}:{kind:'delta',version:i<3?1:2,ops:[['a',['text'],'中😀'],['p',['xs'],0,0,[i]]]}}],()=>{},{});
 if(retire)await storage.commit([{type:'document.retire',id:2}],()=>{},{});
 const stored=storage.state.documents.get(2),answers=[];
 for(const at of [0,1,2,3,4,5,6,7,8,9,10,'current'])try{const value=await storage.document(2,at,{});answers.push({at,ok:true,value:value??null});}catch{answers.push({at,ok:false});}
 output.push({stored,answers});await storage.close({});
}console.log(JSON.stringify(output));
'''
class Revision(ctypes.Structure):
    _fields_=[('sequence',ctypes.c_uint64),('version',ctypes.c_uint64),('kind',ctypes.c_uint),('reserved',ctypes.c_uint),('content',ctypes.c_void_p)]
class RevisionView(ctypes.Structure):
    _fields_=[('created_at',ctypes.c_uint64),('retired_at',ctypes.c_uint64),('current_only',ctypes.c_uint),('reserved',ctypes.c_uint),('revisions',ctypes.POINTER(Revision)),('count',ctypes.c_size_t)]
class Snapshot(ctypes.Structure):
    _fields_=[('value',ctypes.c_void_p),('version',ctypes.c_uint64),('deltas',ctypes.c_size_t)]
def revision_cases(native,library):
    for relative in ('packages/durable/src/storage/memory.ts','packages/durable/src/storage/scan.ts','packages/durable/src/errors.ts','packages/durable/src/ids.ts'):
        assert (ROOT/'references/pi'/relative).read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':'+relative]),relative+' drift'
    result=subprocess.run(['node','--input-type=module','-e',REVISION_ORACLE,str(ROOT/'references/pi')],text=True,capture_output=True,check=True,timeout=30)
    native.pg_document_revision_read.argtypes=[ctypes.POINTER(RevisionView),ctypes.c_uint64,ctypes.POINTER(Snapshot)]
    native.pg_document_revision_read.restype=ctypes.c_int
    total=0
    for fixture in json.loads(result.stdout):
        stored=fixture['stored'];record=stored['record'];revisions=stored['revisions'];array=(Revision*len(revisions))();objects=[]
        try:
            for i,revision in enumerate(revisions):
                value=library.json_tokener_parse(json.dumps(revision.get('value',revision.get('ops')),ensure_ascii=False).encode());objects.append(value)
                array[i]=Revision(revision['seq'],revision['version'],int(revision['kind']=='delta'),0,value)
            view=RevisionView(record['createdAt'],record.get('retiredAt',0),int(record['scope']['kind']!='conversation' or record.get('history')=='latest'),0,array,len(array))
            for answer in fixture['answers']:
                out=Snapshot();status=native.pg_document_revision_read(ctypes.byref(view),2**64-1 if answer['at']=='current' else answer['at'],ctypes.byref(out));actual=dict(ok=status==0)
                try:
                    if not status:actual['value']=None if not out.version else dict(record=record,version=out.version,value=json.loads(library.json_object_to_json_string_ext(out.value,0)),deltasSinceBase=out.deltas)
                    else:assert not out.value and not out.version and not out.deltas
                    assert actual=={k:v for k,v in answer.items() if k!='at'},(fixture,answer,actual)
                    total+=1
                finally:library.json_object_put(out.value)
        finally:
            for value in objects:library.json_object_put(value)
    print(f'PASS: {total} revision/lifetime/current-only reads match actual Pi MemoryStorage')

class Change(ctypes.Structure):
    _fields_=[('base',ctypes.c_void_p),('ops',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('active',ctypes.c_uint),('reserved',ctypes.c_uint)]
class Prepared(ctypes.Structure):
    _fields_=[('value',ctypes.c_void_p),('ops',ctypes.c_void_p)]
def plan_cases(native,library):
    path=ROOT/'references/pi/packages/durable/src/session/transaction.ts'
    assert path.read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':packages/durable/src/session/transaction.ts'])
    cases=[dict(id=id,stored=stored,version=version,checkpoint=checkpoint,value={'text':'same','items':[1]},ops=ops) for id in [1,123,9007199254740991] for stored in [1,2] for version in [stored,stored+1] for checkpoint in [False,True] for ops in [[],[['s',['text'],'same']],[['p',['items'],0,0,[]]]]]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';
const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('function planDocument('),end=source.indexOf('/** The publication',start);const planDocument=eval('('+stripTypeScriptTypes(source.slice(start,end))+')');const cs=source.indexOf('if (plan.content?.type === "document.change"',source.indexOf('// Checkpoint predicates run last')),ce=source.indexOf('if (plan.content !== undefined) writes.push',cs);const finish=eval('('+stripTypeScriptTypes('function finish(plan){const change=plan.change;'+source.slice(cs,ce)+'return plan.content??null;}')+')');const out=[];
for(const c of JSON.parse(readFileSync(0,'utf8'))){const loaded={record:{id:c.id},storedVersion:c.stored,tracker:null,deltasSinceBase:3};const document={addressId:'test',retireOnCommit:false,target:{kind:'loaded',document:loaded},definition:{version:c.version,checkpointWhen:()=>c.checkpoint},prepared:{value:c.value,ops:c.ops}};out.push(finish(planDocument(document)));}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(path)],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    native.pg_document_plan_change.argtypes=[ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(Prepared),ctypes.c_uint,ctypes.POINTER(ctypes.c_void_p)]
    for case,want in zip(cases,expected,strict=True):
        value=library.json_tokener_parse(json.dumps(case['value']).encode());ops=library.json_tokener_parse(json.dumps(case['ops']).encode());prepared=Prepared(value,ops);out=ctypes.c_void_p()
        try:
            assert not native.pg_document_plan_change(case['id'],case['stored'],case['version'],ctypes.byref(prepared),case['checkpoint'],ctypes.byref(out))
            actual=json.loads(library.json_object_to_json_string_ext(out,0)) if out.value else None
            assert actual==want,(case,actual,want)
            assert json.loads(library.json_object_to_json_string_ext(value,0))==case['value'] and json.loads(library.json_object_to_json_string_ext(ops,0))==case['ops']
        finally:library.json_object_put(out);library.json_object_put(ops);library.json_object_put(value)
    print(f'PASS: {len(cases)} document change plans match actual Pi planDocument/checkpoint branch')

def adoption_cases(native,library):
    path=ROOT/'references/pi/packages/durable/src/session/transaction.ts'
    assert path.read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':packages/durable/src/session/transaction.ts'])
    cases=[dict(stored=stored,version=version,changed=changed,checkpoint=checkpoint) for stored,version in [(1,1),(1,2),(2,2)] for changed in [False,True] for checkpoint in [False,True]]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';import {pathToFileURL} from 'node:url';
const {track}=await import(pathToFileURL(process.argv[2]));const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('adopt(seq: Seq): AdoptedDocuments'),end=source.indexOf('async #assemble()',start);const describe=()=>undefined;const text='class Probe{#plans;#host={install(){throw Error("install")},evict(){throw Error("evict")}};constructor(plan){this.#plans=[plan]}'+source.slice(start,end)+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];
for(const c of JSON.parse(readFileSync(0,'utf8'))){const tracker=track({n:1}),old=tracker.value,change=tracker.beginChange();if(c.changed)change.state.n=2;const prepared=change.prepare(),loaded={record:{id:2,createdAt:1},storedVersion:c.stored,valueVersion:c.version,deltasSinceBase:3,tracker};const kind=c.stored<c.version||c.checkpoint?'base':'delta';const plan={record:loaded.record,retire:false,change:{tracker,prepared,loaded,version:c.version},...(c.changed||c.stored<c.version?{content:{type:'document.change',id:2,content:{kind}}}:{})};new Probe(plan).adopt(2);out.push({stored:loaded.storedVersion,deltas:loaded.deltasSinceBase,value:tracker.value,same:old===tracker.value});}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(path),str(ROOT/'references/pi/packages/chord/src/delta/tracker.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Loaded(ctypes.Structure):
        _fields_=[('owner',ctypes.c_void_p),('value',ctypes.c_void_p),('definition',ctypes.c_void_p),('record',ctypes.c_byte*64),('stored',ctypes.c_uint64),('version',ctypes.c_uint64),('deltas',ctypes.c_uint64),('busy',ctypes.c_uint),('active',ctypes.c_uint)]
    class Adoption(ctypes.Structure):
        _fields_=[('loaded',ctypes.c_void_p),('expected',ctypes.c_void_p),('candidate',ctypes.c_void_p),('stored',ctypes.c_uint64),('version',ctypes.c_uint64),('deltas',ctypes.c_uint64),('id',ctypes.c_uint64),('next_stored',ctypes.c_uint64),('next_deltas',ctypes.c_uint64),('busy',ctypes.c_uint),('state',ctypes.c_uint)]
    native.pg_document_adoption_prepare.argtypes=[ctypes.POINTER(Adoption),ctypes.POINTER(Loaded),ctypes.POINTER(Prepared),ctypes.c_uint]
    native.pg_document_adoption_apply.argtypes=[ctypes.POINTER(Adoption)]
    native.pg_document_adoption_discard.argtypes=[ctypes.POINTER(Adoption)]
    for case,want in zip(cases,expected,strict=True):
        base=library.json_tokener_parse(b'{"n":1}');value=library.json_tokener_parse(b'{"n":2}' if case['changed'] else b'{"n":1}');ops=library.json_tokener_parse(b'[["s",["n"],2]]' if case['changed'] else b'[]');loaded=Loaded();loaded.value=base;loaded.stored=case['stored'];loaded.version=case['version'];loaded.deltas=3;loaded.active=1;ctypes.c_uint64.from_buffer(loaded,72).value=2;prepared=Prepared(value,ops);adoption=Adoption()
        kind=1 if case['stored']<case['version'] or (case['changed'] and case['checkpoint']) else 2 if case['changed'] else 0
        try:
            assert not native.pg_document_adoption_prepare(ctypes.byref(adoption),ctypes.byref(loaded),ctypes.byref(prepared),kind)
            assert loaded.value==base and loaded.stored==case['stored'] and loaded.deltas==3
            assert not native.pg_document_adoption_apply(ctypes.byref(adoption))
            actual=dict(stored=loaded.stored,deltas=loaded.deltas,value=json.loads(library.json_object_to_json_string_ext(loaded.value,0)),same=loaded.value==base)
            assert actual==want,(case,actual,want)
        finally:
            assert not native.pg_document_adoption_discard(ctypes.byref(adoption))
            for pointer in [loaded.value,value,ops]:library.json_object_put(pointer)
    print(f'PASS: {len(cases)} loaded adoption value/version/counter outcomes match actual Pi adopt body and Chord tracker')

def main():
    reference=ROOT/'references/pi/packages/chord/src/delta/index.ts'
    for name in ('index.ts','diff.ts','tracker.ts','pending.ts','draft.ts'):
        relative='packages/chord/src/delta/'+name
        assert (ROOT/'references/pi'/relative).read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':'+relative]),relative+' drift'
    cases=corpus()
    oracle=subprocess.run(['node','--input-type=module','-e',ORACLE,str(reference)],input=json.dumps(cases,ensure_ascii=False),text=True,capture_output=True,check=True,timeout=30)
    expected=json.loads(oracle.stdout)
    with tempfile.TemporaryDirectory(prefix='pangu-delta-') as temp:
        path=Path(temp)/'delta.so';flags=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libutf8proc'],text=True).split()
        subprocess.run(['/usr/bin/clang-23','-shared','-fPIC','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'src/runtime/documents.S'),str(ROOT/'src/runtime/delta.S'),*flags,'-o',str(path)],check=True)
        native=ctypes.CDLL(str(path));library=ctypes.CDLL(ctypes.util.find_library('json-c'))
        library.json_tokener_parse.argtypes=[ctypes.c_char_p];library.json_tokener_parse.restype=ctypes.c_void_p
        library.json_object_put.argtypes=[ctypes.c_void_p]
        library.json_object_to_json_string_ext.argtypes=[ctypes.c_void_p,ctypes.c_int];library.json_object_to_json_string_ext.restype=ctypes.c_char_p
        native.pg_document_delta_apply.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)];native.pg_document_delta_apply.restype=ctypes.c_int
        native.pg_document_delta_batches.argtypes=native.pg_document_delta_apply.argtypes;native.pg_document_delta_batches.restype=ctypes.c_int
        revision_cases(native,library)
        plan_cases(native,library)
        adoption_cases(native,library)
        native.pg_document_change_open.argtypes=[ctypes.POINTER(Change),ctypes.c_void_p]
        native.pg_document_change_append.argtypes=[ctypes.POINTER(Change),ctypes.c_void_p]
        native.pg_document_change_prepare.argtypes=[ctypes.POINTER(Change),ctypes.POINTER(Prepared)]
        native.pg_document_change_discard.argtypes=[ctypes.POINTER(Change)]
        change_count=0
        for index,(case,answer) in enumerate(zip(cases,expected,strict=True)):
            value=library.json_tokener_parse(json.dumps(case['value'],ensure_ascii=False).encode());ops=library.json_tokener_parse(json.dumps(case.get('batches',case.get('ops')),ensure_ascii=False).encode());out=ctypes.c_void_p()
            try:
                function=native.pg_document_delta_batches if 'batches' in case else native.pg_document_delta_apply
                status=function(value,ops,ctypes.byref(out));actual=dict(ok=status==0)
                if not status:actual['value']=json.loads(library.json_object_to_json_string_ext(out,0))
                else:assert not out.value
                if 'native_boundary' in case:
                    assert answer['ok'] and status==1 and not out.value,(index,case,actual,answer)
                else:assert actual==answer,(index,case,actual,answer)
                if isinstance(case['value'],(dict,list)):
                    change=Change();prepared=Prepared();assert not native.pg_document_change_open(ctypes.byref(change),value)
                    batches=case.get('batches',[case.get('ops')]);batches=batches if isinstance(batches,list) else [batches]
                    admitted=True
                    try:
                        for batch in batches:
                            pointer=library.json_tokener_parse(json.dumps(batch,ensure_ascii=False).encode())
                            try:
                                if native.pg_document_change_append(ctypes.byref(change),pointer):admitted=False;break
                            finally:library.json_object_put(pointer)
                        status=native.pg_document_change_prepare(ctypes.byref(change),ctypes.byref(prepared)) if admitted else 1
                        replay=dict(ok=status==0)
                        if not status:replay['value']=json.loads(library.json_object_to_json_string_ext(prepared.value,0))
                        if 'native_boundary' in case:assert status==1
                        else:assert replay==answer,(index,case,replay,answer)
                        if admitted:assert change.sealed and not change.active and not change.base and not change.ops
                        change_count+=1
                    finally:
                        library.json_object_put(prepared.value);library.json_object_put(prepared.ops);assert not native.pg_document_change_discard(ctypes.byref(change))
                assert json.loads(library.json_object_to_json_string_ext(value,0))==case['value']
                assert json.loads(library.json_object_to_json_string_ext(ops,0))==case.get('batches',case.get('ops'))
            finally:
                library.json_object_put(out);library.json_object_put(ops);library.json_object_put(value)
    change_boundaries=sum('native_boundary' in case and isinstance(case['value'],(dict,list)) for case in cases)
    print(f'PASS: {change_count-change_boundaries} change journal cases match actual Chord; {change_boundaries} explicit ABI refusals; sealing/ownership checked')
    differences=sum('native_boundary' in case for case in cases)
    print(f'PASS: {len(cases)-differences} decoded single/multi-batch cases match actual Chord; {differences} explicit native ABI boundaries verified; inputs unchanged')
if __name__=='__main__':main()
