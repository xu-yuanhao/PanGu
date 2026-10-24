import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from assembly_first import inspect

class LanguageTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);(self.root/'governance').mkdir();(self.root/'src').mkdir()
        (self.root/'src/core.c').write_text('existing core\n')
        self.policy={'schema':'pangu.assembly-first.v1','priority':['assembly','c','cpp'],'sources':{'src/core.c':{'role':'pending-inplace-assembly','reason':'replace in-place','sha256':hashlib.sha256((self.root/'src/core.c').read_bytes()).hexdigest()}}}
    def check(self, sources=None):
        (self.root/'governance/assembly-first.json').write_text(json.dumps(self.policy))
        return inspect(self.root,sources)
    def test_existing_not_asm_completion(self):
        self.assertEqual(self.check(['src/core.c']),[])
    def test_expansion_and_wrong_inputs_rejected(self):
        self.assertTrue(self.check(['src/core.c','src/new.c']))
        (self.root/'src/core.c').write_text('expanded C core\n')
        self.assertTrue(self.check())
    def test_single_asm_replacement(self):
        (self.root/'src/core.c').unlink();(self.root/'src/core.S').write_text('.text\n')
        self.policy['sources']={'src/core.S':{'role':'assembly-core','reason':'native replacement'}}
        self.assertEqual(self.check(['src/core.S']),[])
        self.assertTrue(self.check(['src/core.c','src/core.S']))
    def test_unexplained_bridge_and_reversed_priority_rejected(self):
        self.policy['sources']['src/core.c']={'role':'c-bridge','reason':'','bridge_kind':'external-library'}
        self.assertTrue(self.check())
        self.policy['priority']=['c','assembly','cpp'];self.assertTrue(self.check())
    def test_builder_uses_one_explicit_implementation_without_c_fallback(self):
        import runpy
        builder=Path(__file__).resolve().parents[1]/'runtime/build_pangu.py'
        module=runpy.run_path(str(builder))
        resolve=module['implementation'];state=resolve.__globals__;original=state['SOURCES']
        try:
            state['SOURCES']=['src/core.S']
            self.assertTrue(resolve('src/core').endswith('src/core.S'))
            state['SOURCES']=['src/core.S','src/core.c']
            with self.assertRaises(ValueError):resolve('src/core')
            state['SOURCES']=[]
            with self.assertRaises(ValueError):resolve('src/core')
        finally:state['SOURCES']=original

    def test_cannot_label_c_as_assembly(self):
        self.policy['sources']['src/core.c']={'role':'assembly-core','reason':'pretend'}
        self.assertTrue(self.check())

if __name__=='__main__':unittest.main()
