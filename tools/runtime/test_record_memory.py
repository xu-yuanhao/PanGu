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
 if(op.op==='docscan'){try{out.push({ok:true,page:await storage.scanDocuments(op.query,op.limit,op.cursor,{})});}catch{out.push({ok:false});}}
 if(op.op==='find'){try{out.push({ok:true,value:await storage.findDocument(op.address,op.at,{})??null});}catch{out.push({ok:false});}}
 if(op.op==='document'){try{out.push({ok:true,value:await storage.document(op.id,op.at,{})??null});}catch{out.push({ok:false});}}
 if(op.op==='mint'){try{out.push({ok:true,id:await storage.mintId()});}catch{out.push({ok:false});}}
 if(op.op==='entry'){try{out.push({ok:true,value:await (op.conversation===undefined?storage.entry(op.id,{}):storage.entry(op.conversation,op.id,{}))??null});}catch{out.push({ok:false});}}
 if(op.op==='head'){try{out.push({ok:true,value:await storage.findLatestHeadMarker(op.conversation,op.at,{})??null});}catch{out.push({ok:false});}}
 if(op.op==='scan'){try{const names={conversation:'scanConversations',entry:'scanEntries',task:'scanTasks',submission:'scanSubmissions'};out.push({ok:true,page:await storage[names[op.table]](op.query,op.limit,op.cursor,{})});}catch{out.push({ok:false});}}
 if(op.op==='read'){let value=await storage[op.table](op.id,{});if(op.table==='entry')value=value?.entry;out.push({ok:true,value:value??null});}
}
await storage.close({});console.log(JSON.stringify(out));
'''
class Memory(ctypes.Structure):
    _fields_=[('rows',ctypes.c_void_p),('count',ctypes.c_size_t),('capacity',ctypes.c_size_t),
              ('next_id',ctypes.c_uint64),('next_sequence',ctypes.c_uint64),('busy',ctypes.c_uint),('closed',ctypes.c_uint)]
class Storage(ctypes.Structure):
    _fields_=[('context',ctypes.c_void_p),('commit',ctypes.c_void_p)]
class Slice(ctypes.Structure):
    _fields_=[('data',ctypes.c_void_p),('size',ctypes.c_size_t)]
class Address(ctypes.Structure):
    _fields_=[('kind',Slice),('key',Slice),('owner',ctypes.c_int64),('scope',ctypes.c_uint),('has_key',ctypes.c_uint)]
class DocumentQuery(ctypes.Structure):
    _fields_=[('scope',ctypes.c_uint),('has_kind',ctypes.c_uint),('owner',ctypes.c_uint64),('kind',Slice),('at',ctypes.c_uint64),('limit',ctypes.c_size_t),('after',ctypes.c_int64),('has_cursor',ctypes.c_uint),('reserved',ctypes.c_uint)]
class Query(ctypes.Structure):
    _fields_=[('table',ctypes.c_uint),('order',ctypes.c_uint),('has_cursor',ctypes.c_uint),('cursor_order',ctypes.c_uint),
              ('after',ctypes.c_int64),('limit',ctypes.c_size_t),('flags',ctypes.c_uint64),('conversation_id',ctypes.c_uint64),
              ('owner_conversation_id',ctypes.c_uint64),('owner_task_id',ctypes.c_uint64),('min_entry_id',ctypes.c_uint64),('max_entry_id',ctypes.c_uint64),
              ('kind',Slice),('status',ctypes.c_uint),('abort_requested',ctypes.c_uint),('background',ctypes.c_uint)]
class Page(ctypes.Structure):
    _fields_=[('items',ctypes.c_void_p),('after',ctypes.c_uint64),('order',ctypes.c_uint),('has_next',ctypes.c_uint)]
def record(table,id,index):
    value=dict(id=float(id) if index%4==0 else id,label=index)
    if table!='conversation':value['conversationId']=1
    if table=='entry':
        value.update(kind='test',data={'index':index})
        if index%3==0:value['head']=id
    if table=='task':
        value.update(kind='task',state={'status':['pending','running','waiting','sleeping','completing','terminal'][index%6]})
        if index%3:value['abortRequested']=index%2==0
        if index%4:value['background']=index%3==0
    if table=='submission':value.update(status=['queued','placed','done','unanswered'][index%4],requestId='request-'+str(id))
    if table=='conversation' and id!=1 and index%2==0:value['owner']=dict(conversationId=1,taskId=5)
    return dict(type=table,value=value)
def submission_transitions(native,library):
    relative='packages/durable/src/session/transaction.ts';path=ROOT/'references/pi'/relative
    assert path.read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':'+relative])
    cases=[dict(current=dict(id=3,type=type,status=status,entry=5,nested={'x':[1]}),change=dict(status=change,entry=7,answer={'nested':[2]},reason='test')) for type in ['input','write'] for status in ['queued','placed','done','unanswered'] for change in ['placed','done','unanswered']]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';import {runInNewContext} from 'node:vm';
const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('function applySubmissionChange('),end=source.indexOf('const INTERNAL_SCAN_PAGE_SIZE',start);const fn=runInNewContext('('+stripTypeScriptTypes(source.slice(start,end))+')');
console.log(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(c=>{try{return {ok:true,value:fn(c.current,c.change)}}catch{return {ok:false}}})));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(path)],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    native.pg_records_submission_change.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
    for case,want in zip(cases,expected,strict=True):
        current=library.json_tokener_parse(json.dumps(case['current']).encode());change=library.json_tokener_parse(json.dumps(case['change']).encode());out=ctypes.c_void_p()
        try:
            status=native.pg_records_submission_change(current,change,ctypes.byref(out));actual=dict(ok=status==0)
            if not status:actual['value']=json.loads(library.json_object_to_json_string_ext(out,0))
            else:assert not out.value
            assert actual==want,(case,actual,want)
            assert json.loads(library.json_object_to_json_string_ext(current,0))==case['current']
            assert json.loads(library.json_object_to_json_string_ext(change,0))==case['change']
        finally:
            for value in [out,current,change]:library.json_object_put(value)
    print(f'PASS: {len(cases)} submission transitions match actual Pi transaction function body')

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
    fork_writes=[dict(type='conversation',value=dict(id=100,parent=dict(conversationId=1,at=25))),dict(type='conversation',value=dict(id=1000,parent=dict(conversationId=100,at=150))),dict(type='conversation',value=dict(id=2000,parent=dict(conversationId=9999,at=0)))]
    fork_writes += [dict(type='entry',value=dict(id=id,conversationId=conversation,kind='test',**({'head':id} if id in (50,101,190) else {}))) for id,conversation in ((50,1),(250,1),(101,100),(120,100),(190,100),(1001,1000),(2001,2000))]
    for index in range(6):
        id=5000+index*100;parent=1 if index==0 else id-100;at=250 if index==0 else parent+1
        fork_writes.append(dict(type='conversation',value=dict(id=id,parent=dict(conversationId=parent,at=at))))
        fork_writes.append(dict(type='entry',value=dict(id=id+1,conversationId=id,kind='test',**({'head':id+1} if index%2==0 else {}))))
    fork_writes.append(dict(type='task',value=dict(id=4000,conversationId=1,kind='盘古\0task',state=dict(status='waiting'),abortRequested=False,background=True)))
    fork_writes.extend([dict(type='conversation',value=dict(id=6000,parent=dict(conversationId=9999,at=100000))),dict(type='entry',value=dict(id=6001,conversationId=6000,kind='test',head=6001)),dict(type='entry',value=dict(id=6002,conversationId=6000,kind='test'))])
    operations.append(dict(op='commit',writes=fork_writes))
    filters={
      'conversation':[{},dict(ownerConversationId=1),dict(ownerTaskId=5),dict(ownerConversationId=1,ownerTaskId=5),dict(ownerTaskId=999)],
      'entry':[dict(conversationId=1),dict(conversationId=100),dict(conversationId=1000),dict(conversationId=1000,minEntryId=10,maxEntryId=120),dict(conversationId=2000,minEntryId=1),dict(conversationId=5500),dict(conversationId=5500,minEntryId=5301,maxEntryId=5501),dict(conversationId=6000),dict(conversationId=9999)],
      'task':[{},*[dict(status=s) for s in ('pending','running','waiting','sleeping','completing','terminal')],dict(conversationId=1),dict(kind='task'),dict(kind='盘古\0task'),dict(abortRequested=False),dict(abortRequested=True,background=False),dict(background=True)],
      'submission':[{},*[dict(status=s) for s in ('queued','placed','done','unanswered')],dict(conversationId=1,status='placed'),dict(conversationId=999)]}
    for table in tables:
        for query in filters[table]:
            for order in (None,'ascending','descending'):
                for limit in (1,2,5,999):
                    q=dict(query)
                    if order is not None:q['order']=order
                    operations.append(dict(op='scan',table=table,query=q,limit=limit))
                for stored in (None,'ascending','descending'):
                    q=dict(query)
                    if order is not None:q['order']=order
                    cursor=dict(after=17)
                    if stored is not None:cursor['order']=stored
                    operations.append(dict(op='scan',table=table,query=q,limit=2,cursor=cursor))
    for after in (-9007199254740991,-1,0,10,150,10000,9007199254740991):
        for order in ('ascending','descending'):
            operations.append(dict(op='scan',table='entry',query=dict(conversationId=1000,order=order),limit=2,cursor=dict(after=after,order=order)))
    for table in tables:
        query=dict(conversationId=1) if table=='entry' else {}
        operations.append(dict(op='scan',table=table,query=dict(query,order='sideways'),limit=2))
        operations.append(dict(op='scan',table=table,query=query,limit=2,cursor=dict(after=1,order='sideways')))
        operations.append(dict(op='scan',table=table,query=query,limit=2,cursor=dict(after=9007199254740992)))
    for conversation in (None,1,100,1000,2000,5500,6000,9999):
        for id in (3,4,25,50,101,120,190,250,1001,2001,6001,6002,999999):
            op=dict(op='entry',id=id)
            if conversation is not None:op['conversation']=conversation
            operations.append(op)
        if conversation is not None:
            for at in (None,0,3,25,100,150,999999):
                op=dict(op='head',conversation=conversation)
                if at is not None:op['at']=at
                operations.append(op)
    start=100000
    for scope,history in [('session',None),('task',None),('conversation','latest'),('conversation','rewindable')]:
        id=start;start+=100
        owner={'kind':scope,**({'conversationId':1} if scope=='conversation' else {'taskId':4000} if scope=='task' else {})}
        definition=dict(id=id,kind='doc.中',scope=owner,**({'history':history,'fork':'current'} if history else {}))
        create=dict(type='document.create',record=definition,content=dict(kind='base',version=1,value=dict(text='a',xs=[])))
        operations.append(dict(op='commit',writes=[create]))
        for index in range(8):
            content=dict(kind='base',version=2,value=dict(text='base',xs=[])) if index==4 else dict(kind='delta',version=1 if index<4 else 2,ops=[['a',['text'],'中😀'],['p',['xs'],0,0,[index]]])
            operations.append(dict(op='commit',writes=[dict(type='document.change',id=id,content=content)]))
            for at in ('current',0,100,150,200):operations.append(dict(op='document',id=id,at=at))
        operations.extend([
            dict(op='commit',writes=[create]),
            dict(op='commit',writes=[dict(type='document.change',id=id,content=dict(kind='delta',version=3,ops=[])),record('task',id+9,0)]),
            dict(op='read',table='task',id=id+9),
            dict(op='commit',writes=[dict(type='document.create',record=dict(definition,id=id+1),content=create['content'])]),
            dict(op='commit',writes=[dict(type='document.retire',id=id),dict(type='document.create',record=dict(definition,id=id+1),content=create['content'])]),
            dict(op='document',id=id,at='current'),dict(op='document',id=id,at=150),dict(op='document',id=id+1,at='current'),
            dict(op='commit',writes=[dict(type='document.retire',id=id)]),
            dict(op='commit',writes=[dict(type='document.retire',id=id+1),dict(type='document.retire',id=id+1)]),
            dict(op='document',id=id+1,at='current')])
    for id in range(100000,100400,100):
        for at in range(121):operations.append(dict(op='document',id=id,at=at))
    for scope,history,index in [('session',None,0),('task',None,1),('conversation','latest',2),('conversation','rewindable',3)]:
        owner={'kind':scope,**({'conversationId':1} if scope=='conversation' else {'taskId':4000} if scope=='task' else {})}
        for at in [*range(121),'current']:
            operations.append(dict(op='find',address=dict(kind='doc.中',scope=owner),at=at))
            operations.append(dict(op='find',address=dict(kind='doc.中',scope=owner,key=''),at=at))
    for offset,(kind,key) in enumerate([('nul\0kind',None),('nul\0kind',''),('family','x\0y'),('family','x')]):
        definition=dict(id=200000+offset,kind=kind,scope=dict(kind='session'))
        if key is not None:definition['key']=key
        operations.append(dict(op='commit',writes=[dict(type='document.create',record=definition,content=dict(kind='base',version=1,value={}))]))
        operations.append(dict(op='find',address={k:v for k,v in definition.items() if k!='id'},at='current'))
    for scope in [dict(kind='session'),dict(kind='conversation',conversationId=1),dict(kind='task',taskId=4000)]:
        for at in [0,1,10,30,60,120,'current']:
            for kind in [None,'doc.中','nul\0kind','family','missing']:
                for limit in [1,2,100]:
                    query=dict(scope=scope,at=at)
                    if kind is not None:query['kind']=kind
                    for after in [None,-1,100000,100001,200000,999999]:
                        operation=dict(op='docscan',query=query,limit=limit)
                        if after is not None:operation['cursor']=dict(after=after)
                        operations.append(operation)
    source=dict(id=300000,kind='copy.中',scope=dict(kind='conversation',conversationId=1),history='rewindable',fork='asOf')
    base=dict(kind='base',version=1,value=dict(text='base',rows=[1]))
    operations.append(dict(op='commit',writes=[dict(type='document.create',record=source,content=base)]))
    operations.append(dict(op='commit',writes=[dict(type='document.change',id=source['id'],content=dict(kind='delta',version=1,ops=[['a',['text'],'tail'],['p',['rows'],0,0,[2]]]))]))
    for index,at in enumerate([*range(151),'current',100000]):
        target=dict(source,id=301000+index,scope=dict(kind='conversation',conversationId=500000+index))
        operations.append(dict(op='commit',writes=[dict(type='document.copy',record=target,source=dict(id=source['id'],at=at))]))
        operations.append(dict(op='document',id=target['id'],at='current'))
    target=dict(source,id=302000,scope=dict(kind='conversation',conversationId=600000))
    for field,value in [('kind','other'),('key',''),('history','latest'),('fork','current'),('scope',dict(kind='session'))]:
        operations.append(dict(op='commit',writes=[dict(type='document.copy',record=dict(target,**{field:value}),source=dict(id=source['id'],at='current'))]))
    for order in [False,True]:
        writes=[dict(type='document.copy',record=target,source=dict(id=source['id'],at='current')),dict(type='document.change',id=source['id'],content=base)]
        operations.append(dict(op='commit',writes=writes[::-1] if order else writes))
    operations.append(dict(op='commit',writes=[dict(type='document.copy',record=target,source=dict(id=source['id'],at='current')),dict(type='document.retire',id=target['id'])]))
    operations.append(dict(op='document',id=target['id'],at='current'))
    operations.append(dict(op='commit',writes=[dict(type='document.retire',id=source['id'])]))
    operations.append(dict(op='commit',writes=[dict(type='document.copy',record=dict(target,id=302001),source=dict(id=source['id'],at='current'))]))
    operations.append(dict(op='document',id=301151,at='current'))
    for index,history in enumerate(['latest','rewindable']):
        doc_record=dict(id=400000+index*10,kind='family.copy',key='key\0中',scope=dict(kind='conversation',conversationId=1),history=history,fork='current')
        target=dict(doc_record,id=doc_record['id']+1,scope=dict(kind='conversation',conversationId=2))
        operations.append(dict(op='commit',writes=[dict(type='document.create',record=doc_record,content=dict(kind='base',version=4,value=dict(value='family')))]))
        operations.append(dict(op='commit',writes=[dict(type='document.copy',record=target,source=dict(id=doc_record['id'],at='current'))]))
        operations.append(dict(op='document',id=target['id'],at='current'))
        operations.append(dict(op='commit',writes=[dict(type='document.copy',record=dict(target,id=doc_record['id']+2,scope=dict(kind='conversation',conversationId=3)),source=dict(id=doc_record['id'],at=100000))]))
        operations.append(dict(op='document',id=doc_record['id']+2,at='current'))
        operations.append(dict(op='commit',writes=[dict(type='document.copy',record=dict(target,id=doc_record['id']+3),source=dict(id=doc_record['id'],at='current')),dict(type='document.copy',record=dict(target,id=doc_record['id']+4,scope=dict(kind='conversation',conversationId=4)),source=dict(id=doc_record['id']+3,at='current'))]))
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(operations),text=True,capture_output=True,check=True,timeout=30).stdout)
    with tempfile.TemporaryDirectory(prefix='pangu-record-memory-') as temp:
        path=Path(temp)/'memory.so';flags=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libutf8proc'],text=True).split()
        subprocess.run(['/usr/bin/clang-23','-shared','-fPIC','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'src/runtime/documents.S'),str(ROOT/'src/runtime/record_memory.S'),str(ROOT/'src/runtime/records.S'),str(ROOT/'src/runtime/delta.S'),*flags,'-o',str(path)],check=True)
        native=ctypes.CDLL(str(path));library=ctypes.CDLL(ctypes.util.find_library('json-c'))
        library.json_tokener_parse.argtypes=[ctypes.c_char_p];library.json_tokener_parse.restype=ctypes.c_void_p
        library.json_object_put.argtypes=[ctypes.c_void_p]
        library.json_object_to_json_string_ext.argtypes=[ctypes.c_void_p,ctypes.c_int];library.json_object_to_json_string_ext.restype=ctypes.c_char_p
        submission_transitions(native,library)
        native.pg_record_memory_open.argtypes=[ctypes.POINTER(Memory)];native.pg_record_memory_close.argtypes=[ctypes.POINTER(Memory)]
        native.pg_record_memory_mint.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(ctypes.c_uint64)]
        native.pg_record_memory_read.argtypes=[ctypes.POINTER(Memory),ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
        native.pg_record_memory_entry.argtypes=[ctypes.POINTER(Memory),ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_uint64)]
        native.pg_record_memory_head.argtypes=[ctypes.POINTER(Memory),ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
        native.pg_record_memory_scan.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(Query),ctypes.POINTER(Page)]
        native.pg_record_memory_storage.argtypes=[ctypes.POINTER(Memory)];native.pg_record_memory_storage.restype=Storage
        native.pg_record_memory_document.argtypes=[ctypes.POINTER(Memory),ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
        native.pg_record_memory_find_document.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(Address),ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
        native.pg_record_memory_scan_documents.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(DocumentQuery),ctypes.POINTER(Page)]
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
                elif op['op']=='docscan':
                    definition=op['query'];scope=definition['scope'];kind=definition.get('kind','').encode();buffer=ctypes.create_string_buffer(kind);cursor=op.get('cursor',{});query=DocumentQuery(('session','conversation','task').index(scope['kind']),int('kind' in definition),scope.get('conversationId',scope.get('taskId',0)),Slice(ctypes.addressof(buffer),len(kind)),2**64-1 if definition['at']=='current' else definition['at'],op['limit'],cursor.get('after',0),int('cursor' in op),0);page=Page()
                    status=native.pg_record_memory_scan_documents(ctypes.byref(memory),ctypes.byref(query),ctypes.byref(page));actual=dict(ok=status==0)
                    try:
                        if not status:
                            result=dict(items=json.loads(library.json_object_to_json_string_ext(page.items,0)))
                            if page.has_next:result['next']=dict(after=page.after,order='ascending')
                            actual['page']=result
                    finally:library.json_object_put(page.items)
                elif op['op']=='find':
                    definition=op['address'];kind=ctypes.create_string_buffer(definition['kind'].encode());key=ctypes.create_string_buffer(definition.get('key','').encode());scope=definition['scope'];address=Address(Slice(ctypes.addressof(kind),len(definition['kind'].encode())),Slice(ctypes.addressof(key),len(definition.get('key','').encode())),scope.get('conversationId',scope.get('taskId',0)),('session','conversation','task').index(scope['kind']),int('key' in definition))
                    value=ctypes.c_void_p();status=native.pg_record_memory_find_document(ctypes.byref(memory),ctypes.byref(address),2**64-1 if op['at']=='current' else op['at'],ctypes.byref(value))
                    try:
                        actual=dict(ok=status in (0,1))
                        if status in (0,1):actual['value']=json.loads(library.json_object_to_json_string_ext(value,0)) if not status else None
                    finally:library.json_object_put(value)
                elif op['op']=='document':
                    value=ctypes.c_void_p();status=native.pg_record_memory_document(ctypes.byref(memory),op['id'],2**64-1 if op['at']=='current' else op['at'],ctypes.byref(value))
                    try:
                        actual=dict(ok=status in (0,1))
                        if status in (0,1):actual['value']=json.loads(library.json_object_to_json_string_ext(value,0)) if not status else None
                    finally:library.json_object_put(value)
                elif op['op'] in ('entry','head'):
                    value=ctypes.c_void_p();seq=ctypes.c_uint64()
                    if op['op']=='entry':status=native.pg_record_memory_entry(ctypes.byref(memory),op['id'],op.get('conversation',0),ctypes.byref(value),ctypes.byref(seq))
                    else:status=native.pg_record_memory_head(ctypes.byref(memory),op['conversation'],op.get('at',2**64-1),ctypes.byref(value))
                    try:
                        actual=dict(ok=status in (0,1))
                        if not status:
                            result=json.loads(library.json_object_to_json_string_ext(value,0))
                            actual['value']=dict(entry=result,commitSeq=seq.value) if op['op']=='entry' else result
                        elif status==1:actual['value']=None
                        else:assert status==2 and not value.value
                    finally:library.json_object_put(value)
                elif op['op']=='scan':
                    q=op['query'];query=Query(table=tables.index(op['table']),limit=op['limit']);orders={None:0,'ascending':1,'descending':2}
                    query.order=orders.get(q.get('order'),99)
                    if 'cursor' in op:
                        query.has_cursor=1;query.after=op['cursor']['after'];query.cursor_order=orders.get(op['cursor'].get('order'),99)
                    for key,field,mask in [('conversationId','conversation_id',1),('ownerConversationId','owner_conversation_id',2),('ownerTaskId','owner_task_id',4),('minEntryId','min_entry_id',8),('maxEntryId','max_entry_id',16),('abortRequested','abort_requested',128),('background','background',256)]:
                        if key in q:query.flags|=mask;setattr(query,field,q[key])
                    if 'kind' in q:
                        kind=q['kind'].encode();kind_buffer=ctypes.create_string_buffer(kind);query.kind=Slice(ctypes.addressof(kind_buffer),len(kind));query.flags|=32
                    if 'status' in q:
                        statuses=['pending','running','waiting','sleeping','completing','terminal'] if op['table']=='task' else ['queued','placed','done','unanswered']
                        query.status=statuses.index(q['status']);query.flags|=64
                    page=Page();status=native.pg_record_memory_scan(ctypes.byref(memory),ctypes.byref(query),ctypes.byref(page))
                    try:
                        actual=dict(ok=status==0)
                        if not status:
                            actual['page']=dict(items=json.loads(library.json_object_to_json_string_ext(page.items,0)))
                            if page.has_next:actual['page']['next']=dict(after=page.after,order=['','ascending','descending'][page.order])
                        else:assert status==2 and not page.items
                    finally:library.json_object_put(page.items)
                else:
                    value=ctypes.c_void_p();status=native.pg_record_memory_read(ctypes.byref(memory),tables.index(op['table']),op['id'],ctypes.byref(value));assert status in (0,1),(index,status)
                    try:actual=dict(ok=True,value=json.loads(library.json_object_to_json_string_ext(value,0)) if value.value else None)
                    finally:library.json_object_put(value)
                assert actual==answer,(index,op,actual,answer)
        finally:assert native.pg_record_memory_close(ctypes.byref(memory))==0
    print(f'PASS: {len(operations)} table/document batch/read/scan/entry/head/ID operations match actual fixed Pi MemoryStorage')
if __name__=='__main__':main()
