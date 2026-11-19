"""Deterministic MCP stdio peer: protocol assertions run in a real child."""
import json
import os
import pathlib
import signal
import subprocess
import sys
import time

mode, directory = sys.argv[1:]
root = pathlib.Path(directory)
if mode == 'normal':
    assert pathlib.Path.cwd() == root
    assert os.environ['PANGU_MCP_FIXTURE'] == 'controlled'
    assert 'PANGU_MCP_INHERITED' not in os.environ
log = root / (mode + '.jsonl')
(root / (mode + '.pid')).write_text(str(os.getpid()))
if mode == 'stubborn':
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    child = subprocess.Popen([sys.executable, '-c',
        'import signal,time,pathlib,sys;signal.signal(signal.SIGTERM,signal.SIG_IGN);'
        'time.sleep(1.5);pathlib.Path(sys.argv[1]).write_text("leaked");time.sleep(30)',
        str(root / 'escaped')], stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    (root / 'grandchild.pid').write_text(str(child.pid))

def send(value):
    data = (json.dumps(value, ensure_ascii=False) + '\r\n').encode()
    # Force fragmented UTF-8 and NDJSON across transport reads.
    for i in range(0, len(data), 7):
        os.write(1, data[i:i + 7])

def reply(mid, value):
    send({'jsonrpc': '2.0', 'id': mid, 'result': value})

names = ['echo', 'wait', 'rpc_error', 'tool_error', 'structured', 'bad_result', 'drop', 'image']
initialized = False
for line in sys.stdin:
    message = json.loads(line)
    with log.open('a') as f:
        f.write(json.dumps(message) + '\n')
    method, mid = message.get('method'), message.get('id')
    if method == 'initialize':
        assert message['params']['clientInfo']['name'] == 'PanGu'
        assert message['params']['capabilities'] == {}
        assert message['params']['protocolVersion'] == '2025-11-25'
        if mode == 'init-timeout':
            continue
        if mode == 'init-invalid':
            reply(mid, {'protocolVersion': '2025-11-25'})
            continue
        version = '1999-01-01' if mode == 'bad-version' else '2024-11-05'
        reply(mid, {'protocolVersion': version, 'capabilities': {'tools': {}},
                    'serverInfo': {'name': 'fixture', 'version': '1'},
                    'instructions': 'fixture only'})
        os.write(2, b'fixture stderr not protocol\n' * 10000)
    elif method == 'notifications/initialized':
        initialized = True
    elif method == 'tools/list':
        assert initialized
        if mode == 'list-invalid':
            reply(mid, {'tools': [{'name': 'missing schema'}]})
            continue
        cursor = message.get('params', {}).get('cursor')
        if mode == 'repeat-cursor':
            reply(mid, {'tools': [], 'nextCursor': 'same'})
            continue
        if mode == 'duplicate':
            reply(mid, {'tools': [{'name': 'echo', 'inputSchema': {}}] * 2})
            continue
        selected = names[:4] if cursor is None else names[4:]
        assert cursor is None or cursor == '第二页'
        tools = [{'name': name, 'description': 'MCP 测试 ' + name,
                  'inputSchema': {'type': 'object', 'properties': {'text': {'type': 'string'}},
                                  'additionalProperties': False}} for name in selected]
        reply(mid, {'tools': tools, 'nextCursor': '第二页' if cursor is None else ''})
    elif method == 'tools/call':
        assert initialized
        name = message['params']['name']
        if name == 'wait':
            continue
        if name == 'drop':
            os._exit(0)
        if name == 'rpc_error':
            send({'jsonrpc': '2.0', 'id': mid,
                  'error': {'code': 1234, 'message': 'fixture RPC error', 'data': {'retryable': False}}})
        elif name == 'tool_error':
            reply(mid, {'content': [{'type': 'text', 'text': 'fixture tool error'}], 'isError': True})
        elif name == 'structured':
            reply(mid, {'structuredContent': [1, None, '结构化']})
        elif name == 'bad_result':
            reply(mid, {'content': 'not an array'})
        elif name == 'image':
            reply(mid, {'content': [{'type': 'image', 'mimeType': 'image/png', 'data': 'aGVsbG8='},
                                   {'type': 'resource', 'resource': {'uri': 'fixture://text', 'text': 'resource text'}}]})
        else:
            assert name == 'echo'
            # Ignore malformed lines/unknown responses/notifications; still accept real response.
            os.write(1, b'not json\n' + b'x' * 9000 + b'\n\n')
            send({'jsonrpc': '2.0', 'id': 999999, 'result': {}})
            send({'jsonrpc': '2.0', 'method': 'notifications/progress', 'params': {'progress': 1}})
            reply(mid, {'content': [{'type': 'text', 'text': message['params']['arguments'].get('text', '你好 MCP')}],
                        'structuredContent': {'ok': True}})
    elif method == 'notifications/cancelled':
        # Late reply must not become the next request's result.
        reply(message['params']['requestId'], {'content': [{'type': 'text', 'text': 'late cancelled reply'}]})
if mode == 'stubborn':
    while True:
        time.sleep(1)
