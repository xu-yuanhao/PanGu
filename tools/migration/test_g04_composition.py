import copy
import json
from pathlib import Path
import unittest
import g04_composition as checker
ROOT=Path(__file__).resolve().parents[2]
class CompositionTests(unittest.TestCase):
 def setUp(self):
  self.data=json.loads((ROOT/'docs/migration/t14-g04-composition-fixture.json').read_text())['trace_jsonl'].encode();self.rows=[json.loads(x) for x in self.data.splitlines()]
 def encode(self,rows):return ('\n'.join(json.dumps(x) for x in rows)+'\n').encode()
 def reject(self,fn):
  rows=copy.deepcopy(self.rows);fn(rows)
  with self.assertRaises(ValueError):checker.validate(self.encode(rows))
 def test_actual_fixture(self):self.assertEqual(checker.validate(self.data),{'status':'PASS','cases':8,'snapshots':24})
 def test_all_exact_scalar_fields(self):
  for i in range(24):
   for k in ('case','q','many','rc','phase','started','setups','cleanups'):
    with self.subTest(i=i,k=k):self.reject(lambda b:b[i].__setitem__(k,b[i][k]+1))
 def test_every_owner_snapshot_field(self):
  for i in range(24):
   for k in ('state','refs','resources'):
    for owner in range(4):
     with self.subTest(i=i,k=k,owner=owner):self.reject(lambda b:b[i][k].__setitem__(owner,b[i][k][owner]+1))
 def test_budget_bounds(self):
  for i in range(24):
   self.reject(lambda b:b[i].__setitem__('max_work',b[i]['q']+1));self.reject(lambda b:b[i].__setitem__('work',b[i]['ticks']*b[i]['q']+1));self.reject(lambda b:b[i].__setitem__('ticks',-1))
 def test_partial_does_not_release_other_provider(self):
  for i,r in enumerate(self.rows):
   if r['event']=='partial':self.reject(lambda b:b[i]['state'].__setitem__(1,4));self.reject(lambda b:b[i]['refs'].__setitem__(1,0))
 def test_missing_extra_and_order(self):
  for i in range(24):self.reject(lambda b:b.pop(i))
  self.reject(lambda b:b.append(b[-1]));self.reject(lambda b:b.reverse())
 def test_parser(self):
  for data in (b'',b'[]',b'null',b'{}',b'\xff',b' '*65537):
   with self.assertRaises((ValueError,UnicodeError)):checker.validate(data)
  self.reject(lambda b:b[0].__setitem__('q',True));self.reject(lambda b:b[0]['state'].__setitem__(0,True));self.reject(lambda b:b[0].__setitem__('unexpected',0))
  with self.assertRaises(ValueError):checker.validate(self.data.replace(b'"case":1',b'"case":1,"case":1',1))
if __name__=='__main__':unittest.main()
