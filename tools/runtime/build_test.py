"""Build the exact first Runtime subset; artifacts stay outside the repository."""
from pathlib import Path
import subprocess
import tempfile
import sys
import os
ROOT = Path(__file__).resolve().parents[2]
from build_pangu import implementation
from test_mimo_asm import ABI_WRAPS, CURL_WRAPS, ALLOC_WRAPS
MODEL_SOURCES = [implementation('src/runtime/' + name) for name in ('mimo', 'mimo_projection', 'mimo_stream')]
DURABLE_SOURCES = [implementation('src/runtime/' + name) for name in ['documents', 'records', 'record_memory', 'delta', 'pending', 'document_tx', 'holds', 'hold_timer', 'durable_session', 'record_sqlite', 'record_jsonl', 'record_jsonl_posix', 'task_runner', 'durable_tool', 'durable_generation', 'durable_client', 'durable_inputs', 'model_failure', 'model_regex_posix', 'mimo', 'mimo_projection', 'mimo_stream', 'tools', 'read_tool', 'write_tool', 'bash_tool', 'process', 'stream_decode']] + [implementation('src/client/tui/runtime_bridge')]
PROCESS_WRAPS = ['-Wl,--wrap=' + name for name in (
    'pipe2', 'fcntl', 'sigemptyset', 'sigaddset', 'sigaction', 'clock_gettime', 'read', 'waitid', 'waitpid', 'kill',
    'posix_spawn_file_actions_init', 'posix_spawnattr_init', 'posix_spawn_file_actions_addfchdir_np',
    'posix_spawn_file_actions_addopen', 'posix_spawn_file_actions_adddup2', 'posix_spawn_file_actions_addclosefrom_np',
    'posix_spawnattr_setsigmask', 'posix_spawnattr_setsigdefault', 'posix_spawnattr_setpgroup',
    'posix_spawnattr_setflags', 'posix_spawn')]

SOURCES = [implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/submissions'), str(ROOT / 'tests/runtime/test_agent.c')]

def main():
    with tempfile.TemporaryDirectory(prefix='pangu-agent-') as directory:
        if any(flag in sys.argv for flag in ('--live-generation','--live-partial','--live-deferred','--live-sequential','--live-inputs')):
            if not (os.environ.get('PANGU_MODEL_API_KEY') or os.environ.get('MIMO_API_KEY')):
                raise SystemExit('Model API key required in the environment')
            output = str(Path(directory) / 'live-durable-generation')
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c', 'libutf8proc', 'sqlite3', 'libcurl'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                            *DURABLE_SOURCES, str(ROOT / 'tests/runtime/test_durable_storage.c'), *flags, '-o', output], check=True)
            subprocess.run([output, '--live-inputs' if '--live-inputs' in sys.argv else '--live-sequential' if '--live-sequential' in sys.argv else '--live-deferred' if '--live-deferred' in sys.argv else '--live-partial' if '--live-partial' in sys.argv else '--live-generation'], check=True, timeout=350)
            return 0
        if any(flag in sys.argv for flag in ('--live-batch', '--live-batch-execute', '--live-snapshot-edit', '--live-capacity')):
            if not os.environ.get('MIMO_API_KEY'):
                raise SystemExit('MIMO_API_KEY is required; environment only')
            output = str(Path(directory) / 'live-model-batch')
            model_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c', 'libutf8proc'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                            *MODEL_SOURCES, implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/submissions'), implementation('src/runtime/tools'), implementation('src/runtime/read_tool'), implementation('src/runtime/write_tool'), implementation('src/runtime/edit_text'), implementation('src/runtime/edit_diff'), implementation('src/runtime/edit_tool'),
                            str(ROOT / 'tests/runtime/live_mimo_batch.c'), *model_flags, '-o', output], check=True)
            subprocess.run([output, *(['--capacity'] if '--live-capacity' in sys.argv else ['--snapshot'] if '--live-snapshot-edit' in sys.argv else ['--execute'] if '--live-batch-execute' in sys.argv else [])], check=True, timeout=150)
            return 0
        if any(flag in sys.argv for flag in ('--live', '--live-session', '--live-journal', '--live-resume', '--live-submission')):
            if not os.environ.get('MIMO_API_KEY'):
                raise SystemExit('MIMO_API_KEY is required; never put credentials in source or arguments')
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            output = str(Path(directory) / 'live-mimo')
            live_source = 'live_journal.c' if '--live-journal' in sys.argv or '--live-resume' in sys.argv or '--live-submission' in sys.argv else ('live_session.c' if '--live-session' in sys.argv else 'live_mimo.c')
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', '-O2',
                            implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/submissions'), *MODEL_SOURCES,
                            implementation('src/runtime/tools'), implementation('src/runtime/read_tool'), implementation('src/runtime/journal'), implementation('src/runtime/journal_posix'), str(ROOT / 'tests/runtime' / live_source), *flags, '-o', output], check=True)
            if '--live-journal' in sys.argv or '--live-resume' in sys.argv or '--live-submission' in sys.argv:
                storage_dir = Path(directory) / 'storage'
                storage_dir.mkdir()
                phases = ('key-interrupt', 'key-resume', 'key-recall') if '--live-submission' in sys.argv else (('interrupt', 'resume', 'recall') if '--live-resume' in sys.argv else ('store', 'recall'))
                for phase in phases:
                    subprocess.run([output, phase, str(storage_dir)], check=True, timeout=350)
            else:
                subprocess.run([output], check=True, timeout=350)
            return 0
        subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_documents.py')], check=True, timeout=45)
        subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_record_memory.py')], check=True, timeout=45)
        subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_delta.py')], check=True, timeout=45)
        native_paths = []
        for name, define in (('good', None), ('abi', 'BAD_ABI'), ('symbol', 'NO_ENTRY'), ('dependency', 'MISSING_DEP'), ('start', 'START_FAIL')):
            library = str(Path(directory) / (name + '.so'))
            subprocess.run(['/usr/bin/clang-23', '-shared', '-fPIC', *(['-D' + define] if define else []),
                            str(ROOT / 'tests/runtime/native_plugin_fixture.S'), '-o', library], check=True)
            native_paths.append(library)
        for mode in ('-O0', '-O2'):
            sqlite_test = str(Path(directory) / 'test-record-sqlite')
            sqlite_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c', 'libutf8proc', 'sqlite3', 'libcurl'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in ['documents', 'records', 'record_memory', 'delta', 'record_sqlite']],
                            str(ROOT / 'tests/runtime/test_record_sqlite.c'), *sqlite_flags, '-o', sqlite_test], check=True)
            subprocess.run([sqlite_test], check=True, timeout=30)
            jsonl_test = str(Path(directory) / 'test-record-jsonl')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in ['documents', 'records', 'record_memory', 'delta', 'record_jsonl', 'record_jsonl_posix']],
                            str(ROOT / 'tests/runtime/test_record_jsonl.c'), '-Wl,--wrap=write', *sqlite_flags, '-o', jsonl_test], check=True)
            subprocess.run([jsonl_test], check=True, timeout=30)
            durable_store_test = str(Path(directory) / 'test-durable-storage')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *DURABLE_SOURCES,
                            str(ROOT / 'tests/runtime/test_durable_storage.c'), *sqlite_flags, '-o', durable_store_test], check=True)
            subprocess.run([durable_store_test], check=True, timeout=30)
            host_test = str(Path(directory) / 'test-host')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/host'),
                            str(ROOT / 'tests/runtime/test_host.c'), '-o', host_test], check=True)
            subprocess.run([host_test], check=True, timeout=15)
            native_test = str(Path(directory) / 'test-native-plugin')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', '-Wl,--export-dynamic-symbol=pg_host_*',
                            implementation('src/runtime/host'), implementation('src/runtime/native_plugin'),
                            str(ROOT / 'tests/runtime/test_native_plugin.c'), '-ldl', '-o', native_test], check=True)
            subprocess.run([native_test, *native_paths], check=True, timeout=15)
            documents_test = str(Path(directory) / 'test-documents')
            documents_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-DPG_DOC_FAULTS', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/documents'), str(ROOT / 'tests/runtime/test_documents.c'),
                            '-Wl,--wrap=json_object_new_array_ext,--wrap=json_object_new_string_len,--wrap=json_object_new_string,--wrap=json_object_new_int64,--wrap=json_object_array_add,--wrap=json_object_to_json_string_length,--wrap=json_object_deep_copy,--wrap=json_object_new_object,--wrap=json_object_object_add',
                            *documents_flags, '-o', documents_test], check=True)
            subprocess.run([documents_test], check=True, timeout=15)
            records_test = str(Path(directory) / 'test-records')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-DPG_RECORD_FAULTS', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/documents'), implementation('src/runtime/records'),
                            str(ROOT / 'tests/runtime/test_records.c'), '-Wl,--wrap=json_object_deep_copy',
                            *documents_flags, '-o', records_test], check=True)
            subprocess.run([records_test], check=True, timeout=15)
            memory_test = str(Path(directory) / 'test-record-memory')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-DPG_MEMORY_FAULTS', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/documents'), implementation('src/runtime/records'), implementation('src/runtime/record_memory'), implementation('src/runtime/delta'), implementation('src/runtime/pending'), implementation('src/runtime/document_tx'), implementation('src/runtime/holds'), implementation('src/runtime/hold_timer'), implementation('src/runtime/durable_session'), implementation('src/runtime/document_reader'),
                            str(ROOT / 'tests/runtime/test_record_memory.c'),
                            '-Wl,--wrap=json_object_deep_copy,--wrap=calloc,--wrap=reallocarray,--wrap=json_object_new_array_ext,--wrap=json_object_array_add,--wrap=json_object_new_object,--wrap=json_object_new_int64,--wrap=json_object_new_string,--wrap=json_object_object_add,--wrap=lh_table_new,--wrap=lh_table_insert,--wrap=lh_table_resize,--wrap=lh_kptr_table_new,--wrap=json_object_array_put_idx,--wrap=json_object_new_string_len,--wrap=json_object_to_json_string_ext,--wrap=json_object_new_double,--wrap=json_object_new_boolean',
                            *documents_flags, '-lutf8proc', '-o', memory_test], check=True)
            subprocess.run([memory_test], check=True, timeout=15)
            delta_test = str(Path(directory) / 'test-delta')
            delta_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c', 'libutf8proc'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-DPG_DELTA_FAULTS', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/documents'), implementation('src/runtime/delta'), implementation('src/runtime/pending'),
                            str(ROOT / 'tests/runtime/test_delta.c'),
                            '-Wl,--wrap=json_object_deep_copy,--wrap=json_object_new_array_ext,--wrap=json_object_new_string_len,--wrap=json_object_array_add,--wrap=json_object_array_put_idx,--wrap=json_object_object_add,--wrap=malloc,--wrap=calloc,--wrap=json_object_new_object,--wrap=json_object_new_int64,--wrap=json_object_to_json_string_ext',
                            *delta_flags, '-o', delta_test], check=True)
            subprocess.run([delta_test], check=True, timeout=15)
            prompt_test = str(Path(directory) / 'test-prompt')
            prompt_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c', 'libgit2'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in ('prompt', 'prompt_posix', 'tools', 'read_tool')],
                            str(ROOT / 'tests/runtime/test_prompt.c'),
                            '-Wl,--wrap=json_object_new_object,--wrap=json_object_new_string_len,--wrap=json_object_object_add,--wrap=json_object_to_json_string_ext',
                            *prompt_flags, '-o', prompt_test], check=True)
            subprocess.run([prompt_test], check=True, timeout=15)
            model_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            for name, sources, wraps in [
                ('asm', ['test_mimo_asm.c', 'mimo_abi.S'], ABI_WRAPS),
                ('transport', ['test_mimo_transport.c', 'mimo_abi.S'], ABI_WRAPS + CURL_WRAPS),
                ('alloc', ['test_mimo_alloc.c'], ALLOC_WRAPS),
            ]:
                model_test = str(Path(directory) / ('test-mimo-' + name))
                subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra',
                                '-Werror', '-pedantic', '-Wno-cast-function-type', mode, '-fsanitize=undefined',
                                '-fno-sanitize-recover=all', *MODEL_SOURCES,
                                *[str(ROOT / 'tests/runtime' / source) for source in sources],
                                *wraps, *model_flags, '-o', model_test], check=True)
                subprocess.run([model_test], check=True, timeout=30)
            submissions_test = str(Path(directory) / 'test-submissions')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'),
                            implementation('src/runtime/session'), implementation('src/runtime/submissions'),
                            implementation('src/runtime/journal'), implementation('src/runtime/journal_posix'),
                            str(ROOT / 'tests/runtime/test_submissions.c'), '-Wl,--wrap=fsync', '-o', submissions_test], check=True)
            subprocess.run([submissions_test], check=True, timeout=30)
            profile_test = str(Path(directory) / 'test-profile')
            profile_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/profile'),
                            implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/submissions'),
                            implementation('src/runtime/journal'), implementation('src/runtime/journal_posix'),
                            implementation('src/runtime/tools'), str(ROOT / 'tests/runtime/test_profile.c'),
                            *profile_flags, '-o', profile_test], check=True)
            subprocess.run([profile_test], check=True, timeout=30)
            resume_test = str(Path(directory) / 'test-resume')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'),
                            implementation('src/runtime/session'), implementation('src/runtime/submissions'), implementation('src/runtime/journal'),
                            implementation('src/runtime/journal_posix'), str(ROOT / 'tests/runtime/test_agent_resume.c'),
                            '-Wl,--wrap=pwrite', '-o', resume_test], check=True)
            subprocess.run([resume_test], check=True, timeout=30)
            journal_test = str(Path(directory) / 'test-journal')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/journal'),
                            implementation('src/runtime/journal_posix'), implementation('src/runtime/session'), implementation('src/runtime/submissions'),
                            str(ROOT / 'tests/runtime/test_journal.c'), str(ROOT / 'tests/runtime/journal_abi.S'),
                            '-Wl,--wrap=pwrite,--wrap=pread,--wrap=fsync,--wrap=ftruncate', '-o', journal_test], check=True)
            subprocess.run([journal_test], check=True, timeout=30)
            session_test = str(Path(directory) / 'test-session')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/session'), implementation('src/runtime/submissions'),
                            implementation('src/runtime/agent'), str(ROOT / 'tests/runtime/test_session_commit.c'), '-o', session_test], check=True)
            subprocess.run([session_test], check=True, timeout=10)
            write_test = str(Path(directory) / 'test-write-tool')
            write_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/tools'),
                            implementation('src/runtime/write_tool'), str(ROOT / 'tests/runtime/test_write_tool.c'),
                            '-Wl,--wrap=write,--wrap=ftruncate,--wrap=fsync', *write_flags, '-o', write_test], check=True)
            subprocess.run([write_test], check=True, timeout=10)
            edit_test = str(Path(directory) / 'test-edit-text')
            edit_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c', 'libutf8proc'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/edit_text'),
                            str(ROOT / 'tests/runtime/test_edit_text.c'), '-Wl,--wrap=calloc,--wrap=utf8proc_map',
                            *edit_flags, '-o', edit_test], check=True)
            subprocess.run([edit_test], check=True, timeout=10)
            subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_edit_text.py'),
                            '--binary', edit_test], check=True, timeout=45)
            edit_tool_test = str(Path(directory) / 'test-edit-tool')
            edit_tool_sources = ['agent', 'session', 'submissions', 'journal', 'journal_posix',
                                 'tools', 'read_tool', 'write_tool', 'edit_text', 'edit_diff', 'edit_tool']
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in edit_tool_sources],
                            str(ROOT / 'tests/runtime/test_edit_tool.c'),
                            '-Wl,--wrap=json_tokener_new,--wrap=json_object_new_object,--wrap=json_object_new_string_len,--wrap=json_object_object_add,--wrap=json_object_to_json_string_ext',
                            *edit_flags, '-o', edit_tool_test], check=True)
            subprocess.run([edit_tool_test], check=True, timeout=15)
            subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_edit_text.py'),
                            '--tool-binary', edit_tool_test], check=True, timeout=45)
            batch_storage_test = str(Path(directory) / 'test-tool-batch-storage')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in
                              ['session', 'submissions', 'journal', 'journal_posix']],
                            str(ROOT / 'tests/runtime/test_tool_batch_storage.c'),
                            '-Wl,--wrap=pwrite,--wrap=fsync', '-o', batch_storage_test], check=True)
            subprocess.run([batch_storage_test], check=True, timeout=20)
            bash_test = str(Path(directory) / 'test-bash-tool')
            bash_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in
                              ['tools', 'process', 'stream_decode', 'bash_tool', 'agent', 'session', 'submissions']],
                            str(ROOT / 'tests/runtime/test_bash_tool.c'),
                            '-Wl,--wrap=mkostemp,--wrap=write,--wrap=close,--wrap=unlink,--wrap=json_tokener_new',
                            *bash_flags, '-o', bash_test], check=True)
            subprocess.run([bash_test], check=True, timeout=20)
            decoder_test = str(Path(directory) / 'test-stream-decode')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/stream_decode'),
                            str(ROOT / 'tests/runtime/test_stream_decode.c'), '-o', decoder_test], check=True)
            subprocess.run([decoder_test], check=True, timeout=10)
            subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_stream_decode.py'),
                            '--binary', decoder_test], check=True, timeout=45)
            process_test = str(Path(directory) / 'test-process')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/process'),
                            str(ROOT / 'tests/runtime/test_process.c'), *PROCESS_WRAPS, '-o', process_test], check=True)
            subprocess.run([process_test], check=True, timeout=20)
            async_test = str(Path(directory) / 'test-async-tools')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *[implementation('src/runtime/' + name) for name in
                              ['agent', 'session', 'submissions', 'tools', 'process']],
                            str(ROOT / 'src/client/tui/runtime_bridge.S'),
                            str(ROOT / 'tests/runtime/test_async_tools.c'), *write_flags, '-o', async_test], check=True)
            subprocess.run([async_test], check=True, timeout=20)
            tools_asm_test = str(Path(directory) / 'test-tools-asm')
            tool_flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-pedantic', '-Wno-cast-function-type', mode, '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            '-I' + str(ROOT / 'src/runtime'), implementation('src/runtime/tools'), implementation('src/runtime/read_tool'),
                            str(ROOT / 'tests/runtime/test_tools_asm.c'), *tool_flags, '-o', tools_asm_test], check=True)
            subprocess.run([tools_asm_test], check=True, timeout=30)
            output = str(Path(directory) / 'test-agent')
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-pedantic', mode, '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *SOURCES, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            flags = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libcurl', 'json-c'], text=True).split()
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11',
                            '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            *MODEL_SOURCES, str(ROOT / 'tests/runtime/test_mimo_stream.c'),
                            *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/tools'), str(ROOT / 'tests/runtime/test_tools.c'),
                            *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            implementation('src/runtime/tools'), implementation('src/runtime/read_tool'),
                            str(ROOT / 'tests/runtime/test_read_tool.c'), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-D_POSIX_C_SOURCE=200809L', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', *MODEL_SOURCES,
                            str(ROOT / 'tests/runtime/test_projection.c'), *flags, '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/submissions'),
                            implementation('src/client/tui/runtime_bridge'), str(ROOT / 'tests/client/tui/test_runtime_bridge.c'),
                            str(ROOT / 'tests/runtime/runtime_bridge_abi.S'),
                            '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
            subprocess.run(['/usr/bin/clang-23', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', mode,
                            '-fsanitize=undefined', '-fno-sanitize-recover=all', implementation('src/runtime/agent'), implementation('src/runtime/session'), implementation('src/runtime/submissions'),
                            implementation('src/runtime/profile'), str(ROOT / 'tests/runtime/agent_abi.S'), str(ROOT / 'tests/runtime/test_agent_asm.c'),
                            '-o', output], check=True)
            subprocess.run([output], check=True, timeout=10)
    subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_record_sqlite.py')], check=True)
    subprocess.run([sys.executable, '-B', str(ROOT / 'tools/runtime/test_record_sqlite.py'), '--jsonl'], check=True)
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
