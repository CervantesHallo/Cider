#!/usr/bin/env python3
"""Run one owned Wine acceptance program through the packaged GUI/XPC path.

Requires a prepared private CIDER_HOME and private signed Cider.app. It never
selects a normal user bottle, changes sync mode, or launches Wine directly.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--bottle', required=True)
parser.add_argument('--arch', choices=['i686', 'x86_64'], required=True)
parser.add_argument('--mode', choices=['correctness', 'stress', 'duration', 'bench'], required=True)
parser.add_argument('--suite', choices=['waitall', 'lifecycle'], default='waitall')
parser.add_argument('--stats', action='store_true')
parser.add_argument('--iterations', type=int, default=10000)
parser.add_argument('--threads', type=int, default=4)
parser.add_argument('--timeout', type=int, default=150)
args = parser.parse_args()
root = args.root.resolve(strict=True)
owner = json.loads((root / 'owner.json').read_text())
if not root.name.startswith('waitall-mac-') or owner['root'] != str(root):
    raise SystemExit('Unowned acceptance root')
if not 1 <= args.timeout <= 3900 or not 1 <= args.iterations <= 1000000000 or not 1 <= args.threads <= 32:
    raise SystemExit('Invalid bounded run parameters')
if args.mode == 'duration' and (args.iterations > 3600 or args.timeout < args.iterations + 20):
    raise SystemExit('Duration requires cleanup margin')
home = root / 'private-home'
app = root / 'source/out/Cider.app'
cli = app / 'Contents/MacOS/ciderctl'
configurations = [(p, json.loads(p.read_text())) for p in (home / 'AppSupport/Bottles').glob('*/cider-bottle.json')]
selected = [(p, c) for p, c in configurations if c['name'] == args.bottle]
if len(selected) != 1:
    raise SystemExit('Expected exactly one named owned bottle')
config_path, config = selected[0]
prefix = (config_path.parent / 'prefix').resolve(strict=True)
if not prefix.is_relative_to(root) or not config['engine']['id'].startswith('cider-lab-'):
    raise SystemExit('Non-laboratory prefix or engine')
lock = os.open(root / 'wine-runtime.lock', os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
source = root / (('waitall-' if args.suite == 'waitall' else 'waitall-lifecycle-') + args.arch + '.exe')
destination = prefix / 'drive_c/CiderTests' / source.name
destination.parent.mkdir(exist_ok=True)
shutil.copy2(source, destination)
arguments = ['--' + args.mode]
if args.suite == 'lifecycle':
    if args.mode != 'correctness' or args.iterations > 4096:
        raise SystemExit('Lifecycle requires a bounded correctness run')
    arguments = ['--iterations', str(args.iterations)]
elif args.mode != 'correctness':
    arguments += [str(args.iterations), str(args.threads)]
environment = {k: os.environ[k] for k in ['HOME', 'USER', 'LOGNAME', 'TMPDIR', 'PATH'] if k in os.environ}
environment.update(CIDER_HOME=str(home), CIDER_APP=str(app))
command = [str(cli), 'run', '-b', args.bottle, '--wait', '--debug', 'quiet']
if args.stats:
    command += ['--env', 'WINEMSYNC_STATS=1']
command += ['C:\\CiderTests\\' + source.name, '--'] + arguments
runs = root / 'runs'
runs.mkdir(exist_ok=True)
name = '%s-%s-%s-%s' % (time.time_ns(), config['engine']['id'], args.arch, args.mode)
result_path = runs / (name + '.json')
record = {'schema': 'cider.waitall-mac-run/v1', 'state': 'running', 'bottle': config['id'],
          'engine': config['engine']['id'], 'sync': config['settings'].get('sync', 'msync'),
          'arch': args.arch, 'arguments': arguments, 'prefix': str(prefix),
          'suite': args.suite, 'statisticsRequested': args.stats,
          'exeSHA256': hashlib.sha256(source.read_bytes()).hexdigest(), 'timeoutSeconds': args.timeout,
          'windowsOperations': 0, 'loadAverageBefore': os.getloadavg()}
engine_root = home / 'AppSupport/Engines' / config['engine']['id'] / 'engine'
if not engine_root.is_dir():
    raise SystemExit('Missing owned engine layout')
record['nativeSHA256'] = {str(p.relative_to(engine_root)): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [engine_root / 'bin/wineserver', engine_root / 'lib/wine/x86_64-unix/ntdll.so',
              engine_root / 'lib/wine/i386-windows/ntdll.dll', engine_root / 'lib/wine/x86_64-windows/ntdll.dll']}
result_path.write_text(json.dumps(record, indent=2) + '\n')
start = time.monotonic()
wall_start = time.time()
record['wallStartUnixSeconds'] = wall_start
record['monotonicClock'] = time.get_clock_info('monotonic').implementation
snapshots = []
system_pauses = []
sampling_done = threading.Event()
def sample_statistics():
    last_monotonic, last_wall = start, wall_start
    while not sampling_done.is_set():
        now_monotonic, now_wall = time.monotonic(), time.time()
        gap = (now_wall - last_wall) - (now_monotonic - last_monotonic)
        if args.mode == 'duration' and gap > 2:
            # mach_absolute_time stops in macOS sleep. Preserve a failed attempt
            # rather than count sleep as sustained pressure. A wall-clock jump
            # is likewise ambiguous and cannot establish a continuous run.
            pause = {'wallGapSeconds': gap, 'elapsedActiveSeconds': now_monotonic - start}
            try:
                cleanup = subprocess.run([str(root / 'owned-processes'), 'stop', str(root)],
                                         text=True, capture_output=True, timeout=20)
                pause['cleanup'] = {'exitCode': cleanup.returncode, 'stdout': cleanup.stdout, 'stderr': cleanup.stderr}
            except (OSError, subprocess.TimeoutExpired) as error:
                pause['cleanupError'] = str(error)
            system_pauses.append(pause)
            sampling_done.set()
            break
        last_monotonic, last_wall = now_monotonic, now_wall
        files = []
        for path in (prefix.glob('.cider-msync-coord-*') if args.stats else []):
            try:
                files.append((path.stat().st_birthtime, path))
            except FileNotFoundError:
                pass  # server exited/unlinked between directory scan and stat
        files.sort(key=lambda item: item[0], reverse=True)
        files = [item[1] for item in files]
        if files:
            row = {'elapsedSeconds': time.monotonic() - start, 'file': files[0].name}
            try:
                p = subprocess.run([str(root / 'stats-snapshot'), str(files[0])],
                                   text=True, capture_output=True, timeout=3)
                row['exitCode'] = p.returncode
                if p.returncode == 0:
                    row['statistics'] = json.loads(p.stdout)
            except (OSError, ValueError, subprocess.TimeoutExpired) as error:
                row['error'] = str(error)
            snapshots.append(row)
        sampling_done.wait(1)
sampler = threading.Thread(target=sample_statistics, daemon=True) if args.stats or args.mode == 'duration' else None
if sampler:
    sampler.start()
try:
    process = subprocess.run(command, env=environment, text=True, capture_output=True, timeout=args.timeout)
    record.update(cliExitCode=process.returncode, stdout=process.stdout, stderr=process.stderr)
except subprocess.TimeoutExpired:
    record.update(cliExitCode=124, timeout=True)
    try:
        cleanup = subprocess.run([str(root / 'owned-processes'), 'stop', str(root)],
                                 text=True, capture_output=True, timeout=20)
        record['timeoutCleanup'] = {'exitCode': cleanup.returncode, 'stdout': cleanup.stdout, 'stderr': cleanup.stderr}
    except (OSError, subprocess.TimeoutExpired) as error:
        record['timeoutCleanupError'] = str(error)
finally:
    sampling_done.set()
    if sampler:
        sampler.join(timeout=25)
        if sampler.is_alive():
            record['statisticsSamplerDidNotStop'] = True
record['pumpSnapshots'] = snapshots
record['systemPauses'] = system_pauses
record.update(seconds=time.monotonic() - start, wallElapsedSeconds=time.time() - wall_start, loadAverageAfter=os.getloadavg())
if args.mode == 'duration' and (record['seconds'] < args.iterations or
        record['wallElapsedSeconds'] - record['seconds'] > 2):
    record['continuousDurationNotEstablished'] = True
match = re.search(r'\blog (/.+)', record.get('stdout', ''))
rows = []
if match:
    log = Path(match.group(1).strip())
    if not log.resolve().is_relative_to(home / 'Logs/sessions'):
        raise SystemExit('Foreign session log')
    record['log'] = str(log)
    if log.stat().st_size <= 4 * 1024 * 1024:
        for line in log.read_text(errors='replace').splitlines():
            if line.startswith('{'):
                try:
                    row = json.loads(line)
                    if row.get('type') in ['begin', 'check', 'complete', 'stress', 'worker', 'resources', 'bench', 'roundtrip', 'wakeup', 'progress', 'layout', 'limits', 'case']:
                        rows.append(row)
                except json.JSONDecodeError:
                    record['invalidJSONLine'] = True
    else:
        record['logTooLargeForAcceptance'] = True
begins = [r for r in rows if r.get('type') == 'begin']
completes = [r for r in rows if r.get('type') == 'complete']
failed = [r for r in rows if r.get('type') == 'check' and not r.get('ok')]
expected_bits = 32 if args.arch == 'i686' else 64
record['rows'] = rows
if args.suite == 'lifecycle':
    begins = [r for r in rows if r.get('type') == 'limits']
metadata_ok = len(begins) == len(completes) == 1 and begins[0].get('pointer_bits') == expected_bits
if metadata_ok:
    metadata_ok = (completes[0].get('requested_per_case') == args.iterations if args.suite == 'lifecycle'
                   else completes[0].get('mode') == args.mode)
record['passed'] = (record['cliExitCode'] == 0 and metadata_ok and completes[0].get('ok') is True
                    and not failed and not record.get('invalidJSONLine') and not record.get('logTooLargeForAcceptance')
                    and not record.get('statisticsSamplerDidNotStop')
                    and not record.get('continuousDurationNotEstablished') and not system_pauses)
record['state'] = 'complete'
result_path.write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps({'result': str(result_path), 'passed': record['passed'], 'cliExitCode': record['cliExitCode'],
                  'seconds': record['seconds'], 'failedChecks': failed,
                  'measurements': [r for r in rows if r.get('type') in ['bench', 'roundtrip', 'wakeup', 'stress', 'resources']]}), flush=True)
sys.exit(0 if record['passed'] else 1)
