import subprocess
import sys
import tempfile
import struct
import zlib
from pathlib import Path
with tempfile.TemporaryDirectory(prefix='pangu services ') as cwd:
    root = Path(cwd)
    (root / 'many.txt').write_text('line\n' * 2001)
    (root / 'long.txt').write_text('你' * 18000)
    (root / 'crlf.txt').write_bytes(b'\xef\xbb\xbfalpha\r\nbeta\r\n')
    (root / 'parallel.txt').write_text('alpha\nbeta\n')
    (root / 'duplicate.txt').write_text('alpha alpha')
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b'\x00\xff\x00\x00')) + chunk(b'IEND', b'')
    (root / 'image.png').write_bytes(png)
    (root / 'broken.png').write_bytes(b'\x89PNG\r\n\x1a\ninvalid')
    subprocess.run([sys.argv[1],cwd],check=True,timeout=10)
