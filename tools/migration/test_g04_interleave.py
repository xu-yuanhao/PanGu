import copy,json,unittest
from pathlib import Path
import g04_interleave as checker
ROOT=Path(__file__).resolve().parents[2]
class InterleaveTests(unittest.TestCase):
 def setUp(self):
  self.data=json.loads((ROOT/'docs/migration/t14-g04-interleave-fixture.json').read_text())['trace_jsonl'].encode();self.rows=[json.loads(x) for x in self.data.splitlines()]
 def encode(self,rows):return ('\n'.join(json.dumps(x) for x in rows)+'\n').encode()
 def reject(self,fn):
  rows=copy.deepcopy(self.rows);fn(rows)
  with self.assertRaises(ValueError):checker.validate(self.encode(rows))
 def test_actual(self):self.assertEqual(checker.validate(self.data),{'status':'PASS','cases':12,'snapshots':72,'legal_completion_orders':3})
 def test_each_exact_scalar(self):
  for i in range(72):
   for k in ('case','q','order','rc','phase','started','error'):
    with self.subTest(i=i,k=k):self.reject(lambda b:b[i].__setitem__(k,b[i][k]+1))
 def test_every_owner_field(self):
  for i in range(72):
   for k in checker.ARRAYS:
    for j in range(4):
     with self.subTest(i=i,k=k,j=j):self.reject(lambda b:b[i][k].__setitem__(j,b[i][k][j]+1))
 def test_budget_and_final_counters(self):
  for i,r in enumerate(self.rows):
   self.reject(lambda b:b[i].__setitem__('max_work',b[i]['q']+1));self.reject(lambda b:b[i].__setitem__('steps',-1))
   if r['event']=='final':self.reject(lambda b:b[i].__setitem__('steps',b[i]['steps']+1))
 def test_schedule_missing_extra(self):
  for i in range(72):self.reject(lambda b:b.pop(i))
  self.reject(lambda b:b.reverse());self.reject(lambda b:b.append(b[-1]))
 def test_first_error_is_preserved(self):
  for i,r in enumerate(self.rows):
   if r['event']=='final':self.reject(lambda b:b[i].__setitem__('error',-11 if b[i]['error']==-33 else -33))
 def test_parser(self):
  for data in (b'',b'[]',b'null',b'{}',b'\xff',b' '*65537):
   with self.assertRaises((ValueError,UnicodeError)):checker.validate(data)
  self.reject(lambda b:b[0].__setitem__('q',True));self.reject(lambda b:b[0].__setitem__('extra',0));self.reject(lambda b:b[0]['state'].__setitem__(0,True))
  with self.assertRaises(ValueError):checker.validate(self.data.replace(b'"case":1',b'"case":1,"case":1',1))
if __name__=='__main__':unittest.main()
