"""Negative tests for the T09 comparator.

The comparator is only useful if it rejects the failure modes the plan names:
dropped results, swapped causal events, hidden normalizer changes, empty logs,
duplicate/missing cases and a missing native side. These tests use temporary
fixtures, never the real Pi oracle.
"""

import json
import hashlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).resolve().parent / "compare_trace.py"
MANIFEST = {
    "schema": "pangu.t09.scenarios.v1",
    "reference_commit": "test",
    "cases": [
        {"case_id": "C1", "feature_ids": ["AGENT-01"], "scenario_revision": 1},
        {"case_id": "C2", "feature_ids": ["TOOL-01"], "scenario_revision": 1},
    ],
}

BASE = {
    "C1": [
        {"seq": 0, "event": {"type": "agent_start"}},
        {"seq": 1, "event": {"type": "message_end", "message": {"role": "assistant", "content": "hi"}}},
        {"seq": 2, "event": {"type": "agent_end"}},
    ],
    "C2": [
        {"seq": 0, "event": {"type": "agent_start"}},
        {"seq": 1, "event": {"type": "tool_execution_start", "toolCallId": "a"}},
        {"seq": 2, "event": {"type": "tool_execution_end", "toolCallId": "a", "isError": False}},
        {"seq": 3, "event": {"type": "agent_end"}},
    ],
}


def write_jsonl(path, rows):
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


class ComparatorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps(MANIFEST), encoding="utf-8")
        self.oracle = self.root / "oracle"
        self.native = self.root / "native"
        self.oracle.mkdir()
        self.native.mkdir()
        for case_id, rows in BASE.items():
            write_jsonl(self.oracle / f"{case_id}.raw.jsonl", rows)
            write_jsonl(self.native / f"{case_id}.raw.jsonl", rows)

    def tearDown(self):
        self.tmp.cleanup()

    def run_compare(self, native=True, output="out"):
        command = [sys.executable, "-B", str(TOOL), "--manifest", str(self.manifest),
                   "--oracle-dir", str(self.oracle), "--output", str(self.root / output)]
        if native:
            command += ["--native-dir", str(self.native)]
        for label, folder in (("oracle", self.oracle), ("native", self.native)):
            if label == "native" and not native:
                continue
            records = []
            for case in json.loads(self.manifest.read_text()).get("cases", []):
                case_id = case["case_id"]
                path = folder / (case_id + ".raw.jsonl")
                records.append({"case_id": case_id, "exit": 0, "error": None,
                                "raw_sha256": hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else "missing",
                                "event_count": len(path.read_text().splitlines()) if path.is_file() else 0})
            run = {"manifest_sha256": hashlib.sha256(self.manifest.read_bytes()).hexdigest(),
                   "runner_sha256": "a"*64, "source_artifacts": {"fixture": "b"*64}, "cases": records}
            receipt = folder / "run.json"
            receipt.write_text(json.dumps(run))
            command += ["--" + label + "-run-sha256", hashlib.sha256(receipt.read_bytes()).hexdigest()]
        result = subprocess.run(command, capture_output=True, text=True)
        summary = None
        path = self.root / output / "summary.json"
        if path.is_file():
            summary = json.loads(path.read_text())
        return result, summary

    def test_identical_traces_pass(self):
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(summary["verdict"], "PASS")

    def test_dropped_tool_result_fails(self):
        write_jsonl(self.native / "C2.raw.jsonl", BASE["C2"][:2])
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 2)
        self.assertEqual(summary["verdict"], "FAIL")
        case = next(c for c in summary["cases"] if c["case_id"] == "C2")
        self.assertIn("incomplete", case["reason"])

    def test_swapped_causal_events_fail(self):
        rows = [BASE["C2"][0], BASE["C2"][2], BASE["C2"][1], BASE["C2"][3]]
        write_jsonl(self.native / "C2.raw.jsonl", rows)
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 2)
        case = next(c for c in summary["cases"] if c["case_id"] == "C2")
        self.assertEqual(case["verdict"], "FAIL")

    def test_unregistered_field_change_fails(self):
        rows = json.loads(json.dumps(BASE["C2"]))
        rows[2]["event"]["isError"] = True
        write_jsonl(self.native / "C2.raw.jsonl", rows)
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 2)
        case = next(c for c in summary["cases"] if c["case_id"] == "C2")
        self.assertEqual(case["verdict"], "FAIL")

    def test_declared_noise_fields_are_ignored(self):
        rows = json.loads(json.dumps(BASE["C1"]))
        rows[1]["event"]["message"]["timestamp"] = 12345
        rows[1]["event"]["message"]["timestamp"] = 99
        write_jsonl(self.native / "C1.raw.jsonl", rows)
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_empty_trace_is_not_a_pass(self):
        (self.native / "C1.raw.jsonl").write_text("", encoding="utf-8")
        (self.oracle / "C1.raw.jsonl").write_text("", encoding="utf-8")
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 2)
        case = next(c for c in summary["cases"] if c["case_id"] == "C1")
        self.assertEqual(case["verdict"], "BLOCKED")

    def test_missing_native_is_blocked_not_pass(self):
        result, summary = self.run_compare(native=False)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(summary["verdict"], "BLOCKED")
        self.assertTrue(all(c["verdict"] == "BLOCKED" for c in summary["cases"]))

    def test_truncated_json_line_fails(self):
        (self.native / "C2.raw.jsonl").write_text('{"seq": 0, "event": {"type"', encoding="utf-8")
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 2)
        case = next(c for c in summary["cases"] if c["case_id"] == "C2")
        self.assertEqual(case["verdict"], "BLOCKED")

    def test_missing_case_artifact_is_blocked(self):
        (self.native / "C2.raw.jsonl").unlink()
        result, summary = self.run_compare()
        self.assertEqual(result.returncode, 2)
        case = next(c for c in summary["cases"] if c["case_id"] == "C2")
        self.assertEqual(case["verdict"], "BLOCKED")

    def test_duplicate_case_id_is_rejected(self):
        manifest = json.loads(json.dumps(MANIFEST))
        manifest["cases"].append({"case_id": "C1", "feature_ids": [], "scenario_revision": 1})
        self.manifest.write_text(json.dumps(manifest), encoding="utf-8")
        result, _ = self.run_compare()
        self.assertEqual(result.returncode, 1)
        self.assertIn("duplicate case id", result.stdout)

    def test_empty_manifest_is_rejected(self):
        self.manifest.write_text(json.dumps({"cases": []}), encoding="utf-8")
        result, _ = self.run_compare()
        self.assertEqual(result.returncode, 1)
        self.assertIn("empty case manifest", result.stdout)

    def test_business_timestamp_is_preserved(self):
        rows = json.loads(json.dumps(BASE["C2"]))
        rows[2]["event"]["result"] = {"timestamp": 123, "durationMs": 9}
        write_jsonl(self.native / "C2.raw.jsonl", rows)
        result, summary = self.run_compare()
        self.assertEqual(summary["verdict"], "FAIL")

    def test_equal_truncated_traces_fail(self):
        for folder in (self.oracle, self.native):
            write_jsonl(folder / "C1.raw.jsonl", BASE["C1"][:1])
        result, summary = self.run_compare()
        self.assertEqual(summary["verdict"], "FAIL")

    def test_equal_unknown_event_fails(self):
        rows = json.loads(json.dumps(BASE["C1"]))
        rows[1]["event"]["type"] = "invented"
        for folder in (self.oracle, self.native):
            write_jsonl(folder / "C1.raw.jsonl", rows)
        result, summary = self.run_compare()
        self.assertEqual(summary["verdict"], "FAIL")

    def test_unsafe_case_id_rejected(self):
        self.manifest.write_text(json.dumps({"cases": [{"case_id": "../escape"}]}))
        result, _ = self.run_compare()
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / "escape.diff.json").exists())

    def test_run_receipt_rejects_failed_execution_and_raw_tampering(self):
        from compare_trace import verify_run, sha
        result, summary = self.run_compare()
        self.assertEqual(summary["verdict"], "PASS")
        receipt = self.oracle / "run.json"
        original = json.loads(receipt.read_text())
        changed = json.loads(receipt.read_text())
        changed["cases"][0]["exit"] = 1
        receipt.write_text(json.dumps(changed))
        self.assertEqual(verify_run(self.oracle, sha(receipt), sha(self.manifest), MANIFEST["cases"]), "execution did not succeed")
        receipt.write_text(json.dumps(original))
        pin = sha(receipt)
        (self.oracle / "C1.raw.jsonl").write_text("{}\n")
        self.assertEqual(verify_run(self.oracle, pin, sha(self.manifest), MANIFEST["cases"]), "raw artifact mismatch")

    def test_unpinned_receipt_is_blocked(self):
        from compare_trace import verify_run, sha
        self.assertEqual(verify_run(self.oracle, None, sha(self.manifest), MANIFEST["cases"]), "run receipt has no external pin")

    def test_existing_output_is_not_overwritten(self):
        (self.root / "out").mkdir()
        result, _ = self.run_compare()
        self.assertEqual(result.returncode, 1)
        self.assertIn("FileExistsError", result.stderr)


if __name__ == "__main__":
    unittest.main()
