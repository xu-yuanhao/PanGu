"""Offline protocol consumer: no external requests or real credentials."""
import http.server
import json
import subprocess
import sys
import threading

class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    def handle_error(self, *_):
        pass

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass
    def do_POST(self):
        payload = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        assert payload['hooked'] is True
        assert payload['model'] == 'deepseek-chat'
        assert payload['thinking']['type'] == 'enabled'
        second = any(m['role'] == 'tool' for m in payload['messages'])
        if self.path == '/empty' or (self.path == '/replace' and second):
            assert 'tools' not in payload or payload['tools'] == []
        else:
            names = [t['function']['name'] for t in payload['tools']]
            assert names == (['runtime_echo'] if self.path == '/replace' else ['echo'])
        assert self.headers['Authorization'] == ('Bearer fixture-two' if second else 'Bearer fixture-one')
        if second:
            assert any(m.get('tool_calls') for m in payload['messages'])
            assert next(m for m in payload['messages'] if m['role']=='tool')['content'] == 'pong'
        status = {'/error': 429, '/too-large': 413, '/bad-request': 400}.get(self.path, 200)
        error = status != 200
        self.send_response(status)
        self.send_header('Content-Type', 'text/event-stream')
        self.send_header('x-request-id', 'redacted-fixture')
        self.end_headers()
        if error:
            self.wfile.write(b'{"error":{"message":"fixture-one secret prompt context_overflow 413"}}')
            return
        chunks = ([{'choices':[{'delta':{'content':'done'},'finish_reason':'stop'}]}, {'choices':[], 'usage':{'prompt_tokens':10,'prompt_cache_hit_tokens':3,'completion_tokens':2,'total_tokens':12,'completion_tokens_details':{'reasoning_tokens':0}}}] if second else [
            {'choices':[{'delta':{'reasoning_content':'think'}}]},
            {'choices':[{'delta':{'content':'checking'}}]},
            {'choices':[{'delta':{'tool_calls':[{'index':0,'id':'call-1','function':{'name':'echo','arguments':'{"value":'}}]}}]},
            {'choices':[{'delta':{'tool_calls':[{'index':0,'function':{'arguments':'"ping"}'}}]},'finish_reason':'tool_calls'}]},
        ])
        if self.path == '/replace' and not second:
            chunks[2]['choices'][0]['delta']['tool_calls'][0]['function']['name'] = 'runtime_echo'
        if self.path == '/empty':
            chunks = [{'choices':[{'delta':{'content':'no tools'},'finish_reason':'stop'}]}]
        if self.path in ('/metadata', '/metadata-cancel', '/metadata-error', '/metadata-default'):
            reason = 'content_filter' if self.path == '/metadata-error' else 'length'
            chunks = [
                {'id':'', 'model':'deepseek-chat', 'choices':[]},
                {'id':'response-首', 'model':'', 'choices':[]},
                {'id':'response-later', 'model':'served-模型', 'choices':[{'delta':{'content':'metadata'}}]},
                {'id':'response-last', 'model':'served-later', 'choices':[{'delta':{}, 'finish_reason':reason}]},
                {'choices':[], 'usage':{'prompt_tokens':1,'completion_tokens':1,'total_tokens':2}},
            ]
            if self.path == '/metadata-default':
                chunks = [{'model':'deepseek-chat','choices':[{'delta':{'content':'default'},'finish_reason':'stop'}]}]
        if self.path == '/truncated':
            chunks = chunks[:1]
        for c in chunks:
            data = ('data: '+json.dumps(c)+'\r\n\r\n').encode()
            # Fragment across CRLF, UTF-8 and JSON token boundaries.
            for i in range(0,len(data),7):
                self.wfile.write(data[i:i+7])
                self.wfile.flush()
        self.wfile.write(b'data: [DONE]\n\n')

server = Server(('127.0.0.1',0),Handler)
threading.Thread(target=server.serve_forever,daemon=True).start()
try:
    for case in ['ok','cancel','deny','error','truncated','replace','empty','too-large','bad-request','metadata','metadata-default','metadata-cancel','metadata-error']:
        url = f'http://127.0.0.1:{server.server_port}/'+case
        subprocess.run([sys.argv[1],url,case],check=True,timeout=10)
finally:
    server.shutdown()
