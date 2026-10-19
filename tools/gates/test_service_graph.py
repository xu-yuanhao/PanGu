import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from module_graph import Invalid
from service_graph import resolve


def fixture():
    modules = {"schema": "pangu.modules.v1", "modules": [
        {"id": "api", "role": "interface", "scope": "product", "files": ["api/services.h"],
         "public_headers": ["api/services.h"], "dependencies": []}]}
    for name in ("provider-a", "provider-b", "loop"):
        modules["modules"].append({"id": name, "role": "plugin", "scope": "product",
                                    "files": [name + "/main.c"], "public_headers": [], "dependencies": ["api"]})
    composition = {"schema": "pangu.service-composition.v1",
                   "services": [{"id": "model", "version": 1, "definition_module": "api"}],
                   "plugins": [
                       {"module": name, "provides": [{"service": "model", "version": 1}], "requires": []}
                       for name in ("provider-a", "provider-b")],
                   "profile": {"id": "minimal", "scope": "product", "modules": ["provider-a", "loop"],
                               "bindings": {"model": "provider-a"}}}
    composition["plugins"].append({"module": "loop", "provides": [], "requires": [
        {"service": "model", "version": 1, "optional": False, "on_missing": "pending"}]})
    return modules, composition


class ServiceGraphTests(unittest.TestCase):
    def setUp(self):
        self.modules, self.composition = fixture()

    def run_graph(self):
        return resolve(self.modules, self.composition)

    def test_provider_before_consumer(self):
        report = self.run_graph()
        self.assertEqual(report["service_manifest_preflight"], "PASS")
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(report["planned_activation_order"], ["provider-a", "loop"])
        self.assertEqual(report["planned_disposal_order"], ["loop", "provider-a"])

    def test_swap_provider_without_changing_consumer_or_host(self):
        consumer = copy.deepcopy(self.composition["plugins"][2])
        self.composition["profile"]["modules"] = ["loop", "provider-b"]
        self.composition["profile"]["bindings"]["model"] = "provider-b"
        report = self.run_graph()
        self.assertEqual(report["planned_activation_order"], ["provider-b", "loop"])
        self.assertEqual(self.composition["plugins"][2], consumer)
        self.assertEqual(self.modules["modules"][3]["dependencies"], ["api"])

    def test_missing_required_pending(self):
        self.composition["profile"]["modules"] = ["loop"]
        self.composition["profile"]["bindings"] = {}
        report = self.run_graph()
        self.assertEqual(report["service_manifest_preflight"], "PENDING")
        self.assertEqual(report["required_missing"], [{"module": "loop", "service": "model"}])
        self.assertEqual(report["planned_activation_order"], [])

    def test_optional_absence_explicit(self):
        self.composition["profile"]["modules"] = ["loop"]
        self.composition["profile"]["bindings"] = {}
        requirement = self.composition["plugins"][2]["requires"][0]
        requirement.update(optional=True, on_missing="disable-feature")
        report = self.run_graph()
        self.assertEqual(report["service_manifest_preflight"], "PASS")
        self.assertEqual(report["optional_disabled"], [{"module": "loop", "service": "model"}])

    def test_multiple_providers_rejected(self):
        self.composition["profile"]["modules"].append("provider-b")
        with self.assertRaisesRegex(Invalid, "multiple providers"):
            self.run_graph()

    def test_inactive_provider_rejected(self):
        self.composition["profile"]["bindings"]["model"] = "provider-b"
        with self.assertRaisesRegex(Invalid, "does not offer"):
            self.run_graph()

    def test_binding_required(self):
        self.composition["profile"]["bindings"] = {}
        with self.assertRaisesRegex(Invalid, "cover exactly"):
            self.run_graph()

    def test_version_conflict(self):
        self.composition["plugins"][2]["requires"][0]["version"] = 2
        with self.assertRaisesRegex(Invalid, "version conflict"):
            self.run_graph()

    def test_boolean_version_rejected(self):
        self.composition["services"][0]["version"] = True
        with self.assertRaisesRegex(Invalid, "positive integer"):
            self.run_graph()

    def test_missing_interface_dependency(self):
        self.modules["modules"][3]["dependencies"] = []
        with self.assertRaisesRegex(Invalid, "interface dependency"):
            self.run_graph()

    def test_definition_cannot_be_provider(self):
        self.composition["services"][0]["definition_module"] = "provider-a"
        with self.assertRaisesRegex(Invalid, "interface module"):
            self.run_graph()

    def test_product_cannot_select_test_provider(self):
        self.modules["modules"][1]["scope"] = "test"
        with self.assertRaisesRegex(Invalid, "non-product plugin"):
            self.run_graph()

    def test_duplicate_plugin(self):
        self.composition["plugins"].append(copy.deepcopy(self.composition["plugins"][0]))
        with self.assertRaisesRegex(Invalid, "duplicate"):
            self.run_graph()

    def test_duplicate_service(self):
        self.composition["services"].append(copy.deepcopy(self.composition["services"][0]))
        with self.assertRaisesRegex(Invalid, "duplicate service"):
            self.run_graph()

    def test_unknown_service(self):
        self.composition["plugins"][2]["requires"][0]["service"] = "unknown"
        with self.assertRaisesRegex(Invalid, "unknown or duplicate service"):
            self.run_graph()

    def test_cycle(self):
        self.composition["services"].append({"id": "loop-state", "version": 1, "definition_module": "api"})
        self.composition["plugins"][2]["provides"] = [{"service": "loop-state", "version": 1}]
        self.composition["plugins"][0]["requires"] = [
            {"service": "loop-state", "version": 1, "optional": False, "on_missing": "pending"}]
        self.composition["profile"]["bindings"]["loop-state"] = "loop"
        with self.assertRaisesRegex(Invalid, "activation cycle"):
            self.run_graph()

    def test_missing_behavior_cannot_be_implicit(self):
        self.composition["plugins"][2]["requires"][0]["on_missing"] = "ignore"
        with self.assertRaisesRegex(Invalid, "missing dependency behavior"):
            self.run_graph()

    def test_unknown_fields(self):
        self.composition["approved"] = True
        with self.assertRaisesRegex(Invalid, "unknown or missing"):
            self.run_graph()

    def test_empty_composition_blocked(self):
        self.composition["profile"]["modules"] = []
        with self.assertRaisesRegex(Invalid, "empty service composition"):
            self.run_graph()

    def test_profile_change_invalidates_composition_digest(self):
        before = self.run_graph()
        self.composition["profile"]["id"] = "another-profile"
        after = self.run_graph()
        self.assertNotEqual(before["composition_sha256"], after["composition_sha256"])
        self.assertEqual(before["modules_sha256"], after["modules_sha256"])

    def test_cli_never_returns_release_success(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "modules.json"
            composition = root / "composition.json"
            manifest.write_text(json.dumps(self.modules))
            composition.write_text(json.dumps(self.composition))
            command = [sys.executable, "-B", str(Path(__file__).with_name("service_graph.py")),
                       "--manifest", str(manifest), "--composition", str(composition)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(json.loads(result.stdout)["status"], "BLOCKED")
            composition.write_text('{"schema": "unknown"}')
            result = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 1)


if __name__ == "__main__":
    unittest.main()
