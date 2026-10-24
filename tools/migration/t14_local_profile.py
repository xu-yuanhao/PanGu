"""Finite local static-owner experiment profile, not whole-stage or production acceptance."""
import json
import re
from pathlib import Path
import sys
import g04_bundle

GUARD='guard_owner_cases=256 guard_canary_checks=2048 guard_maps=8 guard_unmaps=8'
ABI='start_abi_calls=6 start_abi_zero_returns=2 start_abi_corruption_controls=3'
BUILDS=('async-setup-core-asm','async-setup-core-alternate','guard-fault-left','guard-fault-right')
EXPECTED={(profile,build,opt) for profile in ('normal','ubsan') for build in BUILDS for opt in ('debug','release')}
FIELDS={'profile','build','optimization','returncode','stdout','stderr','elf_sha256'}
def require(ok,message):
 if not ok:raise ValueError(message)
def unique(items):
 d={}
 for key,value in items:require(key not in d,'duplicate key');d[key]=value
 return d
def validate_main(data):
 require(type(data) is bytes and 0<len(data)<=1048576,'main output byte limit')
 text=data.decode('utf-8');lines=text.splitlines();require(text.endswith('\n') and len(lines)==378,'main output count')
 require(lines[-2:]==[GUARD,ABI],'guard/ABI summary mismatch')
 return g04_bundle.validate(('\n'.join(lines[:-2])+'\n').encode())
def validate(value):
 require(type(value) is dict and set(value)=={'schema','records'} and value['schema']=='pangu.t14.local-execution-matrix.v1','matrix schema')
 require(type(value['records']) is list and len(value['records'])==16,'matrix count');seen=set();ordinary=faults=0
 for row in value['records']:
  require(type(row) is dict and set(row)==FIELDS,'record fields')
  require(all(type(row[k]) is str for k in FIELDS-{'returncode'}) and type(row['returncode']) is int,'record types')
  key=(row['profile'],row['build'],row['optimization']);require(key in EXPECTED and key not in seen,'unknown/duplicate combination');seen.add(key)
  require(re.fullmatch('[0-9a-f]{64}',row['elf_sha256']) is not None,'ELF digest syntax');require(not row['stderr'],'nonempty stderr')
  if row['build'].startswith('guard-fault-'):
   side=row['build'].removeprefix('guard-fault-');require(row['returncode']==139 and row['stdout']==f'guard-{side}-ready\n','fault control mismatch');faults+=1
  else:
   require(row['returncode']==0,'ordinary execution failed');validate_main(row['stdout'].encode());ordinary+=1
 require(seen==EXPECTED,'missing combination')
 return {'status':'PASS_LOCAL_STATIC_OWNER_PROFILE','profile':'t14.local.static-owner.v1','ordinary_executions':ordinary,'expected_fault_executions':faults,'full_T14':'NOT_COMPLETED','production_admission':'NOT_GRANTED','binding_scope':'Semantic output matrix only; caller must separately verify source/controller/ELF receipts.'}
def parse(data):
 require(type(data) is bytes and 0<len(data)<=2097152,'matrix byte limit');return validate(json.loads(data.decode('utf-8'),object_pairs_hook=unique))
def main(argv):
 try:
  require(len(argv)==1,'usage: t14_local_profile.py MATRIX_JSON')
  with Path(argv[0]).open('rb') as stream:result=parse(stream.read(2097153))
 except (OSError,ValueError,UnicodeError,RecursionError) as error:
  print(json.dumps({'status':'BLOCKED','error':str(error)}));return 2
 print(json.dumps(result));return 0
if __name__=='__main__':sys.exit(main(sys.argv[1:]))
