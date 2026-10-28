"""Exercise the actual product composition resource service; --live uses a model."""
from pathlib import Path
import argparse
import subprocess
import tempfile
from build_pangu import ROOT, SOURCES

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live',action='store_true')
    parser.add_argument('--live-compaction',action='store_true')
    parser.add_argument('--live-auto-compaction',action='store_true')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pangu-product-resource-test-') as directory:
        output=Path(directory)/'test-product-resources'
        flags=subprocess.check_output(['pkg-config','--cflags','--libs','libcurl','json-c','libutf8proc','sqlite3','libgit2','yaml-0.1','libpcre2-8'],text=True).split()
        sources=[str(ROOT/name) for name in SOURCES if name!='src/composition/pangu.c']
        subprocess.run(['/usr/bin/clang-23','-D_POSIX_C_SOURCE=200809L','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2',*sources,str(ROOT/'tests/runtime/test_product_resources.c'),*flags,'-ldl','-o',str(output)],check=True)
        subprocess.run([str(output),*(['--live-auto-compaction'] if args.live_auto_compaction else ['--live-compaction'] if args.live_compaction else ['--live'] if args.live else [])],check=True,timeout=330)
if __name__=='__main__':
    main()
