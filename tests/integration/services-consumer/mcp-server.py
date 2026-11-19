"""Installed SDK's real stdio peer; no network or credentials."""
import json
import sys

initialized = False
for line in sys.stdin:
    message = json.loads(line)
    method = message['method']
    if method == 'notifications/initialized':
        initialized = True
        continue
    if method == 'initialize':
        assert message['params']['clientInfo']['name'] == 'PanGu'
        result = {'protocolVersion': message['params']['protocolVersion'],
                  'capabilities': {'tools': {}}, 'serverInfo': {'name': 'installed-peer', 'version': '1'}}
    elif method == 'tools/list':
        assert initialized
        result = {'tools': [{'name': 'echo', 'description': 'Installed SDK fixture',
                            'inputSchema': {'type': 'object', 'properties': {'text': {'type': 'string'}},
                                            'required': ['text'], 'additionalProperties': False}}]}
    elif method == 'tools/call':
        assert initialized and message['params'] == {'name': 'echo', 'arguments': {'text': 'SDK'}}
        result = {'content': [{'type': 'text', 'text': 'installed MCP reply'},
                              {'type': 'image', 'data': 'AAAA', 'mimeType': 'image/png'}],
                  'structuredContent': {'sdk': True}, 'isError': False}
    else:
        raise AssertionError(method)
    print(json.dumps({'jsonrpc': '2.0', 'id': message['id'], 'result': result}), flush=True)
