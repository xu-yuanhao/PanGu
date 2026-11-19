#!/usr/bin/env python3
"""Measure installed Linux CLI/TUI startup, idle and one deterministic turn.

Uses the existing real-PTY harness and validates saved/reopened behavior before
retaining each sample. This does not measure network or tool execution cost.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import pty
import statistics
import subprocess
import sys
import tempfile
import termios
import time
from resource_sampling import proc

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tests/tui'))
from session_pty import Terminal

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--default-cli', '--baseline-cli', type=Path, required=True)
parser.add_argument('--overflow-cli', '--comparison-cli', type=Path, required=True)
parser.add_argument('--baseline-label', default='default')
parser.add_argument('--comparison-label', default='overflow')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--repeats', type=int, default=3)
args = parser.parse_args()
if platform.system() != 'Linux' or not 1 <= args.repeats <= 20:
    parser.error('Linux /proc and 1..20 repeats are required')
if not args.baseline_label or not args.comparison_label or args.baseline_label == args.comparison_label:
    parser.error('Two distinct nonempty result labels are required')


def sample(cli):
    with tempfile.TemporaryDirectory(prefix='pangu-product-profile-') as directory:
        work = Path(directory)
        session = work / 'session.jsonl'
        command = [str(cli), 'create', str(session), '--cwd', str(work),
                   '--agent-dir', str(work / 'agent'), '--tui-demo']
        t = Terminal.__new__(Terminal)
        t.master, t.slave = pty.openpty()
        t.saved = termios.tcgetattr(t.slave)
        t.output, t.cursor = bytearray(), 0
        t.proc = None
        started = time.perf_counter()
        try:
            t.proc = subprocess.Popen(command, stdin=t.slave, stdout=t.slave, stderr=t.slave,
                                      env={**os.environ, 'TERM': 'xterm-256color'})
            t.until(b'/tree /switch ID /branch ID /reset /handoff TEXT /compact [keep tokens] /usage')
            ready = time.perf_counter()
            idle_ticks, idle_rss, _ = proc(t.proc.pid)
            idle_start = time.perf_counter()
            time.sleep(.5)
            idle_end = time.perf_counter()
            after_idle_ticks, _, _ = proc(t.proc.pid)
            work_start = time.perf_counter()
            t.send(b'profile product turn\r')
            t.until(b'PanGu deterministic demo response')
            t.until(b'Tokens:')
            work_end = time.perf_counter()
            work_ticks, retained_rss, high = proc(t.proc.pid)
            t.close()  # also checks terminal restoration and successful exit
            reopened = subprocess.check_output([str(cli), 'open', str(session),
                '--cwd', str(work), '--agent-dir', str(work / 'agent')], text=True)
            if 'user: profile product turn' not in reopened or \
                    'assistant: PanGu deterministic demo response' not in reopened:
                raise RuntimeError('Saved/reopened product result mismatch')
            scale = 1000 / os.sysconf('SC_CLK_TCK')
            return {'startup_to_banner_ms': (ready-started)*1000,
                    'idle_sample_ms': (idle_end-idle_start)*1000,
                    'idle_cpu_ms': (after_idle_ticks-idle_ticks)*scale,
                    'idle_rss_kib': idle_rss, 'retained_rss_kib': retained_rss,
                    'peak_rss_kib': high, 'turn_to_usage_ms': (work_end-work_start)*1000,
                    'turn_cpu_ms': (work_ticks-after_idle_ticks)*scale,
                    'process_cpu_through_turn_ms': work_ticks*scale,
                    'validation': 'one submitted USER and deterministic assistant reopened; exit/termios restored'}
        finally:
            if t.proc is not None:
                t.dispose()
            else:
                os.close(t.master)
                os.close(t.slave)


report = {'platform': platform.platform(), 'machine': platform.machine(),
          'clock_ticks_per_second': os.sysconf('SC_CLK_TCK'),
          'scope': 'Installed Release CLI/TUI, isolated JSONL session, deterministic model, one input turn',
          'caveats': ['Startup ends at first observed command banner; includes process/loader/plugins/storage/UI setup and PTY observation.',
                      'RSS and peak are Linux /proc VmRSS/VmHWM after exec; system libraries are resident only as used.',
                      'Idle CPU is sampled over 500ms at kernel clock granularity, not proof of zero CPU.',
                      'Turn timing includes input parsing, model, persistence, rendering and PTY observation through usage output.',
                      'No network, tool execution, external MCP child, large stream or multi-session workload is exercised.',
                      'No Pi product or other-platform comparison; startup samples use warm filesystem caches.'],
          'results': {}}
for name, cli in [(args.baseline_label, args.default_cli), (args.comparison_label, args.overflow_cli)]:
    cli = cli.resolve()
    samples = [sample(cli) for _ in range(args.repeats)]
    report['results'][name] = {'cli': str(cli),
        'cli_sha256': hashlib.sha256(cli.read_bytes()).hexdigest(),
        'samples': samples,
        'median': {key: statistics.median(row[key] for row in samples)
                   for key in samples[0] if key != 'validation'}}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
for name, result in report['results'].items():
    print(name, json.dumps(result['median']))
