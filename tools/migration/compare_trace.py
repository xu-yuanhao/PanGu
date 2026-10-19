"""T09 trace comparator and evidence validator (development tool only).

Compares normalized oracle/native traces against the evidence contract in
docs/migration/evidence-contract.json and writes raw/normalized/diff/summary.
Never turns a missing artifact, an empty case, a dropped event or an
unregistered difference into PASS.
"""

import argparse
import hashlib
import json
import re
import copy
from pathlib import Path

ALLOWED_VERDICTS = ("PASS", "FAIL", "BLOCKED", "NOT_APPLICABLE")
NOISE_FIELDS = ("timestamp", "durationMs", "duration_ms")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_jsonl(path):
    """Return (events, error). An empty log is an error, never an empty PASS."""
    if not path.is_file():
        return None, "artifact missing"
    lines = [line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not lines:
        return None, "artifact empty"
    events = []
    for number, line in enumerate(lines, 1):
        try:
            events.append(json.loads(line))
        except ValueError:
            return None, f"invalid JSON on line {number}"
    return events, None


def strip_noise(row):
    # Only Pi metadata paths are noise. Tool arguments/results/details are opaque.
    row = copy.deepcopy(row)
    event = row.get("event", {}) if isinstance(row, dict) else {}
    def message(value):
        if isinstance(value, dict) and value.get("role") in {"system", "user", "assistant", "toolResult"}:
            value.pop("timestamp", None)
            if value.get("role") == "toolResult":
                value.pop("durationMs", None)
    if isinstance(event, dict):
        message(event.get("message"))
        for key in ("messages", "toolResults"):
            for value in event.get(key, []):
                message(value)
        stream = event.get("assistantMessageEvent", {})
        if isinstance(stream, dict):
            for key in ("partial", "message", "error"):
                message(stream.get(key))
        if event.get("type") == "tool_execution_end":
            event.pop("durationMs", None)
    return row


def validate_trace(rows):
    if not isinstance(rows, list) or len(rows) < 2:
        return "incomplete trace"
    active = set()
    for seq, row in enumerate(rows):
        if not isinstance(row, dict) or set(row) != {"seq", "event"} or type(row["seq"]) is not int or row["seq"] != seq:
            return "invalid sequence or row structure"
        event = row["event"]
        if not isinstance(event, dict) or not isinstance(event.get("type"), str):
            return "invalid event"
        kind = event["type"]
        if kind not in {"agent_start", "agent_end", "turn_start", "turn_end", "message_start", "message_update", "message_end", "tool_execution_start", "tool_execution_update", "tool_execution_end"}:
            return "unknown event type"
        if (kind == "agent_start" and seq != 0) or (kind == "agent_end" and seq != len(rows)-1):
            return "invalid agent boundary"
        if kind.startswith("tool_execution_"):
            call = event.get("toolCallId")
            if not isinstance(call, str) or not call:
                return "invalid tool call id"
            if kind == "tool_execution_start":
                if call in active:
                    return "duplicate tool start"
                active.add(call)
            elif call not in active:
                return "tool event without active call"
            elif kind == "tool_execution_end":
                active.remove(call)
    if rows[0]["event"]["type"] != "agent_start" or rows[-1]["event"]["type"] != "agent_end" or active:
        return "incomplete agent or tool lifecycle"
    return None


def compare(case, oracle_events, native_events):
    """Return a list of structural differences; empty means identical."""
    if oracle_events is None:
        return ["oracle artifact missing or empty"]
    if native_events is None:
        return ["native artifact missing or empty"]
    for label, rows in (("oracle", oracle_events), ("native", native_events)):
        error = validate_trace(rows)
        if error:
            return [label + ": " + error]
    if len(oracle_events) != len(native_events):
        return [f"event count differs: oracle={len(oracle_events)} native={len(native_events)}"]
    diffs = []
    for index, (left, right) in enumerate(zip(oracle_events, native_events)):
        a = strip_noise(left)
        b = strip_noise(right)
        if a != b:
            diffs.append({"seq": index, "oracle": a, "native": b})
    return diffs


def case_verdict(case, diffs, oracle_stats, native_stats):
    if oracle_stats["missing"] or native_stats["missing"]:
        return "BLOCKED", "required artifact missing"
    if diffs:
        first = diffs[0]
        return "FAIL", first if isinstance(first, str) else "trace differs at seq " + str(first["seq"])
    if oracle_stats["events"] == 0:
        return "FAIL", "zero events is not a pass"
    return "PASS", "identical event sequence"


def verify_run(folder, pin, manifest_sha, cases):
    if not pin:
        return "run receipt has no external pin"
    path = folder / "run.json"
    if not path.is_file() or sha(path) != pin:
        return "run receipt pin mismatch"
    run = json.loads(path.read_text())
    if run.get("manifest_sha256") != manifest_sha:
        return "run manifest mismatch"
    if not re.fullmatch(r"[0-9a-f]{64}", run.get("runner_sha256", "")) or not run.get("source_artifacts"):
        return "missing runner/source provenance"
    records = run.get("cases", [])
    if len(records) != len(cases) or [r.get("case_id") for r in records] != [c["case_id"] for c in cases]:
        return "run case coverage mismatch"
    for record in records:
        raw = folder / (record["case_id"] + ".raw.jsonl")
        if record.get("exit") != 0 or record.get("error") is not None:
            return "execution did not succeed"
        if not raw.is_file() or record.get("raw_sha256") != sha(raw):
            return "raw artifact mismatch"
        rows, error = load_jsonl(raw)
        if error or len(rows) != record.get("event_count"):
            return "event count mismatch"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--oracle-dir", type=Path, required=True)
    parser.add_argument("--native-dir", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--oracle-run-sha256")
    parser.add_argument("--native-run-sha256")
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    cases = manifest.get("cases", [])
    if not cases:
        print("compare FAIL: empty case manifest")
        return 1

    seen = set()
    results = []
    for case in cases:
        case_id = case["case_id"]
        if not isinstance(case_id, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*", case_id):
            raise ValueError("unsafe case id")
        if case_id in seen:
            print(f"compare FAIL: duplicate case id {case_id}")
            return 1
        seen.add(case_id)
        oracle_path = args.oracle_dir / f"{case_id}.raw.jsonl"
        oracle_events, oracle_error = load_jsonl(oracle_path)
        oracle_stats = {"events": 0 if oracle_events is None else len(oracle_events),
                        "missing": oracle_error is not None, "error": oracle_error, "sha256": sha(oracle_path) if oracle_error is None else None}
        if args.native_dir is None:
            verdict, reason = "BLOCKED", "native implementation not provided"
            diffs = []
            native_stats = {"events": 0, "missing": True, "error": "native not supplied"}
        else:
            native_path = args.native_dir / f"{case_id}.raw.jsonl"
            native_events, native_error = load_jsonl(native_path)
            native_stats = {"events": 0 if native_events is None else len(native_events),
                            "missing": native_error is not None, "error": native_error, "sha256": sha(native_path) if native_error is None else None}
            diffs = compare(case, oracle_events, native_events)
            verdict, reason = case_verdict(case, diffs, oracle_stats, native_stats)
        results.append({
            "case_id": case_id,
            "features": case.get("feature_ids", []),
            "oracle": oracle_stats,
            "native": native_stats,
            "diff": diffs,
            "verdict": verdict,
            "reason": reason,
        })

    provenance = {"oracle": verify_run(args.oracle_dir, args.oracle_run_sha256, sha(args.manifest), cases)}
    if args.native_dir is not None:
        provenance["native"] = verify_run(args.native_dir, args.native_run_sha256, sha(args.manifest), cases)
    if any(provenance.values()):
        for result in results:
            if result["verdict"] == "PASS":
                result["verdict"] = "BLOCKED"
                result["reason"] = "execution evidence not bound: " + str(provenance)
    args.output.mkdir(parents=True, exist_ok=False)
    verdicts = [r["verdict"] for r in results]
    overall = "PASS" if all(v == "PASS" for v in verdicts) else (
        "BLOCKED" if any(v == "BLOCKED" for v in verdicts) and "FAIL" not in verdicts else "FAIL")
    summary = {
        "schema": "pangu.t09.comparison.v1",
        "manifest_sha256": sha(args.manifest),
        "comparator_sha256": sha(Path(__file__)),
        "claim": "PINNED_TRACE_COMPARISON; caller must establish external authority of receipt pins; not runtime isolation attestation",
        "oracle_dir": str(args.oracle_dir.resolve()),
        "native_dir": None if args.native_dir is None else str(args.native_dir.resolve()),
        "execution_receipt_errors": provenance,
        "case_count": len(results),
        "verdict": overall,
        "cases": results,
        "release": "BLOCKED",
    }
    (args.output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for case, result in zip(cases, results):
        for label, folder in (("oracle", args.oracle_dir), ("native", args.native_dir)):
            if folder is None:
                continue
            source = folder / (case["case_id"] + ".raw.jsonl")
            if source.is_file():
                (args.output / (case["case_id"] + "." + label + ".raw.jsonl")).write_bytes(source.read_bytes())
                rows, error = load_jsonl(source)
                if error is None:
                    normalized = [strip_noise(row) for row in rows]
                    (args.output / (case["case_id"] + "." + label + ".normalized.json")).write_text(json.dumps(normalized, ensure_ascii=False) + "\n")
        raw = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        (args.output / f"{result['case_id']}.diff.json").write_text(raw, encoding="utf-8")
    print(json.dumps({"verdict": overall, "cases": len(results), "output": str(args.output)}))
    return 0 if overall == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
