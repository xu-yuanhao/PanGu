"""Actual app and CLI resource consumers, with synthetic local model requests."""
import http.server
import json
import os
from pathlib import Path
import subprocess
import sqlite3
import sys
import tempfile
import threading

# Deterministic app fixtures must not inherit real user credentials.
app_env = os.environ.copy()
app_env.pop('DEEPSEEK_API_KEY', None)
env_only = sys.argv[1] == '--env-only'
classifier_enabled = len(sys.argv) > 4 and sys.argv[4] == '1'
if env_only:
    app, cli = None, str(Path(sys.argv[2]).resolve())
else:
    app, cli = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])

def snapshot(path, backend):
    if backend == 'sqlite':
        with sqlite3.connect(path) as database:
            return json.loads(database.execute('SELECT snapshot FROM pangu_session WHERE id=1').fetchone()[0])
    return json.loads(path.read_text().splitlines()[-1])

def fixture(root):
    user, project = root / 'user/.pangu/agent', root / 'project'
    for path in (user / 'prompts', user / 'skills/example', project / '.pangu'):
        path.mkdir(parents=True)
    (user / 'settings.json').write_text(json.dumps({
        'defaultModel': 'user-model', 'defaultProvider': 'deepseek',
        'defaultThinkingLevel': 'high'}))
    (project / '.pangu/settings.json').write_text('{"defaultModel":"project-model"}')
    (user / 'SYSTEM.md').write_text('user-system')
    (project / '.pangu/SYSTEM.md').write_text('project-system')
    (user / 'prompts/hello.md').write_text('before $1')
    (user / 'skills/example/SKILL.md').write_text(
        '---\nname: example\ndescription: Example skill\ndisable-model-invocation: true\n---\nSkill instructions')
    return user, project

with tempfile.TemporaryDirectory(prefix='pangu-agent-resources 空格 ') as directory:
    root = Path(directory)
    for backend in (() if env_only else (0, 1)):
        user, project = fixture(root / f'app{backend}')
        subprocess.run([app, str(user), str(project), str(root / f'app{backend}.db'), str(backend)],
                       check=True, timeout=10, env=app_env)

    if not env_only and len(sys.argv) > 3:
        presentation = str(Path(sys.argv[3]).resolve())
        for backend in (0, 1):
            user, project = fixture(root / f'presentation{backend}')
            subprocess.run([presentation, str(user), str(project),
                            str(root / f'presentation{backend}.db'), str(backend)],
                           check=True, timeout=10, env=app_env)

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            self.server.requests.append(body)
            self.server.auth.append(self.headers.get('Authorization'))
            if self.server.rotate:
                user, _ = self.server.paths
                (user / 'auth.json').write_text(json.dumps({
                    'deepseek': {'type': 'api_key', 'key': 'synthetic-rotated-key'}}))
            if self.server.mutate and len(self.server.requests) == 1:
                user, project = self.server.paths
                (project / '.pangu/settings.json').write_text('{"defaultModel":"reloaded-model"}')
                (project / '.pangu/SYSTEM.md').write_text('reloaded-system')
                (user / 'prompts/hello.md').write_text('after $1')
            chunks = [
                {'choices': [{'index': 0, 'delta': {'role': 'assistant', 'content': 'synthetic resource response'}, 'finish_reason': None}]},
                {'choices': [{'index': 0, 'delta': {}, 'finish_reason': 'stop'}]},
            ]
            if self.server.tool_round and len(self.server.requests) == 1:
                chunks = [{'choices': [{'index': 0, 'delta': {'tool_calls': [{
                    'index': 0, 'id': 'credential-write', 'type': 'function',
                    'function': {'name': 'write', 'arguments': json.dumps({
                        'path': 'credential-result.txt', 'content': 'synthetic tool result'})}}]},
                    'finish_reason': 'tool_calls'}]}]
            if self.server.recovery:
                attempt = len(self.server.requests)
                if attempt == 1 and self.server.recovery in ('http413', 'http400'):
                    self.send_response(413 if self.server.recovery == 'http413' else 400)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                if attempt == 2 and self.server.recovery == 'summary-failure':
                    self.send_response(500)
                    self.send_header('Content-Length', '0')
                    self.end_headers()
                    return
                truncated = attempt == 1 or (attempt == 3 and self.server.recovery == 'repeated')
                text = 'truncated-attempt' if truncated else ('protocol summary' if attempt == 2 else 'recovered reply')
                chunks = [{'choices': [{'index': 0, 'delta': {'content': text},
                                        'finish_reason': 'length' if truncated else 'stop'}]},
                          {'choices': [], 'usage': {'prompt_tokens': 10000 if truncated else 5,
                                                    'completion_tokens': 0 if truncated else 2,
                                                    'total_tokens': 10000 if truncated else 7}}]
            if self.server.file_phase:
                is_summary = 'Do NOT continue' in json.dumps(body['messages'][:1])
                if is_summary:
                    text = 'file checkpoint'
                    chunks = [{'choices': [{'index': 0, 'delta': {'content': text}, 'finish_reason': 'stop'}]}]
                else:
                    operations = ([('read', {'path': 'a.txt'}),
                                   ('write', {'path': 'b 空格.txt', 'content': 'alpha'}),
                                   ('edit', {'path': 'b 空格.txt', 'edits': [{'oldText': 'alpha', 'newText': 'beta'}]}),
                                   ('read', {'path': 'b 空格.txt'})] if self.server.file_phase == 'initial'
                                  else [('read', {'path': 'c.txt'})])
                    self.server.file_step += 1
                    if self.server.file_step <= len(operations):
                        name, arguments = operations[self.server.file_step - 1]
                        chunks = [{'choices': [{'index': 0, 'delta': {'tool_calls': [{
                            'index': 0, 'id': f'file-{self.server.file_phase}-{self.server.file_step}',
                            'type': 'function', 'function': {'name': name, 'arguments': json.dumps(arguments)}}]},
                            'finish_reason': 'tool_calls'}]}]
                    else:
                        chunks = [{'choices': [{'index': 0, 'delta': {'content': 'files complete'}, 'finish_reason': 'stop'}]}]
                for chunk in chunks:
                    chunk.update(id=f'response-{len(self.server.requests)}', model='served-files-model')
            data = ''.join('data: ' + json.dumps(c) + '\n\n' for c in chunks).encode() + b'data: [DONE]\n\n'
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Content-Length', str(len(data)))
            self.end_headers()
            self.wfile.write(data)

    with http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
        server.auth, server.rotate, server.tool_round = [], False, False
        server.recovery = None
        server.file_phase, server.file_step = None, 0
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        env = os.environ.copy()
        for name in ('PANGU_MODEL', 'PANGU_CONTEXT_TOKENS', 'PANGU_AGENT_DIR'):
            env.pop(name, None)
        env['DEEPSEEK_API_KEY'] = 'synthetic-test-key'
        env['PANGU_DEEPSEEK_ENDPOINT'] = f'http://127.0.0.1:{server.server_port}/chat/completions'
        if env_only:
            for backend in ('jsonl', 'sqlite'):
                path = root / f'env-only-{backend}.db'
                flags = ['--no-tools'] + (['--sqlite'] if backend == 'sqlite' else [])
                server.requests, server.auth = [], []
                server.mutate = False
                created = subprocess.run([cli, 'create', str(path), *flags,
                                          '--deepseek', 'environment authentication'],
                                         env=env, capture_output=True, check=True, timeout=10)
                assert server.auth == ['Bearer synthetic-test-key']
                reopened = subprocess.run([cli, 'open', str(path), *flags], env=env,
                                          capture_output=True, check=True, timeout=10)
                assert created.stdout == reopened.stdout
                assert b'synthetic-test-key' not in path.read_bytes()
        else:
            for backend in ('jsonl', 'sqlite'):
                user, project = fixture(root / backend)
                flags = ['--agent-dir', str(user), '--cwd', str(project), '--no-tools']
                if backend == 'sqlite':
                    flags += ['--sqlite']
                # Untrusted default must not consume project settings or SYSTEM.md.
                server.requests, server.mutate, server.paths = [], False, (user, project)
                path = root / f'untrusted-{backend}.db'
                subprocess.run([cli, 'create', str(path), *flags[2:], '--deepseek', '/hello default'],
                               env=dict(env, PANGU_AGENT_DIR=str(user)), check=True,
                               capture_output=True, timeout=10)
                req = server.requests[0]
                assert req['model'] == 'user-model', req['model']
                assert 'user-system' in req['messages'][0]['content']
                assert 'project-system' not in json.dumps(req)
                assert req['messages'][-1]['content'] == 'before default'
                server.requests, server.mutate = [], True
                path = root / f'trusted-{backend}.db'
                commands = ['--deepseek', '/hello "one two"', '--reload',
                            '--deepseek', '/hello next', '--deepseek', '/skill:example explicit args']
                created = subprocess.check_output([cli, 'create', str(path), *flags, '--trust-project', *commands],
                                                  env=env, text=True, timeout=10)
                assert len(server.requests) == 3
                first, second, skill = server.requests
                assert first['model'] == 'project-model'
                assert 'project-system' in first['messages'][0]['content']
                assert first['messages'][-1]['content'] == 'before one two'
                assert second['model'] == 'reloaded-model'
                assert 'reloaded-system' in second['messages'][0]['content']
                assert second['messages'][-1]['content'] == 'after next'
                expanded = skill['messages'][-1]['content']
                assert '<skill name="example"' in expanded and 'Skill instructions' in expanded
                assert 'References are relative to ' in expanded and expanded.endswith('explicit args')
                assert '<skills>' not in skill['messages'][0]['content']  # disabled discovery, explicit allowed
                reopened = subprocess.check_output([cli, 'open', str(path), *flags, '--trust-project'],
                                                   env=env, text=True, timeout=10)
                assert created == reopened
                assert 'synthetic-test-key' not in path.read_bytes().decode('utf-8', errors='ignore')
                # An explicit environment model wins over settings.
                override_env = dict(env, PANGU_MODEL='explicit-model')
                subprocess.run([cli, 'open', str(path), *flags, '--trust-project', '--deepseek', '/unknown passthrough'],
                               env=override_env, check=True, capture_output=True, timeout=10)
                assert server.requests[-1]['model'] == 'explicit-model'
                assert server.requests[-1]['messages'][-1]['content'] == '/unknown passthrough'
            # No DEEPSEEK_API_KEY: only the user auth file supplies authentication.
            # No reload between requests: auth is never part of the snapshot.
            for backend in ('jsonl', 'sqlite'):
                user, project = fixture(root / f'credentials-{backend}')
                flags = ['--agent-dir', str(user), '--cwd', str(project)]
                if backend == 'sqlite':
                    flags += ['--sqlite']
                path = root / f'credentials-{backend}.db'
                auth_env = dict(env)
                auth_env.pop('DEEPSEEK_API_KEY', None)
                (user / 'auth.json').write_text(json.dumps({
                    'deepseek': {'type': 'api_key', 'key': 'synthetic-initial-key'}}))
                # Even explicitly trusted project resources never supply secrets.
                (project / '.pangu/auth.json').write_text('invalid-project-secret')
                server.requests, server.auth = [], []
                server.mutate, server.rotate, server.tool_round, server.paths = False, True, True, (user, project)
                created = subprocess.run([cli, 'create', str(path), *flags,
                                          '--trust-project', '--deepseek', 'first',
                                          '--deepseek', 'second'], env=auth_env,
                                         capture_output=True, check=True, timeout=10)
                assert server.auth == ['Bearer synthetic-initial-key',
                                       'Bearer synthetic-rotated-key',
                                       'Bearer synthetic-rotated-key']
                assert server.requests[1]['messages'][-1]['role'] == 'tool'
                assert (project / 'credential-result.txt').read_text() == 'synthetic tool result'
                server.rotate, server.tool_round = False, False
                reopened = subprocess.run([cli, 'open', str(path), *flags], env=auth_env,
                                          capture_output=True, check=True, timeout=10)
                assert created.stdout == reopened.stdout
                # A read failure stops before HTTP and does not echo file contents.
                (user / 'auth.json').write_text('{invalid synthetic-private-marker')
                count = len(server.requests)
                failed = subprocess.run([cli, 'open', str(path), *flags,
                                         '--deepseek', 'auth failure'], env=auth_env,
                                        capture_output=True, timeout=10)
                assert failed.returncode != 0 and len(server.requests) == count
                assert b'synthetic-private-marker' not in failed.stdout + failed.stderr
                (user / 'auth.json').unlink()
                (user / 'auth.json').mkdir()
                failed = subprocess.run([cli, 'open', str(path), *flags,
                                         '--deepseek', 'auth read error'], env=auth_env,
                                        capture_output=True, timeout=10)
                assert failed.returncode != 0 and len(server.requests) == count
                assert b'synthetic-private-marker' not in failed.stdout + failed.stderr
                (user / 'auth.json').rmdir()
                # PG_NOT_FOUND + absent fallback fails; project auth is never used.
                failed = subprocess.run([cli, 'open', str(path), *flags,
                                         '--trust-project', '--deepseek', 'missing auth'],
                                        env=auth_env, capture_output=True, timeout=10)
                assert failed.returncode != 0 and len(server.requests) == count
                # Environment fallback remains usable after a file is removed.
                subprocess.run([cli, 'open', str(path), *flags, '--deepseek', 'env auth'],
                               env=env, capture_output=True, check=True, timeout=10)
                assert server.auth[-1] == 'Bearer synthetic-test-key'
                persisted = path.read_bytes()
                for secret in (b'synthetic-initial-key', b'synthetic-rotated-key',
                               b'synthetic-test-key', b'synthetic-private-marker',
                               b'invalid-project-secret'):
                    assert secret not in persisted
                    assert secret not in created.stdout + created.stderr + reopened.stdout
        for backend in ('jsonl', 'sqlite'):
            for scenario in (('success', 'repeated', 'summary-failure') +
                             (('http413', 'http400') if classifier_enabled else ())):
                user, project = fixture(root / f'recovery-{backend}-{scenario}')
                path = root / f'recovery-{backend}-{scenario}.db'
                flags = (['--cwd', str(project), '--no-tools'] if env_only else
                         ['--agent-dir', str(user), '--cwd', str(project), '--no-tools'])
                if backend == 'sqlite':
                    flags += ['--sqlite']
                server.requests, server.auth, server.recovery = [], [], None
                server.mutate, server.rotate, server.tool_round = False, False, False
                subprocess.run([cli, 'create', str(path), *flags,
                                '--deepseek', 'old input', '--deepseek', 'recent input'],
                               env=env, capture_output=True, check=True, timeout=10)
                server.requests, server.auth, server.recovery = [], [], scenario
                (user / 'auth.json').write_text(json.dumps({
                    'deepseek': {'type': 'api_key', 'key': 'synthetic-recovery-initial'}}))
                server.rotate, server.paths = not env_only, (user, project)
                policy_env = dict(env, PANGU_CONTEXT_TOKENS='10000',
                                  PANGU_RESERVE_TOKENS='100', PANGU_KEEP_RECENT_TOKENS='1')
                result = subprocess.run([cli, 'open', str(path), *flags,
                                         '--deepseek', 'recover input'],
                                        env=policy_env, capture_output=True, timeout=10)
                assert (result.returncode == 0) == (scenario not in ('summary-failure', 'http400'))
                assert len(server.requests) == (1 if scenario == 'http400' else
                                                2 if scenario == 'summary-failure' else 3)
                summary = server.requests[1] if scenario != 'http400' else None
                if summary:
                    assert 'Do NOT continue' in json.dumps(summary['messages'])
                    assert 'truncated-attempt' not in json.dumps(summary['messages'])
                if scenario not in ('summary-failure', 'http400'):
                    retry = server.requests[2]
                    assert retry['messages'][-1]['content'] == 'recover input'
                    assert 'truncated-attempt' not in json.dumps(retry['messages'])
                    assert 'protocol summary' in json.dumps(retry['messages'])
                assert server.auth == (['Bearer synthetic-test-key'] * len(server.requests)
                    if env_only else ['Bearer synthetic-recovery-initial'] + [
                        'Bearer synthetic-rotated-key'] * (len(server.requests) - 1))
                server.recovery, server.rotate = None, False
                reopened = subprocess.run([cli, 'open', str(path), *flags, '--tree', '--usage'],
                                          env=env, capture_output=True, check=True, timeout=10)
                assert (b'kind=omission' in reopened.stdout) == (scenario != 'http400')
                assert (b'kind=compaction' in reopened.stdout) == (scenario not in ('summary-failure', 'http400'))
                assert reopened.stdout.count(b'user: recover input') == 1
                assert (b'assistant: truncated-attempt' in reopened.stdout) == (scenario == 'repeated')
                for secret in (b'synthetic-test-key', b'synthetic-recovery-initial',
                               b'synthetic-rotated-key'):
                    assert secret not in path.read_bytes()
        if not env_only:
            for backend in ('jsonl', 'sqlite'):
                user, project = fixture(root / f'file-metadata-{backend}')
                (project / 'a.txt').write_text('read-only source')
                (project / 'c.txt').write_text('later source')
                path = root / f'file-metadata-{backend}.db'
                flags = ['--agent-dir', str(user), '--cwd', str(project)]
                if backend == 'sqlite':
                    flags += ['--sqlite']
                server.requests, server.auth = [], []
                server.mutate, server.rotate, server.tool_round = False, False, False
                server.recovery, server.file_phase, server.file_step = None, 'initial', 0
                subprocess.run([cli, 'create', str(path), *flags,
                                '--deepseek', 'perform file operations', '--compact', '1'],
                               env=env, capture_output=True, check=True, timeout=10)
                assert len(server.requests) == 6  # four tools, final reply, summary
                assert (project / 'b 空格.txt').read_text() == 'beta'
                saved = snapshot(path, backend)
                summary = next(n['message'] for n in reversed(saved['tree']['nodes']) if n['kind'] == 2)
                assert json.loads(summary['details_json']) == {'readFiles': ['a.txt'], 'modifiedFiles': ['b 空格.txt']}
                assert '<read-files>\na.txt\n</read-files>' in summary['content'][0]['data']
                assert '<modified-files>\nb 空格.txt\n</modified-files>' in summary['content'][0]['data']
                attempts = [n['message'] for n in saved['tree']['nodes']
                            if n.get('message', {}).get('role') == 2]
                assert attempts[0]['response_id'] == 'response-1'
                assert attempts[0]['response_model'] == 'served-files-model'
                assert attempts[0]['model'] == 'user-model'
                assert attempts[0]['raw_stop_reason'] == 'tool_calls'
                assert attempts[-1]['response_id'] == 'response-5'
                assert attempts[-1]['raw_stop_reason'] == 'stop'
                # A new process must inherit details even if the new model text
                # never mentions paths from the first summary.
                server.requests, server.auth = [], []
                server.file_phase, server.file_step = 'second', 0
                subprocess.run([cli, 'open', str(path), *flags,
                                '--deepseek', 'read next file', '--compact', '1'],
                               env=env, capture_output=True, check=True, timeout=10)
                assert len(server.requests) == 3
                assert '<read-files>\\na.txt\\n</read-files>' in json.dumps(server.requests[0]['messages'])
                saved = snapshot(path, backend)
                summary = next(n['message'] for n in reversed(saved['tree']['nodes']) if n['kind'] == 2)
                assert json.loads(summary['details_json']) == {'readFiles': ['a.txt', 'c.txt'], 'modifiedFiles': ['b 空格.txt']}
                assert '<read-files>\na.txt\nc.txt\n</read-files>' in summary['content'][0]['data']
                reopened = subprocess.run([cli, 'open', str(path), *flags], env=env,
                                          capture_output=True, check=True, timeout=10)
                assert b'<modified-files>' in reopened.stdout
                assert b'synthetic-test-key' not in path.read_bytes()
            server.file_phase = None
        server.shutdown()
        thread.join(timeout=3)
print('agent resources: ' + ('resource-OFF HTTP env auth/numeric recovery/reopen' if env_only else
      'app ctx/fallback/env/nested runs + presentation auth + HTTP auth rotation/tool continuation/trust/errors/reopen') +
      ' (JSONL/SQLite) passed')
