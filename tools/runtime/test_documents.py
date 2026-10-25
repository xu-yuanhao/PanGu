"""Compare native address tuples with fixed Pi Durable resolveAddress/addressId."""
import ctypes
import ctypes.util
import itertools
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
COMMIT = 'd5b3de22d0e24a3e9c38a9410d3329b099717283'
ORACLE = r'''
import {registerHooks} from 'node:module';
import {pathToFileURL} from 'node:url';
import {readFileSync} from 'node:fs';
const base=pathToFileURL(process.argv[1]+'/');
registerHooks({resolve(specifier,context,next){
 if(specifier==='@earendil-works/chord')return {url:new URL('packages/chord/src/json.ts',base).href,shortCircuit:true};
 return next(specifier,context);
}});
const {resolveAddress,addressId,materializeDocumentValue}=await import(new URL('packages/durable/src/documents.ts',base));
const {isJsonValue}=await import(new URL('packages/chord/src/json.ts',base));
const cases=JSON.parse(readFileSync(0,'utf8'));
console.log(JSON.stringify(cases.map(c=>{
 if(c.valueCheck){
  const value={item:1};
  if(c.shape==='nan')value.item=NaN;
  if(c.shape==='infinity')value.item=Infinity;
  if(c.shape==='negative-infinity')value.item=-Infinity;
  if(c.shape==='cycle'){value.child={parent:value};}
  if(c.shape==='dag'){const child={};value.a=child;value.b=child;}
  return isJsonValue(value);
 }
 if(c.definition){
  const d={...c.definition};
  if(c.migrate)d.migrate=(value,from)=>({...value,migrated:from});
  try{return {status:0,value:materializeDocumentValue(d,c.record,c.version,c.value)};}
  catch(error){return {status:error.message.includes("semantics")?4:error.message.includes("newer version")?5:error.message.includes("requires migration")?6:7};}
 }
 const family=Object.hasOwn(c,'key');
 const args=c.scope==='session'?[]:[c.owner];
 if(family)args.push(c.key);
 const result=resolveAddress({kind:c.kind,scope:c.scope,family},args);
 if(result.id!==addressId(result.address))throw Error('inconsistent address');
 return result.id;
})));
'''

class Slice(ctypes.Structure):
    _fields_ = [('data', ctypes.c_void_p), ('size', ctypes.c_size_t)]
class Address(ctypes.Structure):
    _fields_ = [('kind', Slice), ('key', Slice), ('owner', ctypes.c_int64),
                ('scope', ctypes.c_uint), ('has_key', ctypes.c_uint)]

class Definition(ctypes.Structure):
    _fields_ = [('kind', Slice), ('version', ctypes.c_uint64), ('scope', ctypes.c_uint),
                ('history', ctypes.c_uint), ('fork', ctypes.c_uint), ('family', ctypes.c_uint),
                ('context', ctypes.c_void_p), ('migrate', ctypes.c_void_p)]
class Record(ctypes.Structure):
    _fields_ = [('address', Address), ('id', ctypes.c_uint64), ('history', ctypes.c_uint), ('fork', ctypes.c_uint)]

def materialization(native):
    cases=[]
    scopes=('session','conversation','task')
    for scope in scopes:
        for history, fork in ([('latest','current'),('latest','initial'),('rewindable','asOf')] if scope=='conversation' else [(None,None)]):
            for stored_scope, version, migrate in itertools.product(scopes,[1,2,3],[False,True]):
                definition=dict(kind='counter',version=2,scope=scope)
                record=dict(id=1,kind='counter',scope=dict(kind=stored_scope))
                if scope=='conversation': definition.update(history=history,fork=fork)
                if stored_scope=='conversation': record.update(history=history or 'latest',fork=fork or 'current')
                cases.append(dict(definition=definition,record=record,version=version,migrate=migrate,value={'count':1,'nested':[1,{'x':True}]}))
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    library=ctypes.CDLL(ctypes.util.find_library('json-c'))
    library.json_tokener_parse.argtypes=[ctypes.c_char_p];library.json_tokener_parse.restype=ctypes.c_void_p
    library.json_object_get.argtypes=[ctypes.c_void_p];library.json_object_get.restype=ctypes.c_void_p
    library.json_object_put.argtypes=[ctypes.c_void_p]
    library.json_object_new_int64.argtypes=[ctypes.c_int64];library.json_object_new_int64.restype=ctypes.c_void_p
    library.json_object_object_add.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_void_p]
    library.json_object_to_json_string_ext.argtypes=[ctypes.c_void_p,ctypes.c_int];library.json_object_to_json_string_ext.restype=ctypes.c_char_p
    callback_type=ctypes.CFUNCTYPE(ctypes.c_int,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint64,ctypes.POINTER(ctypes.c_void_p))
    @callback_type
    def migrate(context,value,version,out):
        library.json_object_object_add(value,b'migrated',library.json_object_new_int64(version))
        out[0]=library.json_object_get(value)
        return 0
    native.pg_document_materialize.argtypes=[ctypes.POINTER(Definition),ctypes.POINTER(Record),ctypes.c_uint64,ctypes.c_void_p,ctypes.POINTER(ctypes.c_void_p)]
    native.pg_document_materialize.restype=ctypes.c_int
    for index,(case,answer) in enumerate(zip(cases,expected,strict=True)):
        d,r=case['definition'],case['record'];kind=ctypes.create_string_buffer(b'counter');view=Slice(ctypes.addressof(kind),7)
        hist=lambda value: {None:0,'latest':1,'rewindable':2}[value]
        fork=lambda value: {None:0,'current':1,'initial':2,'asOf':3}[value]
        definition=Definition(view,2,scopes.index(d['scope']),hist(d.get('history')),fork(d.get('fork')),0,None,ctypes.cast(migrate,ctypes.c_void_p).value if case['migrate'] else None)
        record=Record(Address(view,Slice(),0 if r['scope']['kind']=='session' else 1,scopes.index(r['scope']['kind']),0),1,hist(r.get('history')),fork(r.get('fork')))
        value=library.json_tokener_parse(json.dumps(case['value']).encode());out=ctypes.c_void_p()
        try:
            result=native.pg_document_materialize(ctypes.byref(definition),ctypes.byref(record),case['version'],value,ctypes.byref(out))
            actual={'status':result}
            if result==0: actual['value']=json.loads(library.json_object_to_json_string_ext(out,0))
            else: assert not out.value
            assert actual==answer,(index,actual,answer)
            assert json.loads(library.json_object_to_json_string_ext(value,0))==case['value']
        finally:
            library.json_object_put(out);library.json_object_put(value)
    print(f'PASS: {len(cases)} document scope/version/migration cases match actual fixed Pi functions')

def strict_values(native):
    shapes=['normal','nan','infinity','negative-infinity','cycle','dag']
    cases=[dict(valueCheck=True,shape=name) for name in shapes]
    expected=json.loads(subprocess.run(['node','--input-type=module','-e',ORACLE,str(ROOT/'references/pi')],input=json.dumps(cases),text=True,capture_output=True,check=True).stdout)
    library=ctypes.CDLL(ctypes.util.find_library('json-c'))
    library.json_tokener_parse.argtypes=[ctypes.c_char_p];library.json_tokener_parse.restype=ctypes.c_void_p
    library.json_object_get.argtypes=[ctypes.c_void_p];library.json_object_get.restype=ctypes.c_void_p
    library.json_object_put.argtypes=[ctypes.c_void_p]
    library.json_object_new_double.argtypes=[ctypes.c_double];library.json_object_new_double.restype=ctypes.c_void_p
    library.json_object_object_add.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_void_p]
    library.json_object_object_del.argtypes=[ctypes.c_void_p,ctypes.c_char_p]
    native.pg_document_value_valid.argtypes=[ctypes.c_void_p];native.pg_document_value_valid.restype=ctypes.c_int
    for shape,answer in zip(shapes,expected,strict=True):
        root=library.json_tokener_parse(b'{"item":1}');child=None
        try:
            if shape in ('nan','infinity','negative-infinity'):
                number={'nan':float('nan'),'infinity':float('inf'),'negative-infinity':-float('inf')}[shape]
                assert library.json_object_object_add(root,b'item',library.json_object_new_double(number))==0
            if shape in ('cycle','dag'):
                child=library.json_tokener_parse(b'{}')
                assert library.json_object_object_add(root,b'a',library.json_object_get(child))==0
                if shape=='cycle': assert library.json_object_object_add(child,b'parent',library.json_object_get(root))==0
                else: assert library.json_object_object_add(root,b'b',library.json_object_get(child))==0
            assert bool(native.pg_document_value_valid(root))==answer,shape
        finally:
            if child:
                if shape=='cycle':library.json_object_object_del(child,b'parent')
                library.json_object_put(child)
            library.json_object_put(root)
    print('PASS: finite/nonfinite, cyclic and shared JSON graphs match actual Chord isJsonValue')

def main():
    for name in ('packages/durable/src/documents.ts', 'packages/durable/src/ids.ts', 'packages/chord/src/json.ts'):
        frozen = subprocess.check_output(['git', '-C', str(ROOT / 'references/pi'), 'show', COMMIT + ':' + name])
        assert (ROOT / 'references/pi' / name).read_bytes() == frozen, name + ' reference drift'
    cases = []
    strings = ['', 'counter', 'a/b', 'a\x00b', '\n\r\t\b\f"\\', '盘古😀é', '\u2028\u2029', 'x' * 4096]
    for kind, scope, key in itertools.product(strings, ('session', 'conversation', 'task'), [None, *strings]):
        for owner in ([0] if scope == 'session' else [0, 1, -1, 9007199254740991, -9007199254740991]):
            case = dict(kind=kind, scope=scope, owner=owner)
            if key is not None:
                case['key'] = key
            cases.append(case)
    oracle = subprocess.run(['node', '--input-type=module', '-e', ORACLE, str(ROOT / 'references/pi')],
                            input=json.dumps(cases, ensure_ascii=False), text=True,
                            capture_output=True, check=True, timeout=30)
    expected = json.loads(oracle.stdout)
    with tempfile.TemporaryDirectory(prefix='pangu-documents-') as temp:
        library = Path(temp) / 'documents.so'
        flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
        subprocess.run(['/usr/bin/clang-23', '-shared', '-fPIC', '-O2', '-fsanitize=undefined',
                        '-fno-sanitize-recover=all', str(ROOT / 'src/runtime/documents.S'), *flags, '-o', str(library)], check=True)
        native = ctypes.CDLL(str(library))
        native.pg_document_address_encode.argtypes = [ctypes.POINTER(Address), ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        native.pg_document_address_encode.restype = ctypes.c_int
        for index, (case, answer) in enumerate(zip(cases, expected, strict=True)):
            kind, key = case['kind'].encode(), case.get('key', '').encode()
            kind_buffer, key_buffer = ctypes.create_string_buffer(kind), ctypes.create_string_buffer(key)
            address = Address(Slice(ctypes.addressof(kind_buffer), len(kind)), Slice(ctypes.addressof(key_buffer), len(key)),
                              case['owner'], ('session', 'conversation', 'task').index(case['scope']), 'key' in case)
            answer = answer.encode()
            out = ctypes.create_string_buffer(len(answer) + 1)
            size = ctypes.c_size_t()
            result = native.pg_document_address_encode(ctypes.byref(address), out, len(out), ctypes.byref(size))
            assert result == 0 and size.value == len(answer) and out.raw == answer + b'\0', index
        materialization(native)
        strict_values(native)
    print(f'PASS: {len(cases)} document addresses match actual fixed Pi Durable functions byte-for-byte')

if __name__ == '__main__':
    main()
