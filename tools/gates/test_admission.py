import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import admission
from admission import execute, registry
from module_graph import Invalid
from receipt import canonical, sha, snapshot
from toolchain_probe import ASSEMBLY


def module(name, role, files, languages, exports):
    return {"id": name, "role": role, "scope": "test", "files": files, "public_headers": [], "dependencies": [],
            "source_root": "src/" + name, "responsibility": "fixed native gate fixture",
            "non_responsibilities": ["not a production agent"],
            "boundary": {"reason": "independent-lifecycle", "evidence": ["README.md"]},
            "languages": languages, "exports": exports, "targets": ["linux-x86_64"], "linkage": "static",
            "compositions": ["probe"], "activation_condition": "profile-selected",
            "lifecycle": {"activate": None, "dispose": None, "state_owner": "caller", "threading": "single-thread",
                          "callbacks": "none", "resource_contract": "fixture owns no persistent resources"},
            "provides": [], "requires": [], "third_party_dependencies": [], "required_modules": [],
            "budgets": {"object_bytes": {"debug": None, "release": None}, "measurement_plan": "fixture only; production budget remains unbaselined"}}


class AdmissionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.controller = self.base / "controller"
        self.candidate = self.base / "candidate"
        (self.controller / "governance").mkdir(parents=True)
        (self.controller / "tools/gates").mkdir(parents=True)
        (self.controller / "README.md").write_text("test-only architecture boundary\n")
        (self.controller / "tools/gates/check.py").write_text("raise RuntimeError('must never execute candidate scripts')\n")
        toolchain = Path(__file__).resolve().parents[2] / "governance/native-probe-toolchain.json"
        shutil.copy2(toolchain, self.controller / "governance/native-probe-toolchain.json")
        self.policy = {"schema_version": 1, "phase": "design-only", "excluded_roots": [".git", "references"], "files": {
            "README.md": "document", "governance/bootstrap-policy.json": "control", "governance/native-admission.json": "control",
            "governance/native-probe-toolchain.json": "control", "tools/gates/check.py": "gate"}}
        (self.controller / "governance/bootstrap-policy.json").write_bytes(canonical(self.policy))
        entry = module("entry", "platform", ["src/entry/start.S"], ["gas"], ["probe_start"])
        entry["dependencies"] = ["answer"]
        entry["required_modules"] = ["answer"]
        answer = module("answer", "foundation", ["src/answer/answer.c"], ["c17"], ["answer"])
        self.proposal = {"schema": "pangu.native-admission.v1", "phase": "CANDIDATE_NOT_APPROVED", "target": "linux-x86_64",
                         "toolchain": "governance/native-probe-toolchain.json", "modules": [entry, answer], "service_definitions": [],
                         "builds": [{"id": "probe", "scope": "test", "modules": ["entry", "answer"], "modes": ["debug", "release"],
                                     "entry": {"module": "entry", "symbol": "probe_start"}, "bindings": {},
                                     "budgets": {"artifact_bytes": {"debug": None, "release": None}, "measurement_plan": "fixture measurement only"}}]}
        self.save()
        shutil.copytree(self.controller, self.candidate)
        (self.candidate / "src/entry").mkdir(parents=True)
        (self.candidate / "src/answer").mkdir(parents=True)
        (self.candidate / "src/entry/start.S").write_text(ASSEMBLY)
        (self.candidate / "src/answer/answer.c").write_text("int answer(void) { return 7; }\n")

    def save(self):
        path = "governance/native-admission.json"
        (self.controller / path).write_bytes(canonical(self.proposal))
        if self.candidate.exists():
            shutil.copy2(self.controller / path, self.candidate / path)

    def invoke(self, controller_pin=None, output=None):
        policy_pin = sha((self.controller / "governance/bootstrap-policy.json").read_bytes())
        if controller_pin is None:
            controller_pin = sha(canonical(snapshot(self.controller, self.policy)))
        return execute(self.candidate, self.controller, policy_pin, controller_pin,
                       "governance/native-admission.json", output or self.base / "build")

    def test_real_matrix_and_pinned_candidate_controls(self):
        report = self.invoke()
        self.assertEqual(report["native_matrix"], "PASS")
        self.assertEqual([r["mode"] for r in report["builds"]], ["debug", "release"])
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual(report["release"], "BLOCKED")
        self.assertTrue(report["unbaselined_budgets"])
        self.assertTrue((self.base / "build/probe-debug/probe.elf").is_file())
        self.assertTrue((self.base / "build/probe-release/probe.elf").is_file())
        self.assertEqual(report["service_preflight"]["probe"]["service_manifest_preflight"], "PASS")
        self.assertEqual(self.recheck()["verified_builds"], 2)

    def recheck(self, data=None):
        path = self.base / "build/admission.json"
        if data is not None:
            path.write_bytes(canonical(data))
        return admission.verify(self.candidate, self.controller, sha((self.controller / "governance/bootstrap-policy.json").read_bytes()),
                                sha(canonical(snapshot(self.controller, self.policy))), "governance/native-admission.json",
                                self.base / "build", sha(path.read_bytes()))

    def test_matrix_recheck_rejects_missing_modes_forged_measurements_and_unbaselined_omission(self):
        report = self.invoke()
        for attack in ("coverage", "measurements", "budgets", "approval"):
            data = copy.deepcopy(report)
            if attack == "coverage":
                data["builds"].pop()
            elif attack == "measurements":
                data["builds"][0]["artifact_bytes"] = 1
            elif attack == "budgets":
                data["unbaselined_budgets"] = []
            else:
                data["release"] = "PASS"
            with self.subTest(attack=attack), self.assertRaises(Invalid):
                self.recheck(data)

    def test_matrix_recheck_rejects_source_and_binary_changes(self):
        self.invoke()
        source = self.candidate / "src/answer/answer.c"
        saved = source.read_bytes()
        source.write_text("int answer(void) { return 9; }\n")
        with self.assertRaisesRegex(Invalid, "source changed"):
            self.recheck()
        source.write_bytes(saved)
        executable = self.base / "build/probe-release/probe.elf"
        executable.write_bytes(executable.read_bytes() + b"tampered")
        with self.assertRaisesRegex(Invalid, "artifact changed"):
            self.recheck()

    def add_service(self):
        api = module("api", "interface", ["src/api/value.h"], ["c-header"], [])
        api["public_headers"] = ["src/api/value.h"]
        api["activation_condition"] = "definition-only"
        self.proposal["modules"].append(api)
        self.proposal["builds"][0]["modules"].append("api")
        self.proposal["service_definitions"] = [{"id": "value", "version": 1, "definition_module": "api"}]
        self.proposal["modules"][0]["requires"] = [{"service": "value", "version": 1, "optional": False, "on_missing": "pending"}]
        self.proposal["modules"][1]["provides"] = [{"service": "value", "version": 1}]
        for m in self.proposal["modules"][:2]:
            m["dependencies"].append("api")
            m["required_modules"].append("api")
        self.proposal["builds"][0]["bindings"] = {"value": "answer"}
        (self.candidate / "src/api").mkdir()
        (self.candidate / "src/api/value.h").write_text('#ifndef __ASSEMBLER__\nint answer(void);\n#endif\n')
        (self.candidate / "src/entry/start.S").write_text('#include "src/api/value.h"\n' + ASSEMBLY)
        (self.candidate / "src/answer/answer.c").write_text('#include "src/api/value.h"\nint answer(void) { return 7; }\n')

    def test_two_provider_builds_keep_consumer_unchanged(self):
        self.add_service()
        alternate = module("alternate", "foundation", ["src/alternate/answer.c"], ["c17"], ["answer"])
        alternate["dependencies"] = alternate["required_modules"] = ["api"]
        alternate["compositions"] = ["alternate-probe"]
        alternate["provides"] = [{"service": "value", "version": 1}]
        self.proposal["modules"].append(alternate)
        entry, api = self.proposal["modules"][0], self.proposal["modules"][2]
        entry["dependencies"].append("alternate")
        entry["required_modules"].remove("answer")
        entry["compositions"].append("alternate-probe")
        api["compositions"].append("alternate-probe")
        target = copy.deepcopy(self.proposal["builds"][0])
        target.update(id="alternate-probe", modules=["entry", "alternate", "api"], bindings={"value": "alternate"})
        self.proposal["builds"].append(target)
        (self.candidate / "src/alternate").mkdir()
        (self.candidate / "src/alternate/answer.c").write_text('#include "src/api/value.h"\nint answer(void) { return 9; }\n')
        self.save()
        source_before = (self.candidate / "src/entry/start.S").read_bytes()
        report = self.invoke()
        self.assertEqual(len(report["builds"]), 4)
        self.assertEqual((self.candidate / "src/entry/start.S").read_bytes(), source_before)
        for name, expected in (("probe", 7), ("alternate-probe", 9)):
            executable = self.base / "build" / (name + "-release") / "probe.elf"
            executable.chmod(0o700)
            result = subprocess.run([str(executable)], capture_output=True, timeout=5)
            self.assertEqual(result.returncode, expected)
        self.assertEqual(report["service_preflight"]["alternate-probe"]["service_edges"][0]["provider"], "alternate")

    def test_required_service_cannot_be_silently_omitted(self):
        self.add_service()
        self.proposal["modules"][1]["provides"] = []
        self.proposal["builds"][0]["bindings"] = {}
        with self.assertRaisesRegex(Invalid, "required service missing"):
            registry(self.proposal, self.policy["files"])

    def test_interface_module_cannot_embed_translation_units(self):
        self.proposal["modules"][1]["role"] = "interface"
        with self.assertRaisesRegex(Invalid, "interface module"):
            registry(self.proposal, self.policy["files"])

    def test_required_module_must_be_in_allowed_dependencies(self):
        self.proposal["modules"][0]["required_modules"] = ["not-allowed"]
        with self.assertRaisesRegex(Invalid, "declared allowed dependencies"):
            registry(self.proposal, self.policy["files"])

    def test_unknown_file_even_with_noncode_suffix(self):
        (self.candidate / "hidden.payload").write_bytes(b"hidden")
        with self.assertRaisesRegex(Invalid, "unknown or non-regular"):
            self.invoke()

    def test_unknown_empty_directory(self):
        (self.candidate / "hidden").mkdir()
        with self.assertRaisesRegex(Invalid, "unknown or symlink"):
            self.invoke()

    def test_missing_source(self):
        (self.candidate / "src/answer/answer.c").unlink()
        with self.assertRaisesRegex(Invalid, "file missing"):
            self.invoke()

    def test_symlink_source(self):
        path = self.candidate / "src/answer/answer.c"
        path.unlink()
        path.symlink_to(self.controller / "README.md")
        with self.assertRaisesRegex(Invalid, "non-regular"):
            self.invoke()

    def test_fake_pass_checker(self):
        (self.candidate / "tools/gates/check.py").write_text("print('PASS')\n")
        with self.assertRaisesRegex(Invalid, "controller-owned"):
            self.invoke()

    def test_candidate_cannot_remove_module_registration(self):
        path = self.candidate / "governance/native-admission.json"
        forged = copy.deepcopy(self.proposal)
        forged["modules"].pop()
        path.write_bytes(canonical(forged))
        with self.assertRaisesRegex(Invalid, "controller-owned"):
            self.invoke()

    def test_candidate_cannot_change_toolchain(self):
        (self.candidate / "governance/native-probe-toolchain.json").write_text("{}")
        with self.assertRaisesRegex(Invalid, "controller-owned"):
            self.invoke()

    def test_external_controller_pin_required(self):
        with self.assertRaisesRegex(Invalid, "controller tree pin mismatch"):
            self.invoke(controller_pin="0" * 64)

    def test_missing_required_mode(self):
        self.proposal["builds"][0]["modes"] = ["debug"]
        with self.assertRaisesRegex(Invalid, "required build modes"):
            registry(self.proposal, self.policy["files"])

    def test_orphan_module_not_covered_by_any_build(self):
        self.proposal["builds"][0]["modules"].remove("answer")
        self.proposal["modules"][0]["dependencies"] = []
        self.proposal["modules"][0]["required_modules"] = []
        with self.assertRaisesRegex(Invalid, "build coverage"):
            registry(self.proposal, self.policy["files"])

    def test_missing_dependency_closure(self):
        self.proposal["builds"][0]["modules"].remove("answer")
        with self.assertRaisesRegex(Invalid, "dependency-closed"):
            registry(self.proposal, self.policy["files"])

    def test_low_level_cannot_hide_business_transitively(self):
        self.proposal["modules"][1]["role"] = "plugin"
        with self.assertRaisesRegex(Invalid, "low-level module"):
            registry(self.proposal, self.policy["files"])

    def test_product_cannot_select_test_modules(self):
        self.proposal["builds"][0]["scope"] = "product"
        with self.assertRaisesRegex(Invalid, "non-product"):
            registry(self.proposal, self.policy["files"])

    def test_required_registration_details(self):
        mutations = [("responsibility", ""), ("non_responsibilities", []), ("boundary", {"reason": "arbitrary", "evidence": ["README.md"]}),
                     ("languages", ["c++"]), ("linkage", "dynamic"), ("third_party_dependencies", ["libc"]),
                     ("activation_condition", "run arbitrary command"), ("targets", ["windows-x86_64"])]
        for key, value in mutations:
            proposal = copy.deepcopy(self.proposal)
            proposal["modules"][1][key] = value
            with self.subTest(key=key), self.assertRaises(Invalid):
                registry(proposal, self.policy["files"])

    def test_lifecycle_symbol_must_be_registered(self):
        self.proposal["modules"][1]["lifecycle"]["dispose"] = "unregistered"
        with self.assertRaisesRegex(Invalid, "lifecycle entry"):
            registry(self.proposal, self.policy["files"])

    def test_control_directory_cannot_be_native_source_root(self):
        self.proposal["modules"][1]["source_root"] = "tools"
        with self.assertRaisesRegex(Invalid, "control directories"):
            registry(self.proposal, self.policy["files"])

    def test_missing_budget_plan(self):
        self.proposal["modules"][1]["budgets"]["measurement_plan"] = ""
        with self.assertRaisesRegex(Invalid, "measurement plan"):
            registry(self.proposal, self.policy["files"])

    def test_object_budget_failure_never_signs_matrix_receipt(self):
        self.proposal["modules"][1]["budgets"]["object_bytes"]["debug"] = 1
        self.save()
        with self.assertRaisesRegex(Invalid, "module object byte budget exceeded"):
            self.invoke()
        self.assertFalse((self.base / "build/admission.json").exists())

    def test_final_artifact_budget_enforced(self):
        self.proposal["builds"][0]["budgets"]["artifact_bytes"]["debug"] = 1
        self.save()
        with self.assertRaisesRegex(Invalid, "final artifact byte budget exceeded"):
            self.invoke()

    def test_empty_project_stays_blocked_without_vacuous_native_pass(self):
        self.proposal["modules"] = []
        self.proposal["builds"] = []
        self.save()
        shutil.rmtree(self.candidate / "src")
        report = self.invoke()
        self.assertEqual(report["native_matrix"], "NOT_RUN")
        self.assertEqual(report["status"], "BLOCKED")

    def test_no_overwrite(self):
        (self.base / "build").mkdir()
        with self.assertRaisesRegex(Invalid, "output must be new"):
            self.invoke()

    def test_no_output_inside_candidate(self):
        with self.assertRaisesRegex(Invalid, "outside input trees"):
            self.invoke(output=self.candidate / "output")

    def test_source_change_during_build_invalidates_matrix(self):
        actual = admission.build
        def mutate(*args, **kwargs):
            report = actual(*args, **kwargs)
            (self.candidate / "src/answer/answer.c").write_text("int answer(void) { return 9; }\n")
            return report
        with patch.object(admission, "build", side_effect=mutate), self.assertRaisesRegex(Invalid, "inputs changed"):
            self.invoke()
        self.assertFalse((self.base / "build/admission.json").exists())

    def test_default_build_coverage_cannot_be_forged(self):
        self.proposal["modules"][0]["compositions"] = ["not-built"]
        with self.assertRaisesRegex(Invalid, "build coverage"):
            registry(self.proposal, self.policy["files"])

    def test_unknown_registration_field_rejected(self):
        self.proposal["modules"][0]["skip_checks"] = True
        with self.assertRaisesRegex(Invalid, "invalid module fields"):
            registry(self.proposal, self.policy["files"])


if __name__ == "__main__":
    unittest.main()
