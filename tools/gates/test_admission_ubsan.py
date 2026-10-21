"""UBSan-only native admission matrix and scheduling contracts."""
import copy
import json
import shutil
import unittest
from unittest.mock import patch

import admission
from module_graph import Invalid
from receipt import canonical
import test_admission as fixture


class UbsanAdmissionTests(unittest.TestCase):
    setUp = fixture.AdmissionTests.setUp
    save = fixture.AdmissionTests.save
    invoke = fixture.AdmissionTests.invoke
    recheck = fixture.AdmissionTests.recheck

    def configure_ubsan_profile(self):
        from native_build import UBSAN_MODES, CPP_ASSEMBLER_MODES
        path = self.controller / "governance/native-probe-toolchain.json"
        value = json.loads(path.read_text())
        value.update(schema="pangu.native-ubsan-trap-toolchain.v3", modes=UBSAN_MODES, assembler_modes=CPP_ASSEMBLER_MODES)
        path.write_bytes(canonical(value))
        shutil.copyfile(path, self.candidate / "governance/native-probe-toolchain.json")

    def test_real_ubsan_test_composition_and_recheck(self):
        self.configure_ubsan_profile()
        result = self.invoke()
        self.assertEqual(result["native_matrix"], "PASS")
        self.assertEqual(self.recheck()["verified_builds"], 2)
        self.assertEqual(result["release"], "BLOCKED")

    def add_ubsan_compositions(self):
        self.configure_ubsan_profile()
        for name in ("second", "third"):
            item = copy.deepcopy(self.proposal["builds"][0])
            item["id"] = name
            self.proposal["builds"].append(item)
            for module in self.proposal["modules"]:
                module["compositions"].append(name)
        self.save()

    def test_ubsan_parallel_bound_order_and_all_retained_builds(self):
        import threading
        self.add_ubsan_compositions()
        actual_build = admission.build
        lock, barrier = threading.Lock(), threading.Barrier(2)
        active, peak, calls = 0, 0, 0

        def tracked(*args):
            nonlocal active, peak, calls
            with lock:
                active += 1
                peak = max(peak, active)
                calls += 1
                first_pair = calls <= 2
            try:
                if first_pair:
                    barrier.wait(timeout=10)
                return actual_build(*args)
            finally:
                with lock:
                    active -= 1

        with patch.object(admission, "build", side_effect=tracked):
            result = self.invoke()
        self.assertEqual(peak, 2)
        self.assertEqual(calls, 6)
        self.assertEqual([(r["id"], r["mode"]) for r in result["builds"]],
                         [(name, mode) for name in ("probe", "second", "third") for mode in ("debug", "release")])
        self.assertEqual(self.recheck()["verified_builds"], 6)

    def test_ubsan_worker_failure_cannot_publish_matrix_receipt(self):
        self.add_ubsan_compositions()
        actual_build = admission.build

        def fail_second(*args):
            if args[-1].name == "second-debug":
                raise Invalid("deliberate worker failure")
            return actual_build(*args)

        with patch.object(admission, "build", side_effect=fail_second):
            with self.assertRaisesRegex(Invalid, "deliberate worker failure"):
                self.invoke()
        self.assertFalse((self.base / "build/admission.json").exists())

    def test_ubsan_non_test_composition_rejected(self):
        self.configure_ubsan_profile()
        for item in self.proposal["modules"]:
            item["scope"] = "product"
        for scope in ("product", "development"):
            self.proposal["builds"][0]["scope"] = scope
            self.save()
            with self.subTest(scope=scope), self.assertRaisesRegex(Invalid, "restricted to test compositions"):
                self.invoke(output=self.base / scope)


if __name__ == "__main__":
    unittest.main()
