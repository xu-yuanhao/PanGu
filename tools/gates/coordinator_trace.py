"""Seven coordinator migration groups; local trace conformance, not full G04."""
import argparse
import hashlib
import json
from pathlib import Path

MAX_BYTES = 1048576
FUEL = 1000000
SPECS = [
    dict(id=1, count=3, launched=3, fail=-1, cancel=-1, hold=-1, cleanup_fail=-1, flow='normal'),
    dict(id=2, count=4, launched=3, fail=2, cancel=-1, hold=-1, cleanup_fail=2, flow='failure'),
    dict(id=3, count=4, launched=2, fail=-1, cancel=1, hold=-1, cleanup_fail=-1, flow='cancel'),
    dict(id=4, count=3, launched=3, fail=-1, cancel=-1, hold=2, cleanup_fail=-1, flow='hold-active'),
    dict(id=5, count=4, launched=2, fail=1, cancel=-1, hold=1, cleanup_fail=-1, flow='hold-failed'),
    dict(id=6, count=2, launched=0, fail=-1, cancel=-1, hold=-1, cleanup_fail=-1, flow='invalid'),
    dict(id=7, count=0, launched=0, fail=-1, cancel=-1, hold=-1, cleanup_fail=-1, flow='empty'),
]
FIELDS = {'case', 'seq', 'event', 'id', 'rc', 'phase', 'count', 'started', 'launched', 'stop',
          'load_error', 'cleanup_error', 'state', 'inflight', 'resources', 'has_report', 'work', 'pending', 'reason'}


class TraceError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise TraceError(message)


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def parse_trace(data):
    require(isinstance(data, bytes) and 0 < len(data) <= MAX_BYTES, 'trace empty or byte limit exceeded')
    try:
        lines = data.decode('utf-8').splitlines()
        require(0 < len(lines) <= 7 * 128, 'trace record limit exceeded')
        return [json.loads(line, object_pairs_hook=unique_pairs,
                           parse_constant=lambda _: (_ for _ in ()).throw(TraceError('non-finite JSON number')))
                for line in lines]
    except TraceError:
        raise
    except (UnicodeError, ValueError, RecursionError) as error:
        raise TraceError('invalid UTF-8/JSON trace') from error


def grammar(spec):
    result = [('case-begin', -1, 0), ('bind', -1, 1 if spec['flow'] == 'invalid' else 0)]
    def setups():
        for i in range(spec['launched']):
            result.extend([('setup-begin', i, 0), ('setup-registered', i, 0)])
            if i == spec['hold']:
                result.append(('hold-acquired', i, 0))
            if i == spec['cancel']:
                result.extend([('stop-reentrant-setup', i, 5), ('register-after-stop', i, 10)])
            result.append(('setup-return', i, -70 if i == spec['fail'] else 0))
    def cleanups():
        for i in reversed(range(spec['launched'])):
            result.extend([('cleanup-begin', i, 0), ('stop-reentrant-cleanup', i, 5),
                           ('register-during-cleanup', i, 10),
                           ('cleanup-return', i, -90 if i == spec['cleanup_fail'] else 0)])
    setups()
    flow = spec['flow']
    if flow == 'normal':
        result.append(('start-return', -1, 0)); cleanups()
        result.extend([('stop-return', -1, 0), ('stop-again', -1, 0)])
    elif flow == 'failure':
        cleanups(); result.extend([('start-return', -1, -70), ('stop-again', -1, -90)])
    elif flow == 'cancel':
        cleanups(); result.append(('start-return', -1, 6))
    elif flow in {'hold-active', 'hold-failed'}:
        result.append(('start-return', -1, 0 if flow == 'hold-active' else 5))
        if flow == 'hold-active':
            result.extend([('stop-wait', -1, 5), ('start-again', -1, 10)])
        result.extend([('business-after-stop', -1, 10), ('hold-finished', -1, 0),
                       ('hold-finish-duplicate', -1, 10)])
        cleanups(); result.append(('stop-return', -1, 0))
    elif flow == 'invalid':
        result.append(('start-return', -1, 1))
    elif flow == 'empty':
        result.extend([('start-return', -1, 0), ('stop-return', -1, 0)])
    result.append(('case-end', -1, 0))
    return result


def schema(record):
    require(type(record) is dict and set(record) == FIELDS, 'unknown or missing trace fields')
    for key in FIELDS - {'event', 'state', 'inflight', 'resources'}:
        require(type(record[key]) is int, 'trace numeric fields require integers, not booleans')
    require(type(record['event']) is str and 0 < len(record['event']) <= 64, 'invalid event')
    require(1 <= record['case'] <= 7 and 0 <= record['seq'] < 128, 'invalid case or sequence')
    require(-2147483648 <= record['rc'] <= 2147483647, 'invalid status')
    require(0 <= record['phase'] <= 6 and record['count'] in range(5), 'invalid runtime state')
    require(0 <= record['started'] <= record['launched'] <= record['count'], 'invalid owner counts')
    for key, lower, upper in [('state', 1, 4), ('inflight', 0, 3), ('resources', 0, 2)]:
        require(type(record[key]) is list and len(record[key]) == 4, 'invalid owner array')
        require(all(type(x) is int and lower <= x <= upper for x in record[key]), 'invalid owner value')
    require(record['has_report'] in {0, 1} and record['pending'] in {0, 1} and record['reason'] in {0, 1, 2}, 'invalid report')
    require(record['stop'] in {0, 1} and 0 <= record['work'] <= FUEL, 'invalid stop/work value')


def validate(records, contract):
    require(type(contract) is dict and contract.get('schema') == 'pangu.coordinator.trace.contract.v1', 'unsupported trace contract')
    cases = contract.get('cases')
    require(type(cases) is list and len(cases) == len(SPECS), 'mandatory seven-group contract differs')
    for actual, expected in zip(cases, SPECS):
        require(type(actual) is dict and set(actual) == set(expected), 'mandatory seven-group contract differs')
        require(all(type(actual[k]) is type(v) and actual[k] == v for k, v in expected.items()),
                'mandatory seven-group contract differs')
    require(type(records) is list and 0 < len(records) <= 896, 'invalid trace matrix')
    cursor = 0
    for spec in SPECS:
        state = dict(phase=0, count=spec['count'], started=0, launched=0, stop=0, load_error=0, cleanup_error=0,
                     state=[1]*4, inflight=[0]*4, resources=[0]*4)
        for seq, marker in enumerate(grammar(spec)):
            require(cursor < len(records), 'missing required trace event')
            row = records[cursor]; cursor += 1; schema(row)
            event, owner, rc = marker
            require((row['case'], row['seq'], row['event'], row['id'], row['rc']) == (spec['id'], seq, event, owner, rc),
                    'case/action/order/status differs from mandatory contract')
            if event == 'setup-begin':
                state.update(phase=1, started=owner+1, launched=owner+1); state['inflight'][owner] += 1
            elif event == 'setup-registered':
                state['resources'][owner] = 1
            elif event == 'hold-acquired':
                state['inflight'][owner] += 1; state['inflight'][owner-1] += 1
            elif event == 'stop-reentrant-setup':
                state['stop'] = 1; state['state'][owner] = 3
            elif event == 'cleanup-begin':
                state.update(phase=3, stop=1); state['state'][owner] = 3; state['resources'][owner] -= 1
                require(state['inflight'][owner] == 0, 'reference cleanup must wait for operation completion')
            elif event == 'start-return':
                if spec['flow'] == 'hold-failed':
                    state.update(phase=3, stop=1); state['state'][spec['hold']] = 3
                elif spec['flow'] in {'normal', 'empty', 'hold-active'}:
                    state['phase'] = 2
                elif spec['flow'] in {'failure', 'cancel'}:
                    state.update(phase=4, stop=1)
            elif event == 'stop-wait':
                state.update(phase=3, stop=1); state['state'][spec['hold']] = 3
            elif event == 'hold-finished':
                state['inflight'][spec['hold']] -= 1; state['inflight'][spec['hold']-1] -= 1
            elif event in {'stop-return', 'stop-again'}:
                state.update(phase=4, stop=1)
            for field, expected in state.items():
                require(row[field] == expected, f"case{spec['id']} seq{seq} {field} differs")
            reported = event in {'hold-acquired', 'stop-reentrant-setup', 'stop-reentrant-cleanup', 'stop-wait',
                                 'business-after-stop', 'stop-return', 'stop-again'} or (event == 'start-return' and spec['flow'] != 'invalid')
            waiting = event in {'stop-reentrant-setup', 'stop-reentrant-cleanup', 'stop-wait'} or (event == 'start-return' and spec['flow'] == 'hold-failed')
            require(row['has_report'] == int(reported), 'report presence differs')
            require(row['pending'] == int(waiting) and row['reason'] == (2 if waiting else 0), 'report reason/pending differs')
            if not reported:
                require(row['work'] == 0, 'no-report event has work')
            # Operation/callback completion takes effect only after the callback returns.
            if event == 'setup-return':
                state['inflight'][owner] -= 1
                if rc:
                    state['load_error'] = rc
                elif not state['stop']:
                    state['state'][owner] = 2
            elif event == 'cleanup-return':
                state['state'][owner] = 4; state['started'] -= 1
                if rc and not state['cleanup_error']:
                    state['cleanup_error'] = rc
    require(cursor == len(records), 'extra trace events')
    return {'scope': 'seven public-ABI coordinator migration groups only', 'cases': 7, 'events': cursor,
            'trace_conformance': 'PASS', 'full_G04': 'NOT_COMPLETED', 'release': 'BLOCKED'}


def read_bounded(path):
    with path.open('rb') as source:
        data = source.read(MAX_BYTES + 1)
    require(0 < len(data) <= MAX_BYTES, 'trace empty or byte limit exceeded')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trace', required=True, type=Path)
    parser.add_argument('--contract', required=True, type=Path)
    parser.add_argument('--contract-sha256', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    require(not args.output.exists() and not args.output.is_symlink(), 'output must be new')
    args.output.write_text('{"trace_conformance":"BLOCKED","release":"BLOCKED"}\n')
    try:
        contract_bytes = args.contract.read_bytes()
        require(hashlib.sha256(contract_bytes).hexdigest() == args.contract_sha256, 'contract pin mismatch')
        data = read_bounded(args.trace)
        result = validate(parse_trace(data), json.loads(contract_bytes, object_pairs_hook=unique_pairs))
        result.update(trace_sha256=hashlib.sha256(data).hexdigest(), contract_sha256=args.contract_sha256)
        args.output.write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result)); return 0
    except (TraceError, ValueError, OSError) as error:
        print('coordinator trace BLOCKED:', error); return 2


if __name__ == '__main__':
    raise SystemExit(main())
