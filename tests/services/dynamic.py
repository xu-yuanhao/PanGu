"""Exercise actual OS shared objects under explicit UTF-8/space local paths."""
import pathlib
import shutil
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='pangu native 插件 space ') as directory:
    root = pathlib.Path(directory)
    modules = []
    for filename in sys.argv[2:]:
        source = pathlib.Path(filename)
        destination = root / source.name
        shutil.copyfile(source, destination)
        modules.append(str(destination))
    malformed = root / 'not-a-library.bin'
    malformed.write_bytes(b'plain fixture data')
    subprocess.run([sys.argv[1], *modules, str(malformed)], check=True, timeout=15)
