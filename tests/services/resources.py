import json
import pathlib
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='pangu resources 空格 ') as directory:
    root = pathlib.Path(directory)
    user = root / 'user/.pangu/agent'
    project = root / 'project/sub'
    for folder in (user / 'prompts', user / 'skills/example', project / '.pangu/prompts', project / '.pangu/skills'):
        folder.mkdir(parents=True)
    (user / 'settings.json').write_text('{"defaultModel":"user-model","nested":{"keep":1,"override":2},"defaultTools":["read","bash"]}')
    (project / '.pangu/settings.json').write_text('{"nested":{"override":9},"defaultTools":["-bash","+grep"]}')
    (user / 'AGENTS.md').write_text('user context')
    (root / 'project/AGENTS.md').write_text('ancestor context')
    (user / 'SYSTEM.md').write_text('user marker')
    (project / '.pangu/SYSTEM.md').write_text('project marker')
    (user / 'prompts/hello.md').write_text('---\ndescription: "Quoted template"\n---\n$1|${2:-fallback}|$3|$@|${@:3}|${@:1:2}')
    (user / 'skills/invalid').mkdir()
    (user / 'skills/invalid/SKILL.md').write_text('---\nname: invalid\n---\ninvalid')
    (project / '.pangu/prompts/hello.md').write_text('shadowed duplicate')
    (user / 'skills/example/SKILL.md').write_text('---\nname: example\ndescription: |\n  Example skill\ndisable-model-invocation: false\n---\nDo something')
    # All three source ignore files, negation, case folding and nested rules.
    for name in ('blocked', 'ignored', 'FD-hidden', 'keep', 'nested/drop', 'nested/take'):
        folder = user / 'skills' / name
        folder.mkdir(parents=True)
        (folder / 'SKILL.md').write_text('---\nname: ' + name.replace('/', '-') + '\ndescription: fixture\n---\nfixture')
    (user / 'skills/.gitignore').write_text('blocked/\nkeep/SKILL.md\n!keep/SKILL.md\n')
    (user / 'skills/.ignore').write_text('ignored/\n')
    (user / 'skills/.fdignore').write_text('fd-HIDDEN/\n')
    (user / 'skills/nested/.ignore').write_text('drop/\n')
    subprocess.run([sys.argv[1], str(user), str(project)], check=True, timeout=15)
    workers = [subprocess.Popen([sys.argv[1], str(user), str(project), json.dumps({"worker" + str(i): i})]) for i in range(4)]
    for worker in workers:
        assert worker.wait(timeout=15) == 0
    saved = json.loads((user / 'settings.json').read_text())
    assert all(saved["worker" + str(i)] == i for i in range(4))
    assert saved['nested']['keep'] == 1
    if len(sys.argv) > 2:
        subprocess.run([sys.argv[2], str(user), str(project)], check=True, timeout=15)

    # Port resource-loader's nested worktree context cases through the installed
    # plugin/snapshot consumer, without requiring a git executable.
    def put(path, body):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(body, encoding='utf-8')

    def linked(common, worktree):
        gitdir = common / 'worktrees/feat'
        put(common / 'HEAD', 'ref: refs/heads/main\n')
        put(gitdir / 'HEAD', 'ref: refs/heads/feat\n')
        put(gitdir / 'commondir', '../..')
        put(worktree / '.git', 'gitdir: ' + str(gitdir) + '\n')

    context_cases = ('nested', 'symlink', 'shared-symlink', 'inherit', 'different-name',
                     'bare', 'outer', 'sibling', 'submodule', 'ordinary', 'corrupt')
    for case in context_cases:
        outer = root / 'contexts' / case
        main = outer / 'main'
        worktree = main / 'worktrees/feat'
        expected = ['worktree']
        if case == 'sibling':
            worktree = outer / 'sibling'
        if case == 'bare':
            worktree = main / 'checkout'
        cwd = worktree / 'src'
        cwd.mkdir(parents=True)
        context_user = outer / 'user'
        context_user.mkdir()
        if case == 'bare':
            linked(main / '.bare', worktree)
            put(main / 'AGENTS.md', 'container')
            put(worktree / 'AGENTS.md', 'worktree')
            expected = ['container', 'worktree']
        elif case == 'submodule':
            gitdir = main / '.git/modules/lib'
            put(gitdir / 'HEAD', 'ref: refs/heads/main\n')
            put(worktree / '.git', 'gitdir: ' + str(gitdir) + '\n')
            put(main / 'AGENTS.md', 'superproject')
            put(worktree / 'AGENTS.md', 'submodule')
            expected = ['superproject', 'submodule']
        elif case == 'ordinary':
            put(main / '.git/HEAD', 'ref: refs/heads/main\n')
            put(main / 'AGENTS.md', 'main')
            put(worktree / 'AGENTS.md', 'worktree')
            expected = ['main', 'worktree']
        elif case == 'corrupt':
            put(main / 'AGENTS.md', 'main')
            put(worktree / '.git', 'gitdir: /nonexistent/pangu-worktree\n')
            put(worktree / 'AGENTS.md', 'worktree')
            expected = ['main', 'worktree']
        else:
            linked(main / '.git', worktree)
            put(main / ('CLAUDE.md' if case == 'different-name' else 'AGENTS.md'), 'main')
            if case == 'symlink':
                (worktree / 'AGENTS.md').symlink_to(main / 'AGENTS.md')
                expected = ['main']
            elif case == 'shared-symlink':
                (main / 'AGENTS.md').unlink()
                put(outer / 'shared.md', 'shared')
                (main / 'AGENTS.md').symlink_to(outer / 'shared.md')
                (worktree / 'AGENTS.md').symlink_to(outer / 'shared.md')
                expected = ['shared']
            elif case == 'inherit':
                expected = ['main']
            else:
                put(worktree / 'AGENTS.md', 'worktree')
                if case == 'different-name':
                    expected = ['main', 'worktree']
            if case in ('outer', 'sibling'):
                put(outer / 'AGENTS.md', 'outer')
                expected.insert(0, 'outer')
        subprocess.run([sys.argv[1], str(context_user), str(cwd), 'contexts', json.dumps(expected)],
                       check=True, timeout=15)
