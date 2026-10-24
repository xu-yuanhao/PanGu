"""Local language admission for the active composition, not proof of ASM semantics."""
import hashlib
import json
from pathlib import Path, PurePosixPath

POLICY = 'governance/assembly-first.json'

def inspect(root, sources=None):
    errors = []
    try:
        policy = json.loads((root / POLICY).read_text())
        if set(policy) != {'schema', 'priority', 'sources'} or policy['schema'] != 'pangu.assembly-first.v1' or policy['priority'] != ['assembly', 'c', 'cpp']:
            raise ValueError('invalid language policy')
        entries = policy['sources']
        if not isinstance(entries, dict) or not entries:
            raise ValueError('empty language inventory')
        if sources is not None and (len(sources) != len(set(sources)) or set(sources) != set(entries)):
            errors.append('actual build inputs differ from language inventory')
        for name, rule in entries.items():
            p = PurePosixPath(name)
            if p.is_absolute() or '..' in p.parts or str(p) != name or not name.startswith('src/'):
                errors.append('invalid source path: ' + name)
                continue
            if not isinstance(rule, dict) or not isinstance(rule.get('reason'), str) or not rule['reason'].strip():
                errors.append('missing language rationale: ' + name)
                continue
            file = root / name
            if file.is_symlink() or not file.is_file():
                errors.append('missing/nonregular source: ' + name)
                continue
            role = rule.get('role')
            if role == 'pending-inplace-assembly':
                if set(rule) != {'role', 'reason', 'sha256'} or p.suffix != '.c' or hashlib.sha256(file.read_bytes()).hexdigest() != rule['sha256']:
                    errors.append('pending C core changed instead of in-place ASM replacement: ' + name)
            elif role == 'assembly-core':
                if set(rule) != {'role', 'reason'} or p.suffix != '.S':
                    errors.append('ASM core must be a registered .S input: ' + name)
            elif role == 'c-bridge':
                if set(rule) != {'role', 'reason', 'bridge_kind'} or p.suffix != '.c' or rule['bridge_kind'] not in {'external-library', 'platform-layout', 'static-composition'}:
                    errors.append('invalid C bridge: ' + name)
            elif role == 'cpp-bridge':
                if set(rule) != {'role', 'reason', 'bridge_kind'} or p.suffix != '.cpp' or rule['bridge_kind'] != 'approved-gui-bridge':
                    errors.append('invalid C++ bridge: ' + name)
            else:
                errors.append('unapproved implementation language/role: ' + name)
    except (OSError, ValueError, TypeError, KeyError) as error:
        errors.append('language policy error: ' + str(error))
    return errors

if __name__ == '__main__':
    root = Path(__file__).resolve().parents[2]
    errors = inspect(root)
    for error in errors:
        print('Assembly policy FAIL:', error)
    if not errors:
        policy = json.loads((root / POLICY).read_text())
        pending = sum(v['role'] == 'pending-inplace-assembly' for v in policy['sources'].values())
        print(f'Assembly policy inventory PASS; {pending} existing C core files await in-place replacement (not ASM completion)')
    raise SystemExit(bool(errors))
