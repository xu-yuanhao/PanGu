import copy
import json
from pathlib import Path
import unittest
import g04_async_trace as checker

ROOT=Path(__file__).resolve().parents[2]
class AsyncTraceTests(unittest.TestCase):
 def setUp(self):
  self.data=json.loads((ROOT/'docs/migration/t14-g04-async-trace-fixture.json').read_text())['trace_jsonl'].encode()
  self.rows=[json.loads(x) for x in self.data.splitlines()]
 def encode(self,rows):return ('\n'.join(json.dumps(x) for x in rows)+'\n').encode()
 def test_actual_fixture(self):
  self.assertEqual(checker.validate(self.data),{'cases':3,'events':50,'status':'PASS'})
 def test_each_field_at_each_event(self):
  for i,row in enumerate(self.rows):
   for key,value in row.items():
    with self.subTest(event=i,field=key):
     bad=copy.deepcopy(self.rows);bad[i][key]=value+1 if type(value) is int else value+'-changed'
     with self.assertRaises(ValueError):checker.validate(self.encode(bad))
 def test_missing_or_extra_event(self):
  for i in range(len(self.rows)):
   with self.subTest(i=i):
    with self.assertRaises(ValueError):checker.validate(self.encode(self.rows[:i]+self.rows[i+1:]))
  with self.assertRaises(ValueError):checker.validate(self.encode(self.rows+[self.rows[-1]]))
 def test_swap_events(self):
  for i in range(len(self.rows)-1):
   bad=copy.deepcopy(self.rows);bad[i],bad[i+1]=bad[i+1],bad[i]
   with self.assertRaises(ValueError):checker.validate(self.encode(bad))
 def test_schema_and_parser(self):
  for payload in (b'',b'\xff',b'[]',b'null',b'{}',b'{}\n'*97,b' '*65537):
   with self.assertRaises((ValueError,UnicodeError)):checker.validate(payload)
  bad=copy.deepcopy(self.rows);bad[0]['case']=True
  with self.assertRaises(ValueError):checker.validate(self.encode(bad))
  bad=copy.deepcopy(self.rows);bad[0]['extra']=0
  with self.assertRaises(ValueError):checker.validate(self.encode(bad))
  bad=copy.deepcopy(self.rows);del bad[0]['state']
  with self.assertRaises(ValueError):checker.validate(self.encode(bad))
  with self.assertRaises(ValueError):checker.validate(self.data.replace(b'"case":1',b'"case":1,"case":1',1))
if __name__=='__main__':unittest.main()
