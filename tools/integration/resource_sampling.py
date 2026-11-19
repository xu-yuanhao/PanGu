"""Shared Linux /proc and handshake sampling for measured hosts."""
import json
import os
from pathlib import Path
import select
import subprocess
import time

def proc(pid):
    # stat fields after comm: state, ppid, ..., utime(field14), stime(field15).
    fields = Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()
    ticks = int(fields[11]) + int(fields[12])
    status = Path(f'/proc/{pid}/status').read_text().splitlines()
    rss = int(next(line.split()[1] for line in status if line.startswith('VmRSS:')))
    high = int(next(line.split()[1] for line in status if line.startswith('VmHWM:')))
    return ticks, rss, high


def line(process, timeout=60):
    if not select.select([process.stdout], [], [], timeout)[0]:
        raise RuntimeError('Measurement host did not respond')
    data = process.stdout.readline()
    if not data:
        raise RuntimeError('Measurement host exited early')
    return json.loads(data)


def sample(command, shape):
    started = time.perf_counter()
    process = subprocess.Popen([*command, *map(str, shape)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
    try:
        assert line(process) == {'ready': True}
        ready = time.perf_counter()
        ticks, idle_rss, _ = proc(process.pid)
        time.sleep(.2)
        after_ticks, _, _ = proc(process.pid)
        idle_elapsed = time.perf_counter() - ready
        work_started = time.perf_counter()
        process.stdin.write(b'\n')
        outcome = line(process)
        work_finished = time.perf_counter()
        _, retained_rss, high = proc(process.pid)
        process.stdin.write(b'\n')
        process.stdin.close()
        _, status, usage = os.wait4(process.pid, 0)
        process.returncode = os.waitstatus_to_exitcode(status)
        finished = time.perf_counter()
        errors = process.stderr.read().decode()
        if process.returncode:
            raise RuntimeError(f'Measurement host failed: {process.returncode}: {errors}')
        instances, turns, chunks, chunk_bytes = shape
        expected = {'messages': instances * turns * 2,
                    'bytes': instances * turns * (7 + chunks * chunk_bytes),
                    'events': instances * turns * (chunks + 7)}
        if outcome != expected:
            raise RuntimeError(f'Workload mismatch: {outcome} != {expected}')
        return {'startup_ms': (ready-started)*1000,
                'work_ms': (work_finished-work_started)*1000,
                'cleanup_ms': (finished-work_finished)*1000,
                'peak_rss_kib': high, 'retained_rss_kib': retained_rss, 'idle_rss_kib': idle_rss,
                'idle_cpu_ms': (after_ticks-ticks)*1000/os.sysconf('SC_CLK_TCK'),
                'idle_sample_ms': idle_elapsed*1000,
                'process_cpu_ms': (usage.ru_utime+usage.ru_stime)*1000,
                'outcome': outcome}
    finally:
        if process.returncode is None:
            process.kill()
            process.wait()
        if not process.stdin.closed:
            process.stdin.close()
        process.stdout.close()
        process.stderr.close()


