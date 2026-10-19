import copy
import hashlib
import json
from pathlib import Path
import unittest

from module_graph import Invalid
import test_link_graph
from toolchain import execute, validate_profile


class ToolchainTests(unittest.TestCase):
    def setUp(self):
        test_link_graph.LinkGraphTests.setUp(self)
        test_link_graph.LinkGraphTests.prepare(self)
        path = Path(__file__).resolve().parents[2] / "governance/c17-probe-toolchain.json"
        self.profile = json.loads(path.read_text())

    def run_probe(self, pin=None):
        if pin is None:
            pin = hashlib.sha256(json.dumps(self.profile, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
        return execute(self.root, self.manifest, self.probe["exports"], self.profile, pin)

    def test_pinned_compile_and_link(self):
        report = self.run_probe()
        self.assertEqual(report["toolchain_identity_audit"], "PASS")
        self.assertEqual(report["link_evidence"]["symbol_link_audit"], "PASS")
        self.assertEqual(report["status"], "BLOCKED")

    def test_changed_profile_pin(self):
        with self.assertRaisesRegex(Invalid, "profile pin mismatch"):
            self.run_probe("0" * 64)

    def test_flags_cannot_be_weakened(self):
        for flag in ("-w", "-Wno-error", "-include", "@response.txt"):
            with self.subTest(flag=flag):
                profile = copy.deepcopy(self.profile)
                profile["compile_flags"].append(flag)
                with self.assertRaisesRegex(Invalid, "unapproved probe flags"):
                    validate_profile(profile)

    def test_link_flags_cannot_load_plugin(self):
        self.profile["link_flags"].extend(["--plugin", "/tmp/plugin.so"])
        with self.assertRaisesRegex(Invalid, "unapproved probe flags"):
            self.run_probe()

    def test_unknown_configuration_rejected(self):
        self.profile["pre_build"] = "arbitrary script"
        with self.assertRaisesRegex(Invalid, "unknown or missing"):
            self.run_probe()

    def test_tool_binary_digest(self):
        self.profile["tools"]["compiler"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(Invalid, "compiler digest mismatch"):
            self.run_probe()

    def test_tool_version(self):
        self.profile["tools"]["nm"]["version"] = "wrong version"
        with self.assertRaisesRegex(Invalid, "nm version mismatch"):
            self.run_probe()

    def test_missing_tool(self):
        del self.profile["tools"]["linker"]
        with self.assertRaisesRegex(Invalid, "incomplete toolchain"):
            self.run_probe()

    def test_unapproved_target(self):
        self.profile["compiler_target"] = "aarch64-linux-gnu"
        with self.assertRaisesRegex(Invalid, "unsupported compiler target"):
            self.run_probe()

    def test_no_self_approval(self):
        self.profile["status"] = "APPROVED"
        with self.assertRaisesRegex(Invalid, "approval claim"):
            self.run_probe()


if __name__ == "__main__":
    unittest.main()
