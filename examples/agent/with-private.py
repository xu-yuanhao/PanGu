"""Inject private test credentials into a child process, without shell evaluation.
Usage: python3 examples/agent/with-private.py .private/model-credentials.md -- COMMAND ...
Only explicit key/model/endpoint fields and sk- tokens are accepted. No values
are printed. The child inherits a process-only environment; no env file is written.
"""
import os
from pathlib import Path
import re
import subprocess
import sys


def environment(path):
    text = Path(path).read_text()
    result = os.environ.copy()
    names = {'DEEPSEEK_API_KEY', 'PANGU_DEEPSEEK_ENDPOINT', 'PANGU_MODEL'}
    for line in text.splitlines():
        match = re.match(r'^\s*(?:export\s+)?([A-Z_]+)\s*=\s*(.*?)\s*$', line)
        if match and match[1] in names:
            result[match[1]] = match[2].strip('\'"')
    if 'DEEPSEEK_API_KEY' not in result:
        match = re.search(r'\bsk-[A-Za-z0-9_-]+', text)
        if match:
            result['DEEPSEEK_API_KEY'] = match[0]
        else:
            match = re.search(r'(?im)^\s*(?:api[_ -]?key|密钥)\s*[:：]\s*[`\'"]?([^\s`\'"]+)', text)
            if match:
                result['DEEPSEEK_API_KEY'] = match[1]
    if 'PANGU_DEEPSEEK_ENDPOINT' not in result:
        match = re.search(r'https?://[^\s<>`\'"]+', text)
        if match:
            endpoint = match[0].rstrip('/')
            if not endpoint.endswith('/chat/completions'):
                endpoint += '/chat/completions'
            result['PANGU_DEEPSEEK_ENDPOINT'] = endpoint
    if 'PANGU_MODEL' not in result:
        match = re.search(r'\bdeepseek-(?:chat|reasoner)\b', text)
        result['PANGU_MODEL'] = match[0] if match else 'deepseek-chat'
    if not result.get('DEEPSEEK_API_KEY'):
        raise ValueError('private credential file has no supported API key field')
    return result


if __name__ == '__main__':
    if len(sys.argv) < 5 or sys.argv[2] != '--':
        sys.exit('usage: with-private.py PRIVATE_FILE -- COMMAND ...')
    try:
        env = environment(sys.argv[1])
        status = subprocess.call(sys.argv[3:], env=env)
    except (OSError, ValueError):
        sys.exit('private credential injection failed (values withheld)')
    sys.exit(status)
