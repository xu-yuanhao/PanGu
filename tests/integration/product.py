"""Actual app/plugin/Core/HTTP/files/storage flow with a deterministic server."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import threading

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass
    def do_POST(self):
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert self.headers['Authorization'] == 'Bearer redacted-integration'
        assert payload['model'] == 'deepseek-chat'
        if self.path.endswith(('/recovery', '/recovery-repeat')):
            self.recovery(payload)
            return
        if self.path.endswith('/files') and any(m['role'] == 'system' and 'context summarization assistant' in m.get('content', '') for m in payload['messages']):
            assert not payload.get('tools')
            transcript = json.dumps(payload['messages'], ensure_ascii=False)
            assert 'result.txt' in transcript and 'Successfully wrote file' in transcript
            self.server.file_summaries += 1
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.end_headers()
            chunk = {'choices': [{'delta': {'content': 'File task completed.'}, 'finish_reason': 'stop'}]}
            self.wfile.write(('data: ' + json.dumps(chunk) + '\n\ndata: [DONE]\n\n').encode())
            self.wfile.flush()
            return
        assert {'read', 'write'} <= {t['function']['name'] for t in payload['tools']}
        messages = payload['messages']
        user = next(m['content'] for m in reversed(messages) if m['role'] == 'user')
        if user == 'write' and messages[-1]['role'] != 'tool':
            chunks = [{'choices': [{'delta': {'tool_calls': [{'index': 0, 'id': 'write-1', 'function': {'name': 'write', 'arguments': json.dumps({'path': 'result.txt', 'content': 'saved through plugins'})}}]}, 'finish_reason': 'tool_calls'}]}]
        else:
            if user == 'write':
                assert messages[-1]['content'] == 'Successfully wrote file'
                assert any(m.get('tool_calls') for m in messages)
            text = {'write': 'written', 'cancel': 'partial', 'resume': 'resumed'}[user]
            chunks = [{'choices': [{'delta': {'content': text}, 'finish_reason': 'stop'}]}, {'choices': [], 'usage': {'prompt_tokens': 1, 'completion_tokens': 1, 'total_tokens': 2, 'completion_tokens_details': {'reasoning_tokens': 0}}}]
        chunks[0].update(id='integration-response', model='deepseek-concrete')
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.end_headers()
        try:
            for chunk in chunks:
                raw = ('data: ' + json.dumps(chunk) + '\n\n').encode()
                for offset in range(0, len(raw), 5):
                    self.wfile.write(raw[offset:offset + 5])
                    self.wfile.flush()
            self.wfile.write(b'data: [DONE]\n\n')
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def recovery(self, payload):
        trace = self.server.recovery_traces.setdefault(self.path, [])
        messages = payload['messages']
        summary = any(m['role'] == 'system' and 'context summarization assistant' in m.get('content', '') for m in messages)
        assert not payload.get('tools'), 'summary/recovery fixture has no executable tools'
        if summary:
            trace.append('summary')
            text = 'Earlier work was completed; continue with the last user request.'
        else:
            user = next(m['content'] for m in reversed(messages) if m['role'] == 'user')
            if user == 'overflow' and 'overflow' not in trace:
                trace.append('overflow')
                self.send_response(413)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(b'{"error":"synthetic request size failure"}')
                return
            if user == 'overflow':
                assert trace[-1] == 'summary'
                trace.append('retry')
                if self.path.endswith('/recovery-repeat'):
                    self.send_response(413)
                    self.end_headers()
                    self.wfile.write(b'{"error":"synthetic repeated failure"}')
                    return
                text = 'recovered' 
            else:
                trace.append(user)
                text = 'history'
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.end_headers()
        chunk = {'choices': [{'delta': {'content': text}, 'finish_reason': 'stop'}]}
        self.wfile.write(('data: ' + json.dumps(chunk) + '\n\ndata: [DONE]\n\n').encode())
        self.wfile.flush()

server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    with tempfile.TemporaryDirectory(prefix='pangu-product-') as directory:
        for backend in ('jsonl', 'sqlite'):
            work = Path(directory, backend)
            work.mkdir()
            server.recovery_traces = {}
            server.file_summaries = 0
            completed = subprocess.run([sys.argv[1], f'http://127.0.0.1:{server.server_port}/chat/completions', str(work / 'session'), str(work), backend, '--trace-state'], check=True, timeout=20, text=True, capture_output=True)
            observed = [list(map(int, line.split(':')[1:])) for line in completed.stdout.splitlines() if line.startswith('state:')]
            assert observed == [[4, 0, 0], [6, 0, 1], [4, 1, 1], [6, 1, 2], [4, 2, 2], [6, 2, 3], [4, 3, 3], [6, 3, 4], [4, 2, 4], [6, 2, 5]], observed
            if len(sys.argv) > 2 and sys.argv[2]:
                subprocess.run(['node', str(Path(__file__).resolve().parents[1] / 'core/loop-diff.mjs'), '--published-state', sys.argv[2], json.dumps(observed)], check=True, timeout=10)
            if 'HTTP413/omission/summary/retry/reopen passed' in completed.stdout:
                assert len(server.recovery_traces) == 2
                for trace in server.recovery_traces.values():
                    assert trace == ['seed one', 'seed two', 'overflow', 'summary', 'retry'], trace
            print(completed.stdout.splitlines()[-1])
            assert server.file_summaries == 1
            assert (work / 'result.txt').read_text() == 'saved through plugins'
finally:
    server.shutdown()
    server.server_close()
    thread.join()
