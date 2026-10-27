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
registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord')return {url:new URL('packages/chord/src/json.ts',base).href,shortCircuit:true};if(s==='@earendil-works/chord/delta')return {url:new URL('packages/chord/src/delta/index.ts',base).href,shortCircuit:true};return n(s,c);}});
const {MemoryStorage}=await import(new URL('packages/durable/src/storage/memory.ts',base));
const {prepareForkDocumentCopies}=await import(new URL('packages/durable/src/session/forks.ts',base));
const storage=new MemoryStorage(),out=[];
for(const op of JSON.parse(readFileSync(0,'utf8'))){
 if(op.op==='commit'){const adopted=[];try{const seq=await storage.commit(op.writes,s=>adopted.push(s),{});out.push({ok:true,seq,adopted});}catch{out.push({ok:false,adopted});}}
 if(op.op==='docscan'){try{out.push({ok:true,page:await storage.scanDocuments(op.query,op.limit,op.cursor,{})});}catch{out.push({ok:false});}}
 if(op.op==='find'){try{out.push({ok:true,value:await storage.findDocument(op.address,op.at,{})??null});}catch{out.push({ok:false});}}
 if(op.op==='document'){try{out.push({ok:true,value:await storage.document(op.id,op.at,{})??null});}catch{out.push({ok:false});}}
 if(op.op==='forkplan'){try{out.push({ok:true,value:await prepareForkDocumentCopies(storage,op.parent,op.entry,op.child,{})});}catch{out.push({ok:false});}}
 if(op.op==='mint'){try{out.push({ok:true,id:await storage.mintId()});}catch{out.push({ok:false});}}
 if(op.op==='entry'){try{out.push({ok:true,value:await (op.conversation===undefined?storage.entry(op.id,{}):storage.entry(op.conversation,op.id,{}))??null});}catch{out.push({ok:false});}}
 if(op.op==='head'){try{out.push({ok:true,value:await storage.findLatestHeadMarker(op.conversation,op.at,{})??null});}catch{out.push({ok:false});}}
 if(op.op==='scan'){try{const names={conversation:'scanConversations',entry:'scanEntries',task:'scanTasks',submission:'scanSubmissions'};out.push({ok:true,page:await storage[names[op.table]](op.query,op.limit,op.cursor,{})});}catch{out.push({ok:false});}}
 if(op.op==='request'){out.push({ok:true,value:await storage.submissionByRequest(op.conversation,op.key,{})??null});}
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
class ForkQuery(ctypes.Structure):
    _fields_=[('conversation',ctypes.c_uint64),('at',ctypes.c_uint64),('after',ctypes.c_int64),('has_cursor',ctypes.c_uint),('reserved',ctypes.c_uint)]
class ForkPage(ctypes.Structure):
    _fields_=[('items',ctypes.c_void_p),('after',ctypes.c_int64),('has_next',ctypes.c_uint),('reserved',ctypes.c_uint)]
class ForkStorage(ctypes.Structure):
    _fields_=[('context',ctypes.c_void_p),('entry',ctypes.c_void_p),('scan',ctypes.c_void_p),('mint',ctypes.c_void_p)]
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

def entry_construction(native,library):
    relative='packages/durable/src/session/transaction.ts';path=ROOT/'references/pi'/relative
    for relative in [relative,'packages/chord/src/json.ts']:
        assert (ROOT/'references/pi'/relative).read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':'+relative])
    cases=[]
    for task in [0,7]:
        for head in ['absent','self',3,None,'self\0x']:
            draft=dict(kind='entry.中',id=999,conversationId=999,byTaskId=999,data={'x':[1]})
            if head!='absent':draft['head']=head
            cases.append(dict(draft=draft,task=task))
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';import {runInNewContext} from 'node:vm';import {pathToFileURL} from 'node:url';
const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('#appendEntry(conversationId: ConversationId, value: EntryDraft)'),end=source.indexOf('createTask<',start);const {copyJson}=await import(pathToFileURL(process.argv[2]));
const cs=source.indexOf('async #stageConversation('),ce=source.indexOf('appendEntry(conversationId:',cs);
const text='class Probe { #host={storage:{mintId:async()=>17},conversationCreated:async()=>{}};#scope;#writes=[];#createdConversationIds=new Set();#forkSourceDocumentIds=new Set();#documents=[];#latestDocumentByAddress=new Map();#forkSourceConversationIds=new Set();#context={};constructor(task){this.#scope={taskId:task||undefined}} #assertOpen(){} async #currentTask(id){return id===20?{id,conversationId:9}:undefined} async #requireConversation(){} #write(fn){return fn()} '+source.slice(start,end)+source.slice(cs,ce)+' run(value){return this.#appendEntry(9,value)} create(owner,root){return this.#stageConversation(undefined,owner?{kind:"task",taskId:owner}:{kind:"ownerless"},root?1:undefined)} }';
const TABLE_JSON_COPY_OPTIONS={omitUndefinedProperties:true};const Probe=eval('('+stripTypeScriptTypes(text)+')');const answers=[];for(const c of JSON.parse(readFileSync(0,'utf8')))answers.push(await new Probe(c.task).run(c.draft));const conversations=[];for(const [owner,root] of [[0,0],[0,1],[20,0],[999,0]]){try{conversations.push({ok:true,value:await new Probe(0).create(owner,root)})}catch{conversations.push({ok:false})}}console.log(JSON.stringify({entries:answers,conversations}));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(path),str(ROOT/'references/pi/packages/chord/src/json.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('has_write',ctypes.c_uint),('reserved',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));mint_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64))
    @read_type
    def read(context,table,id,out):
        if table==2 and id!=20:return 1
        out[0]=library.json_tokener_parse(b'{"id":20,"conversationId":9}' if table==2 else b'{"id":9}');return 0
    @mint_type
    def mint(context,out):out[0]=17;return 0
    native.pg_record_tx_begin.argtypes=[ctypes.POINTER(Tx),ctypes.c_void_p,ctypes.c_void_p,read_type]
    native.pg_record_tx_append_entry.argtypes=[ctypes.POINTER(Tx),ctypes.c_uint64,ctypes.c_void_p,ctypes.c_uint64,mint_type,ctypes.POINTER(ctypes.c_void_p)]
    native.pg_record_tx_discard.argtypes=[ctypes.POINTER(Tx)]
    for case,want in zip(cases,expected['entries'],strict=True):
        tx=Tx();session=ctypes.create_string_buffer(88);draft=library.json_tokener_parse(json.dumps(case['draft']).encode());out=ctypes.c_void_p()
        try:
            assert not native.pg_record_tx_begin(ctypes.byref(tx),session,None,read)
            assert not native.pg_record_tx_append_entry(ctypes.byref(tx),9,draft,case['task'],mint,ctypes.byref(out))
            assert json.loads(library.json_object_to_json_string_ext(out,0))==want,(case,want)
            assert json.loads(library.json_object_to_json_string_ext(draft,0))==case['draft']
        finally:library.json_object_put(out);library.json_object_put(draft);assert not native.pg_record_tx_discard(ctypes.byref(tx))
    native.pg_record_tx_create_conversation.argtypes=[ctypes.POINTER(Tx),ctypes.c_uint64,ctypes.c_uint,mint_type,ctypes.POINTER(ctypes.c_void_p)]
    for (owner,root),want in zip([(0,0),(0,1),(20,0),(999,0)],expected['conversations'],strict=True):
        tx=Tx();session=ctypes.create_string_buffer(88);out=ctypes.c_void_p()
        try:
            assert not native.pg_record_tx_begin(ctypes.byref(tx),session,None,read)
            status=native.pg_record_tx_create_conversation(ctypes.byref(tx),owner,root,mint,ctypes.byref(out));actual=dict(ok=status==0)
            if not status:actual['value']=json.loads(library.json_object_to_json_string_ext(out,0))
            else:assert not out.value
            assert actual==want,(owner,root,actual,want)
        finally:library.json_object_put(out);assert not native.pg_record_tx_discard(ctypes.byref(tx))
    print(f'PASS: {len(cases)} Entry and 4 Conversation constructions match actual Pi method bodies with fixed host')

def owner_validation(native,library):
    path=ROOT/'references/pi/packages/durable/src/session/transaction.ts'
    assert path.read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':packages/durable/src/session/transaction.ts'])
    cases=[dict(kind=kind,status=status,abort=abort,missing=False) for kind in ['conversation','task'] for status in ['pending','running','waiting','sleeping','completing','terminal'] for abort in [False,True]]
    cases += [dict(kind=kind,status='pending',abort=False,missing=True) for kind in ['conversation','task']]
    cases += [dict(kind='existing',status='terminal',abort=True,missing=False)]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';
const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('async #validateOwners()'),end=source.indexOf('#rejectForkSourceWrites(',start);
const text='class Probe{#writes;#tasksById;#task;constructor(c){this.#writes=c.kind==="conversation"?[{type:"conversation",value:{owner:{taskId:20}}}]:[];this.#tasksById=new Map(c.kind!=="conversation"?[[21,{write:{kind:c.kind==="existing"?"replace":"create",record:{owner:20}}}]]:[]);this.#task=c.missing?undefined:{id:20,state:{status:c.status},abortRequested:c.abort}}async #currentTask(id){return id===20?this.#task:undefined}'+source.slice(start,end)+'run(){return this.#validateOwners()}}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){try{await new Probe(c).run();out.push(true)}catch{out.push(false)}}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(path)],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('has_write',ctypes.c_uint),('depth',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));adopt_type=ctypes.CFUNCTYPE(None,ctypes.c_void_p,ctypes.c_uint64);commit_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,adopt_type,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64))
    @read_type
    def read(context,table,id,out):return native.pg_record_memory_read(ctypes.cast(context,ctypes.POINTER(Memory)),table,id,out)
    @adopt_type
    def adopted(context,seq):pass
    native.pg_record_tx_begin.argtypes=[ctypes.POINTER(Tx),ctypes.c_void_p,ctypes.c_void_p,read_type]
    native.pg_record_tx_stage.argtypes=[ctypes.POINTER(Tx),ctypes.c_uint,ctypes.c_void_p]
    native.pg_record_tx_commit.argtypes=[ctypes.POINTER(Tx)];native.pg_record_tx_discard.argtypes=[ctypes.POINTER(Tx)]
    for case,want in zip(cases,expected,strict=True):
        memory=Memory();assert not native.pg_record_memory_open(ctypes.byref(memory));storage=native.pg_record_memory_storage(ctypes.byref(memory));session=ctypes.create_string_buffer(88);ctypes.memmove(session,ctypes.byref(storage),ctypes.sizeof(storage));tx=Tx();owned=[]
        def parse(value):
            pointer=library.json_tokener_parse(json.dumps(value).encode());owned.append(pointer);return pointer
        try:
            seed=[dict(type='conversation',value=dict(id=1))]
            if not case['missing']:seed.append(dict(type='task',value=dict(id=20,conversationId=1,state=dict(status='pending'))))
            if case['kind']=='existing':seed.append(dict(type='task',value=dict(id=21,conversationId=1,owner=20,state=dict(status='pending'))))
            sequence=ctypes.c_uint64();assert not commit_type(storage.commit)(storage.context,parse(seed),adopted,None,ctypes.byref(sequence))
            assert not native.pg_record_tx_begin(ctypes.byref(tx),session,ctypes.cast(ctypes.byref(memory),ctypes.c_void_p),read)
            if not case['missing']:assert not native.pg_record_tx_stage(ctypes.byref(tx),2,parse(dict(type='task',value=dict(id=20,conversationId=1,state=dict(status=case['status']),abortRequested=case['abort']))))
            write=dict(type='conversation',value=dict(id=21,owner=dict(conversationId=1,taskId=20))) if case['kind']=='conversation' else dict(type='task',value=dict(id=21,conversationId=1,owner=20,state=dict(status='waiting')))
            assert not native.pg_record_tx_stage(ctypes.byref(tx),0 if case['kind']=='conversation' else 2,parse(write))
            status=native.pg_record_tx_commit(ctypes.byref(tx));assert (status==0)==want,(case,status,want)
        finally:
            assert not native.pg_record_tx_discard(ctypes.byref(tx))
            for pointer in owned:library.json_object_put(pointer)
            assert not native.pg_record_memory_close(ctypes.byref(memory))
    print(f'PASS: {len(cases)} final owner validations match actual Pi validateOwners body')

def loaded_access(native,library):
    for relative in ['packages/durable/src/session/session.ts','packages/durable/src/documents.ts','packages/chord/src/delta/tracker.ts','packages/chord/src/json.ts']:
        assert (ROOT/'references/pi'/relative).read_bytes()==subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',COMMIT+':'+relative])
    cases=[]
    semantics=[('session',None,None),('task',None,None),('conversation','latest','current'),('conversation','latest','initial'),('conversation','rewindable','asOf'),('conversation','rewindable','current'),('conversation','rewindable','initial')]
    for scope,history,fork in semantics:
        for key in [None,'','key\0中']:
            for stored,version in [(1,1),(1,2),(2,1)]:
                for migration in [False,True]:
                    record=dict(id=2,kind='load\0中',scope={'kind':scope,**({'conversationId':42} if scope=='conversation' else {'taskId':43} if scope=='task' else {})},createdAt=1)
                    if key is not None:record['key']=key
                    if history:record.update(history=history,fork=fork)
                    cases.append(dict(record=record,stored=stored,version=version,migration=migration,find=True,read=True))
    cases += [dict(cases[0],find=False),dict(cases[0],read=False)]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes,registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';
const base=pathToFileURL(process.argv[1]+'/');registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord')return {url:new URL('packages/chord/src/json.ts',base).href,shortCircuit:true};return n(s,c);}});const {materializeDocument}=await import(new URL('packages/durable/src/documents.ts',base));const {track}=await import(new URL('packages/chord/src/delta/tracker.ts',base));
const source=readFileSync(new URL('packages/durable/src/session/session.ts',base),'utf8');const start=source.indexOf('async #loadDocument('),end=source.indexOf('#enqueue<',start);
const text='class Probe{#documents=new Map();#holds=new Map();#storage;calls=[];constructor(c){this.#storage={findDocument:async()=>{this.calls.push("find");return c.find?c.record:undefined},document:async()=>{this.calls.push("read");return c.read?{record:c.record,version:c.stored,value:{text:"old"},deltasSinceBase:3}:undefined}}}#touchDocument(){}#dropDocument(id){this.#documents.delete(id)}async #storeHolds(){this.calls.push("flush");if(this.failFlush)throw Error("flush");this.#holds.clear()}hold(id){this.#holds.set(id,{})}'+source.slice(start,end)+'run(d,a){return this.#loadDocument(d,"test",a,{})}}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];
for(const c of JSON.parse(readFileSync(0,'utf8'))){const p=new Probe(c);const definition={kind:c.record.kind,scope:c.record.scope.kind,version:c.version,...(c.record.history?{history:c.record.history,fork:c.record.fork}:{}),...(c.migration?{migrate:(value,from)=>({...value,from})}:{})};try{const x=await p.run(definition,c.record);out.push({ok:true,value:x?{record:x.record,stored:x.storedVersion,version:x.valueVersion,deltas:x.deltasSinceBase,value:x.tracker.value}:null,calls:p.calls})}catch{out.push({ok:false,calls:p.calls})}}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Record(ctypes.Structure):
        _fields_=[('address',Address),('id',ctypes.c_uint64),('history',ctypes.c_uint),('fork',ctypes.c_uint)]
    class Definition(ctypes.Structure):
        _fields_=[('kind',Slice),('version',ctypes.c_uint64),('scope',ctypes.c_uint),('history',ctypes.c_uint),('fork',ctypes.c_uint),('family',ctypes.c_uint),('context',ctypes.c_void_p),('migrate',ctypes.c_void_p)]
    class Loaded(ctypes.Structure):
        _fields_=[('record_owner',ctypes.c_void_p),('value',ctypes.c_void_p),('definition',ctypes.c_void_p),('record',Record),('stored',ctypes.c_uint64),('version',ctypes.c_uint64),('deltas',ctypes.c_uint64),('busy',ctypes.c_uint),('active',ctypes.c_uint)]
    class Documents(ctypes.Structure):
        _fields_=[('context',ctypes.c_void_p),('find',ctypes.c_void_p),('read',ctypes.c_void_p)]
    find_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(Address),ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));migrate_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    native.pg_document_loaded_open.argtypes=[ctypes.POINTER(Loaded),ctypes.POINTER(Documents),ctypes.POINTER(Definition),ctypes.POINTER(Address)];native.pg_document_loaded_close.argtypes=[ctypes.POINTER(Loaded)]
    for case,want in zip(cases,expected,strict=True):
        calls=[];record=case['record'];kind=record['kind'].encode();key=record.get('key','').encode();kind_buffer=ctypes.create_string_buffer(kind);key_buffer=ctypes.create_string_buffer(key);scope=record['scope'];scope_index=('session','conversation','task').index(scope['kind']);history={None:0,'latest':1,'rewindable':2}[record.get('history')];fork={None:0,'current':1,'initial':2,'asOf':3}[record.get('fork')]
        @find_type
        def find(context,address,at,out):
            calls.append('find');assert at==2**64-1
            if not case['find']:return 1
            out[0]=library.json_tokener_parse(json.dumps(record).encode());return 0
        @read_type
        def read(context,id,at,out):
            calls.append('read');assert id==2 and at==2**64-1
            if not case['read']:return 1
            out[0]=library.json_tokener_parse(json.dumps(dict(record=record,version=case['stored'],value={'text':'old'},deltasSinceBase=3)).encode());return 0
        @migrate_type
        def migrate(context,value,version,out):
            data=json.loads(library.json_object_to_json_string_ext(value,0));data['from']=version;out[0]=library.json_tokener_parse(json.dumps(data).encode());return 0
        definition=Definition(Slice(ctypes.addressof(kind_buffer),len(kind)),case['version'],scope_index,history,fork,int('key' in record),None,ctypes.cast(migrate,ctypes.c_void_p) if case['migration'] else None);address=Address(Slice(ctypes.addressof(kind_buffer),len(kind)),Slice(ctypes.addressof(key_buffer),len(key)),scope.get('conversationId',scope.get('taskId',0)),scope_index,int('key' in record));storage=Documents(None,ctypes.cast(find,ctypes.c_void_p),ctypes.cast(read,ctypes.c_void_p));loaded=Loaded()
        try:
            status=native.pg_document_loaded_open(ctypes.byref(loaded),ctypes.byref(storage),ctypes.byref(definition),ctypes.byref(address));actual=dict(ok=status in (0,8),calls=calls)
            if not status:actual['value']=dict(record=json.loads(library.json_object_to_json_string_ext(loaded.record_owner,0)),stored=loaded.stored,version=loaded.version,deltas=loaded.deltas,value=json.loads(library.json_object_to_json_string_ext(loaded.value,0)))
            elif status==8:actual['value']=None
            else:assert not loaded.active and not loaded.busy and not loaded.value and not loaded.record_owner
            assert actual==want,(case,status,actual,want)
        finally:assert not native.pg_document_loaded_close(ctypes.byref(loaded))
    print(f'PASS: {len(cases)} loaded document acquisitions match actual Pi loadDocument body with injected Storage')
    sequence=[dict(version=1,migration=False),dict(version=1,migration=False),dict(version=2,migration=True),dict(version=2,migration=True),dict(version=1,migration=False),dict(version=3,migration=False),dict(version=1,migration=False),dict(version=2,migration=True,hold=True),dict(version=1,migration=False,hold=True,fail=True),dict(version=2,migration=True),dict(version=1,migration=False)]
    preamble=code[:code.index('const out=[];')]
    tail=r'''
const steps=JSON.parse(readFileSync(0,'utf8'));const fixed={record:{id:2,kind:'cache',scope:{kind:'session'},createdAt:1},stored:1,find:true,read:true};const p=new Probe(fixed),out=[];
for(const c of steps){if(c.hold)p.hold(2);p.failFlush=!!c.fail;const d={kind:'cache',scope:'session',version:c.version,...(c.migration?{migrate:(value,from)=>({...value,from})}:{})};try{const x=await p.run(d,fixed.record);out.push({ok:true,stored:x.storedVersion,version:x.valueVersion,value:x.tracker.value,calls:[...p.calls]})}catch{out.push({ok:false,calls:[...p.calls]})}}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',preamble+tail,str(ROOT/'references/pi')],input=json.dumps(sequence),text=True,capture_output=True,check=True).stdout)
    class Hooks(ctypes.Structure):
        _fields_=[('context',ctypes.c_void_p),('clock',ctypes.c_void_p),('flush',ctypes.c_void_p),('pinned',ctypes.c_void_p)]
    class Cache(ctypes.Structure):
        _fields_=[('table',ctypes.c_void_p),('head',ctypes.c_void_p),('idle_head',ctypes.c_void_p),('idle_tail',ctypes.c_void_p),('count',ctypes.c_size_t),('storage',Documents),('context',ctypes.c_void_p),('clock',ctypes.c_void_p),('retention',ctypes.c_uint64),('flush',ctypes.c_void_p),('pinned',ctypes.c_void_p),('busy',ctypes.c_uint),('closed',ctypes.c_uint)]
    clock_type=ctypes.CFUNCTYPE(ctypes.c_uint64,ctypes.c_void_p);flush_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(Loaded));calls=[];pending=False;failed=False
    @clock_type
    def clock(context):return 0
    @flush_type
    def flush(context,value):
        nonlocal pending
        if pending:
            calls.append('flush')
            if failed:return 1
            pending=False
        return 0
    @find_type
    def cache_find(context,address,at,out):calls.append('find');out[0]=library.json_tokener_parse(b'{"id":2,"kind":"cache","scope":{"kind":"session"},"createdAt":1}');return 0
    @read_type
    def cache_read(context,id,at,out):calls.append('read');out[0]=library.json_tokener_parse(b'{"record":{"id":2,"kind":"cache","scope":{"kind":"session"},"createdAt":1},"version":1,"value":{"text":"old"},"deltasSinceBase":3}');return 0
    hooks=Hooks(None,ctypes.cast(clock,ctypes.c_void_p),ctypes.cast(flush,ctypes.c_void_p),None);storage=Documents(None,ctypes.cast(cache_find,ctypes.c_void_p),ctypes.cast(cache_read,ctypes.c_void_p));cache=Cache();keep=[]
    native.pg_document_cache_open.argtypes=[ctypes.POINTER(Cache),ctypes.POINTER(Documents),ctypes.POINTER(Hooks),ctypes.c_uint64];native.pg_document_cache_get.argtypes=[ctypes.POINTER(Cache),ctypes.POINTER(Definition),ctypes.POINTER(Address),ctypes.POINTER(ctypes.POINTER(Loaded))];native.pg_document_cache_close.argtypes=[ctypes.POINTER(Cache)]
    assert not native.pg_document_cache_open(ctypes.byref(cache),ctypes.byref(storage),ctypes.byref(hooks),100000)
    try:
        for step,want in zip(sequence,expected,strict=True):
            if step.get('hold'):pending=True
            failed=step.get('fail',False);buffer=ctypes.create_string_buffer(b'cache');definition=Definition(Slice(ctypes.addressof(buffer),5),step['version'],0,0,0,0,None,ctypes.cast(migrate,ctypes.c_void_p) if step['migration'] else None);address=Address(Slice(ctypes.addressof(buffer),5),Slice(),0,0,0);keep.append((buffer,definition));out=ctypes.POINTER(Loaded)();status=native.pg_document_cache_get(ctypes.byref(cache),ctypes.byref(definition),ctypes.byref(address),ctypes.byref(out));actual=dict(ok=status==0,calls=list(calls))
            if not status:actual.update(stored=out.contents.stored,version=out.contents.version,value=json.loads(library.json_object_to_json_string_ext(out.contents.value,0)))
            else:assert not out
            assert actual==want,(step,status,actual,want)
    finally:assert not native.pg_document_cache_close(ctypes.byref(cache))
    print(f'PASS: {len(sequence)} cache reuse/version/held-flush steps match actual Pi loadDocument body')


def historical_snapshots(native,library):
    cases=[]
    for key in [None,'','k\0中']:
        for stored,version in [(1,1),(1,2),(2,1)]:
            for migration in [False,True]:
                for mode in ['normal','invisible','absent','missing_read','entry_error','find_error','read_error']:
                    record=dict(id=2,kind='history\0中',scope=dict(kind='conversation',conversationId=7),createdAt=1,history='rewindable',fork='asOf')
                    if key is not None:record['key']=key
                    cases.append(dict(record=record,stored=stored,version=version,migration=migration,mode=mode))
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes,registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';
const base=pathToFileURL(process.argv[1]+'/');registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord')return {url:new URL('packages/chord/src/json.ts',base).href,shortCircuit:true};return n(s,c)}});
const {materializeDocument,resolveAddress}=await import(new URL('packages/durable/src/documents.ts',base));const {idFromNumber}=await import(new URL('packages/durable/src/ids.ts',base));const {SessionFailed,StorageRequestError}=await import(new URL('packages/durable/src/errors.ts',base));
const source=readFileSync(new URL('packages/durable/src/session/session.ts',base),'utf8');const Guard=eval('('+stripTypeScriptTypes(source.slice(source.indexOf('class GuardedStorage')))+')');const start=source.indexOf('async snapshotAsOf(token:'),end=source.indexOf('\n\tclose(context:',start);
const text='class Probe{#storage;constructor(storage){this.#storage=storage}assertUsable(){}#assertHealthy(){}#enqueue(fn){return fn()}'+source.slice(start,end)+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];
for(const c of JSON.parse(readFileSync(0,'utf8'))){const calls=[];const latch={failure:undefined,fail(error){this.failure??={error}}};const backend={entry:async(owner,id)=>{calls.push(['entry',owner,id]);if(c.mode==='entry_error')throw Error('entry');return c.mode==='invisible'?undefined:{entry:{id:20,conversationId:7},commitSeq:9}},findDocument:async(a,p)=>{calls.push(['find',a.scope.conversationId,p]);if(c.mode==='find_error')throw Error('find');return c.mode==='absent'?undefined:c.record},document:async(id,p)=>{calls.push(['read',id,p]);if(c.mode==='read_error')throw Error('read');return c.mode==='missing_read'?undefined:{record:c.record,version:c.stored,value:{text:'old'},deltasSinceBase:3}}};
 const d={kind:c.record.kind,scope:'conversation',history:'rewindable',fork:'asOf',version:c.version,...('key'in c.record?{family:true}:{}),...(c.migration?{migrate:(value,from)=>({...value,from})}:{})};const args=[42,...('key'in c.record?[c.record.key]:[]),20,{}];try{const value=await new Probe(new Guard(backend,latch)).snapshotAsOf({definition:d},...args);out.push({ok:true,value:value??null,calls,failed:!!latch.failure})}catch{out.push({ok:false,calls,failed:!!latch.failure})}}
console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Definition(ctypes.Structure):
        _fields_=[('kind',Slice),('version',ctypes.c_uint64),('scope',ctypes.c_uint),('history',ctypes.c_uint),('fork',ctypes.c_uint),('family',ctypes.c_uint),('context',ctypes.c_void_p),('migrate',ctypes.c_void_p)]
    class History(ctypes.Structure):
        _fields_=[('doc_context',ctypes.c_void_p),('find',ctypes.c_void_p),('read',ctypes.c_void_p),('entry_context',ctypes.c_void_p),('entry',ctypes.c_void_p)]
    find_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(Address),ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    entry_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_uint64))
    migrate_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    native.pg_durable_snapshot_as_of.argtypes=[ctypes.c_void_p,ctypes.POINTER(History),ctypes.POINTER(Definition),ctypes.POINTER(Address),ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
    for case,want in zip(cases,expected,strict=True):
        calls=[];record=case['record'];kind=record['kind'].encode();key=record.get('key','').encode();kb=ctypes.create_string_buffer(kind);vb=ctypes.create_string_buffer(key)
        @entry_type
        def entry(context,owner,id,out,seq):
            calls.append(['entry',owner,id])
            if case['mode']=='entry_error':return 7
            if case['mode']=='invisible':return 1
            out[0]=library.json_tokener_parse(b'{"id":20,"conversationId":7}');seq[0]=9;return 0
        @find_type
        def find(context,address,point,out):
            calls.append(['find',address.contents.owner,point])
            if case['mode']=='find_error':return 7
            if case['mode']=='absent':return 1
            out[0]=library.json_tokener_parse(json.dumps(record).encode());return 0
        @read_type
        def read(context,id,point,out):
            calls.append(['read',id,point])
            if case['mode']=='read_error':return 7
            if case['mode']=='missing_read':return 1
            out[0]=library.json_tokener_parse(json.dumps(dict(record=record,version=case['stored'],value={'text':'old'},deltasSinceBase=3)).encode());return 0
        @migrate_type
        def migrate(context,value,version,out):
            data=json.loads(library.json_object_to_json_string_ext(value,0));data['from']=version;out[0]=library.json_tokener_parse(json.dumps(data).encode());return 0
        definition=Definition(Slice(ctypes.addressof(kb),len(kind)),case['version'],1,2,3,int('key'in record),None,ctypes.cast(migrate,ctypes.c_void_p) if case['migration'] else None)
        address=Address(Slice(ctypes.addressof(kb),len(kind)),Slice(ctypes.addressof(vb),len(key)),42,1,int('key'in record));history=History(None,ctypes.cast(find,ctypes.c_void_p),ctypes.cast(read,ctypes.c_void_p),None,ctypes.cast(entry,ctypes.c_void_p))
        session=ctypes.create_string_buffer(192);records=ctypes.create_string_buffer(88);ctypes.c_void_p.from_buffer(session,0).value=ctypes.addressof(records);ctypes.c_uint.from_buffer(session,172).value=1;value=ctypes.c_void_p()
        try:
            status=native.pg_durable_snapshot_as_of(session,ctypes.byref(history),ctypes.byref(definition),ctypes.byref(address),20,ctypes.byref(value));actual=dict(ok=status in (0,8),calls=calls,failed=bool(ctypes.c_uint.from_buffer(records,52).value))
            if actual['ok']:actual['value']=json.loads(library.json_object_to_json_string_ext(value,0)) if value.value else None
            else:assert not value.value
            assert actual==want,(case,status,actual,want)
            assert ctypes.c_uint.from_buffer(session,168).value==0
        finally:library.json_object_put(value)
    print(f'PASS: {len(cases)} historical snapshot value/point/owner/failure results match actual Pi snapshotAsOf and GuardedStorage')

def typed_entries(native,library):
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('entry(first:'),end=source.indexOf('\n\ttask(',start);const text='class Probe{#host;#context={};constructor(value){this.#host={storage:{entry:async()=>value===null?undefined:{entry:value}}}}#read(name,fn){return fn()}'+source.slice(start,end)+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8')))out.push((await new Probe(c.value).entry(c.kind===null?3:{kind:c.kind},3))??null);console.log(JSON.stringify(out));
'''
    cases=[dict(value=value,kind=kind) for value in [None,dict(id=3,kind=''),dict(id=3,kind='k\0中'),dict(id=3,kind='k')] for kind in [None,'','k','k\0中','other']]
    want=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/transaction.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    native.pg_record_tx_entry.argtypes=[ctypes.POINTER(Tx),ctypes.c_uint64,ctypes.POINTER(Slice),ctypes.POINTER(ctypes.c_void_p)]
    for case,expected in zip(cases,want,strict=True):
        @read_type
        def read(ctx,table,id,out):
            assert table==1 and id==3
            if case['value'] is None:return 1
            out[0]=library.json_tokener_parse(json.dumps(case['value']).encode());return 0
        writes=library.json_tokener_parse(b'[]');tx=Tx(None,None,ctypes.cast(read,ctypes.c_void_p),writes);output=ctypes.c_void_p();key=(case['kind']or'').encode();buffer=ctypes.create_string_buffer(key);kind=Slice(ctypes.addressof(buffer),len(key))
        try:
            status=native.pg_record_tx_entry(ctypes.byref(tx),3,ctypes.byref(kind) if case['kind'] is not None else None,ctypes.byref(output));assert status in (0,7)
            actual=json.loads(library.json_object_to_json_string_ext(output,0)) if output.value else None
            assert actual==expected and not tx.busy,(case,actual,expected)
        finally:library.json_object_put(output);library.json_object_put(writes)
    print(f'PASS: {len(cases)} typed Entry lookups match actual Pi Transaction.entry body')

def submission_creation(native,library):
    cases=[dict(conversationId=owner,type=kind,status=status,id=999,requestId='r\0中',data=dict(x=1)) for owner in [1,2] for kind in ['input','write'] for status in ['queued','placed','done','unanswered']]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes,registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';const base=pathToFileURL(process.argv[1]+'/');registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord')return {url:new URL('packages/chord/src/json.ts',base).href,shortCircuit:true};return n(s,c)}});const {copyJson}=await import(new URL('packages/chord/src/json.ts',base));const TABLE_JSON_COPY_OPTIONS={omitUndefinedProperties:true};const source=readFileSync(new URL('packages/durable/src/session/transaction.ts',base),'utf8');const start=source.indexOf('createSubmission(create:'),end=source.indexOf('\n\t/**',start);const text='class Probe{#host={storage:{mintId:async()=>17}};#submissions=new Map();#write(fn){return fn()}#assertOpen(){}async #requireConversation(id){if(id!==1)throw Error("missing")}'+source.slice(start,end)+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){try{out.push({ok:true,value:await new Probe().createSubmission(c)})}catch{out.push({ok:false})}}console.log(JSON.stringify(out));
'''
    want=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));mint_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64))
    native.pg_record_tx_create_submission.argtypes=[ctypes.POINTER(Tx),ctypes.c_void_p,mint_type,ctypes.POINTER(ctypes.c_void_p)]
    for case,expected in zip(cases,want,strict=True):
        minted=[]
        @read_type
        def read(ctx,table,id,out):
            assert table==0
            if id!=1:return 1
            out[0]=library.json_tokener_parse(b'{"id":1}');return 0
        @mint_type
        def mint(ctx,out):minted.append(17);out[0]=17;return 0
        writes=library.json_tokener_parse(b'[]');draft=library.json_tokener_parse(json.dumps(case).encode());tx=Tx(None,None,ctypes.cast(read,ctypes.c_void_p),writes);out=ctypes.c_void_p()
        try:
            status=native.pg_record_tx_create_submission(ctypes.byref(tx),draft,mint,ctypes.byref(out));actual=dict(ok=status==0)
            if not status:actual['value']=json.loads(library.json_object_to_json_string_ext(out,0))
            else:assert not out.value
            assert actual==expected and tx.barrier and not tx.busy and len(minted)==int(expected['ok']),(case,status,actual,expected)
        finally:library.json_object_put(out);library.json_object_put(draft);library.json_object_put(writes)
    print(f'PASS: {len(cases)} submission constructions match actual Pi createSubmission body')

def candidate_snapshots(native,library):
    rng=random.Random(743);cases=[]
    for size in [0,1,8,70,140]:
        writes=[]
        for i in range(size):
            task=i%4!=0;writes.append(dict(type='task' if task else 'conversation',value=dict(id=rng.randrange(10,30) if task else i+100,n=i)))
        cases.append(writes)
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(process.argv[1],'utf8');const start=source.indexOf('stagedTasks()'),end=source.indexOf('// ─── Documents',start);const text='class Probe{#writes;#tasksById=new Map();constructor(writes){this.#writes=writes;for(const w of writes)if(w.type==="task")this.#tasksById.set(w.value.id,{write:{record:w.value}})}'+source.slice(start,end)+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');console.log(JSON.stringify(JSON.parse(readFileSync(0,'utf8')).map(w=>{const p=new Probe(w);return [p.stagedTasks(),p.stagedConversations()]})));
'''
    want=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/transaction.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    for name in ['pg_record_tx_staged_tasks','pg_record_tx_staged_conversations']:getattr(native,name).argtypes=[ctypes.POINTER(Tx),ctypes.POINTER(ctypes.c_void_p)]
    for writes,expected in zip(cases,want,strict=True):
        batch=library.json_tokener_parse(json.dumps(writes).encode());tx=Tx(None,None,None,batch,0,0,1,0)
        try:
            for index,name in enumerate(['pg_record_tx_staged_tasks','pg_record_tx_staged_conversations']):
                output=ctypes.c_void_p()
                try:
                    assert not getattr(native,name)(ctypes.byref(tx),ctypes.byref(output)) and not tx.busy
                    actual=json.loads(library.json_object_to_json_string_ext(output,0));assert actual==expected[index],(writes,actual,expected[index])
                finally:library.json_object_put(output)
        finally:library.json_object_put(batch)
    print('PASS: 10 staged candidate arrays match actual Pi stagedTasks/stagedConversations with write-established Task order')

def task_cache_order(native,library):
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const s=readFileSync(process.argv[1],'utf8');const a=s.indexOf('#taskEntry(id:'),b=s.indexOf('\n}',a),c=s.indexOf('stagedTasks()'),d=s.indexOf('\n\t/**',c);const text='class Probe{#tasksById=new Map();#context={};calls=0;#host={storage:{task:async(id)=>{this.calls++;if(id===9)throw Error("read");return id===99?undefined:{id,n:this.calls}}}};'+s.slice(a,b)+s.slice(c,d)+'read(id){return this.#committedTask(id)}stage(record){this.#taskEntry(record.id).write={record}}}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const ids of [[2,3],[3,2],[99,2],[9,2]]){const p=new Probe(),reads=[];for(const id of ids)for(let i=0;i<2;i++){try{reads.push({ok:true,value:(await p.read(id))??null})}catch{reads.push({ok:false})}}p.stage({id:2,n:20});p.stage({id:3,n:30});out.push({reads,calls:p.calls,staged:p.stagedTasks()})}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/transaction.ts')],text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    native.pg_record_tx_read.argtypes=[ctypes.POINTER(Tx),ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)];native.pg_record_tx_stage.argtypes=[ctypes.POINTER(Tx),ctypes.c_uint,ctypes.c_void_p];native.pg_record_tx_discard.argtypes=[ctypes.POINTER(Tx)];native.pg_record_tx_staged_tasks.argtypes=[ctypes.POINTER(Tx),ctypes.POINTER(ctypes.c_void_p)]
    for ids,want in zip([[2,3],[3,2],[99,2],[9,2]],expected,strict=True):
        calls=[]
        @read_type
        def read(ctx,table,id,out):
            calls.append(id);assert table==2
            if id==9:return 7
            if id==99:return 1
            out[0]=library.json_tokener_parse(json.dumps(dict(id=id,n=len(calls))).encode());return 0
        tx=Tx(None,None,ctypes.cast(read,ctypes.c_void_p),library.json_tokener_parse(b'[]'));actual=dict(reads=[])
        try:
            for id in ids:
                for _ in range(2):
                    out=ctypes.c_void_p();status=native.pg_record_tx_read(ctypes.byref(tx),2,id,ctypes.byref(out))
                    try:
                        item=dict(ok=status in (0,7))
                        if item['ok']:item['value']=json.loads(library.json_object_to_json_string_ext(out,0)) if out.value else None
                        actual['reads'].append(item)
                    finally:library.json_object_put(out)
            for id in [2,3]:
                value=library.json_tokener_parse(json.dumps(dict(type='task',value=dict(id=id,n=id*10))).encode())
                try:assert not native.pg_record_tx_stage(ctypes.byref(tx),2,value)
                finally:library.json_object_put(value)
            out=ctypes.c_void_p()
            try:
                assert not native.pg_record_tx_staged_tasks(ctypes.byref(tx),ctypes.byref(out));actual['staged']=json.loads(library.json_object_to_json_string_ext(out,0))
            finally:library.json_object_put(out)
            actual['calls']=len(calls);assert actual==want,(ids,actual,want)
        finally:assert not native.pg_record_tx_discard(ctypes.byref(tx))
    print('PASS: cached Task success/missing/rejection and first-read candidate order match actual Pi Task cache methods')

def task_replacement(native,library):
    cases=[]
    candidates=[None,dict(id=10,conversationId=1,state=dict(status='pending')),dict(id=10,conversationId=1,state=dict(status='waiting'),startedAt=0,endedAt=9),dict(id=10,conversationId=1,state=dict(status='terminal')),dict(id=10,conversationId=2,state=dict(status='pending'))]
    for candidate in candidates:
        for status in ['pending','running','waiting','terminal']:
            for times in [{},{'startedAt':None,'endedAt':None},{'startedAt':-2.5},{'startedAt':8,'endedAt':0}]:cases.append(dict(candidate=candidate,value=dict(id=10,conversationId=1,state=dict(status=status),**times)))
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes,registerHooks} from 'node:module';import {pathToFileURL} from 'node:url';const base=pathToFileURL(process.argv[1]+'/');registerHooks({resolve(s,c,n){if(s==='@earendil-works/chord')return {url:new URL('packages/chord/src/json.ts',base).href,shortCircuit:true};return n(s,c)}});const {copyJson}=await import(new URL('packages/chord/src/json.ts',base));const TABLE_JSON_COPY_OPTIONS={omitUndefinedProperties:true};const s=readFileSync(new URL('packages/durable/src/session/transaction.ts',base),'utf8');const a=s.indexOf('setTask(value:'),b=s.indexOf('\n\t/**',a),c=s.indexOf('#stampTimes(value:'),d=s.indexOf('\n\t#taskEntry(',c);const text='class Probe{calls=0;task={};#host={now:()=>{this.calls++;return 100.25}};constructor(candidate){if(candidate)this.task.write={kind:"replace",record:candidate}}#assertOpen(){}#hasTableWrite=false;#taskEntry(){return this.task}'+s.slice(a,b)+s.slice(c,d)+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){const p=new Probe(c.candidate);let ok=true;try{p.setTask(c.value)}catch{ok=false}out.push({ok,calls:p.calls,value:p.task.write?.record??null})}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    now_type=ctypes.CFUNCTYPE(ctypes.c_double,ctypes.c_void_p);native.pg_record_tx_set_task.argtypes=[ctypes.POINTER(Tx),ctypes.c_void_p,ctypes.c_void_p,now_type];native.pg_record_tx_discard.argtypes=[ctypes.POINTER(Tx)]
    library.json_object_array_length.argtypes=[ctypes.c_void_p];library.json_object_array_length.restype=ctypes.c_size_t
    for case,want in zip(cases,expected,strict=True):
        calls=[]
        @now_type
        def now(context):calls.append(1);return 100.25
        writes=[] if case['candidate'] is None else [dict(type='task',value=case['candidate'])];tx=Tx(None,None,None,library.json_tokener_parse(json.dumps(writes).encode()));value=library.json_tokener_parse(json.dumps(case['value']).encode())
        try:
            status=native.pg_record_tx_set_task(ctypes.byref(tx),value,None,now);batch=json.loads(library.json_object_to_json_string_ext(tx.writes,0));actual=dict(ok=status==0,calls=len(calls),value=batch[-1]['value'] if batch else None);assert actual==want and not tx.busy and tx.barrier,(case,status,actual,want)
        finally:library.json_object_put(value);assert not native.pg_record_tx_discard(ctypes.byref(tx))
    print(f'PASS: {len(cases)} Task replacements and lifecycle timestamps match actual Pi setTask/stampTimes')

def task_options(native,library):
    choices=[None,[],[10],[11],[12],[10,13,12],[99,13],[10,88],[13,10,13]];cases=[dict(staged=staged,options=dict(trace={'parent':'given'},links=[{}],**({} if ids is None else {'submissionIds':ids}))) for staged in [False,True] for ids in choices]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const s=readFileSync(process.argv[1],'utf8');const a=s.indexOf('if (options.submissionIds !== undefined)'),b=s.indexOf('const ownership =',a);const text='class Probe{#submissions=new Map();#context={};calls=[];#host={storage:{submission:async(id)=>{this.calls.push(id);if(id===88)throw Error("read");return ({10:{trace:{parent:"old"}},11:{},12:{trace:null},13:{trace:{parent:"third"}}})[id]}}};constructor(staged){if(staged)this.#submissions.set(10,{trace:{parent:"new"}})}async run(options){'+s.slice(a,b)+'return options}}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){const p=new Probe(c.staged);try{out.push({ok:true,value:await p.run(c.options),calls:p.calls})}catch{out.push({ok:false,calls:p.calls})}}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/transaction.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));native.pg_record_tx_task_options.argtypes=[ctypes.POINTER(Tx),ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)];native.pg_record_tx_discard.argtypes=[ctypes.POINTER(Tx)]
    for case,want in zip(cases,expected,strict=True):
        calls=[]
        @read_type
        def read(ctx,table,id,out):
            assert table==3;calls.append(id)
            if id==88:return 7
            records={10:dict(trace=dict(parent='old')),11:{},12:dict(trace=None),13:dict(trace=dict(parent='third'))}
            if id not in records:return 1
            out[0]=library.json_tokener_parse(json.dumps(records[id]).encode());return 0
        writes=[dict(type='submission',value=dict(id=10,trace=dict(parent='new')))] if case['staged'] else [];tx=Tx(None,None,ctypes.cast(read,ctypes.c_void_p),library.json_tokener_parse(json.dumps(writes).encode()));value=library.json_tokener_parse(json.dumps(case['options']).encode());out=ctypes.c_void_p()
        try:
            status=native.pg_record_tx_task_options(ctypes.byref(tx),value,ctypes.byref(out));actual=dict(ok=status==0,calls=calls)
            if not status:actual['value']=json.loads(library.json_object_to_json_string_ext(out,0))
            else:assert not out.value
            assert actual==want and tx.barrier and not tx.busy,(case,status,actual,want)
        finally:library.json_object_put(value);library.json_object_put(out);assert not native.pg_record_tx_discard(ctypes.byref(tx))
    print(f'PASS: {len(cases)} createTask submission trace/link derivations match actual Pi function body')

def task_creation(native,library):
    cases=[]
    options=[{'ownership':{'kind':'conversation'}}, {'ownership':{'kind':'conversation'},'conversationId':2,'background':True,'abandonOnRestart':True}, {'ownership':{'kind':'task','taskId':10}}, {'ownership':{'kind':'task','taskId':10},'conversationId':2}, {'ownership':{'kind':'task','taskId':99}}, {'ownership':{'kind':'task','taskId':10},'background':True}, {'ownership':{'kind':'conversation'},'conversationId':99}]
    for opt in options:
        for trace in ['absent','root',None,{'parent':'explicit'}]:
            for staged in [False,True]:
                cases.append(dict(options={**opt,**({} if trace=='absent' else {'trace':trace})},staged=staged,scope=1,input={'text':'测试'},failure=''))
    for failure in ['','initial','mint','inject']:
        for ids in [None,[],[20,21],[99,21]]:
            cases.append(dict(options={'ownership':{'kind':'conversation'},**({} if ids is None else {'submissionIds':ids})},staged=False,scope=1,input=None,failure=failure))
    cases.append(dict(options={'ownership':{'kind':'conversation'}},staged=False,scope=0,input=[],failure=''))
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const s=readFileSync(process.argv[1],'utf8');const a=s.indexOf('\tcreateTask<I,'),b=s.indexOf('\n\t/** Create a raw submission',a);const copyJson=v=>JSON.parse(JSON.stringify(v)),TABLE_JSON_COPY_OPTIONS={};
const text=`class Probe{#submissions=new Map();#tasksById=new Map();#scope;#context;#host;calls=[];c;
constructor(c){this.c=c;this.#scope={conversationId:c.scope||undefined};this.#context={telemetry:{propagation:{inject:()=>{this.calls.push('inject');if(c.failure==='inject')throw Error();return {parent:'context'}}}}};this.#host={storage:{submission:async(id)=>{this.calls.push('submission:'+id);return ({20:{trace:{parent:'submission'}},21:{trace:{parent:'link'}}})[id]},mintId:async()=>{this.calls.push('mint');if(c.failure==='mint')throw Error();return 100}}};if(c.staged)this.#tasksById.set(10,{write:{record:{id:10,conversationId:2,state:{status:'pending'}}}})}
#write(fn){return fn()}#assertOpen(){}async #currentTask(id){const staged=this.#tasksById.get(id);if(staged)return staged.write.record;this.calls.push('task:'+id);return id===10?{id:10,conversationId:1,state:{status:'running'}}:undefined}async #requireConversation(id){this.calls.push('conversation:'+id);if(id!==1&&id!==2)throw Error()}
`+s.slice(a,b)+`async run(){const id=await this.createTask({definition:{name:'work\\u0000任务',version:2,initial:(input)=>{this.calls.push('initial');if(this.c.failure==='initial')throw Error();return {phase:'start',input}}}},this.c.input,this.c.options);return this.#tasksById.get(id).write.record}}`;
const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){const p=new Probe(c);try{out.push({ok:true,value:await p.run(),calls:p.calls})}catch{out.push({ok:false,calls:p.calls})}}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/transaction.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Tx(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('context',ctypes.c_void_p),('read',ctypes.c_void_p),('writes',ctypes.c_void_p),('busy',ctypes.c_uint),('sealed',ctypes.c_uint),('barrier',ctypes.c_uint),('depth',ctypes.c_uint)]
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    init_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p))
    mint_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64))
    inject_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p))
    class Services(ctypes.Structure):
        _fields_=[('ctx',ctypes.c_void_p),('initial',init_type),('mint',mint_type),('inject',inject_type),('scope',ctypes.c_uint64)]
    native.pg_record_tx_create_task.argtypes=[ctypes.POINTER(Tx),ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(Services),ctypes.POINTER(ctypes.c_uint64)]
    native.pg_record_tx_staged_tasks.argtypes=[ctypes.POINTER(Tx),ctypes.POINTER(ctypes.c_void_p)]
    native.pg_record_tx_discard.argtypes=[ctypes.POINTER(Tx)]
    def encode(v):return library.json_tokener_parse(json.dumps(v).encode())
    for case,want in zip(cases,expected,strict=True):
        calls=[]
        @read_type
        def read(ctx,table,id,out):
            assert tx.busy;calls.append(f'{("conversation","entry","task","submission")[table]}:{id}')
            value=({'id':id} if table==0 and id in [1,2] else {'id':10,'conversationId':1,'state':{'status':'running'}} if table==2 and id==10 else {20:{'trace':{'parent':'submission'}},21:{'trace':{'parent':'link'}}}.get(id) if table==3 else None)
            if value is None:return 1
            out[0]=encode(value);return 0
        @init_type
        def initial(ctx,input,out):
            assert tx.busy;calls.append('initial');out[0]=encode({'phase':'start','input':case['input']});return 7 if case['failure']=='initial' else 0
        @mint_type
        def mint(ctx,out):
            assert tx.busy;calls.append('mint');out[0]=100;return 7 if case['failure']=='mint' else 0
        @inject_type
        def inject(ctx,out):
            assert tx.busy;calls.append('inject');out[0]=encode({'parent':'context'});return 7 if case['failure']=='inject' else 0
        writes=[{'type':'task','value':{'id':10,'conversationId':2,'state':{'status':'pending'}}}] if case['staged'] else []
        tx=Tx(None,None,ctypes.cast(read,ctypes.c_void_p),encode(writes));services=Services(None,initial,mint,inject,case['scope']);definition=encode({'name':'work\u0000任务','version':2});input=encode(case['input']);options=encode(case['options']);id=ctypes.c_uint64();snapshot=ctypes.c_void_p()
        try:
            status=native.pg_record_tx_create_task(ctypes.byref(tx),definition,input,options,ctypes.byref(services),ctypes.byref(id));actual=dict(ok=status==0,calls=calls)
            if not status:
                assert id.value==100 and not native.pg_record_tx_staged_tasks(ctypes.byref(tx),ctypes.byref(snapshot));actual['value']=json.loads(library.json_object_to_json_string_ext(snapshot,0))[-1]
            else:assert not id.value and json.loads(library.json_object_to_json_string_ext(tx.writes,0))==writes
            assert actual==want and tx.barrier and not tx.busy,(case,status,actual,want)
        finally:
            for value in [definition,input,options,snapshot]:library.json_object_put(value)
            assert not native.pg_record_tx_discard(ctypes.byref(tx))
    print(f'PASS: {len(cases)} full createTask records, owner/conversation, callback order and failures match actual Pi method')

def storage_guard(native,library):
    cases=[dict(mint=mint,kind=kind,cancel=cancel,cross=cross,failed=failed) for mint in [False,True] for kind in ['ok','missing','request','error'] if not(mint and kind=='missing') for cancel in [False,True] for cross in [False,True] for failed in [False,True]]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';class SessionFailed extends Error{}class StorageRequestError extends Error{}const s=readFileSync(process.argv[1],'utf8');const GuardedStorage=eval('('+stripTypeScriptTypes(s.slice(s.indexOf('class GuardedStorage implements Storage')))+')');const out=[];
for(const c of JSON.parse(readFileSync(0,'utf8'))){let calls=0;const latch={failure:c.failed?{error:'prior'}:undefined,fail(e){this.failure??={error:'backend'}}};const run=async()=>{calls++;if(c.cross)latch.failure??={error:'crossed'};if(c.kind==='error')throw Error('backend');if(c.kind==='request')throw new StorageRequestError();return c.kind==='missing'?undefined:c.mint?100:{id:10}};const storage=new GuardedStorage({task:run,mintId:run},latch);const results=[];for(let n=0;n<2;n++){try{const value=await(c.mint?storage.mintId():storage.task(10,{abortSignal:{aborted:c.cancel}}));results.push(value===undefined?'missing':'ok')}catch(e){results.push(e instanceof SessionFailed?'failed':e instanceof StorageRequestError?'request':'error')}}out.push({results,calls,failure:latch.failure?.error??null})}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/session.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    read_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p));mint_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64));error_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_int);cancel_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p)
    class Guard(ctypes.Structure):
        _fields_=[('session',ctypes.c_void_p),('ctx',ctypes.c_void_p),('read',read_type),('mint',mint_type),('error',error_type),('cancel',cancel_type),('busy',ctypes.c_uint),('previous',ctypes.c_uint)]
    native.pg_durable_storage_read.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)];native.pg_durable_storage_mint.argtypes=[ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64)]
    for case,want in zip(cases,expected,strict=True):
        records=ctypes.create_string_buffer(88);session=ctypes.create_string_buffer(192);ctypes.c_void_p.from_buffer(session).value=ctypes.addressof(records);ctypes.c_uint.from_buffer(session,172).value=1;failed=ctypes.c_uint.from_buffer(records,52);failed.value=5 if case['failed'] else 0;calls=0
        def run():
            nonlocal calls
            calls+=1;assert guard.busy and ctypes.c_uint.from_buffer(session,168).value
            if case['cross'] and not failed.value:failed.value=4
            return {'ok':0,'missing':1,'request':8,'error':7}[case['kind']]
        @read_type
        def read(ctx,table,id,out):
            assert table==2 and id==10;out[0]=library.json_tokener_parse(b'{"id":10}');return run()
        @mint_type
        def mint(ctx,out):out[0]=100;return run()
        @error_type
        def error(ctx,status):return status==8
        @cancel_type
        def cancel(ctx):return case['cancel']
        guard=Guard(ctypes.addressof(session),None,read,mint,error,cancel,0,0);results=[]
        for n in range(2):
            value=ctypes.c_void_p();id=ctypes.c_uint64()
            if case['mint']:status=native.pg_durable_storage_mint(ctypes.byref(guard),ctypes.byref(id))
            else:status=native.pg_durable_storage_read(ctypes.byref(guard),2,10,ctypes.byref(value))
            results.append({0:'ok',1:'missing',8:'request',7:'error',64:'failed'}[status]);assert not guard.busy and not ctypes.c_uint.from_buffer(session,168).value
            if status:assert not value.value and not id.value
            elif case['mint']:assert id.value==100
            else:assert json.loads(library.json_object_to_json_string_ext(value,0))=={'id':10}
            library.json_object_put(value)
        actual=dict(results=results,calls=calls,failure={0:None,3:'backend',4:'crossed',5:'prior'}[failed.value]);assert actual==want,(case,actual,want)
    print(f'PASS: {len(cases)} read/mint failure, request-error/cancellation exemptions and crossed failure cases match actual Pi GuardedStorage')

def publications(native,library):
    cases=[]
    for shared in [False,True]:
        for writes in [[],[{'type':'entry','value':{'id':1}},{'type':'document.change','id':2}],[{'type':'task','value':{}},{'type':'conversation','value':{}},{'type':'submission','value':{}}],[{'type':'entry\0bad','value':{}},{'type':'unknown'}]]:
            for documents in [[],[{'type':'document','value':{'text':'中'},'ops':[]},{'type':'document.copy','source':{'id':1,'at':2}}]]:
                cases.append(dict(shared=shared,writes=writes,observed=documents,stored=documents if shared else [{'type':'document','value':None,'ops':[]}]))
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const s=readFileSync(process.argv[1],'utf8'),a=s.indexOf('\n\t#publish('),b=s.indexOf('// The commit is durable:',a),start=s.indexOf('const changes:',a);const body=s.slice(start,b);const Probe=eval('('+stripTypeScriptTypes('class Probe{#commitListeners=new Set([1]);#internalListeners=new Set();run(seq,writes,documents){const context={telemetry:{addEvent(){}}};'+body+'return {observed,stored,shared:observed===stored}}}')+')');const p=new Probe(),out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){out.push(p.run(7,c.writes,{observed:c.observed,stored:c.shared?c.observed:c.stored}))}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/session.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Request(ctypes.Structure):
        _fields_=[('writes',ctypes.c_void_p),('observed',ctypes.c_void_p),('stored',ctypes.c_void_p),('sequence',ctypes.c_uint64),('mode',ctypes.c_uint),('reserved',ctypes.c_uint)]
    class Output(ctypes.Structure):_fields_=[('observed',ctypes.c_void_p),('stored',ctypes.c_void_p)]
    native.pg_session_publications_prepare.argtypes=[ctypes.POINTER(Request),ctypes.POINTER(Output)];native.pg_session_publications_discard.argtypes=[ctypes.POINTER(Output)]
    for case,want in zip(cases,expected,strict=True):
        inputs=[library.json_tokener_parse(json.dumps(case[key]).encode()) for key in ['writes','observed','stored']];request=Request(inputs[0],inputs[1],inputs[1] if case['shared'] else inputs[2],7,0,0);out=Output()
        try:
            assert not native.pg_session_publications_prepare(ctypes.byref(request),ctypes.byref(out));actual={key:json.loads(library.json_object_to_json_string_ext(getattr(out,key),0)) for key in ['observed','stored']};actual['shared']=out.observed==out.stored;assert actual==want,(case,actual,want)
        finally:
            native.pg_session_publications_discard(ctypes.byref(out))
            for value in inputs:library.json_object_put(value)
    print(f'PASS: {len(cases)} observed/stored publication payloads and identity match actual Pi publish body')

def document_notices(native,library):
    cases=[]
    for conversation in [0,1,50]:
        for flags in [0,1,2,3,8,12]:
            for ops in [[],[['a',['text'],'tail']]]:
                cases.append(dict(conversation=conversation,flags=flags,ops=ops))
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(process.argv[1],'utf8'),a=source.indexOf('function describe('),b=source.indexOf('\n/**',a);const EMPTY_OPERATIONS=[];function publishes(plan){return plan.testPublish}const describe=eval('('+stripTypeScriptTypes(source.slice(a,b))+')');const out=[];for(const c of JSON.parse(readFileSync(0,'utf8'))){const record={id:10,kind:'doc',createdAt:2,...(c.flags&1?{retiredAt:7}:{})};const plan={conversationId:c.conversation||undefined,retire:!!(c.flags&1),content:c.flags&2?{type:'document.copy',source:{id:3,at:2}}:undefined,change:{loaded:c.flags&4?undefined:{},version:2,prepared:{value:{text:'new'},ops:c.ops}},testPublish:!!(c.flags&8)};out.push(describe(plan,record)??null)}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code,str(ROOT/'references/pi/packages/durable/src/session/transaction.ts')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    class Request(ctypes.Structure):
        _fields_=[('record',ctypes.c_void_p),('source',ctypes.c_void_p),('value',ctypes.c_void_p),('ops',ctypes.c_void_p),('conversation',ctypes.c_uint64),('version',ctypes.c_uint64),('flags',ctypes.c_uint),('reserved',ctypes.c_uint)]
    native.pg_document_publication_prepare.argtypes=[ctypes.POINTER(Request),ctypes.POINTER(ctypes.c_void_p)]
    for case,want in zip(cases,expected,strict=True):
        record=dict(id=10,kind='doc',createdAt=2,**({'retiredAt':7} if case['flags']&1 else {}));inputs=[library.json_tokener_parse(json.dumps(v).encode()) for v in [record,dict(id=3,at=2),dict(text='new'),case['ops']]];request=Request(*inputs,case['conversation'],2,case['flags'],0);out=ctypes.c_void_p()
        try:
            assert not native.pg_document_publication_prepare(ctypes.byref(request),ctypes.byref(out));actual=json.loads(library.json_object_to_json_string_ext(out,0)) if out.value else None;assert actual==want,(case,actual,want)
        finally:
            library.json_object_put(out)
            for value in inputs:library.json_object_put(value)
    print(f'PASS: {len(cases)} Document creation/update/migration/retirement/copy notices match actual Pi describe function')

def dispatch_notifications(native,library):
    commit='8b0caefc2260a28d2cec0a7ce3daef1d3de24776'
    source=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',commit+':packages/durable/src/session/session.ts'],text=True)
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(0,'utf8'),a=source.indexOf('\n\t#notifyHosts('),b=source.indexOf('\n\t/**',a);const SETTLED=Promise.resolve(),LISTENER={};function contained(fn,onError){try{fn()}catch(e){onError(e)}}const Probe=eval('('+stripTypeScriptTypes('class Probe{#commitListeners=new Set();report(e){throw e}add(fn){this.#commitListeners.add(fn)}remove(fn){this.#commitListeners.delete(fn)}publish(seq){this.#notifyHosts({seq},null)}'+source.slice(a,b)+'}')+')');const p=new Probe(),log=[];let first=true;const callbacks=[1,2,3,4].map(id=>pub=>{log.push(id*10+pub.seq);if(id===2&&pub.seq===1&&first){first=false;p.remove(callbacks[2]);p.add(callbacks[3]);p.publish(3)}});p.add(callbacks[0]);p.publish(1);p.publish(2);p.remove(callbacks[0]);p.add(callbacks[1]);p.add(callbacks[2]);await new Promise(resolve=>setImmediate(resolve));console.log(JSON.stringify(log));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code],input=source,text=True,capture_output=True,check=True).stdout)
    class Listeners(ctypes.Structure):
        _fields_=[('records',ctypes.c_void_p),('entries',ctypes.c_void_p),('count',ctypes.c_size_t),('capacity',ctypes.c_size_t),('ctx',ctypes.c_void_p),('report',ctypes.c_void_p),('busy',ctypes.c_uint),('closed',ctypes.c_uint)]
    class Queue(ctypes.Structure):
        _fields_=[('listeners',ctypes.POINTER(Listeners)),('head',ctypes.c_void_p),('tail',ctypes.c_void_p),('prepared',ctypes.c_size_t),('busy',ctypes.c_uint),('closed',ctypes.c_uint)]
    callback_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p)
    native.pg_session_listen.argtypes=[ctypes.POINTER(Listeners),ctypes.c_uint,callback_type,ctypes.c_void_p];native.pg_session_unlisten.argtypes=native.pg_session_listen.argtypes;native.pg_session_listeners_close.argtypes=[ctypes.POINTER(Listeners),ctypes.c_void_p]
    native.pg_session_dispatch_prepare.argtypes=[ctypes.POINTER(Queue),ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)];native.pg_session_dispatch_enqueue.argtypes=[ctypes.POINTER(Queue),ctypes.c_void_p];native.pg_session_dispatch_drain.argtypes=[ctypes.POINTER(Queue)];native.pg_session_dispatch_close.argtypes=[ctypes.POINTER(Queue)]
    records=ctypes.create_string_buffer(88);listeners=Listeners(ctypes.addressof(records));queue=Queue(ctypes.pointer(listeners));actual=[];nodes=[]
    @callback_type
    def callback(context,publication,delivery):
        id=int(context);seq=json.loads(library.json_object_to_json_string_ext(publication,0))['seq'];actual.append(id*10+seq)
        if id==2 and seq==1:
            native.pg_session_unlisten(ctypes.byref(listeners),1,callback,3);assert not native.pg_session_listen(ctypes.byref(listeners),1,callback,4);assert not native.pg_session_dispatch_enqueue(ctypes.byref(queue),nodes[2])
        return 0
    assert not native.pg_session_listen(ctypes.byref(listeners),1,callback,1)
    for seq in range(1,4):
        publication=library.json_tokener_parse(json.dumps(dict(seq=seq)).encode());node=ctypes.c_void_p();assert not native.pg_session_dispatch_prepare(ctypes.byref(queue),publication,None,ctypes.byref(node));nodes.append(node);library.json_object_put(publication)
    for node in nodes[:2]:assert not native.pg_session_dispatch_enqueue(ctypes.byref(queue),node)
    assert actual==[];native.pg_session_unlisten(ctypes.byref(listeners),1,callback,1)
    for id in [2,3]:assert not native.pg_session_listen(ctypes.byref(listeners),1,callback,id)
    assert not native.pg_session_dispatch_drain(ctypes.byref(queue)) and actual==expected,(actual,expected)
    assert not native.pg_session_dispatch_close(ctypes.byref(queue)) and not native.pg_session_listeners_close(ctypes.byref(listeners),None)
    print('PASS: native FIFO host dispatch and dispatch-time subscription mutations match actual Pi 8b0caef notifyHosts')

def adoption_source_order():
    commit='8b0caefc2260a28d2cec0a7ce3daef1d3de24776';source=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',commit+':packages/durable/src/session/session.ts'],text=True)
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(0,'utf8');function method(marker){const a=source.indexOf(marker),b=source.indexOf('\n\t/**',a);return source.slice(a,b)}const SETTLED=Promise.resolve(),NO_HOLDS=[],LISTENER={};const withoutAbortSignal=c=>c,addressId=()=>'';class ReadOnlySession extends Error{}function contained(fn,onError){try{fn()}catch(e){onError(e)}}const docs=[{type:'document',record:{id:2,createdAt:1},version:1,value:{text:'seedobserve'},ops:[]}];class Transaction{constructor(){}async settleSuccess(){return [{type:'document.change',id:2}]}async settleFailure(){}discard(){}adopt(){return {observed:docs,stored:docs}}takeHeld(){return []}}
const text=`class Probe{#readOnly=false;#host={};#holds=new Map();#failure;#storage;#internalListeners=new Set();#commitListeners=new Set();order=[];reports=0;constructor(mode){this.#internalListeners.add(()=>{this.order.push(2);if(mode===3||mode===4)throw Error('observer')});this.#internalListeners.add(()=>this.order.push(3));this.#commitListeners.add(()=>this.order.push(5));this.#storage={commit:async(writes,adopt)=>{if(mode===1){this.order.push(6);this.fail(Error('storage'));throw Error('storage')}this.order.push(1);adopt(3);this.order.push(4);await Promise.resolve();this.order.push(6);if(mode===2||mode===4){this.fail(Error('storage'));throw Error('storage')}return 3}}}#assertHealthy(){if(this.#failure)throw this.#failure.error}#hold(){}#sweepDocuments(){}#touchDocument(){}#release(){}#event(){}fail(error){this.#failure??={error}}report(){this.reports++}async run(){let ok;try{await this.#runCommit(()=> 'done',{telemetry:{addEvent(){}}},undefined,undefined);ok=true}catch{ok=false}return {ok,order:this.order,reports:this.reports,failure:this.#failure?.error.message??null}}
`+method('\n\tasync #runCommit<')+method('\n\t#publish(')+method('\n\t#notifyHosts(')+'}';const Probe=eval('('+stripTypeScriptTypes(text)+')');const out=[];for(let mode=0;mode<5;mode++)out.push(await new Probe(mode).run());console.log(JSON.stringify(out));
'''
    actual=json.loads(subprocess.run(['node','--input-type=module','-e',code],input=source,text=True,capture_output=True,check=True).stdout)
    expected=[dict(ok=mode in [0,3],order=[6] if mode==1 else [1,2,3,4,5,6],reports=int(mode==4),failure='storage' if mode in [1,2,4] else 'observer' if mode==3 else None) for mode in range(5)]
    assert actual==expected,(actual,expected)
    print('PASS: actual Pi 8b0caef runCommit/publish/notifyHosts adoption order and Storage-over-observer failure precedence match native trace matrix')

def close_source_order():
    commit='8b0caefc2260a28d2cec0a7ce3daef1d3de24776'
    source=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',commit+':packages/durable/src/session/session.ts'],text=True)
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(0,'utf8'),a=source.indexOf('\n\t#notifyClose():'),b=source.indexOf('\n\t/**',a);function contained(fn,onError){try{fn()}catch(e){onError(e)}}const Probe=eval('('+stripTypeScriptTypes(`class Probe{#documentSweep;#holdTimer;#closeListeners=new Set();log=[];constructor(){this.#documentSweep={disarm:()=>this.log.push('sweep')};this.#holdTimer={disarm:()=>this.log.push('timer')};const second=()=>this.log.push(2);this.#closeListeners.add(()=>{this.log.push(1);this.#closeListeners.delete(second);this.#notifyClose();throw Error('listener')});this.#closeListeners.add(second);this.#closeListeners.add(()=>this.log.push(3))}report(){this.log.push('report')}run(){this.#notifyClose();this.#notifyClose();return this.log}`+source.slice(a,b)+'}')+')');console.log(JSON.stringify(new Probe().run()));
'''
    actual=json.loads(subprocess.run(['node','--input-type=module','-e',code],input=source,text=True,capture_output=True,check=True).stdout)
    assert actual==['sweep','timer',1,'report',2,3],actual
    print('PASS: actual Pi 8b0caef notifyClose disarms before callbacks, snapshots once, contains failure and permits reentrant close notification')

def observation_source_state(native,library):
    commit='8b0caefc2260a28d2cec0a7ce3daef1d3de24776'
    source=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show',commit+':packages/durable/src/session/session.ts'],text=True)
    record=dict(id=2,kind='defer',scope=dict(kind='conversation',conversationId=1),history='latest',fork='initial',createdAt=1)
    def notice(id,value,ops,version=1,**extra):return dict(type='document',record={**record,'id':id,**extra},value=value,ops=ops,version=version)
    ops=[['s',['text'],'next']]
    changes=[notice(2,dict(text='next'),ops),notice(2,dict(text='ignored'),[]),notice(20,dict(text='other'),ops,kind='other'),notice(20,dict(text='other'),ops,key=''),notice(20,dict(text='other'),ops,scope=dict(kind='conversation',conversationId=9)),notice(20,None,[]),notice(2,None,[['s',['text'],'retiring']]),notice(2,None,[]),notice(20,dict(text='new'),[],2),notice(20,dict(text='v3'),[],3),notice(20,dict(text='last'),ops,3)]
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const {source,record,changes}=JSON.parse(readFileSync(0,'utf8'));const a=source.indexOf('\n\t#observeDocument<'),b=source.indexOf('\n\tasync #loadDocument',a),c=source.indexOf('function atAddress('),d=source.indexOf('\nfunction cancellationError',c);const NO_OPERATIONS=[];function checkRecordScope(){}function checkRecordVersion(){}const atAddress=eval('('+stripTypeScriptTypes(source.slice(c,d))+')');const Probe=eval('('+stripTypeScriptTypes(`class Probe{#failure;commit;end;#pinDocument(){return ()=>{}}observeCommits(fn){this.commit=fn;return ()=>{}}subscribeClose(fn){this.end=fn;return ()=>{}}report(e){throw e}open(record){const events=[];this.#observeDocument({}, {addressId:'address',record,storedVersion:1,valueVersion:1,tracker:{value:{text:'seed'}}},()=>({advance:x=>events.push(structuredClone(x)),end(){}}),()=>{});return events}`+source.slice(a,b)+'}')+')');const p=new Probe(),events=p.open(record),out=[];for(const change of changes){const before=events.length;p.commit({changes:[change]},null);out.push(events.length===before?null:events.at(-1))}console.log(JSON.stringify(out));
'''
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',code],input=json.dumps(dict(source=source,record=record,changes=changes)),text=True,capture_output=True,check=True).stdout)
    class Record(ctypes.Structure):_fields_=[('address',Address),('id',ctypes.c_uint64),('history',ctypes.c_uint),('fork',ctypes.c_uint)]
    class Definition(ctypes.Structure):_fields_=[('kind',Slice),('version',ctypes.c_uint64),('scope',ctypes.c_uint),('history',ctypes.c_uint),('fork',ctypes.c_uint),('family',ctypes.c_uint),('context',ctypes.c_void_p),('migrate',ctypes.c_void_p)]
    class Loaded(ctypes.Structure):_fields_=[('record_owner',ctypes.c_void_p),('value',ctypes.c_void_p),('definition',ctypes.POINTER(Definition)),('record',Record),('stored',ctypes.c_uint64),('version',ctypes.c_uint64),('deltas',ctypes.c_uint64),('busy',ctypes.c_uint),('active',ctypes.c_uint)]
    class State(ctypes.Structure):_fields_=[('record',ctypes.c_void_p),('value',ctypes.c_void_p),('id',ctypes.c_uint64),('version',ctypes.c_uint64),('active',ctypes.c_uint),('busy',ctypes.c_uint)]
    class Input(ctypes.Structure):_fields_=[('record',ctypes.POINTER(Record)),('value',ctypes.c_void_p),('ops',ctypes.c_void_p),('version',ctypes.c_uint64)]
    class Event(ctypes.Structure):_fields_=[('value',ctypes.c_void_p),('ops',ctypes.c_void_p),('retired',ctypes.c_uint),('replaced',ctypes.c_uint)]
    native.pg_document_record_decode.argtypes=[ctypes.c_void_p,ctypes.POINTER(Record)]
    native.pg_document_observation_open.argtypes=[ctypes.POINTER(State),ctypes.POINTER(Loaded)]
    native.pg_document_observation_advance.argtypes=[ctypes.POINTER(State),ctypes.POINTER(Input),ctypes.POINTER(Event)]
    native.pg_document_observation_close.argtypes=[ctypes.POINTER(State)]
    native.pg_document_observation_change_release.argtypes=[ctypes.POINTER(Event)]
    make=lambda x:library.json_tokener_parse(json.dumps(x).encode())
    r=make(record);v=make(dict(text='seed'));decoded=Record();assert not native.pg_document_record_decode(r,ctypes.byref(decoded))
    name=ctypes.create_string_buffer(b'defer');definition=Definition(Slice(ctypes.addressof(name),5),1,1,1,2,0,None,None);loaded=Loaded(r,v,ctypes.pointer(definition),decoded,1,1,0,0,1);state=State();assert not native.pg_document_observation_open(ctypes.byref(state),ctypes.byref(loaded));library.json_object_put(r);library.json_object_put(v)
    actual=[]
    try:
        for change in changes:
            r=make(change['record']);v=make(change['value']) if change['value'] is not None else None;ops=make(change['ops']);rec=Record();assert not native.pg_document_record_decode(r,ctypes.byref(rec));inp=Input(ctypes.pointer(rec),v,ops,change['version']);event=Event()
            try:
                status=native.pg_document_observation_advance(ctypes.byref(state),ctypes.byref(inp),ctypes.byref(event));assert status in (0,8),status
                actual.append(None if status else dict(value=json.loads(library.json_object_to_json_string_ext(event.value,0)),ops=json.loads(library.json_object_to_json_string_ext(event.ops,0)),retired=bool(event.retired)))
            finally:
                native.pg_document_observation_change_release(ctypes.byref(event));library.json_object_put(r);library.json_object_put(v);library.json_object_put(ops)
    finally:assert not native.pg_document_observation_close(ctypes.byref(state))
    assert actual==expected,(actual,expected)
    print(f'PASS: {len(changes)} native address-following state transitions match actual Pi 8b0caef observeDocument')

def observer_source_lifecycle():
    source=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show','8b0caefc2260a28d2cec0a7ce3daef1d3de24776:packages/durable/src/session/session.ts'],text=True)
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(0,'utf8');const a=source.indexOf('\n\t#observeDocument<'),b=source.indexOf('\n\tasync #loadDocument',a),c=source.indexOf('function atAddress('),d=source.indexOf('\nfunction cancellationError',c),e=source.indexOf('export function guardObserver'),f=source.indexOf('\n/**',e),g=source.indexOf('\n\t#notifyClose():'),h=source.indexOf('\n\t/**',g);function contained(fn,onError){try{fn()}catch(error){onError(error)}}function checkRecordScope(){}function checkRecordVersion(){}const NO_OPERATIONS=[];const atAddress=eval('('+stripTypeScriptTypes(source.slice(c,d))+')'),guardObserver=eval('('+stripTypeScriptTypes(source.slice(e,f).replace('export ',''))+')');const Probe=eval('('+stripTypeScriptTypes(`class Probe{#failure;#documentSweep;#holdTimer;commits=new Set();#closeListeners=new Set();pins=0;unpins=0;advances=0;ends=0;reports=0;failure=0;guard;record;mode;constructor(mode){this.mode=mode;this.record={id:2,kind:'defer',scope:mode===4?{kind:'task',taskId:100}:{kind:'conversation',conversationId:1}};const observer={advance:()=>{this.advances++;if(mode===1)this.guard.detach();if(mode===2)throw 7},end:f=>{this.ends++;this.failure=f?.error??0;if(mode===2)throw 9}};this.guard=this.#observeDocument({}, {addressId:'a',record:this.record,storedVersion:1,valueVersion:1,tracker:{value:{text:'seed'}}},(value,detach,report)=>guardObserver(observer,detach,report),o=>o.end(undefined))}#pinDocument(){this.pins++;let pinned=true;return ()=>{if(pinned){pinned=false;this.unpins++}}}observeCommits(fn){this.commits.add(fn);return ()=>this.commits.delete(fn)}subscribeClose(fn){this.#closeListeners.add(fn);return ()=>this.#closeListeners.delete(fn)}report(){this.reports++}deliver(changes){for(const fn of [...this.commits])fn({changes},null)}change(id=2,version=1,value={text:'notice'}){return {type:'document',record:{...this.record,id},value,ops:[['s',['text'],'notice']],version}}run(){if(this.mode===5)this.guard.detach();if(this.mode===3){this.#failure={error:3};this.#notifyClose()}this.deliver([...(this.mode===4?[{type:'task',value:{id:100,state:{status:'terminal'}}}]:[]),this.change(),this.change()]);if(this.mode===0){this.deliver([this.change(2,1,null),this.change(20,2)]);this.#notifyClose()}return {pins:this.pins,unpins:this.unpins,advances:this.advances,ends:this.ends,reports:this.reports,failure:this.failure}}`+source.slice(a,b)+source.slice(g,h)+'}')+')');console.log(JSON.stringify(Array.from({length:6},(_,mode)=>new Probe(mode).run())));
'''
    actual=json.loads(subprocess.run(['node','--input-type=module','-e',code],input=source,text=True,capture_output=True,check=True).stdout)
    expected=[dict(pins=2 if mode==0 else 1,unpins=2 if mode==0 else 1,advances=[4,1,1,0,2,0][mode],ends=int(mode in [0,2,3,4]),reports=2 if mode==2 else 0,failure=7 if mode==2 else 3 if mode==3 else 0) for mode in range(6)]
    assert actual==expected,(actual,expected)
    print('PASS: actual Pi observeDocument/guardObserver/notifyClose match native six-scenario lifecycle, pin and contained-host-error matrix')

def observer_pin_source():
    source=subprocess.check_output(['git','-C',str(ROOT/'references/pi'),'show','8b0caefc2260a28d2cec0a7ce3daef1d3de24776:packages/durable/src/session/session.ts'],text=True)
    code=r'''
import {readFileSync} from 'node:fs';import {stripTypeScriptTypes} from 'node:module';const source=readFileSync(0,'utf8'),a=source.indexOf('\n\t#pinDocument('),b=source.indexOf('\n\tasync #runCommit',a);const Probe=eval('('+stripTypeScriptTypes(`class Probe{#attachedDocuments=new Map();#idleDocuments=new Map([['a',true]]);#documents=new Map([['a',{record:{id:2}}]]);touches=0;#touchDocument(a){this.touches++;this.#idleDocuments.set(a,true)}run(){const first=this.#pinDocument('a',2),second=this.#pinDocument('a',2),out=[];const snap=()=>out.push([this.#attachedDocuments.get(2)??0,this.#idleDocuments.has('a'),this.touches]);snap();first();first();snap();second();snap();const old=this.#pinDocument('a',2);this.#documents.set('a',{record:{id:20}});old();snap();return out}`+source.slice(a,b)+'}')+')');console.log(JSON.stringify(new Probe().run()));
'''
    actual=json.loads(subprocess.run(['node','--input-type=module','-e',code],input=source,text=True,capture_output=True,check=True).stdout)
    assert actual==[[2,False,0],[1,False,0],[0,True,1],[0,False,1]],actual
    print('PASS: actual Pi pinDocument last-owner idleness, idempotent release and replaced-incarnation no-touch match native cache scenarios')

def main():
    for name in ('packages/durable/src/session/session.ts','packages/durable/src/session/transaction.ts','packages/durable/src/storage/memory.ts','packages/durable/src/storage/scan.ts','packages/durable/src/session/forks.ts','packages/durable/src/documents.ts','packages/durable/src/errors.ts','packages/durable/src/ids.ts','packages/chord/src/delta/index.ts'):
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
    fork_writes=[dict(type='conversation',value=dict(id=500000)),dict(type='conversation',value=dict(id=500001,parent=dict(conversationId=500000,at=500010))),dict(type='entry',value=dict(id=500010,conversationId=500000,kind='fork'))]
    for index in range(600):
        record_value=dict(id=500100+index,kind='fork.'+str(index),scope=dict(kind='conversation',conversationId=500000),history='rewindable',fork='initial' if index%7==0 else 'asOf')
        if index%5==0:record_value['key']='key\0中'
        fork_writes.append(dict(type='document.create',record=record_value,content=dict(kind='base',version=1,value=dict(index=index))))
    fork_writes.append(dict(type='document.create',record=dict(id=500900,kind='current.parent',scope=dict(kind='conversation',conversationId=500001),history='latest',fork='current'),content=dict(kind='base',version=1,value={})))
    operations.append(dict(op='commit',writes=fork_writes))
    for parent,entry in [(500001,500010),(500000,500010),(500001,999999),(999999,500010)]:operations.append(dict(op='forkplan',parent=parent,entry=entry,child=999000))
    operations.append(dict(op='commit',writes=[dict(type='document.retire',id=500101),dict(type='document.create',record=dict(id=501000,kind='fork.1',scope=dict(kind='conversation',conversationId=500000),history='latest',fork='current'),content=dict(kind='base',version=1,value={}))]))
    operations.append(dict(op='forkplan',parent=500000,entry=500010,child=999001))
    operations.append(dict(op='mint'))
    request_rng=random.Random(5701)
    for step in range(80):
        writes=[]
        for _ in range(request_rng.randint(1,4)):
            value=dict(id=request_rng.randrange(700000,700008),conversationId=request_rng.choice([1,2]),status='queued')
            key=request_rng.choice([None,'','r','s','k\0中'])
            if key is not None:value['requestId']=key
            writes.append(dict(type='submission',value=value))
        operations.append(dict(op='commit',writes=writes))
        for owner in [1,2]:
            for key in ['', 'r','s','k\0中']:operations.append(dict(op='request',conversation=owner,key=key))
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(operations),text=True,capture_output=True,check=True,timeout=30).stdout)
    with tempfile.TemporaryDirectory(prefix='pangu-record-memory-') as temp:
        path=Path(temp)/'memory.so';flags=subprocess.check_output(['pkg-config','--cflags','--libs','json-c','libutf8proc'],text=True).split()
        subprocess.run(['/usr/bin/clang-23','-shared','-fPIC','-O2','-fsanitize=undefined','-fno-sanitize-recover=all',str(ROOT/'src/runtime/documents.S'),*[str(ROOT/'src/runtime'/f'{name}.S') for name in ['document_reader','durable_session','hold_timer','holds','document_tx','pending']],str(ROOT/'src/runtime/record_memory.S'),str(ROOT/'src/runtime/records.S'),str(ROOT/'src/runtime/delta.S'),*flags,'-o',str(path)],check=True)
        native=ctypes.CDLL(str(path));library=ctypes.CDLL(ctypes.util.find_library('json-c'))
        library.json_tokener_parse.argtypes=[ctypes.c_char_p];library.json_tokener_parse.restype=ctypes.c_void_p
        library.json_object_put.argtypes=[ctypes.c_void_p]
        library.json_object_to_json_string_ext.argtypes=[ctypes.c_void_p,ctypes.c_int];library.json_object_to_json_string_ext.restype=ctypes.c_char_p
        submission_transitions(native,library)
        entry_construction(native,library)
        native.pg_record_memory_open.argtypes=[ctypes.POINTER(Memory)];native.pg_record_memory_close.argtypes=[ctypes.POINTER(Memory)]
        native.pg_record_memory_mint.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(ctypes.c_uint64)]
        class RequestQuery(ctypes.Structure):
            _fields_=[('conversation',ctypes.c_uint64),('request',ctypes.c_void_p),('size',ctypes.c_size_t)]
        native.pg_record_memory_submission_by_request.argtypes=[ctypes.POINTER(Memory),ctypes.POINTER(RequestQuery),ctypes.POINTER(ctypes.c_void_p)]
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
        entry_callback=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p),ctypes.POINTER(ctypes.c_uint64))
        scan_callback=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ForkQuery),ctypes.POINTER(ForkPage))
        mint_callback=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.POINTER(ctypes.c_uint64))
        @entry_callback
        def fork_entry(context,conversation,id,out,seq):return native.pg_record_memory_entry(ctypes.byref(memory),id,conversation,out,seq)
        @scan_callback
        def fork_scan(context,input,out):
            source=input.contents;query=DocumentQuery(1,0,source.conversation,Slice(),source.at,256,source.after,source.has_cursor,0);page=Page();status=native.pg_record_memory_scan_documents(ctypes.byref(memory),ctypes.byref(query),ctypes.byref(page));out[0]=ForkPage(page.items,page.after,page.has_next,0);return status
        @mint_callback
        def fork_mint(context,out):return native.pg_record_memory_mint(ctypes.byref(memory),out)
        fork_service=ForkStorage(None,ctypes.cast(fork_entry,ctypes.c_void_p),ctypes.cast(fork_scan,ctypes.c_void_p),ctypes.cast(fork_mint,ctypes.c_void_p))
        native.pg_records_prepare_fork.argtypes=[ctypes.POINTER(ForkStorage),ctypes.c_uint64,ctypes.c_uint64,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p)]
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
                elif op['op']=='request':
                    key=op['key'].encode();buffer=ctypes.create_string_buffer(key);query=RequestQuery(op['conversation'],ctypes.addressof(buffer),len(key));value=ctypes.c_void_p()
                    status=native.pg_record_memory_submission_by_request(ctypes.byref(memory),ctypes.byref(query),ctypes.byref(value))
                    try:
                        assert status in (0,1),(op,status)
                        actual=dict(ok=True,value=json.loads(library.json_object_to_json_string_ext(value,0)) if value.value else None)
                    finally:library.json_object_put(value)
                elif op['op']=='forkplan':
                    value=ctypes.c_void_p();status=native.pg_records_prepare_fork(ctypes.byref(fork_service),op['parent'],op['entry'],op['child'],ctypes.byref(value));actual=dict(ok=status==0)
                    try:
                        if not status:actual['value']=json.loads(library.json_object_to_json_string_ext(value,0))
                        else:assert not value.value
                    finally:library.json_object_put(value)
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
        owner_validation(native,library)
        loaded_access(native,library)
        historical_snapshots(native,library)
        typed_entries(native,library)
        submission_creation(native,library)
        candidate_snapshots(native,library)
        task_cache_order(native,library)
        task_replacement(native,library)
        task_options(native,library)
        task_creation(native,library)
        storage_guard(native,library)
        publications(native,library)
        document_notices(native,library)
        dispatch_notifications(native,library)
        adoption_source_order()
        close_source_order()
        observation_source_state(native,library)
        observer_source_lifecycle()
        observer_pin_source()
    print(f'PASS: {len(operations)} table/document batch/read/scan/entry/head/ID operations match actual fixed Pi MemoryStorage')
if __name__=='__main__':main()
