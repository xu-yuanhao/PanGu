"""Local HTTP protocol fixture; no credentials or external network."""
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import subprocess
import sys
import threading

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass
    def do_POST(self):
        body = self.rfile.read(int(self.headers['Content-Length']))
        assert self.headers['Authorization'] == 'Bearer redacted-test'
        assert self.headers['Content-Type'] == 'application/json'
        parsed = json.loads(body)
        if self.path == '/nonfinite':
            previous, = parsed['context']['messages']
            assert previous['role'] == 'assistant'
            assert previous['usage']['cost'] == dict(input=None, output=None, cacheRead=0, cacheWrite=0, total=None)
            parsed['context']['messages'] = []
        if self.path == '/cancel':
            assert parsed['context']['messages'][-1]['role'] == 'user'
            assert len(parsed['context']['messages']) in (1, 3)
            parsed['context']['messages'] = []
        assert parsed == {'model': {'id': '', 'provider': '', 'api': ''}, 'context': {'messages': []}, 'options': {'temperature': 0.5, 'sessionId': 'test-session'}}
        self.send_response(403 if self.path == '/error' else 200)
        self.end_headers()
        if self.path == '/error':
            self.wfile.write(b'{"error":"redacted"}')
            return
        data = ('data: \ufeff{"type":"start"}\u00a0\u2028\n'
                'data: \ufeff\u00a0\n'
                'data: {"type":"text_start","contentIndex":0}\n'
                'data: {"type":"text_delta","contentIndex":0,"delta":"世界"}\n')
        if self.path == '/ignored-tool-end':
            data += 'data: {"type":"toolcall_end","contentIndex":0}\n'
            data += 'data: {"type":"toolcall_end","contentIndex":4}\n'
        if self.path != '/eof':
            data += 'data: {"type":"done","reason":"stop","usage":{}}'
        try:
            for byte in data.encode():
                self.wfile.write(bytes([byte]))
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass  # client cancellation closes an in-flight response

server = HTTPServer(('127.0.0.1', 0), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    for case in ('ok', 'error', 'eof', 'cancel', 'nonfinite', 'ignored-tool-end'):
        subprocess.run([sys.argv[1], f'http://127.0.0.1:{server.server_port}/{case}', case], check=True)
finally:
    server.shutdown()
    server.server_close()
    thread.join()
