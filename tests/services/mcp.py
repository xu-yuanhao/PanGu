import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

with tempfile.TemporaryDirectory(prefix='pangu MCP 中文 space ') as directory:
    root = pathlib.Path(directory)
    server = str(pathlib.Path(sys.argv[2]).resolve())
    try:
        subprocess.run([sys.argv[1], sys.executable, server, directory], check=True, timeout=20,
                       env=dict(os.environ, PANGU_MCP_INHERITED='must not reach child'))
    except BaseException:
        # A failed assertion must not leave a stubborn fixture/process group behind.
        for pidfile in root.glob('*.pid'):
            try:
                os.killpg(int(pidfile.read_text()), signal.SIGKILL)
            except ProcessLookupError:
                pass
        raise
    logs = [json.loads(line) for line in (root / 'normal.jsonl').read_text().splitlines()]
    assert [m.get('method') for m in logs[:4]] == ['initialize', 'notifications/initialized', 'tools/list', 'tools/list']
    calls = [m for m in logs if m.get('method') == 'tools/call']
    # Invalid arguments were rejected by production Schema before remote dispatch.
    assert all(m['params']['arguments'].get('text') != 17 for m in calls)
    cancels = [m for m in logs if m.get('method') == 'notifications/cancelled']
    assert len(cancels) == 2
    pending = {m['id']: m for m in calls if m['params']['name'] == 'wait'}
    assert {m['params']['requestId'] for m in cancels} == set(pending)
    assert {m['params']['reason'] for m in cancels} == {'Aborted', 'Request timed out'}
    init = [json.loads(line) for line in (root / 'init-timeout.jsonl').read_text().splitlines()]
    assert [m.get('method') for m in init] == ['initialize']
    # Main server is reaped; no descendant can perform a delayed write.
    for pidfile in root.glob('*.pid'):
        pid = int(pidfile.read_text())
        if pidfile.name == 'grandchild.pid':
            continue  # orphan zombie reaping is owned by this container's init
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            pass
        else:
            raise AssertionError('unreaped MCP child: ' + pidfile.name)
    time.sleep(1.7)
    assert not (root / 'escaped').exists()
    grandchild = pathlib.Path('/proc') / (root / 'grandchild.pid').read_text()
    if (grandchild / 'stat').exists():
        assert (grandchild / 'stat').read_text().split()[2] == 'Z'
    print('mcp peer: order, Schema preflight, timeout/cancel ids, reap and process-tree cleanup passed')

    example_root = root / 'example'
    example_root.mkdir()
    result = subprocess.run([sys.argv[3], 'echo', '{"text":"stdio example"}',
                             sys.executable, server, 'example', str(example_root)],
                            check=True, capture_output=True, text=True, timeout=10)
    assert result.stdout == 'stdio example\n'
