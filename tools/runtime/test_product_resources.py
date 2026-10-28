"""Exercise the actual product composition resource service; --live uses a model."""
from pathlib import Path
import argparse
import subprocess
import tempfile
from build_pangu import ROOT, SOURCES, PLUGIN_EXPORTS

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live',action='store_true')
    parser.add_argument('--live-agent',action='store_true')
    parser.add_argument('--compaction-hooks',action='store_true')
    parser.add_argument('--native-tasks',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--live-compaction',action='store_true')
    parser.add_argument('--live-auto-compaction',action='store_true')
    parser.add_argument('--live-background-compaction',action='store_true')
    parser.add_argument('--live-dynamic-context',action='store_true')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='pangu-product-resource-test-') as directory:
        output=Path(directory)/'test-product-resources'
        flags=subprocess.check_output(['pkg-config','--cflags','--libs','libcurl','json-c','libutf8proc','sqlite3','libgit2','yaml-0.1','libpcre2-8'],text=True).split()
        sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitize else []
        sources=[str(ROOT/name) for name in SOURCES if name!='src/composition/pangu.c']
        subprocess.run(['/usr/bin/clang-23','-D_POSIX_C_SOURCE=200809L','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2',*sanitize,*PLUGIN_EXPORTS,*sources,str(ROOT/'tests/runtime/test_product_resources.c'),*flags,'-ldl','-o',str(output)],check=True)
        library=Path(directory)/'tasks.so'
        if args.native_tasks:
            subprocess.run(['/usr/bin/clang-23','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-fPIC','-shared',*sanitize,'-DPG_TASK_FIXTURE',str(ROOT/'tests/runtime/product_plugin_fixture.c'),*flags,'-o',str(library)],check=True)
        subprocess.run([str(output),*(['--live-agent'] if args.live_agent else ['--native-tasks',str(library)] if args.native_tasks else ['--compaction-hooks'] if args.compaction_hooks else ['--live-dynamic-context'] if args.live_dynamic_context else ['--live-background-compaction'] if args.live_background_compaction else ['--live-auto-compaction'] if args.live_auto_compaction else ['--live-compaction'] if args.live_compaction else ['--live'] if args.live else [])],check=True,timeout=330)
if __name__=='__main__':
    main()
