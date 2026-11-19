import pathlib
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='pangu 执行 space ') as directory:
    root = pathlib.Path(directory)
    folder = root / '搜索 空格'
    (folder / 'sub').mkdir(parents=True)
    (folder / 'a.txt').write_text('needle\n', encoding='utf-8')
    (folder / 'sub/b.txt').write_text('needle two\n', encoding='utf-8')
    (root / '.gitignore').write_text('ignored.txt\n', encoding='utf-8')
    (root / '.git').mkdir()
    (folder / 'ignored.txt').write_text('needle ignored\n', encoding='utf-8')
    subprocess.run([sys.argv[1], directory], check=True, timeout=20)
