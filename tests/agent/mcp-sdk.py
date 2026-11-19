import os
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='PanGu SDK MCP 中文 ') as directory:
    env = os.environ.copy()
    env['PANGU_MCP_FIXTURE'] = 'controlled'
    env.pop('PANGU_MCP_INHERITED', None)
    subprocess.run([sys.argv[1], sys.executable, sys.argv[2], directory], check=True, timeout=10, env=env)
    for pid in Path(directory).glob('*.pid'):
        try:
            os.kill(int(pid.read_text()), 0)
        except ProcessLookupError:
            continue
        raise AssertionError('SDK close did not reap peer')
