"""Real loopback MCP HTTP peer; no external credentials or remote requests."""
import collections
import http.server
import json
import pathlib
import subprocess
import sys
import tempfile
import threading
import time
import traceback

logs = []
errors = []
lock = threading.Lock()
pending = {}
counts = collections.Counter()
initialized = set()
ping_replied = threading.Event()
unknown_replied = threading.Event()
names = ['echo', 'wait', 'rpc_error', 'tool_error', 'structured', 'bad_result', 'drop', 'image']

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *args): pass
    def handle(self):
        try:
            super().handle()
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass
        except BaseException as error:
            with lock: errors.append(traceback.format_exc())
    def response(self, status, body=b'', content='application/json', extra=None):
        self.send_response(status)
        self.send_header('Content-Type', content)
        self.send_header('Content-Length', str(len(body)))
        for k, v in (extra or {}).items(): self.send_header(k, v)
        self.end_headers()
        # Real byte fragmentation, including UTF-8 and CRLF boundaries.
        for i in range(0, len(body), 7):
            self.wfile.write(body[i:i + 7])
        self.wfile.flush()
    def json(self, value, extra=None):
        self.response(200, json.dumps(value, ensure_ascii=False).encode(), extra=extra)
    def sse(self, value, extra=None, prefix=b''):
        data = json.dumps(value, ensure_ascii=False, indent=1)
        body = prefix + b': comment\r\nevent: message\r\n' + b''.join(
            b'data: ' + line.encode() + b'\r\n' for line in data.splitlines()) + b'\r\n'
        self.response(200, body, 'text/event-stream; charset=utf-8', extra)
    def record(self, message=None):
        mode = self.path[1:]
        with lock:
            logs.append({'mode': mode, 'verb': self.command,
                         'headers': dict(self.headers.items()), 'message': message})
        return mode
    def session(self, mode):
        assert self.headers['Mcp-Session-Id'] == 'fixture-' + mode
        assert self.headers['MCP-Protocol-Version'] == '2024-11-05'
    def do_DELETE(self):
        mode = self.record()
        assert self.headers['Mcp-Session-Id'] == 'fixture-' + mode
        if mode not in ('bad-version', 'init-invalid'):
            assert self.headers['MCP-Protocol-Version'] == '2024-11-05'
        if mode in ('auth401', 'auth403', 'auth-resume', 'get-auth'): assert self.headers['Authorization'] == 'Bearer fixture-fresh', mode
        if mode == 'auth-cancel': assert self.headers['Authorization'] == 'Bearer fixture-old'
        self.response(204)
    def do_GET(self):
        mode = self.record(); self.session(mode)
        assert self.headers['Accept'] == 'text/event-stream'
        cursor = self.headers.get('Last-Event-ID')
        if mode == 'get-auth' and self.headers.get('Authorization') != 'Bearer fixture-fresh':
            self.response(401, extra={'WWW-Authenticate': 'Bearer fixture'}); return
        if mode == 'auth-resume' and cursor:
            assert cursor == 'auth-primed'
            if self.headers.get('Authorization') != 'Bearer fixture-fresh':
                self.response(401, extra={'WWW-Authenticate': 'Bearer fixture'}); return
            self.sse({'jsonrpc': '2.0', 'id': pending[(mode, 'resume')], 'result': {'content': [{'type': 'text', 'text': '你好 MCP'}]}})
        elif mode == 'resume' and cursor:
            assert cursor == 'response-primed'
            counts['resume'] += 1
            if counts['resume'] == 1:
                self.response(503, b'transient fixture')
                return
            mid = pending[(mode, 'resume')]
            self.sse({'jsonrpc': '2.0', 'id': mid, 'result': {'content': [{'type': 'text', 'text': '你好 MCP'}]}})
        elif mode == 'ping':
            self.response(200, b'data: {"jsonrpc":"2.0","id":"fixture-ping","method":"ping"}\n\ndata: {"jsonrpc":"2.0","id":17,"method":"roots/list"}\n\n', 'text/event-stream')
        elif mode == 'get-reconnect':
            counts['get'] += 1
            if counts['get'] == 1:
                assert cursor is None
            else:
                assert cursor == 'g1'
            self.response(200, b'id: g1\nretry: 1\ndata: {"jsonrpc":"2.0","method":"notifications/tools/list_changed"}\n\n', 'text/event-stream')
        else:
            assert cursor is None
            self.response(405)
    def do_POST(self):
        message = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        mode = self.record(message)
        assert self.headers['Accept'] == 'application/json, text/event-stream'
        assert self.headers['Content-Type'] == 'application/json'
        method, mid = message.get('method'), message.get('id')
        if method is None:
            assert mode == 'ping'
            if mid == 'fixture-ping':
                assert message['result'] == {}; ping_replied.set()
            else:
                assert mid == 17 and message['error']['code'] == -32601
                unknown_replied.set()
            self.response(202); return
        if mode in ('auth401', 'auth403', 'auth-always', 'auth-cancel'):
            rejected = self.headers.get('Authorization') != 'Bearer fixture-fresh'
            if mode == 'auth-always' or (rejected and (mode != 'auth-cancel' or method == 'tools/call')):
                self.response(403 if mode == 'auth403' else 401, b'fixture secret must not leak',
                              extra={'WWW-Authenticate': 'Bearer ERROR="INSUFFICIENT_SCOPE"'})
                return
        elif mode == 'normal':
            assert self.headers['X-PanGu-Fixture'] == 'controlled'
            assert self.headers['Authorization'] == 'Basic Zml4dHVyZTpwYXNz'
        if mode == 'unauthenticated':
            self.response(401, b'fixture secret must not leak'); return
        def reply(result):
            value = {'jsonrpc': '2.0', 'id': mid, 'result': result}
            if method == 'tools/list' or mode == 'json-batch':
                self.json([{'jsonrpc': '2.0', 'id': 999999, 'result': {}}, value] if mode == 'json-batch' else value)
            else:
                self.sse(value, prefix=b'data: not json\n\nevent: unrelated\ndata: ignored\n\n')
        if method == 'initialize':
            assert message['params']['clientInfo']['name'] == 'PanGu'
            assert message['params']['capabilities'] == {}
            assert message['params']['protocolVersion'] == '2025-11-25'
            assert self.headers.get('Mcp-Session-Id') is None
            assert self.headers.get('MCP-Protocol-Version') is None
            if mode == 'init-timeout':
                time.sleep(.6); self.response(202); return
            if mode == 'accepted-init': self.response(202); return
            if mode == 'bad-content-init': self.response(200, b'invalid', 'text/plain'); return
            if mode == 'oversize-init': self.response(200, b' ' * 9000); return
            result = {'protocolVersion': '1999-01-01' if mode == 'bad-version' else '2024-11-05',
                      'capabilities': {'tools': {}}, 'serverInfo': {'name': 'fixture', 'version': '1'}}
            if mode == 'init-invalid': del result['capabilities']
            emit = self.sse if mode == 'init-sse' else self.json
            emit({'jsonrpc': '2.0', 'id': mid, 'result': result}, {'Mcp-Session-Id': 'fixture-' + mode})
            return
        self.session(mode)
        if method == 'notifications/initialized':
            initialized.add(mode); self.response(202); return
        assert mode in initialized
        if method == 'tools/list':
            if mode == 'list-invalid': reply({'tools': [{'name': 'missing schema'}]}); return
            if mode == 'repeat-cursor': reply({'tools': [], 'nextCursor': 'same'}); return
            if mode == 'duplicate': reply({'tools': [{'name': 'echo', 'inputSchema': {}}] * 2}); return
            cursor = message['params'].get('cursor')
            assert cursor is None or cursor == '第二页'
            selected = names[:4] if cursor is None else names[4:]
            reply({'tools': [{'name': name, 'description': 'fixture ' + name,
                'inputSchema': {'type': 'object', 'properties': {'text': {'type': 'string'}}, 'additionalProperties': False}}
                for name in selected], 'nextCursor': '第二页' if cursor is None else ''})
        elif method == 'tools/call':
            name = message['params']['name']
            if name == 'wait':
                pending[(mode, mid)] = threading.Event()
                self.send_response(200); self.send_header('Content-Type', 'text/event-stream')
                self.send_header('Connection', 'close'); self.end_headers(); self.wfile.flush()
                pending[(mode, mid)].wait(2); self.close_connection = True; return
            if name == 'echo':
                if mode == 'auth-resume':
                    pending[(mode, 'resume')] = mid
                    self.response(200, b'id: auth-primed\nretry: 1\ndata:\n\n', 'text/event-stream'); return
                if mode == 'get-auth': time.sleep(.04)
                if mode == 'resume':
                    pending[(mode, 'resume')] = mid
                    self.response(200, b'id: response-primed\nretry: 1\ndata:\n\n', 'text/event-stream'); return
                if mode == 'broken': self.response(200, b'data: not json\n\n', 'text/event-stream'); return
                if mode == 'accepted': self.response(202); return
                if mode == 'bad-content': self.response(200, b'invalid', 'text/plain'); return
                if mode == 'oversize': self.response(200, b''.join([b'data: ' + b'x' * 128 + b'\n' for _ in range(70)]), 'text/event-stream'); return
                if mode == 'expired': self.response(404, b'fixture secret must not leak'); return
                if mode == 'auth-error': self.response(401, b'fixture secret must not leak'); return
                if mode == 'server-error': self.response(500, b'fixture secret must not leak'); return
                if mode == 'get-reconnect': time.sleep(.03)
                if mode == 'ping':
                    assert ping_replied.wait(.2) and unknown_replied.wait(.2)
                reply({'content': [{'type': 'text', 'text': message['params']['arguments'].get('text', '你好 MCP')}],
                       'structuredContent': {'ok': True}})
            elif name == 'rpc_error': self.json({'jsonrpc': '2.0', 'id': mid, 'error': {'code': 1234, 'message': 'fixture RPC error', 'data': {'retryable': False}}})
            elif name == 'tool_error': reply({'content': [{'type': 'text', 'text': 'fixture tool error'}], 'isError': True})
            elif name == 'structured': reply({'structuredContent': [1, None, '结构化']})
            elif name == 'bad_result': reply({'content': 'not an array'})
            elif name == 'image': reply({'content': [{'type': 'image', 'mimeType': 'image/png', 'data': 'aGVsbG8='}, {'type': 'resource', 'resource': {'uri': 'fixture://text', 'text': 'resource text'}}]})
            else: raise AssertionError(name)
        elif method == 'notifications/cancelled':
            event = pending.get((mode, message['params']['requestId']))
            if event: event.set()
            self.response(202)
        else: raise AssertionError(method)

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
server.daemon_threads = True
thread = threading.Thread(target=server.serve_forever, daemon=True); thread.start()
url = 'http://127.0.0.1:' + str(server.server_port)
try:
    if len(sys.argv) > 2 and sys.argv[2] == '--installed':
        # External public-header consumer receives the complete endpoint as argv[1].
        subprocess.run([sys.argv[1], url + '/normal'], timeout=10, check=True)
        assert not errors, errors
        normal = [x for x in logs if x['mode'] == 'normal']
        posts = [x['message'] for x in normal if x['verb'] == 'POST']
        assert [x.get('method') for x in posts] == [
            'initialize', 'notifications/initialized', 'tools/list', 'tools/list', 'tools/call']
        call = posts[-1]
        assert call['params'] == {'name': 'echo', 'arguments': {'text': 'installed HTTP reply'}}
        assert sum(x['verb'] == 'DELETE' for x in normal) == 1
        assert not any(x['verb'] == 'GET' for x in normal)
        print('Installed MCP HTTP consumer: discovery, echo, close passed')
        sys.exit(0)
    with tempfile.TemporaryDirectory(prefix='pangu HTTP 中文 ') as directory:
        result = subprocess.run([sys.argv[1], url, 'unused', directory], timeout=20)
        assert result.returncode == 0, errors
        if len(sys.argv) > 2:
            example = subprocess.run([sys.argv[2], 'echo', '{"text":"HTTP example"}', url + '/example'], capture_output=True, text=True, timeout=5, check=True)
            assert example.stdout == 'HTTP example\n'
    assert not errors, errors
    normal = [x for x in logs if x['mode'] == 'normal']
    posts = [x['message'] for x in normal if x['verb'] == 'POST']
    assert [x.get('method') for x in posts[:4]] == ['initialize', 'notifications/initialized', 'tools/list', 'tools/list']
    calls = [x for x in posts if x.get('method') == 'tools/call']
    assert all(x['params']['arguments'].get('text') != 17 for x in calls)
    cancels = [x for x in posts if x.get('method') == 'notifications/cancelled']
    assert len(cancels) == 2
    assert {x['params']['requestId'] for x in cancels} == {x['id'] for x in calls if x['params']['name'] == 'wait'}
    assert {x['params']['reason'] for x in cancels} == {'Aborted', 'Request timed out'}
    assert any(x['verb'] == 'GET' for x in normal)
    assert sum(x['verb'] == 'DELETE' for x in normal) == 1
    assert not any(x['message'] and x['message'].get('method') == 'notifications/cancelled' for x in logs if x['mode'] == 'init-timeout')
    resume = [x for x in logs if x['mode'] == 'resume']
    assert sum(x['message'] is not None and x['message'].get('method') == 'tools/call' and x['message']['params']['name'] == 'echo' for x in resume) == 1
    assert counts['resume'] == 2 and counts['get'] >= 2
    assert sum(x['message'] is not None and x['message'].get('method') == 'tools/call' for x in logs if x['mode'] == 'auth-resume') == 1
    assert not any(x['verb'] == 'GET' for x in logs if x['mode'] == 'disabled-get')
    assert ping_replied.is_set() and unknown_replied.is_set()
    assert not any(x['message'] and x['message'].get('method') == 'tools/call' for x in logs if x['mode'] == 'token-cancel')
    cancelled = [x for x in logs if x['mode'] == 'auth-cancel' and x['message'] and x['message'].get('method') == 'tools/call']
    assert len(cancelled) == 1, cancelled
    print('MCP HTTP peer: sessions, JSON/SSE, pagination, auth, reconnect, cancel IDs, close passed')
finally:
    server.shutdown(); server.server_close(); thread.join()
