import copy,json,unittest
from pathlib import Path
import t14_local_profile as checker
ROOT=Path(__file__).resolve().parents[2]
class LocalProfileTests(unittest.TestCase):
 def setUp(self):
  names=('async-trace','branch-trace','composition','interleave');traces=[json.loads((ROOT/f'docs/migration/t14-g04-{n}-fixture.json').read_text())['trace_jsonl'] for n in names]
  self.stdout='\n'.join(checker.g04_bundle.HEADERS)+'\n'+''.join(traces)+checker.GUARD+'\n'+checker.ABI+'\n'
  self.value={'schema':'pangu.t14.local-execution-matrix.v1','records':[dict(profile=p,build=b,optimization=o,returncode=139 if b.startswith('guard-fault-') else 0,stdout=f'guard-{b.removeprefix("guard-fault-")}-ready\n' if b.startswith('guard-fault-') else self.stdout,stderr='',elf_sha256='a'*64) for p,b,o in sorted(checker.EXPECTED)]}
 def reject(self,fn):
  bad=copy.deepcopy(self.value);fn(bad)
  with self.assertRaises(ValueError):checker.validate(bad)
 def test_complete(self):
  result=checker.validate(self.value);self.assertEqual(result['ordinary_executions'],8);self.assertEqual(result['expected_fault_executions'],8);self.assertEqual(result['production_admission'],'NOT_GRANTED')
 def test_every_missing_or_duplicate_combination(self):
  for i in range(16):
   self.reject(lambda b:b['records'].pop(i));self.reject(lambda b:b['records'].__setitem__(i,b['records'][(i+1)%16]))
 def test_failure_cannot_be_passing(self):
  for i in range(16):
   self.reject(lambda b:b['records'][i].__setitem__('returncode',0 if b['records'][i]['returncode'] else 139));self.reject(lambda b:b['records'][i].__setitem__('stderr','error'));self.reject(lambda b:b['records'][i].__setitem__('elf_sha256','wrong'))
 def test_no_summary_or_suite_omissions(self):
  for header in (checker.GUARD,checker.ABI,checker.g04_bundle.HEADERS[0]):
   with self.assertRaises(ValueError):checker.validate_main(self.stdout.replace(header+'\n','').encode())
  with self.assertRaises(ValueError):checker.validate_main(self.stdout.replace('start_abi_zero_returns=2','start_abi_zero_returns=0').encode())
 def test_fault_ready_marker_required(self):
  for i,row in enumerate(self.value['records']):
   if row['build'].startswith('guard-fault-'):self.reject(lambda b:b['records'][i].__setitem__('stdout',''))
 def test_schema_and_parser(self):
  for data in (b'',b'null',b'[]',b'{}',b'\xff',b' '*2097153):
   with self.assertRaises((ValueError,UnicodeError)):checker.parse(data)
  self.reject(lambda b:b['records'][0].__setitem__('returncode',True));self.reject(lambda b:b['records'][0].__setitem__('extra',0))
  data=json.dumps(self.value).encode();self.assertEqual(checker.parse(data)['status'],'PASS_LOCAL_STATIC_OWNER_PROFILE')
  with self.assertRaises(ValueError):checker.parse(data.replace(b'"schema":',b'"schema":"fake","schema":',1))
if __name__=='__main__':unittest.main()
