"""Catalogue rejection cases; none of these are Pi conformance tests."""

from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
from surface_review import build, classify_unknown, sha, traceability


class CatalogueTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pangu-catalogue-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "tools/migration").mkdir(parents=True)
        self.driver = self.root / "tools/migration/surface_inventory.py"
        self.driver.write_text("fixture driver\n")
        self.path = "references/pi/packages/coding-agent/src/cli/args.ts"
        source = self.root / self.path
        source.parent.mkdir(parents=True)
        source.write_text("export {};")
        docs = self.root / "references/pi/packages/coding-agent/docs"
        docs.mkdir()
        (docs / "cli.md").write_text('| `--help` | Help |\n```text\n| `not-a-setting` | text |\n```\n')
        self.discovery = {"schema": "pangu.migration.ast-discovery.v1",
                          "driver_sha256": sha(self.driver), "sources": [{
                              "path": self.path, "sha256": sha(source), "syntactic_diagnostics": 0,
                              "items": [{"id": "one", "kind": "flag-comparison",
                                         "line": 1, "symbol": "--help"}],
                          }], "reexport_edges": [], "package_entries": []}
        self.features = {"features": [{"id": "CLI-01", "sources": [self.path,
                         "references/pi/packages/coding-agent/docs/cli.md"]}]}

    def test_catalogue_preserves_source_doc_cases_without_claiming_pass(self):
        result = build(self.root, self.discovery, self.features)
        self.assertEqual(result["counts"]["proposed_cases"], 2)
        self.assertTrue(all(i["verification"] == "NOT_RUN" for i in result["items"]))
        self.assertEqual(result["items"][1]["symbol"], "--help")

    def test_stale_driver_is_rejected(self):
        self.driver.write_text("changed")
        with self.assertRaisesRegex(ValueError, "Stale AST"):
            build(self.root, self.discovery, self.features)

    def test_changed_source_is_rejected(self):
        (self.root / self.path).write_text("changed")
        with self.assertRaisesRegex(ValueError, "provenance mismatch"):
            build(self.root, self.discovery, self.features)

    def test_duplicate_ast_ids_are_rejected(self):
        self.discovery["sources"][0]["items"] *= 2
        with self.assertRaisesRegex(ValueError, "Duplicate AST"):
            build(self.root, self.discovery, self.features)

    def test_missing_reexport_target_inventory_is_rejected(self):
        self.discovery["reexport_edges"] = [{"status": "RESOLVED", "to": "missing.ts"}]
        with self.assertRaisesRegex(ValueError, "Uninventoried"):
            build(self.root, self.discovery, self.features)

    def test_unknown_source_is_an_owned_open_gap(self):
        unknown = self.root / "unclassified.ts"
        unknown.write_text("export {};")
        self.discovery["sources"][0]["path"] = "unclassified.ts"
        self.discovery["sources"][0]["sha256"] = sha(unknown)
        result = build(self.root, self.discovery, self.features)
        self.assertTrue(result["unknowns"])
        self.assertTrue(all(u["status"] == "OPEN" and u["owner_role"] for u in result["unknowns"]))

    def test_unknown_reexport_state_is_rejected(self):
        self.discovery["reexport_edges"] = [{"status": "PASS_BECAUSE_SKIPPED"}]
        with self.assertRaisesRegex(ValueError, "Unknown reexport"):
            build(self.root, self.discovery, self.features)

    def test_declared_entry_cannot_be_omitted(self):
        self.discovery["package_entries"] = [{"status": "RESOLVED", "source_paths": ["missing.ts"]}]
        with self.assertRaisesRegex(ValueError, "entry missing"):
            build(self.root, self.discovery, self.features)

    def test_changed_package_exports_cannot_reuse_old_inventory(self):
        manifest = self.root / "package.json"
        manifest.write_text('{"exports":{}}')
        self.discovery["package_entries"] = [{"status": "PACKAGE_METADATA", "manifest": "package.json", "manifest_sha256": sha(manifest)}]
        manifest.write_text('{"exports":{"./new":"./new.ts"}}')
        with self.assertRaisesRegex(ValueError, "manifest changed"):
            build(self.root, self.discovery, self.features)

    def test_legacy_setting_is_an_open_compatibility_obligation(self):
        result = classify_unknown({"id": "UNKNOWN-RECONCILE-x", "source": "settings_source_only", "symbol": "queueMode"})
        self.assertEqual(result["disposition"], "LEGACY_CONFIG_INPUT")
        self.assertIn("T37", result["steps"])

    def test_tool_modifier_is_not_invented_as_a_cli_flag(self):
        result = classify_unknown({"id": "UNKNOWN-RECONCILE-x", "source": "document_flags_without_comparison", "symbol": "-write"})
        self.assertEqual(result["disposition"], "TOOL_SELECTION_VALUE")

    def test_new_unknown_flag_cannot_silently_enter_ledger(self):
        catalogue = {"items": [], "unknowns": [{"id": "UNKNOWN-RECONCILE-x", "source": "document_flags_without_comparison", "symbol": "--mystery"}]}
        with self.assertRaisesRegex(ValueError, "Unclassified discovery"):
            traceability(catalogue, self.features, {"profiles": []})

    def test_requirements_start_unimplemented_with_stable_case_ids(self):
        catalogue = {"items": [], "unknowns": []}
        result = traceability(catalogue, self.features, {"profiles": [{"id": "P1", "required_cli_subset": ["--help"]}]})
        requirement = result["scope_requirements"][0]
        self.assertEqual(requirement["status"], "NOT_IMPLEMENTED")
        self.assertEqual(requirement["evidence"], [])
        self.assertTrue(requirement["case_id"].startswith("CASE-REQ-"))

    def test_unknown_required_feature_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unknown feature"):
            traceability({"items": [], "unknowns": []}, self.features,
                         {"profiles": [{"id": "P1", "required_features": ["INVALID"]}]})


if __name__ == "__main__":
    unittest.main()
