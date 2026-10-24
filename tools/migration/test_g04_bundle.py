import contextlib,io,json,subprocess,sys,tempfile,unittest
from pathlib import Path
import g04_bundle as checker
ROOT=Path(__file__).resolve().parents[2]
class BundleTests(unittest.TestCase):
 def setUp(self):
  names=('async-trace','branch-trace','composition','interleave');self.traces=[json.loads((ROOT/f'docs/migration/t14-g04-{n}-fixture.json').read_text())['trace_jsonl'] for n in names]
  self.data=('\n'.join(checker.HEADERS)+'\n'+''.join(self.traces)).encode()
 def test_complete(self):
  d=checker.validate(self.data);self.assertEqual((d['status'],d['suite_count'],d['records']),('PASS_LOCAL_BUNDLE',4,374));self.assertEqual(d['full_G04'],'NOT_COMPLETED')
 def test_missing_each_suite(self):
  for i in range(4):
   data=('\n'.join(checker.HEADERS)+'\n'+''.join(x for j,x in enumerate(self.traces) if j!=i)).encode()
   with self.assertRaises(ValueError):checker.validate(data)
 def test_reordered_or_duplicated_suites(self):
  for i in range(3):
   traces=list(self.traces);traces[i],traces[i+1]=traces[i+1],traces[i]
   with self.assertRaises(ValueError):checker.validate(('\n'.join(checker.HEADERS)+'\n'+''.join(traces)).encode())
  with self.assertRaises(ValueError):checker.validate(self.data+self.traces[0].encode())
 def test_every_summary_and_suite_mutation(self):
  for h in checker.HEADERS:
   with self.assertRaises(ValueError):checker.validate(self.data.replace(h.encode(),h.replace('=','=-',1).encode(),1))
  offset=2;lines=self.data.decode().splitlines()
  for _,count,_ in checker.SUITES:
   bad=list(lines);row=json.loads(bad[offset]);row['case']=999;bad[offset]=json.dumps(row)
   with self.assertRaises(ValueError):checker.validate(('\n'.join(bad)+'\n').encode())
   offset+=count
 def test_boundaries(self):
  for data in (b'',b'\xff',b' '*1048577,self.data[:-1],self.data+b'\n',b'log\n'+self.data):
   with self.assertRaises((ValueError,UnicodeError)):checker.validate(data)
 def test_cli(self):
  with tempfile.TemporaryDirectory() as td:
   path=Path(td)/'trace';path.write_bytes(self.data)
   good=subprocess.run([sys.executable,'-B',str(ROOT/'tools/migration/g04_bundle.py'),str(path)],capture_output=True,text=True);self.assertEqual(good.returncode,0);self.assertEqual(json.loads(good.stdout)['status'],'PASS_LOCAL_BUNDLE')
   path.write_bytes(b'bad');bad=subprocess.run([sys.executable,'-B',str(ROOT/'tools/migration/g04_bundle.py'),str(path)],capture_output=True,text=True);self.assertEqual(bad.returncode,2);self.assertEqual(json.loads(bad.stdout)['status'],'BLOCKED')
   path.unlink();missing=subprocess.run([sys.executable,'-B',str(ROOT/'tools/migration/g04_bundle.py'),str(path)],capture_output=True,text=True);self.assertEqual(missing.returncode,2)
if __name__=='__main__':unittest.main()
