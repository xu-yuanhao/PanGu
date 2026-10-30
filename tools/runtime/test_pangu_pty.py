"""Actual Durable product PTY: terminal lifetime, coding tools and crash recovery."""
from pathlib import Path
import argparse
import json
import os
import pty
import re
import select
import signal
import subprocess
import tempfile
import termios
import time
from build_pangu import build

class Terminal:
    def __init__(self, binary, workspace, env):
        self.master, self.slave = pty.openpty()
        self.saved = termios.tcgetattr(self.slave)
        self.output = b''
        self.secret = env.get('PANGU_MODEL_API_KEY', env.get('MIMO_API_KEY', '')).encode()
        self.process = subprocess.Popen([str(binary), str(workspace)], stdin=self.slave,
                                        stdout=self.slave, stderr=self.slave, env=env)
    def until(self, marker, timeout=80):
        deadline = time.monotonic() + timeout
        while marker not in self.output:
            if time.monotonic() >= deadline:
                tail = self.output[-1000:]
                if self.secret:
                    tail = tail.replace(self.secret, b'[REDACTED]')
                raise AssertionError('PTY output marker not received within deadline: ' + repr(tail))
            readable, _, _ = select.select([self.master], [], [], 0.1)
            if readable:
                data = os.read(self.master, 8192)
                if not data:
                    raise AssertionError('PTY closed before expected marker')
                self.output += data
                if len(self.output) > 262144:
                    raise AssertionError('PTY transcript limit exceeded')
            if marker == b'[run] finished: success' and b'[run] finished: error' in self.output:
                raise AssertionError('actual run ended with error: ' + repr(self.output[-500:].replace(self.secret, b'[REDACTED]') if self.secret else self.output[-500:]))
            if self.process.poll() is not None and not readable:
                raise AssertionError('process exited ('+str(self.process.returncode)+') before expected marker: '+repr(self.output[-2000:].replace(self.secret,b'[REDACTED]') if self.secret else self.output[-2000:]))
    def send(self, data):
        os.write(self.master, data)
    def finish(self, expected=0):
        code = self.process.wait(timeout=8)
        assert code == expected, code
        assert termios.tcgetattr(self.slave) == self.saved, 'terminal attributes were not restored'
    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=5)
        os.close(self.master)
        os.close(self.slave)

def stored_bytes(directory):
    return b''.join(p.read_bytes() for p in sorted(directory.iterdir()) if p.is_file())

def terminal_exit(binary, root, env):
    for mode in ('ctrl-d', 'signal', 'resize'):
        terminal = Terminal(binary, root, env)
        try:
            terminal.until(b'> ', 5)
            assert termios.tcgetattr(terminal.slave) != terminal.saved
            if mode == 'signal':
                terminal.process.send_signal(signal.SIGTERM)
            else:
                if mode == 'resize':
                    terminal.process.send_signal(signal.SIGWINCH)
                terminal.send(b'\x04')
            terminal.finish()
        finally:
            terminal.close()
    directory = root / '.pangu'
    original = stored_bytes(directory)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert stored_bytes(directory) == original, 'idle reopen changed committed facts'
    assert b'offline-placeholder' not in original
    for name, content in [('corrupt', b'not-json\n'), ('torn', b'{')]:
        location = root / name
        location.mkdir()
        (location / 'main.jsonl').write_bytes(content)
        terminal = Terminal(binary, root, dict(env, PANGU_SESSION_DIR=str(location)))
        try:
            if name == 'corrupt':
                terminal.until(b'Cannot activate native plugin', 5)
                terminal.finish(2)
            else:
                terminal.until(b'> ', 5)
                terminal.send(b'\x04')
                terminal.finish()
        finally:
            terminal.close()
    print('PASS: Durable default .pangu create/reopen, idle stability, corruption/torn tail and terminal restoration', flush=True)

def run_input(terminal, text, after_submit=None):
    terminal.output = b''
    terminal.send(text.encode() + b'\r')
    if after_submit is not None:
        after_submit()
    terminal.until(b'[run] finished: success', 160)
    return terminal.output

def workflow(binary, root, env):
    marker = 'PANGUPTY' + os.urandom(6).hex()
    (root / 'source.txt').write_text(marker + '\n')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Read source.txt using read. Write its exact content to target.txt using write. Then use edit to append the literal suffix _EDIT to its first line. Run bash command cat target.txt to verify the result. Finish by replying with the final file content. Use all four tools.')
        assert (root / 'target.txt').read_text() == marker + '_EDIT\n'
        for tool in (b'read', b'write', b'edit', b'bash'):
            assert b'[tool] ' + tool in re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', output), ('missing actual tool', tool)
        assert marker.encode() in output and b'_EDIT' in output
        output = run_input(terminal, 'Use bash to run exactly this command: `printf PANGU_EXIT_CHECK; exit 7`. Then report the exit code. Do not repeat the command.')
        assert b'7' in output and b'PANGU_EXIT_CHECK' in output
        output = run_input(terminal, 'What product are you? Answer only its name.')
        assert b'PanGu' in output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    directory = Path(env.get('PANGU_SESSION_DIR', str(root / '.pangu')))
    saved = stored_bytes(directory)
    secret = env.get('PANGU_MODEL_API_KEY', env.get('MIMO_API_KEY', '')).encode()
    assert secret and secret not in saved
    def records(value):
        if isinstance(value, dict):
            if value.get('kind') in ('pangu.tool-result', 'pangu.assistant'):
                yield value
            for child in value.values():
                yield from records(child)
        elif isinstance(value, list):
            for child in value:
                yield from records(child)
    edits = []
    failed_bash = []
    model_results = []
    for path in directory.iterdir():
        if path.is_file():
            for line in path.read_bytes().splitlines():
                try:
                    value = json.loads(line)
                except (ValueError, UnicodeDecodeError):
                    continue
                for entry in records(value):
                    message = entry['model'][0]
                    if entry['kind'] == 'pangu.assistant':
                        model_results.append(message)
                        continue
                    if message['toolName'] == 'edit' and not message['isError']:
                        edits.append(entry)
                    if message['toolName'] == 'bash' and message['isError']:
                        failed_bash.append(entry)
    assert model_results and all(m['stopReason'] in ('stop', 'toolUse', 'length') for m in model_results)
    assert all(m['usage']['totalTokens'] > 0 and m['usage']['output'] > 0 for m in model_results)
    assert edits, 'missing committed actual edit receipt'
    assert any(e['data']['structuredOutput']['exitCode'] == 7
               and any(d['code'] == 'exit_code' for d in e['data']['diagnostics'])
               and any('[error] Command exited with code 7' in b.get('text', '') for b in e['model'][0]['content'])
               for e in failed_bash), 'missing actual nonzero bash diagnostics'
    assert all(e['data']['status'] == 'completed' and e['data']['arguments']['path'] == 'target.txt'
               and e['model'][0]['details']['firstChangedLine'] == 1 for e in edits)
    assert all(e['model'][0]['details']['diff'] and e['model'][0]['details']['patch'] for e in edits)
    subprocess.run(['git', 'apply', '--reverse', '--check', '-'], cwd=root,
                   input=edits[-1]['model'][0]['details']['patch'].encode(), check=True,
                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        assert marker.encode() in terminal.output and b'[history]' in terminal.output
        assert b'[run] started' not in terminal.output
        assert stored_bytes(directory) == saved
        output = run_input(terminal, 'Without tools, quote the first line of source.txt that you read earlier.')
        assert marker.encode() in output, repr(output[-4000:].replace(secret,b'[REDACTED]'))
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual model/TUI read-write-edit-bash, PanGu identity, committed history and resumed model context', flush=True)

def recovery(binary, root, env, crash=False):
    directory = root / ('crash-session' if crash else 'detach-session')
    env = dict(env, PANGU_SESSION_DIR=str(directory))
    target = root / ('crash-effects.txt' if crash else 'detach-effects.txt')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        child_pid = root / 'crash-child.pid'
        command = (f'printf "%s" "$$" > {child_pid.name}; ' if crash else '') + f'printf once >> {target.name}; printf PANGU_EFFECT_STARTED; sleep 30; printf late >> {target.name}'
        terminal.send(('Use bash exactly once to run: '+json.dumps(command)+'. If interrupted, never repeat it. Report the result briefly.').encode()+b'\r')
        # Tool previews may buffer a source line until LF; observe the actual
        # side effect while draining the PTY instead of synchronizing on text.
        deadline = time.monotonic()+60
        while not target.exists() or target.read_bytes()!=b'once':
            assert time.monotonic()<deadline, 'command effect was not observed: '+repr(target.read_bytes() if target.exists() else None)
            if target.exists():
                assert target.read_bytes() in (b'',b'once'), 'unexpected command effect before exit: '+repr(target.read_bytes())
            readable, _, _ = select.select([terminal.master], [], [], .1)
            if readable:
                terminal.output += os.read(terminal.master,8192)
        if crash:
            terminal.process.kill()
            terminal.process.wait(timeout=5)
            # External shell effects outlive SIGKILL; reap this test's process group.
            if child_pid.exists():
                try:
                    os.killpg(int(child_pid.read_text()), signal.SIGKILL)
                except ProcessLookupError:
                    pass
        else:
            terminal.send(b'\x04')
            terminal.finish()
    finally:
        terminal.close()
    assert target.read_bytes()==b'once', ('effect after exit',target.read_bytes())
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'[run] finished: success',160)
        assert b'interrupted' in terminal.output
        assert target.read_bytes()==b'once', 'unsafe command was replayed'
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: Durable '+('process death' if crash else 'cooperative exit')+' and unsafe-tool recovery without repeating effects',flush=True)

def configuration_recovery(binary, root, environment):
    root = root / 'configuration-recovery'
    root.mkdir()
    env = dict(environment)
    env['PANGU_MODEL_OPTIONS'] = json.dumps({'maxTokens': 768, 'temperature': 0})
    directory = root / '.pangu'
    marker = 'PANGUSAVED' + os.urandom(6).hex()
    def facts(kind):
        def walk(value):
            if isinstance(value, dict):
                if value.get('kind') == kind:
                    yield value
                for child in value.values():
                    yield from walk(child)
            elif isinstance(value, list):
                for child in value:
                    yield from walk(child)
        if directory.exists():
            for path in directory.iterdir():
                if path.is_file():
                    for line in path.read_bytes().splitlines():
                        try:
                            value = json.loads(line)
                        except (ValueError, UnicodeDecodeError):
                            continue
                        yield from walk(value)
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        terminal.send(('Reply with exactly this token and nothing else: ' + marker).encode() + b'\r')
        deadline = time.monotonic() + 15
        saved = None
        while time.monotonic() < deadline:
            for task in facts('pangu.generation'):
                state = task.get('state', {})
                checkpoint = state.get('checkpoint', {})
                if state.get('status') == 'running' and checkpoint.get('phase') == 'request':
                    saved = checkpoint['configuration']
                    break
            if saved is not None:
                terminal.process.kill()
                terminal.process.wait(timeout=5)
                break
            time.sleep(.005)
        assert saved and saved['model'] == env['PANGU_MODEL_NAME']
        assert saved['endpoint'] == env['PANGU_MODEL_ENDPOINT']
        assert saved['options'] == {'maxTokens': 768, 'temperature': 0}
    finally:
        terminal.close()
    assert not list(facts('pangu.assistant')), 'model completed before the request interruption'
    changed = dict(env)
    changed.update(PANGU_MODEL_NAME='changed-model',
                   PANGU_MODEL_ENDPOINT='https://changed.invalid/chat/completions',
                   PANGU_MODEL_OPTIONS='{"maxTokens":1536,"temperature":1}')
    terminal = Terminal(binary, root, changed)
    try:
        terminal.until(b'[run] finished: success', 160)
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert any(''.join(b.get('text', '') for m in e['model'] for b in m.get('content', [])).strip() == marker
               for e in facts('pangu.assistant')), 'saved request did not produce the actual model answer'
    print('PASS: interrupted request reopens with saved model/endpoint/options despite changed environment (actual model)', flush=True)

def path_workflow(binary, root, environment):
    workspace = root / 'posix-path-workflow'
    shared = root / 'posix-path-shared'
    workspace.mkdir()
    shared.mkdir()
    marker = 'PANGUPOSIX' + os.urandom(5).hex()
    (shared / 'space source.txt').write_text(marker + '\n')
    (workspace / 'alias.txt').symlink_to(shared / 'target.txt')
    env = dict(environment, HOME=str(shared))
    terminal = Terminal(binary, workspace, env)
    try:
        terminal.until(b'> ', 5)
        read_path = (shared / 'space source.txt').as_uri()
        write_path = '~/missing-prefix/../target.txt'
        edit_path = '@./alias.txt'
        output = run_input(terminal, 'Use all four tools and preserve the following literal path arguments exactly: read path=' + read_path + '; write path=' + write_path + ' to copy the first line; edit path=' + edit_path + ' to append _PATH to that line through the symbolic link. Finally bash command cat ../posix-path-shared/target.txt and reply with the exact final content. HOME is ' + str(shared) + '.')
        assert (shared / 'target.txt').read_text().strip() == marker + '_PATH'
        assert (workspace / 'alias.txt').is_symlink()
        assert not (shared / 'missing-prefix').exists()
        assert (marker + '_PATH').encode() in output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    def results(value):
        if isinstance(value, dict):
            if value.get('kind') == 'pangu.tool-result':
                yield value
            for child in value.values():
                yield from results(child)
        elif isinstance(value, list):
            for child in value:
                yield from results(child)
    saved = []
    for path in (workspace / '.pangu').iterdir():
        if path.is_file():
            for line in path.read_bytes().splitlines():
                try:
                    saved.extend(results(json.loads(line)))
                except (ValueError, UnicodeDecodeError):
                    pass
    for name, path in [('read', read_path), ('write', write_path), ('edit', edit_path)]:
        assert any(result.get('model', [{}])[0].get('toolName') == name and result.get('data', {}).get('arguments', {}).get('path') == path for result in saved), (name, 'canonical persisted arguments missing')
    print('PASS: real model file URL read, home/lexical write without extra mkdir, pasted-path symlink edit, bash and unchanged committed arguments', flush=True)

def configured_resource_workflow(binary, root, environment):
    workspace = root / 'configured-workflow'
    workspace.mkdir()
    private = workspace / '.pangu'
    private.mkdir()
    agent = root / 'configured-agent'
    extras = agent / 'extras'
    extras.mkdir(parents=True)
    prompts = agent / 'templates'
    prompts.mkdir()
    marker = 'PANGUCONFIG' + os.urandom(5).hex()
    (extras / 'guide.md').write_text('---\nname: config-recipe\ndescription: Read the configured private payload and create configured.txt.\n---\nUse read to inspect payload.txt relative to this skill directory. Write configured.txt in the workspace with that exact content followed by _CONFIG. Reply with that final content.\n')
    (extras / 'payload.txt').write_text(marker)
    (prompts / 'user-recipe.md').write_text('Use write to create user-result.txt containing exactly "$1" without the quotes. Reply with its exact content.\n')
    blocked_skill = agent / 'skills' / 'blocked'
    blocked_skill.mkdir(parents=True)
    (blocked_skill / 'SKILL.md').write_text('---\nname: config-recipe\ndescription: WRONG_BLOCKED_USER_SKILL\n---\nWrite WRONG to configured.txt.\n')
    blocked_prompts = agent / 'prompts'
    blocked_prompts.mkdir()
    (blocked_prompts / 'user-recipe.md').write_text('Write WRONG to user-result.txt.\n')
    (agent / 'settings.json').write_text(json.dumps({'skills':['extras/guide.md','!**','+extras/guide.md'],'prompts':['templates','!*.md','+templates/user-recipe.md']}))
    team = workspace / 'team'
    team.mkdir()
    (team / 'SKILL.md').write_text('---\nname: project-team\ndescription: Project-configured team instructions.\n---\nProject team body.\n')
    (workspace / 'project-recipe.md').write_text('Use write to create project-result.txt containing exactly "$1" without the quotes. Reply with its exact content.\n')
    (private / 'settings.json').write_text(json.dumps({'skills':['../team'],'prompts':['../project-recipe.md','!*.md','+../project-recipe.md']}))
    env = dict(environment, PANGU_AGENT_DIR=str(agent))
    env.pop('PANGU_SYSTEM_PROMPT_FILE', None)
    terminal = Terminal(binary, workspace, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Use the config-recipe skill to create configured.txt and report the result.')
        assert (workspace / 'configured.txt').read_text().strip() == marker + '_CONFIG'
        assert (marker + '_CONFIG').encode() in output
        for command, filename, suffix in [('user-recipe','user-result.txt','_USER'),('project-recipe','project-result.txt','_PROJECT')]:
            value = marker + suffix
            output = run_input(terminal, '/' + command + ' ' + value)
            assert (workspace / filename).read_text().strip() == value
            assert value.encode() in output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual scoped selection, blocked same-name defaults, forced skill/template includes and user/project model workflows', flush=True)

def template_workflow(binary, root, environment):
    workspace = root / 'template-workflow'
    workspace.mkdir()
    agent = root / 'template-user-agent'
    prompts = agent / 'prompts'
    prompts.mkdir(parents=True)
    (prompts / 'recipe.md').write_text('---\ndescription: Write and verify a supplied value\nargument-hint: "[value]"\n---\nUse write to create template.txt containing exactly "$1" without the surrounding quotes, then bash to read template.txt. Reply with its exact contents.\n')
    project = workspace / '.pangu' / 'prompts'
    project.mkdir(parents=True)
    (project / 'recipe.md').write_text((prompts / 'recipe.md').read_text())
    (prompts / 'recipe.md').write_text('Write WRONG_USER_TEMPLATE to template.txt.')
    skill = agent / 'skills' / 'manual'
    skill.mkdir(parents=True)
    header = '---\nname: manual\ndescription: Disabled automatic recipe\ndisable-model-invocation: true\n---\n'
    (skill / 'SKILL.md').write_text(header + 'Stale initial body.\n')
    env = dict(environment, PANGU_AGENT_DIR=str(agent))
    env.pop('PANGU_SYSTEM_PROMPT_FILE', None)
    marker = 'PANGUTEMPLATE' + os.urandom(5).hex() + ' quoted $1'
    manual = 'PANGUMANUAL' + os.urandom(5).hex()
    terminal = Terminal(binary, workspace, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, "/recipe '" + marker + "'")
        assert (workspace / 'template.txt').read_text().strip() == marker
        assert marker.encode() in output
        (skill / 'SKILL.md').write_text(header + 'Use write to create manual.txt containing exactly ' + manual + '. Then report its exact contents.\n')
        output = run_input(terminal, '/skill:manual Also use bash to read manual.txt after writing.')
        assert (workspace / 'manual.txt').read_text().strip() == manual
        assert manual.encode() in output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    def users(value):
        if isinstance(value, dict):
            if value.get('kind') == 'pangu.user':
                yield value
            for child in value.values():
                yield from users(child)
        elif isinstance(value, list):
            for child in value:
                yield from users(child)
    saved = []
    for path in (workspace / '.pangu').iterdir():
        if path.is_file():
            for line in path.read_bytes().splitlines():
                try:
                    saved.extend(users(json.loads(line)))
                except (ValueError, UnicodeDecodeError):
                    pass
    contents = [value.get('model', [{}])[0].get('content', '') for value in saved]
    assert any(isinstance(text, str) and marker in text and not text.startswith('/recipe') for text in contents)
    assert any(isinstance(text, str) and text.startswith('<skill name="manual"') and manual in text and 'Also use bash' in text for text in contents)
    print('PASS: actual template command, project-first recipe, literal placeholder arguments and fresh explicit disabled skill body committed through Durable', flush=True)

def skill_workflow(binary, root, environment):
    workspace = root / 'skill-workflow'
    workspace.mkdir()
    agent = root / 'skill-user-agent'
    skill = agent / 'skills' / 'verification-recipe'
    skill.mkdir(parents=True)
    marker = 'PANGUSKILL' + os.urandom(5).hex()
    (skill / 'SKILL.md').write_text('---\nname: verification-recipe\ndescription: Apply the verification recipe to create result.txt from its private payload.\n---\nRead payload.txt relative to this skill directory using read. Use write to create result.txt in the current workspace containing exactly that payload followed by _SKILL. Then report the final content.\n')
    (skill / 'payload.txt').write_text(marker)
    project = workspace / '.pangu' / 'skills' / 'verification-recipe'
    project.mkdir(parents=True)
    (project / 'SKILL.md').write_text((skill / 'SKILL.md').read_text())
    (skill / 'payload.txt').rename(project / 'payload.txt')
    (skill / 'SKILL.md').write_text('---\nname: verification-recipe\ndescription: WRONG_USER_SKILL\n---\nWrong duplicate.\n')
    skill = project
    hidden = project.parent / 'hidden'
    hidden.mkdir()
    (hidden / 'SKILL.md').write_text('---\nname: hidden\ndescription: DISABLED_SKILL_DESCRIPTION\ndisable-model-invocation: true\n---\nHidden.\n')
    env = dict(environment, PANGU_AGENT_DIR=str(agent))
    env.pop('PANGU_SYSTEM_PROMPT_FILE', None)
    saved = []
    def visit(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == 'systemPrompt' and isinstance(child, str):
                    yield child
                else:
                    yield from visit(child)
        elif isinstance(value, list):
            for child in value:
                yield from visit(child)
    def capture():
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            for path in (workspace / '.pangu').iterdir():
                if path.is_file():
                    try:
                        for line in path.read_bytes().splitlines():
                            try:
                                saved.extend(visit(json.loads(line)))
                            except (ValueError, UnicodeDecodeError):
                                pass
                    except FileNotFoundError:
                        pass
            if saved:
                return
            time.sleep(0.01)
        raise AssertionError('skill request configuration not captured')
    terminal = Terminal(binary, workspace, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Use the verification recipe skill to create result.txt. Follow its instructions and report the result.', capture)
        assert (workspace / 'result.txt').read_text().strip() == marker + '_SKILL'
        assert (marker + '_SKILL').encode() in output
        assert saved and any(str(skill / 'SKILL.md') in prompt for prompt in saved)
        assert all('WRONG_USER_SKILL' not in prompt and 'DISABLED_SKILL_DESCRIPTION' not in prompt and marker not in prompt for prompt in saved)
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    def receipts(value):
        if isinstance(value, dict):
            if value.get('kind') == 'pangu.tool-result':
                yield value
            for child in value.values():
                yield from receipts(child)
        elif isinstance(value, list):
            for child in value:
                yield from receipts(child)
    results = []
    for path in (workspace / '.pangu').iterdir():
        if path.is_file():
            for line in path.read_bytes().splitlines():
                try:
                    results.extend(receipts(json.loads(line)))
                except (ValueError, UnicodeDecodeError):
                    pass
    for path in [skill / 'SKILL.md', skill / 'payload.txt']:
        assert any(result.get('model', [{}])[0].get('toolName') == 'read' and result.get('data', {}).get('arguments', {}).get('path') == str(path) for result in results), 'skill-relative read missing'
    print('PASS: actual model skill discovery, project-first collision, disabled visibility, on-demand skill/payload reads and workspace write', flush=True)

def thinking_workflow(binary, root, environment):
    root = root / 'thinking-workflow'
    root.mkdir()
    env = dict(environment)
    env['PANGU_MODEL_OPTIONS'] = '{"maxTokens":768,"reasoningEffort":"high"}'
    marker = 'PANGUTHINK' + os.urandom(5).hex()
    (root / 'source.txt').write_text(marker + '\n')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        answer = run_input(terminal, 'Compute 17 times 23. Think carefully and answer only the result. Do not use tools.')
        assert b'391' in answer
        answer = run_input(terminal, 'Use read to inspect source.txt. Answer only the exact file content.')
        assert marker.encode() in answer
        answer = run_input(terminal, 'Without tools, repeat the file content you just read. Nothing else.')
        assert marker.encode() in answer
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    def thinking_blocks(value):
        if isinstance(value, dict):
            if value.get('type') == 'thinking' and value.get('thinkingSignature') == 'reasoning_content' and value.get('thinking'):
                yield value
            for child in value.values():
                yield from thinking_blocks(child)
        elif isinstance(value, list):
            for child in value:
                yield from thinking_blocks(child)
    blocks = []
    for path in (root / '.pangu').iterdir():
        if path.is_file():
            for line in path.read_bytes().splitlines():
                try:
                    blocks.extend(thinking_blocks(json.loads(line)))
                except (ValueError, UnicodeDecodeError):
                    pass
    assert blocks, 'actual model did not yield committed signed reasoning blocks'
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        answer = run_input(terminal, 'Without tools, repeat the remembered file content. Nothing else.')
        assert marker.encode() in answer
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: actual thinking model/read Tool, subsequent request and reopened context preserve reasoning blocks', flush=True)

def resource_workflow(binary, root, environment):
    ancestor_marker = 'PANGUANCESTOR' + os.urandom(5).hex()
    (root / 'AGENTS.md').write_text('The ancestor marker is WRONG_PARENT_PRIORITY.')
    (root / 'AGENTS.override.md').write_text('The ancestor marker is ' + ancestor_marker + '.')
    root = root / 'resource-workflow'
    root.mkdir()
    private = root / '.pangu'
    private.mkdir()
    marker = 'PANGURESOURCE' + os.urandom(5).hex()
    append = 'PANGUAPPEND' + os.urandom(5).hex()
    (private / 'SYSTEM.md').write_text('You are PanGu, a coding assistant. Follow the project instructions. Answer concisely.')
    (private / 'APPEND_SYSTEM.md').write_text('The append token is ' + append + '.')
    (root / 'AGENTS.md').write_text('The project marker is WRONG_PRIORITY.')
    (root / 'AGENTS.override.md').write_text('The project marker is ' + marker + '.')
    env = dict(environment)
    env.pop('PANGU_SYSTEM_PROMPT_FILE', None)
    agent_dir = root.parent / 'user-agent-resources'
    agent_dir.mkdir()
    user_marker = 'PANGUUSERCONTEXT' + os.urandom(5).hex()
    user_prefix = 'PANGUUSERPREFIX' + os.urandom(5).hex()
    user_append = 'PANGUUSERAPPEND' + os.urandom(5).hex()
    (agent_dir / 'SYSTEM.md').write_text('You are PanGu. The user prefix token is ' + user_prefix + '.')
    (agent_dir / 'APPEND_SYSTEM.md').write_text('The append token is ' + user_append + '.')
    (agent_dir / 'AGENTS.md').write_text('The user context marker is ' + user_marker + '.')
    env['PANGU_AGENT_DIR'] = str(agent_dir)
    def prompts(value):
        if isinstance(value, dict):
            for key, child in value.items():
                if key == 'systemPrompt' and isinstance(child, str):
                    yield child
                else:
                    yield from prompts(child)
        elif isinstance(value, list):
            for child in value:
                yield from prompts(child)
    saved = []
    def capture_request(directory=private):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            found = []
            for path in directory.iterdir():
                if path.is_file():
                    try:
                        for line in path.read_bytes().splitlines():
                            try:
                                found.extend(prompts(json.loads(line)))
                            except (ValueError, UnicodeDecodeError):
                                pass
                    except FileNotFoundError:
                        pass
            if found:
                saved.extend(found)
                return found
            time.sleep(0.01)
        raise AssertionError('in-flight persisted request configuration not found')
    for reopen in (False, True):
        terminal = Terminal(binary, root, env)
        try:
            terminal.until(b'> ', 5)
            output = run_input(terminal, 'Report the project marker, user context marker, ancestor marker and append token exactly. Use no tools.', capture_request)
            assert marker.encode() in output and append.encode() in output and user_marker.encode() in output and ancestor_marker.encode() in output
            assert b'WRONG_PRIORITY' not in output and b'WRONG_PARENT_PRIORITY' not in output
            terminal.send(b'\x04')
            terminal.finish()
        finally:
            terminal.close()
    fallback = root.parent / 'resource-user-fallback'
    fallback.mkdir()
    terminal = Terminal(binary, fallback, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Report the user context marker, user prefix token and append token exactly. Use no tools.')
        assert all(token.encode() in output for token in (user_marker, user_prefix, user_append))
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert saved and all(user_prefix not in prompt and user_append not in prompt and user_marker in prompt and ancestor_marker in prompt and prompt.index(user_marker) < prompt.index(ancestor_marker) < prompt.index(marker) and 'WRONG_PARENT_PRIORITY' not in prompt for prompt in saved)
    main_repo = root.parent / 'resource-main'
    subprocess.run(['git','init','-q',str(main_repo)],check=True,capture_output=True)
    subprocess.run(['git','-C',str(main_repo),'-c','user.name=PanGu','-c','user.email=pangu@example.invalid','-c','core.hooksPath=/dev/null','commit','-q','--allow-empty','-m','initial'],check=True,capture_output=True)
    worktree = main_repo / 'worktrees' / 'feature'
    subprocess.run(['git','-C',str(main_repo),'worktree','add','-q','-b','feature',str(worktree)],check=True,capture_output=True)
    checkout_marker = 'PANGUWORKTREE' + os.urandom(5).hex()
    (main_repo / 'AGENTS.md').write_text('The checkout marker is WRONG_MAIN_WORKTREE.')
    (worktree / 'AGENTS.md').write_text('The checkout marker is ' + checkout_marker + '.')
    nested = worktree / 'src'
    nested.mkdir()
    worktree_saved = []
    terminal = Terminal(binary, nested, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Report the checkout marker and user context marker exactly. Use no tools.', lambda: worktree_saved.extend(capture_request(nested / '.pangu')))
        assert checkout_marker.encode() in output and user_marker.encode() in output and b'WRONG_MAIN_WORKTREE' not in output
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    assert worktree_saved and all(checkout_marker in prompt and 'WRONG_MAIN_WORKTREE' not in prompt for prompt in worktree_saved)
    print('PASS: user/project priority, ordered inherited AGENTS, saved prompt, fallback/reopen and actual nested Git worktree context shadow', flush=True)

def plugin_workflow(binary, root, environment):
    root = root / 'plugin-workflow'
    root.mkdir()
    library = root / 'fixture.so'
    receipt = root / 'released.txt'
    source = Path(__file__).resolve().parents[2] / 'tests/runtime/product_plugin_fixture.c'
    subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic',
                    '-fPIC', '-shared', str(source), *subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True).split(), '-o', str(library)], check=True)
    env = dict(environment)
    env.update(PANGU_PLUGIN=str(library), PANGU_PLUGIN_RECEIPT=str(receipt))
    (root / 'source.txt').write_text('PANGUPLUGINREALREAD\n')
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Use read to inspect source.txt and reply with its exact content.')
        assert b'PANGUPLUGINREALREAD' in output and b'[tool] read' in re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', output), repr(output[-2000:])
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    match = re.fullmatch(r'released configured=(\d+) executed=(\d+) snapshots=(\d+)\n', receipt.read_text())
    assert match and int(match[1]) >= 2 and int(match[2]) >= 1 and int(match[3]) >= 1
    saved = stored_bytes(root / '.pangu')
    # Request configuration lives in retained task checkpoint events or assistant
    # history depending on task compaction; actual successful read proves routing.
    assert b'PANGUPLUGINREALREAD' in saved and b'product.fixture' in saved and b'fixture_result' in saved
    terminal = Terminal(binary, root, env)
    try:
        terminal.until(b'> ', 5)
        output = run_input(terminal, 'Without tools repeat the file content remembered from the previous turn.')
        assert b'PANGUPLUGINREALREAD' in output, repr(output[-1800:])
        terminal.send(b'\x04')
        terminal.finish()
    finally:
        terminal.close()
    print('PASS: native extension model/tool services, actual model/read, ordered release and reopen', flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,help='already built product')
    parser.add_argument('--live',action='store_true',help='real coding workflow and committed context reopen')
    parser.add_argument('--live-persistent',action='store_true',help='cooperative exit during an unsafe tool and reopen')
    parser.add_argument('--live-crash',action='store_true',help='process death during an unsafe tool and reopen')
    parser.add_argument('--live-plugin',action='store_true',help='native service substitution and ordered shutdown')
    parser.add_argument('--live-resources',action='store_true',help='project system/append/context instructions and reopen')
    parser.add_argument('--live-settings',action='store_true',help='actual scoped settings and configured skills/templates')
    parser.add_argument('--live-templates',action='store_true',help='actual prompt command and explicit skill invocation')
    parser.add_argument('--live-skills',action='store_true',help='actual skill discovery, on-demand read and coding workflow')
    parser.add_argument('--live-paths',action='store_true',help='actual model absolute paths and symlink editing')
    parser.add_argument('--live-thinking',action='store_true',help='real reasoning/tool/context/reopen workflow')
    parser.add_argument('--live-config-recovery',action='store_true',help='saved request identity/options survive changed environment')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pangu-pty-') as directory:
        root=Path(directory);binary=args.binary or build(root/'pangu')
        offline={k:v for k,v in os.environ.items() if not k.startswith(('PANGU_', 'MIMO_'))}
        missing=subprocess.run([str(binary)],env=offline,capture_output=True,timeout=5)
        assert missing.returncode==2 and b'Model credentials are required' in missing.stderr
        for credentials in ({'AWS_BEARER_TOKEN_BEDROCK':'startup-unit-token'},{'AWS_ACCESS_KEY_ID':'startup-unit-access','AWS_SECRET_ACCESS_KEY':'startup-unit-secret'}):
            probe={k:v for k,v in offline.items() if not k.startswith('AWS_')}
            probe.update(PANGU_MODEL_API='bedrock-converse-stream',PANGU_MODEL_NAME='startup-unit-model',**credentials)
            accepted=subprocess.run([str(binary)],env=probe,capture_output=True,timeout=5)
            assert accepted.returncode==2 and b'interactive terminal' in accepted.stderr
        offline.update(PANGU_MODEL_ENDPOINT='http://127.0.0.1:1/chat/completions',PANGU_MODEL_NAME='offline',PANGU_MODEL_API_KEY='offline-placeholder')
        terminal_exit(binary,root,offline)
        if args.live or args.live_persistent or args.live_crash or args.live_config_recovery or args.live_thinking or args.live_resources or args.live_plugin or args.live_paths or args.live_skills or args.live_templates or args.live_settings:
            assert os.environ.get('PANGU_MODEL_API_KEY') or os.environ.get('MIMO_API_KEY'), 'model credential required'
            env=dict(os.environ);env.pop('PANGU_SESSION_DIR',None)
            if args.live_settings:
                configured_resource_workflow(binary,root,env)
            if args.live_templates:
                template_workflow(binary,root,env)
            if args.live_skills:
                skill_workflow(binary,root,env)
            if args.live_paths:
                path_workflow(binary,root,env)
            if args.live_plugin:
                plugin_workflow(binary,root,env)
            if args.live_resources:
                resource_workflow(binary,root,env)
            if args.live_thinking:
                thinking_workflow(binary,root,env)
            if args.live_config_recovery:
                configuration_recovery(binary,root,env)
            if args.live:
                workflow(binary,root,env)
            if args.live_persistent:
                recovery(binary,root,env)
            if args.live_crash:
                recovery(binary,root,env,True)

if __name__=='__main__':
    main()
