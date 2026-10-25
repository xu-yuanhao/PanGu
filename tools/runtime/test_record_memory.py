"""Table-write/read/mint differential against actual fixed Pi MemoryStorage."""
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
import {registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';import {readFileSync} from 'node:fs';
const base=pathToFileURL(process.argv[1]+'/');
registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord/delta')return {url:new URL('packages/chord/src/delta/index.ts',base).href,shortCircuit:true};return n(s,c);}});
const {MemoryStorage}=await import(new URL('packages/durable/src/storage/memory.ts',base));
const storage=new MemoryStorage(),out=[];
for(const op of JSON.parse(readFileSync(0,'utf8'))){
 if(op.op==='commit'){const adopted=[];try{const seq=await storage.commit(op.writes,s=>adopted.push(s),{});out.push({ok:true,seq,adopted});}catch{out.push({ok:false,adopted});}}
 if(op.op==='mint'){try{out.push({ok:true,id:await storage.mintId()});}catch{out.push({ok:false});}}
 if(op.op==='read'){let value=await storage[op.table](op.id,{});if(op.table==='entry')value=value?.entry;out.push({ok:true,value:value??null});}
}
await storage.close({});console.log(JSON.stringify(out));
'''
class Memory(ctypes.Structure):
    _fields_=[('rows',ctypes.c_void_p),('count',ctypes.c_size_t),('capacity',ctypes.c_size_t),
              ('next_id',ctypes.c_uint64),('next_sequence',ctypes.c_uint64),('busy',ctypes.c_uint),('closed',ctypes.c_uint)]
class Storage(ctypes.Structure):
    _fields_=[('context',ctypes.c_void_p),('commit',ctypes.c_void_p)]
def record(table,id,index):
    value=dict(id=float(id) if index%4==0 else id,label=index)
    if table!='conversation':value['conversationId']=1
    if table=='entry':value.update(kind='test',data={'index':index})
    if table=='task':value.update(kind='task',state={'status':['pending','running','waiting','sleeping','completing','terminal'][index%6]})
    if table=='submission':value.update(status=['queued','placed','done','unanswered'][index%4],requestId='request-'+str(id))
    return dict(type=table,value=value)
def main():
    for name in ('packages/durable/src/storage/memory.ts','packages/durable/src/storage/scan.ts','packages/durable/src/errors.ts','packages/durable/src/ids.ts','packages/chord/src/delta/index.ts'):
        frozen=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':'+name])
        assert (ROOT/'references/pi'/name).read_bytes()==frozen,name+' drift'
    tables=('conversation','entry','task','submission');rng=random.Random(1049)
    operations=[dict(op='mint'),dict(op='commit',writes=[record('conversation',1,0),record('entry',3,0)])]
    for i in range(120):
        writes=[record(rng.choice(tables),rng.randint(1,40),i*3+j) for j in range(rng.randint(0,3))]
        operations.append(dict(op='commit',writes=writes))
        operations.append(dict(op='read',table=rng.choice(tables),id=rng.randint(1,40)))
        if i%5==0:operations.append(dict(op='mint'))
    operations.extend(dict(op='read',table=table,id=id) for table in tables for id in range(1,42))
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(operations),text=True,capture_output=True,check=True,timeout=30).stdout)
    with tempfile.TemporaryDirectory(prefix='pangu-record-memory-') as temp:
        path=Path(temp)/'memory.so';flags=subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True).split()
        subprocess.run(['/usr/bin/clang-23','-shared','-fPIC','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'src/runtime/documents.S'),str(ROOT/'src/runtime/record_memory.S'),*flags,'-o',str(path)],check=True)
        native=ctypes.CDLL(str(path));library=ctypes.CDLL(ctypes.util.find_library('json-c'))
        library.json_tokener_parse.argtypes=[ctypes.c_char_p];library.json_tokener_parse.restype=ctypes.c_void_p
        library.json_object_put.argtypes=[ctypes.c_void_p]
        library.json_object_to_json_string_ext.argtypes=[ctypes.c_void_p,ctypes.c_int];library.json_object_to_json_string_ext.restype=ctypes.c_char_p
        native.pg_record_memory_open.argtypes=[ctypes.POINTER(Memory)];native.pg_record_memory_close.argtypes=[ctypes.POINTER(Memory)]
        native.pg_record_memory_mint.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(ctypes.c_uint64)]
        native.pg_record_memory_read.argtypes=[ctypes.POINTER(Memory),ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
        native.pg_record_memory_storage.argtypes=[ctypes.POINTER(Memory)];native.pg_record_memory_storage.restype=Storage
        adopt_type=ctypes.CFUNCTYPE(None,ctypes.c_void_p,ctypes.c_uint64)
        commit_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,adopt_type,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64))
        memory=Memory();assert native.pg_record_memory_open(ctypes.byref(memory))==0
        service=native.pg_record_memory_storage(ctypes.byref(memory));commit=commit_type(service.commit)
        try:
            for index,(op,answer) in enumerate(zip(operations,expected,strict=True)):
                if op['op']=='mint':
                    id=ctypes.c_uint64();status=native.pg_record_memory_mint(ctypes.byref(memory),ctypes.byref(id));actual=dict(ok=status==0)
                    if not status:actual['id']=id.value
                elif op['op']=='commit':
                    adopted=[]
                    @adopt_type
                    def adopt(context,seq):adopted.append(seq)
                    batch=library.json_tokener_parse(json.dumps(op['writes']).encode());seq=ctypes.c_uint64()
                    try:status=commit(service.context,batch,adopt,None,ctypes.byref(seq))
                    finally:library.json_object_put(batch)
                    actual=dict(ok=status==0,adopted=adopted)
                    if not status:actual['seq']=seq.value
                else:
                    value=ctypes.c_void_p();status=native.pg_record_memory_read(ctypes.byref(memory),tables.index(op['table']),op['id'],ctypes.byref(value));assert status in (0,1),(index,status)
                    try:actual=dict(ok=True,value=json.loads(library.json_object_to_json_string_ext(value,0)) if value.value else None)
                    finally:library.json_object_put(value)
                assert actual==answer,(index,op,actual,answer)
        finally:assert native.pg_record_memory_close(ctypes.byref(memory))==0
    print(f'PASS: {len(operations)} table batch/read/ID operations match actual fixed Pi MemoryStorage')
if __name__=='__main__':main()
