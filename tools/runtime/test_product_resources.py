"""Exercise the actual product composition resource service; --live uses a model."""
from pathlib import Path
import os
import argparse
import subprocess
import tempfile
from build_pangu import ROOT, SOURCES, PLUGIN_EXPORTS
from schema_dependency import compile_inputs

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--live',action='store_true')
    parser.add_argument('--live-agent',action='store_true')
    parser.add_argument('--live-conversations',action='store_true')
    parser.add_argument('--live-process-conversations',action='store_true')
    parser.add_argument('--live-extensions',action='store_true')
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
        flags=subprocess.check_output(['pkg-config','--cflags','--libs','libcurl','json-c','libutf8proc','sqlite3','libgit2','yaml-0.1','libpcre2-8','vips'],text=True).split()
        sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitize else []
        sources=compile_inputs([name for name in SOURCES if name!='src/composition/pangu.c'],directory,sanitize)
        subprocess.run(['/usr/bin/clang-23','-D_POSIX_C_SOURCE=200809L','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-O2',*sanitize,*PLUGIN_EXPORTS,*sources,str(ROOT/'tests/runtime/test_product_resources.c'),*flags,'-ldl','-o',str(output)],check=True)
        if not any((args.live,args.live_agent,args.live_conversations,args.live_process_conversations,args.live_extensions,args.live_compaction,args.live_auto_compaction,args.live_background_compaction,args.live_dynamic_context)):
            for api,base,options in (
                ('google-generative-ai',None,'{}'),
                ('google-generative-ai','https://proxy.invalid/v1beta','{}'),
                ('google-vertex',None,'{"authMode":"api-key"}'),
                ('google-vertex',None,'{"project":"pangu","location":"global"}'),
                ('azure-openai-responses',None,'{"azureResourceName":"pangu","azureDeploymentName":"pangu-deployment"}'),
            ):
                env=os.environ.copy()
                for key in ('PANGU_MODEL_ENDPOINT','PANGU_MODEL_BASE_URL'):
                    env.pop(key,None)
                env.update(PANGU_MODEL_API=api,PANGU_MODEL_NAME='gemini-3-flash',PANGU_MODEL_API_KEY='configuration-placeholder',PANGU_MODEL_OPTIONS=options)
                if base: env['PANGU_MODEL_BASE_URL']=base
                subprocess.run([str(output),'--model-configuration'],env=env,check=True,timeout=30)
        library=Path(directory)/'tasks.so'
        if args.native_tasks or args.live_extensions:
            subprocess.run(['/usr/bin/clang-23','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-fPIC','-shared',*sanitize,'-DPG_TASK_FIXTURE',str(ROOT/'tests/runtime/product_plugin_fixture.c'),*flags,'-o',str(library)],check=True)
        if args.live_process_conversations:
            for suffix in ('','-cancel'):
                workspace=Path(directory)/('workspace'+suffix)
                workspace.mkdir()
                for stage in ('seed','resume'):
                    subprocess.run([str(output),'--nested-process-'+stage+suffix,str(workspace)],check=True,timeout=120)
            return
        subprocess.run([str(output),*(['--live-extensions',str(library)] if args.live_extensions else ['--live-agent'] if args.live_agent else ['--live-conversations'] if args.live_conversations else ['--native-tasks',str(library)] if args.native_tasks else ['--compaction-hooks'] if args.compaction_hooks else ['--live-dynamic-context'] if args.live_dynamic_context else ['--live-background-compaction'] if args.live_background_compaction else ['--live-auto-compaction'] if args.live_auto_compaction else ['--live-compaction'] if args.live_compaction else ['--live'] if args.live else [])],check=True,timeout=330)
if __name__=='__main__':
    main()
