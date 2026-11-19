"""Actual app resource driver through terminal input, persistent queues and reload."""
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
from session_pty import Terminal

exe = str(Path(sys.argv[1]).resolve())
SECRET = b'synthetic-tui-secret'
AUTH = b'synthetic-auth-never-in-snapshot'


def fixture(root):
    user, project = root / 'user/.pangu/agent', root / 'project'
    for folder in (user / 'prompts', user / 'skills/example',
                   project / '.pangu/prompts', project / '.pangu/skills/projectgate'):
        folder.mkdir(parents=True)
    (user / 'settings.json').write_text(json.dumps({
        'defaultProvider': 'deepseek', 'defaultModel': 'user-model',
        'defaultThinkingLevel': 'high'}))
    (user / 'SYSTEM.md').write_text('user-system-v1')
    (user / 'auth.json').write_text(json.dumps({'deepseek': {
        'type': 'api_key', 'key': AUTH.decode()}}))
    (user / 'prompts/hello.md').write_text('template-v1 $1')
    skill(user, 'skill-v1')
    (project / '.pangu/settings.json').write_text('{"defaultModel":"project-model-v1"}')
    (project / '.pangu/SYSTEM.md').write_text('project-system-v1')
    (project / '.pangu/prompts/projectonly.md').write_text('trusted-project-template $1')
    (project / '.pangu/skills/projectgate/SKILL.md').write_text(
        '---\nname: projectgate\ndescription: Project skill\n---\ntrusted-project-skill')
    return user, project


def skill(user, body):
    (user / 'skills/example/SKILL.md').write_text(
        '---\nname: example\ndescription: Example skill\n'
        'disable-model-invocation: true\n---\n' + body)


def output(t, model, system, text, thinking=4):
    t.until(b'Assistant: Model=' + model.encode())
    t.until(f'Thinking={thinking}'.encode())
    t.until(b'System=' + system.encode())
    t.until(text.encode())
    t.until(b'Tokens:')


def snapshot(path, backend):
    if backend == 'jsonl':
        return json.loads(path.read_text().splitlines()[-1])
    with sqlite3.connect(path) as db:
        return json.loads(db.execute('SELECT snapshot FROM pangu_session WHERE id=1').fetchone()[0])


with tempfile.TemporaryDirectory(prefix='pangu-tui-resources 空格 ') as directory:
    root = Path(directory)
    for backend in ('jsonl', 'sqlite'):
        for trusted in (True, False):
            user, project = fixture(root / f'{backend}-{trusted}')
            path = project / 'session.db'
            extra = ('resources', str(user), str(project), str(int(trusted)), backend)
            t = Terminal(path, 'create', extra, executable=exe)
            model = 'project-model-v1' if trusted else 'user-model'
            system = 'project-system-v1' if trusted else 'user-system-v1'
            try:
                t.until(b'/reload refreshes')
                t.send("/hello '中文 first'\r".encode())
                output(t, model, system, 'template-v1 中文 first')
                t.send(b'/skill:example explicit\r')
                output(t, model, system, 'skill-v1')
                # Trust is a composition decision; reload does not grant it.
                t.send(b'/projectonly gate\r')
                output(t, model, system,
                       'trusted-project-template gate' if trusted else '/projectonly gate')
                t.send(b'/skill:projectgate gate\r')
                output(t, model, system,
                       'trusted-project-skill' if trusted else '/skill:projectgate gate')
                if trusted:
                    t.send(b'slow-queue\r')
                    t.until(b'Resource request waiting')
                    # Change disk while the provider is polling. Reload must be
                    # refused; accepted steer/follow-up use the held snapshot.
                    (project / '.pangu/settings.json').write_text(
                        '{"defaultModel":"project-model-v2","defaultThinkingLevel":"low"}')
                    (project / '.pangu/SYSTEM.md').write_text('project-system-v2')
                    (user / 'prompts/hello.md').write_text('template-v2 $1')
                    skill(user, 'skill-v2')
                    t.send(b'/reload\r')
                    t.until(b'Reload requires idle')
                    t.send(b'/hello queued-steer\r/skill:example queued-follow\x1b\r')
                    output(t, model, system, 'slow-queue')
                    output(t, model, system, 'template-v1 queued-steer')
                    output(t, model, system, 'skill-v1')
                t.send(b'/rel\t\r')
                t.until(b'Resources reloaded; next input uses refreshed')
                if trusted:
                    model, system, thinking = 'project-model-v2', 'project-system-v2', 2
                else:
                    thinking = 4
                t.send(b'/hello refreshed\r')
                output(t, model, system,
                       'template-v2 refreshed' if trusted else 'template-v1 refreshed', thinking)
                t.send(b'/skill:example refreshed-skill\r')
                output(t, model, system, 'skill-v2' if trusted else 'skill-v1', thinking)
                # Invalid settings cannot replace the last successful snapshot.
                (user / 'settings.json').write_text('invalid')
                t.send(b'/reload\r')
                t.until(b'Resource reload failed; previous snapshot retained')
                t.send(b'/hello rollback\r')
                output(t, model, system,
                       'template-v2 rollback' if trusted else 'template-v1 rollback', thinking)
                (user / 'settings.json').write_text(json.dumps({
                    'defaultProvider': 'deepseek', 'defaultModel': 'user-model',
                    'defaultThinkingLevel': 'high'}))
                # Cancel and immediately submit: fresh app-run must begin only
                # after the old driver returns and releases its held snapshot.
                t.send(b'slow-cancel\r')
                t.until(b'Resource request waiting')
                t.send(b'\x03/hello after-cancel\r')
                t.until(b'[cancelled]')
                output(t, model, system,
                       'template-v2 after-cancel' if trusted else 'template-v1 after-cancel', thinking)
                # Plain original inputs also survive default app save/open.
                for plain in ('  plain 中文\nline é Straße  ', 'repeat', 'repeat'):
                    t.send(b'\x1b[200~' + plain.encode() + b'\x1b[201~\r')
                    output(t, model, system, plain.strip().replace('\n', '\r\n'), thinking)
                t.close()
                assert SECRET not in t.output and AUTH not in t.output
            finally:
                t.dispose()
            saved = snapshot(path, backend)
            users = [m['content'][0]['data'] for m in saved['messages'] if m['role'] == 1]
            assert users.count('template-v1 queued-steer') == int(trusted), users
            assert sum('queued-follow' in text for text in users) == int(trusted), users
            assert sum('after-cancel' in text for text in users) == 1, users
            assert not any(text.startswith(('/reload', '/rel')) for text in users)
            assert SECRET.decode() not in json.dumps(saved) and AUTH.decode() not in json.dumps(saved)
            # Reopen the actual session: expansion was persisted, not a UI-only
            # preview. Idle reload remains available on the new presentation.
            t = Terminal(path, 'open', extra, executable=exe)
            try:
                t.until(b'You: template-v2 after-cancel' if trusted else b'You: template-v1 after-cancel')
                t.until(b'/reload refreshes')
                # Default app now supplies persisted original-input provenance.
                # Browsing restores the whole draft and selection, and adjacent
                # repeats occupy one history entry after reopening.
                t.send(b'\x1b[200~draft\n' + '草稿é'.encode() + b'\x1b[201~\x1b[1;2D\x10')
                t.until(b'> repeat')
                t.send(b'\x10')
                t.until('> plain 中文'.encode())
                t.send(b'\x0e\x0e!')
                t.until('草稿!'.encode())
                # Search previews use the original command, not its expansion;
                # acceptance stays editable and cancellation restores the draft.
                for query, preview in ((b'/hello', b'/hello after-cancel'),
                                       (b'/skill:example', b'/skill:example refreshed-skill'),
                                       (b'STRASSE', 'plain 中文'.encode())):
                    t.send(b'\x12' + query)
                    t.until(b'Search: ' + query)
                    t.until(preview)
                    t.send(b'\x03')
                if trusted:
                    for query in (b'queued-steer', b'queued-follow'):
                        t.send(b'\x12' + query)
                        t.until(b'Search: ' + query)
                        t.send(b'\x03')
                # Resource-expanded/assistant text and pending originals must
                # never be imported as committed original history.
                for query in (b'template-v', b'skill-v', b'Model=', b'pending-history-only'):
                    t.send(b'\x12' + query + b'\r')
                    t.until(b'No match: ' + query)
                    t.send(b'\x03')
                t.send(b'\x01\x0b\x02\x01\x0b\x0b/rel\t\t\r')
                t.until(b'Resources reloaded; next input uses refreshed')
                t.close()
                assert SECRET not in t.output and AUTH not in t.output
            finally:
                t.dispose()
            assert snapshot(path, backend)['messages'] == saved['messages'], 'history browsing resubmitted an old input'
print('PTY resources: template/skill, trusted/denied projects, busy queues, reload/rollback, cancel/reopen, JSONL/SQLite passed')
