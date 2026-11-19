"""Product stdio configuration -> real MCP children + local tools -> HTTP model."""
import http.server
import json
import os
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
cli, peer = [str(Path(p).resolve()) for p in sys.argv[1:3]]
http_enabled = len(sys.argv) > 3 and sys.argv[3] == "1"
resources_enabled = len(sys.argv) > 4 and sys.argv[4] == "1"

def snapshot(path, sqlite):
    if sqlite:
        with sqlite3.connect(path) as db:
            return json.loads(db.execute('SELECT snapshot FROM pangu_session WHERE id=1').fetchone()[0])
    return json.loads(path.read_text().splitlines()[-1])

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass
    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.server.requests.append(body)
        names = {t['function']['name'] for t in body.get('tools', [])}
        assert {'read', 'write', 'edit', 'mcp__alpha__echo', 'mcp__beta__structured'} <= names
        if len(self.server.requests) == 1:
            calls = [('write', {'path': 'written.txt', 'content': 'actual local write'}),
                     ('mcp__alpha__wait' if self.server.cancel else 'mcp__alpha__echo', {'text': 'actual MCP result'}),
                     ('mcp__beta__structured', {})]
            delta = {'role': 'assistant', 'tool_calls': [
                {'index': i, 'id': f'call{i}', 'type': 'function',
                 'function': {'name': name, 'arguments': json.dumps(args)}}
                for i, (name, args) in enumerate(calls)]}
            reason = 'tool_calls'
        else:
            tools = [m for m in body['messages'] if m['role'] == 'tool']
            assert any('actual MCP result' in m['content'] for m in tools)
            delta, reason = {'role': 'assistant', 'content': 'MCP product completed'}, 'stop'
        frames = [{'choices': [{'index': 0, 'delta': delta, 'finish_reason': None}]},
                  {'choices': [{'index': 0, 'delta': {}, 'finish_reason': reason}]}]
        data = ''.join('data: ' + json.dumps(f) + '\n\n' for f in frames) + 'data: [DONE]\n\n'
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('Content-Length', str(len(data.encode())))
        self.end_headers()
        self.wfile.write(data.encode())

class McpHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_): pass
    def response(self, code, value=None):
        data = json.dumps(value).encode() if value is not None else b''
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Mcp-Session-Id', 'cli-fixture')
        self.end_headers()
        self.wfile.write(data)
    def do_GET(self): self.response(405)
    def do_DELETE(self):
        self.server.closed += 1
        self.response(204)
    def do_POST(self):
        assert self.headers.get('Authorization') == 'Bearer synthetic-http-secret'
        for name, expected in self.server.expected_headers.items():
            assert self.headers.get(name) == expected, (name, self.headers.get(name))
        m = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        method = m['method']
        self.server.methods.append(method)
        if method == 'initialize':
            result = {'protocolVersion': '2024-11-05', 'capabilities': {'tools': {}},
                      'serverInfo': {'name': 'fixture', 'version': '1'}}
        elif method == 'tools/list':
            result = {'tools': [{'name': n, 'description': n, 'inputSchema': {'type': 'object'}}
                                for n in ('echo', 'structured')]}
        elif method == 'tools/call':
            result = {'content': [{'type': 'text', 'text':
                      m['params']['arguments'].get('text', '结构化')}]}
        else:
            self.response(202)
            return
        self.response(200, {'jsonrpc': '2.0', 'id': m['id'], 'result': result})

def assert_reaped(directory):
    for path in directory.glob('**/*.pid'):
        try:
            os.kill(int(path.read_text()), 0)
        except ProcessLookupError:
            continue
        raise AssertionError('MCP child survived product close: ' + str(path))

with tempfile.TemporaryDirectory(prefix='PanGu MCP product 中文 ') as temp:
    root = Path(temp)
    env = os.environ.copy()
    env.pop('PANGU_AGENT_DIR', None)
    if resources_enabled:
        env['PANGU_AGENT_DIR'] = str(root / 'agent')
    env['DEEPSEEK_API_KEY'] = 'synthetic-mcp-key'
    env.pop('PANGU_CONTEXT_TOKENS', None)
    env['PANGU_MCP_FIXTURE'] = 'controlled'
    env.pop('PANGU_MCP_INHERITED', None)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    env['PANGU_DEEPSEEK_ENDPOINT'] = f'http://127.0.0.1:{server.server_port}/chat/completions'
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        for sqlite in (False, True):
            for cancel in (False, True):
                folder = root / f'{sqlite}-{cancel}'
                folder.mkdir()
                servers = {}
                for name in ('alpha', 'beta', 'disabled'):
                    child = folder / name
                    child.mkdir()
                    servers[name] = {'command': sys.executable, 'args': [peer, 'normal', str(child)],
                                     'timeout': 3, 'cwd': str(child), 'enabled': name != 'disabled'}
                config = folder / 'mcp.json'
                config.write_text(json.dumps({'mcpServers': servers}))
                path = folder / ('session.db' if sqlite else 'session.jsonl')
                flags = ['--sqlite'] if sqlite else []
                server.requests, server.cancel = [], cancel
                args = [cli, 'create', str(path), *flags, '--cwd', str(folder),
                        '--mcp-config', 'mcp.json', '--deepseek', 'use all tools']
                process = subprocess.Popen(args, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                if cancel:
                    log = folder / 'alpha/normal.jsonl'
                    deadline = time.monotonic() + 5
                    while time.monotonic() < deadline:
                        if log.exists() and any(json.loads(line).get('method') == 'tools/call'
                                                for line in log.read_text().splitlines()):
                            break
                        if process.poll() is not None:
                            raise AssertionError(process.communicate())
                        time.sleep(.01)
                    else:
                        process.kill()
                        raise AssertionError('MCP call not reached')
                    process.send_signal(signal.SIGINT)
                output, error = process.communicate(timeout=7)
                assert process.returncode == 0, (output, error)
                assert (folder / 'written.txt').read_text() == 'actual local write'
                assert 'fixture stderr' not in output + error
                assert not list((folder / 'disabled').iterdir())
                archived = json.dumps(snapshot(path, sqlite), ensure_ascii=False)
                assert 'synthetic-mcp-key' not in archived
                if cancel:
                    logs = [json.loads(line) for line in (folder / 'alpha/normal.jsonl').read_text().splitlines()]
                    assert any(m.get('method') == 'notifications/cancelled' for m in logs)
                    assert len(server.requests) == 1
                else:
                    assert 'MCP product completed' in output and len(server.requests) == 2
                    assert 'actual MCP result' in archived and '结构化' in archived
                reopened = subprocess.check_output([cli, 'open', str(path), *flags], env=env, text=True)
                assert ('MCP product completed' in reopened) == (not cancel)
                assert_reaped(folder)
        if http_enabled:
            remote = http.server.ThreadingHTTPServer(('127.0.0.1', 0), McpHandler)
            remote.methods, remote.closed = [], 0
            worker = threading.Thread(target=remote.serve_forever, daemon=True)
            worker.start()
            env['PANGU_MCP_HTTP_TEST_TOKEN'] = 'synthetic-http-secret'
            env['PANGU_MCP_EMPTY'] = ''
            env['PANGU_MCP_LITERAL'] = '$PANGU_MCP_INHERITED${PANGU_MCP_EMPTY}!literal'
            env['PANGU_MCP_NEWLINE'] = 'invalid\r\nvalue'
            templates = {
                'X-Mixed': '$PANGU_MCP_HTTP_TEST_TOKEN/${PANGU_MCP_HTTP_TEST_TOKEN}/$PANGU_MCP_HTTP_TEST_TOKEN',
                'X-Escaped': '$$PANGU_MCP_INHERITED|$${PANGU_MCP_INHERITED}|$!literal|$$!literal',
                'X-Invalid': '${}|${9BAD}|${BAD-NAME}|${PANGU_MCP_INHERITED!}|$9|$',
                'X-Unclosed': '${open/$PANGU_MCP_HTTP_TEST_TOKEN',
                'X-Backslash': r'\$PANGU_MCP_HTTP_TEST_TOKEN',
                'X-No-Recursion': '$PANGU_MCP_LITERAL',
            }
            remote.expected_headers = {
                'X-Mixed': '/'.join(['synthetic-http-secret'] * 3),
                'X-Escaped': '$PANGU_MCP_INHERITED|${PANGU_MCP_INHERITED}|!literal|$!literal',
                'X-Invalid': '${}|${9BAD}|${BAD-NAME}|${PANGU_MCP_INHERITED!}|$9|$',
                'X-Unclosed': '${open/synthetic-http-secret',
                'X-Backslash': r'\synthetic-http-secret',
                'X-No-Recursion': env['PANGU_MCP_LITERAL'],
            }
            try:
                for sqlite in (False, True):
                    folder = root / f'http-{sqlite}'
                    folder.mkdir()
                    child = folder / 'alpha'
                    child.mkdir()
                    config = folder / 'mcp.json'
                    config.write_text(json.dumps({'mcpServers': {
                        'alpha': {'command': sys.executable, 'args': [peer, 'normal', str(child)], 'cwd': str(child)},
                        'beta': {'type': 'streamable-http', 'url': f'http://127.0.0.1:{remote.server_port}/mcp',
                                 'headers': {**templates, 'Authorization': 'Bearer ' + ('$PANGU_MCP_HTTP_TEST_TOKEN' if sqlite else '${PANGU_MCP_HTTP_TEST_TOKEN}')}, 'timeout': 2}}}))
                    path = folder / 'session'
                    flags = ['--sqlite'] if sqlite else []
                    server.requests, server.cancel = [], False
                    result = subprocess.run([cli, 'create', str(path), *flags, '--cwd', str(folder),
                        '--mcp-config', str(config), '--deepseek', 'mixed transports'],
                        env=env, capture_output=True, text=True, timeout=7)
                    assert result.returncode == 0, (result.stdout, result.stderr)
                    assert 'MCP product completed' in result.stdout and len(server.requests) == 2
                    archived = json.dumps(snapshot(path, sqlite))
                    assert 'synthetic-http-secret' not in archived + result.stdout + result.stderr
                    before = len(remote.methods)
                    reopened = subprocess.check_output([cli, 'open', str(path), *flags], env=env, text=True)
                    assert 'MCP product completed' in reopened and len(remote.methods) == before
                    assert_reaped(folder)
                assert remote.methods.count('tools/call') == 2 and remote.closed == 2
                # Full-file OrThrow semantics: even a disabled later entry is
                # validated before the preceding HTTP or stdio source connects.
                invalid = root / 'env-invalid'
                invalid.mkdir()
                for enabled in (True, False):
                    for template in ('$PANGU_MCP_INHERITED', '${PANGU_MCP_INHERITED}',
                                     '$PANGU_MCP_EMPTY', '${PANGU_MCP_EMPTY}',
                                     '$PANGU_MCP_HTTP_TEST_TOKEN/$PANGU_MCP_INHERITED',
                                     '${PANGU_MCP_INHERITED}/${PANGU_MCP_EMPTY}',
                                     '$PANGU_MCP_NEWLINE', '!touch command-was-run'):
                        config = invalid / 'mcp.json'
                        config.write_text(json.dumps({'mcpServers': {
                            'httpfirst': {'url': f'http://127.0.0.1:{remote.server_port}/mcp',
                                          'headers': {'Authorization': 'Bearer $PANGU_MCP_HTTP_TEST_TOKEN'}},
                            'stdiofirst': {'command': sys.executable,
                                           'args': [peer, 'normal', str(invalid)], 'cwd': str(invalid)},
                            'invalid': {'url': f'http://127.0.0.1:{remote.server_port}/mcp',
                                        'enabled': enabled, 'headers': {'X-Test': template}}}}))
                        before = len(remote.methods)
                        (invalid / 'session').unlink(missing_ok=True)
                        result = subprocess.run([cli, 'create', str(invalid / 'session'),
                            '--cwd', str(invalid), '--mcp-config', str(config), '--demo', 'never'],
                            env=env, capture_output=True, text=True, timeout=5)
                        assert result.returncode != 0
                        assert 'MCP configuration or server installation failed' in result.stderr
                        assert 'synthetic-http-secret' not in result.stdout + result.stderr
                        assert len(remote.methods) == before and not list(invalid.glob('*.pid'))
                        assert not (invalid / 'command-was-run').exists()

            finally:
                remote.shutdown()
                remote.server_close()
                worker.join()
        # Entire file validation precedes execution, including later bad entries.
        folder = root / 'invalid'
        folder.mkdir()
        valid = {'command': sys.executable, 'args': [peer, 'normal', str(folder)]}
        for bad in ({'command': 'nope', 'args': [1]}, {'url': 'file:///invalid'},
                    {'command': 'nope', 'env': {'KEY': 'secret'}},
                    {'command': 'nope', 'timeout': 0}, {'command': 'nope\x00'},
                    {'url': 'http://localhost', 'command': 'nope'},
                    {'url': 'http://localhost', 'headers': {'Bad Header': 'x'}},
                    {'url': 'http://localhost', 'headers': {'Authorization': '${PANGU_MCP_INHERITED}'}},
                    {'url': 'http://localhost', 'headers': {'X-Test': '!touch should-never-run'}},
                    {'url': 'http://localhost', 'headers': {'X-Test': 'a\r\nb'}}):
            config = folder / 'mcp.json'
            config.write_text(json.dumps({'mcpServers': {'good': valid, 'bad': bad}}))
            result = subprocess.run([cli, 'create', str(folder / 'session'), '--cwd', str(folder),
                                     '--mcp-config', str(config), '--demo', 'never'],
                                    env=env, capture_output=True, text=True, timeout=5)
            assert result.returncode != 0 and 'secret' not in result.stderr
            assert not list(folder.glob('*.pid'))
        config.write_text(json.dumps({'mcpServers': {'a-b': valid, 'a_b': valid}}))
        result = subprocess.run([cli, 'open', str(folder / 'session'), '--mcp-config', str(config)],
                                env=env, capture_output=True, timeout=5)
        assert result.returncode != 0 and not list(folder.glob('*.pid'))
        # A later startup failure closes the already initialized first child.
        startup = root / 'startup-failure'
        startup.mkdir()
        servers = {}
        for name, mode in (('first', 'normal'), ('bad', 'init-invalid')):
            child = startup / name
            child.mkdir()
            servers[name] = {'command': sys.executable, 'args': [peer, mode, str(child)], 'timeout': .2, 'cwd': str(child)}
        config = startup / 'mcp.json'
        config.write_text(json.dumps({'mcpServers': servers}))
        result = subprocess.run([cli, 'create', str(startup / 'session'), '--mcp-config', str(config),
                                 '--demo', 'never accepted'], env=env, capture_output=True, text=True, timeout=5)
        assert result.returncode != 0 and 'never accepted' not in result.stdout
        assert_reaped(startup)
        # Project trust alone does not enable arbitrary process configuration.
        project = root / 'no-auto'
        (project / '.pangu').mkdir(parents=True)
        (project / '.pangu/mcp.json').write_text(json.dumps({'mcpServers': {'evil': valid}}))
        result = subprocess.run([cli, 'create', str(project / 'session'), '--cwd', str(project),
                                 *(['--trust-project'] if resources_enabled else []),
                                 '--demo', 'safe'], env=env, capture_output=True, text=True, timeout=5)
        assert result.returncode == 0 and not list(folder.glob('*.pid'))
        if not resources_enabled:
            # Explicit unsupported resource requests must still fail, rather
            # than being ignored to make the minimal composition test pass.
            for extra, overrides in ((['--trust-project'], {}),
                                     ([], {'PANGU_AGENT_DIR': str(root / 'agent')})):
                result = subprocess.run([cli, 'open', str(project / 'session'), *extra],
                    env={**env, **overrides}, capture_output=True, text=True, timeout=5)
                assert result.returncode != 0 and 'Resource service was not built' in result.stderr
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
print('MCP product: default local tools + two peers, HTTP roundtrip, cancel, both stores/reap passed')
