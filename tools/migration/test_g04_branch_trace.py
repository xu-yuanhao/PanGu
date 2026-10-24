import copy
import json
from pathlib import Path
import unittest
import g04_branch_trace as checker
ROOT=Path(__file__).resolve().parents[2]
class BranchTraceTests(unittest.TestCase):
 def setUp(self):
  self.data=json.loads((ROOT/'docs/migration/t14-g04-branch-trace-fixture.json').read_text())['trace_jsonl'].encode();self.rows=[json.loads(x) for x in self.data.splitlines()]
 def encode(self,rows):return ('\n'.join(json.dumps(x) for x in rows)+'\n').encode()
 def reject(self,mutate):
  bad=copy.deepcopy(self.rows);mutate(bad)
  with self.assertRaises(ValueError):checker.validate(self.encode(bad))
 def test_valid(self):self.assertEqual(checker.validate(self.data),{'status':'PASS','cases':8,'events':228,'ticks':148})
 def test_sequence_and_budget_each_event(self):
  for i in range(len(self.rows)):
   with self.subTest(i=i):
    self.reject(lambda b:b[i].__setitem__('seq',999));self.reject(lambda b:b[i].__setitem__('work',b[i]['q']+1))
 def test_held_endpoint_reference_loss(self):
  for i,row in enumerate(self.rows):
   if row['event'] in ('held','tick-held','independent-drained'):
    for owner in (0,1):
     with self.subTest(i=i,owner=owner):self.reject(lambda b:b[i]['refs'].__setitem__(owner,0))
 def test_independent_branch_not_drained(self):
  for i,row in enumerate(self.rows):
   if row['event']=='independent-drained':
    self.reject(lambda b:b[i]['disposed'].__setitem__(2,0));self.reject(lambda b:b[i]['disposed'].__setitem__(0,1))
 def test_cleanup_callbacks_and_order(self):
  for i,row in enumerate(self.rows):
   if row['event']=='cleanup':
    self.reject(lambda b:b[i]['resources'].__setitem__(b[i]['owner'],1));self.reject(lambda b:b[i]['refs'].__setitem__(b[i]['owner'],1));self.reject(lambda b:b.pop(i));self.reject(lambda b:b[i].__setitem__('owner',3 if b[i]['owner']!=3 else 2))
 def test_final_errors_and_resources(self):
  for i,row in enumerate(self.rows):
   if row['event']=='end':
    self.reject(lambda b:b[i].__setitem__('rc',123));self.reject(lambda b:b[i]['resources'].__setitem__(0,1));self.reject(lambda b:b[i]['state'].__setitem__(0,2))
 def test_required_events(self):
  for i,row in enumerate(self.rows):
   if row['event'] in ('held','zero','independent-drained','finish','duplicate','end'):self.reject(lambda b:b.pop(i))
  self.reject(lambda b:b.append(b[-1]))
 def test_parser(self):
  for data in (b'',b'\xff',b'[]',b'null',b'{}',b' '*1048577):
   with self.assertRaises((ValueError,UnicodeError)):checker.validate(data)
  self.reject(lambda b:b[0].__setitem__('case',True));self.reject(lambda b:b[0].__setitem__('extra',0));self.reject(lambda b:b[0]['state'].__setitem__(0,True))
  with self.assertRaises(ValueError):checker.validate(self.data.replace(b'"case":1',b'"case":1,"case":1',1))
if __name__=='__main__':unittest.main()
