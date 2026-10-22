"""Counterexamples for the deliberately limited coordinator trace contract."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import MagicMock

import coordinator_trace as gate


class CoordinatorTraceTests(unittest.TestCase):
    def setUp(self):
        root = Path(__file__).resolve().parents[2]
        self.contract_path = root / 'docs/migration/t14-coordinator-trace-contract.json'
        self.contract = json.loads(self.contract_path.read_text())
        fixture = json.loads((root / 'docs/migration/t14-coordinator-trace-sample.json').read_text())
        self.rows = fixture['records']

    def reject(self, rows):
        with self.assertRaises(gate.TraceError):
            gate.validate(rows, self.contract)

    def test_observed_input_obeys_separate_contract_and_scope(self):
        result = gate.validate(self.rows, self.contract)
        self.assertEqual((result['cases'], result['events']), (7, 137))
        self.assertEqual(result['trace_conformance'], 'PASS')
        self.assertEqual(result['full_G04'], 'NOT_COMPLETED')
        self.assertEqual(result['release'], 'BLOCKED')

    def test_missing_extra_and_reordered_events_cannot_pass(self):
        for rows in [[], self.rows[:-1], self.rows + self.rows[-1:],
                     [x for x in self.rows if x['case'] != 6], list(reversed(self.rows))]:
            with self.subTest(length=len(rows)):
                self.reject(rows)

    def test_lifecycle_liveness_errors_and_stop_gate_mutations(self):
        attacks = [(4, 'stop-wait', 'state', [4, 2, 3, 1]),
                   (4, 'hold-acquired', 'inflight', [0, 0, 2, 0]),
                   (2, 'start-return', 'load_error', 0),
                   (2, 'stop-again', 'cleanup_error', 0),
                   (3, 'register-after-stop', 'rc', 0),
                   (1, 'case-end', 'resources', [1, 0, 0, 0]),
                   (4, 'start-again', 'rc', 0),
                   (5, 'start-return', 'pending', 0),
                   (1, 'stop-return', 'work', gate.FUEL + 1)]
        for case, event, field, value in attacks:
            rows = copy.deepcopy(self.rows)
            next(x for x in rows if x['case'] == case and x['event'] == event)[field] = value
            with self.subTest(case=case, event=event, field=field):
                self.reject(rows)

    def test_schema_types_arrays_and_unknown_fields_rejected(self):
        for field, value in [('case', True), ('seq', 0.0), ('id', '0'), ('state', [1]),
                             ('inflight', [True, 0, 0, 0]), ('resources', [0, 0, 0, -1]),
                             ('count', 5), ('rc', 2147483648), ('bypass', True)]:
            rows = copy.deepcopy(self.rows)
            rows[0][field] = value
            with self.subTest(field=field):
                self.reject(rows)

    def test_parser_rejects_duplicates_malformed_nonfinite_and_limits(self):
        for value in [b'', b'\xff', b'\n', b'{', b'{"case":1,"case":2}\n',
                      b'{"rc":NaN}\n', b'{"rc":Infinity}\n', b'x' * (gate.MAX_BYTES + 1),
                      b'{}\n' * 897, ('{"rc":' + '9' * 5000 + '}').encode()]:
            with self.subTest(length=len(value)), self.assertRaises(gate.TraceError):
                gate.parse_trace(value)

    def test_mandatory_contract_cannot_drop_or_retype_cases(self):
        for attack in ['missing', 'different', 'boolean', 'float']:
            contract = copy.deepcopy(self.contract)
            if attack == 'missing':
                contract['cases'].pop()
            elif attack == 'different':
                contract['cases'][0]['count'] = 0
            elif attack == 'boolean':
                contract['cases'][0]['id'] = True
            else:
                contract['cases'][0]['count'] = 3.0
            with self.subTest(attack=attack), self.assertRaises(gate.TraceError):
                gate.validate(self.rows, contract)

    def test_cli_pin_binding_and_failed_attempt_leaves_blocked_receipt(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            trace = base / 'trace.jsonl'
            trace.write_text(''.join(json.dumps(row) + '\n' for row in self.rows))
            pin = hashlib.sha256(self.contract_path.read_bytes()).hexdigest()
            command = [sys.executable, '-I', '-B', str(Path(gate.__file__).resolve()), '--trace', str(trace),
                       '--contract', str(self.contract_path), '--contract-sha256']
            good = subprocess.run(command + [pin, '--output', str(base / 'pass.json')], capture_output=True, timeout=10)
            self.assertEqual(good.returncode, 0, good.stdout + good.stderr)
            bad = subprocess.run(command + ['0' * 64, '--output', str(base / 'blocked.json')], capture_output=True, timeout=10)
            self.assertNotEqual(bad.returncode, 0)
            self.assertEqual(json.loads((base / 'blocked.json').read_text())['trace_conformance'], 'BLOCKED')

    def test_file_reader_is_bounded_before_parsing(self):
        path = MagicMock()
        stream = path.open.return_value.__enter__.return_value
        stream.read.return_value = b'x' * (gate.MAX_BYTES + 1)
        with self.assertRaises(gate.TraceError):
            gate.read_bounded(path)
        stream.read.assert_called_once_with(gate.MAX_BYTES + 1)


if __name__ == '__main__':
    unittest.main()
