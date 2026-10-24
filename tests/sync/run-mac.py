#!/usr/bin/env python3
"""Run one owned Wine acceptance program through the packaged GUI/XPC path.

Requires a prepared private CIDER_HOME and private signed Cider.app. It never
selects a normal user bottle, changes sync mode, or launches Wine directly.
"""
import argparse
from collections import Counter
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import threading
import time
import uuid

def valid_metadata(suite, arch, iterations, mode, begins, completes):
    if len(begins) != 1 or len(completes) != 1:
        return False
    begin, complete = begins[0], completes[0]
    bits = 32 if arch == 'i686' else 64
    if begin.get('pointer_bits') != bits:
        return False
    if suite == 'matrix':
        return (mode == 'correctness'
                and begin.get('mode') == 'sync-matrix-correctness'
                and complete.get('mode') == 'sync-matrix-correctness'
                and type(complete.get('checks')) is int and complete['checks'] > 0
                and complete.get('failures') == 0 and complete.get('output_error') is False)
    if suite == 'nt-extra':
        return (mode == 'correctness' and begin.get('mode') == 'nt-extra-correctness'
                and complete.get('mode') == 'nt-extra-correctness'
                and complete.get('checks') == 18 and complete.get('failures') == 0
                and complete.get('output_error') is False)
    if suite == 'user-bench':
        return mode == 'bench' and begin.get('benchmark') == 'sync_user'
    if suite == 'wait-widths':
        return (mode == 'correctness' and begin.get('mode') == 'wait-widths-correctness'
                and complete.get('mode') == 'wait-widths-correctness'
                and complete.get('checks') == 214 and complete.get('failures') == 0
                and complete.get('output_error') is False)
    if suite == 'profile-args':
        return (mode == 'correctness' and begin.get('subject') == 'profile-arguments'
                and complete.get('mode') == 'correctness' and complete.get('checks') == 23
                and complete.get('failures') == 0 and complete.get('output_error') is False)
    if suite == 'renderer-smoke':
        return (mode == 'correctness' and begin.get('subject') == 'd3d9-readback'
                and complete.get('mode') == 'correctness' and complete.get('checks') == 16
                and complete.get('failures') == 0 and complete.get('output_error') is False)
    if suite == 'single-event':
        expected = {'requested_per_case': iterations, 'created': iterations * 2,
                    'reaped': iterations * 2, 'completed_rounds': iterations,
                    'terminated': iterations, 'woken': iterations,
                    'termination_requests': iterations, 'suspended': iterations,
                    'contexts': iterations, 'resumed': iterations,
                    'cleanup_terminations': 0}
        return (begin.get('iterations') == iterations
                and begin.get('subject_reused') is True
                and all(type(complete.get(key)) is int and complete[key] == value
                        for key, value in expected.items())
                and complete.get('passed') is True
                and complete.get('failures') == 0
                and complete.get('output_error') is False
                and complete.get('all_owned_threads_reaped') is True
                and complete.get('cleanupChecked') is True)
    if suite == 'lifecycle':
        return complete.get('requested_per_case') == iterations
    if suite == 'processes':
        # Reject partial or contradictory receipts, even if their CLI exited 0.
        return (type(begin.get('iterations_per_role')) is int
                and begin['iterations_per_role'] == iterations
                and type(complete.get('iterations_per_role')) is int
                and complete['iterations_per_role'] == iterations
                and complete.get('pointer_bits') == bits
                and begin.get('children') == 4 and begin.get('roles_per_child') == 4
                and complete.get('cleanup_checked') is True)
    return complete.get('mode') == mode


def clock_discontinuity(active_delta, wall_delta):
    # mach_absolute_time stops during macOS sleep; wall time can also jump in
    # either direction. Neither establishes an uninterrupted bounded test.
    return abs(wall_delta - active_delta) > 2


def valid_matrix_checks(rows, complete, inventory=None):
    checks = [row for row in rows if row.get('type') == 'check']
    if (not checks or type(complete.get('checks')) is not int
            or complete['checks'] != len(checks)
            or type(complete.get('failures')) is not int
            or complete['failures'] != sum(row.get('ok') is not True for row in checks)):
        return False
    if any(type(row.get('name')) is not str or type(row.get('ok')) is not bool
           or type(row.get('actual')) is not int or type(row.get('expected')) is not int
           for row in checks):
        return False
    names = Counter(row.get('name') for row in checks)
    if inventory is None:
        inventory_path = Path(__file__).with_name('sync-matrix-inventory.json')
        inventory = json.loads(inventory_path.read_text())
    expected = Counter(inventory['common'])
    if not any(names == expected + Counter(alternative) for alternative in inventory['oneOf']):
        return False
    required = {'U.CS.worker': 4, 'U.SRW.worker': 4, 'U.CV.worker': 2,
                'U.WaitOnAddress.worker': 4, 'U.InitOnce.worker': 4, 'U.SRW.holder-exit': 2}
    for name in ['U.CS.final', 'U.SRW.final', 'U.CV.final', 'U.WaitOnAddress.available',
                 'U.WaitOnAddress.actual-calls', 'U.InitOnce.exactly-one',
                 'U.SRW.exclusive_blocks_shared', 'U.SRW.shared_blocks_exclusive',
                 'U.SRW.uncontended_shared', 'U.SRW.uncontended_exclusive',
                 'S.timer.manual.timer_reset_semantics',
                 'S.timer.synchronization.timer_reset_semantics',
                 'S.timer.manual.cancel_does_not_reset_signaled_state',
                 'S.timer.synchronization.cancel_does_not_reset_signaled_state',
                 'S.iocp.first.bytes', 'S.iocp.first.key', 'S.iocp.first.token',
                 'S.iocp.null_token.bytes', 'S.iocp.null_token.key', 'S.iocp.null_token.token',
                 'S.iocp.drained_empty.overlapped_is_null',
                 'S.pipe.connect.start_pending', 'S.pipe.connect.result_completed',
                 'S.pipe.read.start_pending', 'S.pipe.read.mixed_completion_index',
                 'S.pipe.read.result_completed', 'S.pipe.read.bytes',
                 'S.pipe.read.payload_memcmp', 'S.pipe.read.manual_completion_event_stays_signaled']:
        required[name] = 1
    return all(names[name] == count for name, count in required.items())


parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--bottle', required=True)
parser.add_argument('--arch', choices=['i686', 'x86_64'], required=True)
parser.add_argument('--mode', choices=['correctness', 'stress', 'duration', 'bench'], required=True)
parser.add_argument('--suite', choices=['waitall', 'lifecycle', 'processes', 'single-event', 'matrix', 'nt-extra', 'user-bench', 'wait-widths', 'profile-args', 'renderer-smoke'], default='waitall')
parser.add_argument('--user-kind', choices=['cs', 'srw', 'cv', 'address', 'once'], default='cs')
parser.add_argument('--batches', type=int, default=4)
parser.add_argument('--stats', action='store_true')
parser.add_argument('--benchmark-fixture', choices=['v3', 'v4'], default='v3')
parser.add_argument('--lifecycle-fixture', choices=['standard', 'timeout-diagnostic'], default='standard')
parser.add_argument('--iterations', type=int, default=10000)
parser.add_argument('--threads', type=int, default=4)
parser.add_argument('--timeout', type=int, default=150)
args = parser.parse_args()
requires_owned_cleanup = args.suite in ['profile-args', 'renderer-smoke']
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
if args.benchmark_fixture != 'v3' and (args.suite != 'waitall' or args.mode != 'bench'):
    raise SystemExit('The alternative benchmark fixture is only valid for waitall bench mode')
if args.lifecycle_fixture != 'standard' and (args.suite != 'lifecycle' or args.mode != 'correctness'):
    raise SystemExit('The timeout diagnostic is only valid for lifecycle correctness mode')
source = root / ({'waitall': 'waitall-', 'lifecycle': 'waitall-lifecycle-',
                  'processes': 'waitall-processes-', 'single-event': 'event-lifecycle-',
                  'matrix': 'sync-matrix-', 'nt-extra': 'sync-nt-extra-',
                  'user-bench': 'sync-user-bench-', 'wait-widths': 'sync-wait-widths-',
                  'profile-args': 'profile-args-', 'renderer-smoke': 'renderer-smoke-'}[args.suite] + args.arch + '.exe')
if args.benchmark_fixture == 'v4':
    source = root / ('waitall-bench-v4-' + args.arch + '.exe')
if args.lifecycle_fixture == 'timeout-diagnostic':
    source = root / ('waitall-lifecycle-diag-' + args.arch + '.exe')
destination = prefix / 'drive_c/CiderTests' / source.name
destination.parent.mkdir(exist_ok=True)
shutil.copy2(source, destination)
arguments = ['--' + args.mode]
if args.suite == 'user-bench':
    if (args.mode != 'bench' or args.threads not in [2, 4, 8] or not 1 <= args.batches <= 8
            or args.stats or args.benchmark_fixture != 'v3' or args.timeout > 120):
        raise SystemExit('U benchmark needs bounded bench mode without statistics or other fixture selection')
    arguments = ['--bench', args.user_kind, str(args.iterations), str(args.threads), str(args.batches)]
elif args.suite in ['matrix', 'nt-extra', 'wait-widths', 'profile-args', 'renderer-smoke']:
    if args.mode != 'correctness':
        raise SystemExit('Additional contract suites currently support correctness only')
    arguments = []
elif args.suite in ['lifecycle', 'single-event']:
    maximum = 256 if args.suite == 'single-event' else 4096
    if args.mode != 'correctness' or args.iterations > maximum:
        raise SystemExit('Lifecycle requires a bounded correctness run')
    arguments = ['--iterations', str(args.iterations)]
elif args.suite == 'processes':
    if args.mode != 'correctness' or args.iterations > 1000000:
        raise SystemExit('Cross-process suite requires bounded correctness iterations')
    arguments = [str(args.iterations)]
elif args.mode != 'correctness':
    arguments += [str(args.iterations), str(args.threads)]
environment = {k: os.environ[k] for k in ['HOME', 'USER', 'LOGNAME', 'TMPDIR', 'PATH'] if k in os.environ}
environment.update(CIDER_HOME=str(home), CIDER_APP=str(app))
run_id = str(uuid.uuid4())
command = [str(cli), 'run', '-b', args.bottle, '--wait', '--debug', 'quiet',
           '--env', 'WAITALL_RUN_ID=' + run_id]
if args.stats:
    command += ['--env', 'WINEMSYNC_STATS=1']
if args.lifecycle_fixture == 'timeout-diagnostic':
    command += ['--env', 'WAITALL_DIAGNOSTIC_HOLD_FAILURE=1']
command += ['C:\\CiderTests\\' + source.name, '--'] + arguments
runs = root / 'runs'
runs.mkdir(exist_ok=True)
name = '%s-%s-%s-%s' % (time.time_ns(), config['engine']['id'], args.arch, args.mode)
result_path = runs / (name + '.json')
record = {'schema': 'cider.waitall-mac-run/v1', 'state': 'running', 'bottle': config['id'],
          'engine': config['engine']['id'], 'sync': config['settings'].get('sync', 'msync'),
          'arch': args.arch, 'arguments': arguments, 'prefix': str(prefix),
          'suite': args.suite, 'statisticsRequested': args.stats,
          'benchmarkFixture': args.benchmark_fixture if args.mode == 'bench' and args.suite != 'user-bench' else None,
          'userBenchmark': {'kind': args.user_kind, 'batches': args.batches} if args.suite == 'user-bench' else None,
          'lifecycleFixture': args.lifecycle_fixture if args.suite == 'lifecycle' else None,
          'diagnosticFailureGraceSeconds': 10 if args.lifecycle_fixture == 'timeout-diagnostic' else 0,
          'runnerSHA256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'cleanupHelperSHA256': hashlib.sha256((root / 'owned-processes').read_bytes()).hexdigest(),
          'runID': run_id,
          'exeSHA256': hashlib.sha256(source.read_bytes()).hexdigest(), 'timeoutSeconds': args.timeout,
          'windowsOperations': 0, 'loadAverageBefore': os.getloadavg()}
engine_root = home / 'AppSupport/Engines' / config['engine']['id'] / 'engine'
if not engine_root.is_dir():
    raise SystemExit('Missing owned engine layout')
record['nativeSHA256'] = {str(p.relative_to(engine_root)): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in [engine_root / 'bin/wineserver', engine_root / 'lib/wine/x86_64-unix/ntdll.so',
              engine_root / 'lib/wine/i386-windows/ntdll.dll', engine_root / 'lib/wine/x86_64-windows/ntdll.dll']}
if args.suite == 'matrix':
    matrix_inventory_path = Path(__file__).with_name('sync-matrix-inventory.json')
    matrix_inventory_bytes = matrix_inventory_path.read_bytes()
    matrix_inventory = json.loads(matrix_inventory_bytes)
    record['matrixInventorySHA256'] = hashlib.sha256(matrix_inventory_bytes).hexdigest()
if args.suite == 'user-bench':
    user_validator_path = Path(__file__).with_name('user-bench-validation.py')
    record['userValidatorSHA256'] = hashlib.sha256(user_validator_path.read_bytes()).hexdigest()
    user_validator = runpy.run_path(str(user_validator_path))['valid_user_bench']
result_path.write_text(json.dumps(record, indent=2) + '\n')
start = time.monotonic()
wall_start = time.time()
record['wallStartUnixSeconds'] = wall_start
record['monotonicClock'] = time.get_clock_info('monotonic').implementation
snapshots = []
system_pauses = []
sampling_done = threading.Event()
clock_failed = threading.Event()
def sample_statistics():
    last_monotonic, last_wall = start, wall_start
    while not sampling_done.is_set():
        now_monotonic, now_wall = time.monotonic(), time.time()
        gap = (now_wall - last_wall) - (now_monotonic - last_monotonic)
        if clock_discontinuity(now_monotonic - last_monotonic, now_wall - last_wall):
            # Lifecycle deadlines are invalidated too, even when the guest
            # happens to exit 0. Preserve its original checks and stop only
            # this laboratory; do not infer a lock failure from a clock gap.
            pause = {'wallGapSeconds': gap, 'elapsedActiveSeconds': now_monotonic - start,
                     'cause': 'sleep-or-wall-clock-discontinuity-not-distinguished'}
            system_pauses.append(pause)
            clock_failed.set()
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
sampler = threading.Thread(target=sample_statistics, daemon=True)
sampler.start()
process = None
try:
    # Only this thread may submit or stop a run. The sampler merely reports a
    # clock failure, so it cannot finish cleanup before a delayed submission.
    if clock_failed.is_set() or clock_discontinuity(time.monotonic() - start, time.time() - wall_start):
        record.update(cliExitCode=125, launchSkipped=True, stdout='', stderr='')
        clock_failed.set()
    else:
        process = subprocess.Popen(command, env=environment, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        while True:
            elapsed = time.monotonic() - start
            if clock_failed.is_set() or clock_discontinuity(elapsed, time.time() - wall_start):
                clock_failed.set()
                record['cliExitCode'] = 125
                break
            if elapsed >= args.timeout:
                record.update(cliExitCode=124, timeout=True)
                break
            try:
                stdout, stderr = process.communicate(timeout=min(.25, args.timeout - elapsed))
                record.update(cliExitCode=process.returncode, stdout=stdout, stderr=stderr)
                break
            except subprocess.TimeoutExpired:
                # communicate() retains its partial output across calls. Drain
                # it after termination too; do not discard TimeoutExpired data.
                continue
except OSError as error:
    record.update(cliExitCode=126, launchError=str(error))
finally:
    if process is not None and (process.poll() is None or 'stdout' not in record):
        if process.poll() is None:
            process.terminate()
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            try:
                stdout, stderr = process.communicate(timeout=5)
            except subprocess.TimeoutExpired as error:
                # Another process may retain a pipe after the client dies.
                # EOF is not a prerequisite for cleanup or the result record.
                def partial(value):
                    return value.decode('utf-8', 'replace') if isinstance(value, bytes) else value or ''
                stdout, stderr = partial(error.output), partial(error.stderr)
                record['clientOutputDrainIncomplete'] = True
                for stream in (process.stdout, process.stderr):
                    if stream is not None:
                        stream.close()
                try:
                    process.wait(timeout=1)
                except subprocess.TimeoutExpired:
                    record['clientReapUnconfirmed'] = True
        record.update(stdout=stdout, stderr=stderr, clientExitCodeAfterStop=process.returncode)
    sampling_done.set()
    if sampler:
        sampler.join(timeout=25)
        if sampler.is_alive():
            record['statisticsSamplerDidNotStop'] = True
record['pumpSnapshots'] = snapshots
record['systemPauses'] = system_pauses
record.update(seconds=time.monotonic() - start, wallElapsedSeconds=time.time() - wall_start, loadAverageAfter=os.getloadavg())
record['timingDiscontinuity'] = clock_failed.is_set() or bool(system_pauses) or clock_discontinuity(record['seconds'], record['wallElapsedSeconds'])
if record['timingDiscontinuity'] or record.get('timeout') or record.get('launchError') or requires_owned_cleanup:
    # Includes gaps only discovered after the last sampler tick. Reap the
    # laboratory GUI (the only Wine spawner), then drain its owned processes;
    # an empty snapshot alone cannot exclude a queued or paused launch.
    cleanup_record = {}
    try:
        cleanup = subprocess.run([str(root / 'owned-processes'), 'stop-runtime', str(root)],
                                 text=True, capture_output=True, timeout=20)
        receipt = json.loads(cleanup.stdout)
        cleanup_record['ownedRoot'] = {'exitCode': cleanup.returncode, 'receipt': receipt}
        cleanup_record['confirmed'] = (cleanup.returncode == 0 and not record.get('clientReapUnconfirmed')
                                       and receipt.get('mode') == 'stop-runtime'
                                       and receipt.get('hostShutdown', {}).get('confirmed') is True
                                       and not receipt.get('pending', [None]))
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        cleanup_record.update(confirmed=False, ownedRootError=str(error))
    if requires_owned_cleanup:
        cleanup_record['confirmed'] = (cleanup_record.get('confirmed') is True
            and cleanup_record.get('ownedRoot', {}).get('receipt', {}).get('emptyObservations', 0) >= 2)
        record['ownedRootCleanup'] = cleanup_record
    else:
        record['interruptionCleanup'] = cleanup_record
if args.mode == 'duration' and (record['seconds'] < args.iterations or
        record['timingDiscontinuity']):
    record['continuousDurationNotEstablished'] = True
match = re.search(r'\blog (/.+)', record.get('stdout', ''))
session_logs = []
session_root = home / 'Logs/sessions' / config['id']
for manifest in session_root.glob('*/session.json'):
    if manifest.stat().st_size > 1024 * 1024:
        continue
    try:
        plan = json.loads(manifest.read_text())
        if (plan.get('env', {}).get('WAITALL_RUN_ID') == run_id
                and plan.get('bottleID') == config['id'] and plan.get('engineID') == config['engine']['id']):
            session_logs.append(manifest.with_name('wine.log'))
    except (OSError, ValueError):
        continue
if len(session_logs) != 1:
    record['sessionBindingNotEstablished'] = True
rows = []
if len(session_logs) == 1:
    log = session_logs[0]
    if match and Path(match.group(1).strip()) != log:
        record['sessionBindingNotEstablished'] = True
    if not log.resolve().is_relative_to(home / 'Logs/sessions'):
        raise SystemExit('Foreign session log')
    record['log'] = str(log)
    if not log.exists():
        record['sessionLogMissing'] = True
    elif log.stat().st_size <= 4 * 1024 * 1024:
        for line in log.read_text(errors='replace').splitlines():
            if line.startswith('{'):
                try:
                    row = json.loads(line)
                    if row.get('type') in ['begin', 'check', 'complete', 'stress', 'worker', 'resources', 'bench', 'roundtrip', 'wakeup', 'progress', 'layout', 'limits', 'case', 'result', 'error', 'idle', 'lifecycle-timeout-capture', 'timeout_observation', 'batch', 'summary', 'adapter']:
                        rows.append(row)
                except json.JSONDecodeError:
                    record['invalidJSONLine'] = True
    else:
        record['logTooLargeForAcceptance'] = True
record['rows'] = rows
if args.lifecycle_fixture == 'timeout-diagnostic':
    phases = [r.get('phase') for r in rows if r.get('type') == 'lifecycle-timeout-capture']
    record['diagnosticCaptureIncomplete'] = 'begin' in phases and 'end' not in phases
reported_errors = [r for r in rows if r.get('type') == 'error']
record['reportedErrors'] = reported_errors
begins = [r for r in rows if r.get('type') == 'begin']
completes = [r for r in rows if r.get('type') == 'complete']
failed = [r for r in rows if r.get('type') == 'check' and not r.get('ok')]
if args.suite in ['lifecycle', 'single-event']:
    begins = [r for r in rows if r.get('type') == 'limits']
metadata_ok = valid_metadata(args.suite, args.arch, args.iterations, args.mode, begins, completes)
if args.suite == 'user-bench':
    record['userValidatorUnchanged'] = hashlib.sha256(user_validator_path.read_bytes()).hexdigest() == record['userValidatorSHA256']
    metadata_ok = (metadata_ok and record['userValidatorUnchanged']
                   and user_validator(rows, args.arch, args.user_kind, args.iterations, args.threads, args.batches))
if args.suite == 'profile-args':
    checks = [r for r in rows if r.get('type') == 'check']
    expected = Counter({name: 1 for name in ['new_owned_key', 'missing', 'append', 'unterminated',
        'implicit', 'odd', 'type', 'empty', 'oversized', 'limit', 'limit-nul', 'registry_close', 'registry_delete']})
    expected['registry_write'] = 10
    metadata_ok = (metadata_ok and len(checks) == 23
        and all(type(r.get('name')) is str and r.get('ok') is True
            and type(r.get('actual')) is int and type(r.get('expected')) is int for r in checks)
        and Counter(r['name'] for r in checks) == expected)
if args.suite == 'renderer-smoke':
    checks = [r for r in rows if r.get('type') == 'check']
    expected = Counter({name: 1 for name in ['register_class', 'create_window', 'd3d9_create',
        'adapter_identifier', 'create_hal_device', 'clear', 'get_render_target', 'surface_layout',
        'create_system_copy', 'render_target_readback', 'lock_copy', 'row_layout', 'exact_rgb_pixels',
        'unlock_copy', 'destroy_window', 'unregister_class']})
    metadata_ok = (metadata_ok and len(checks) == 16
        and all(type(r.get('name')) is str and r.get('ok') is True
            and type(r.get('actual')) is int and type(r.get('expected')) is int for r in checks)
        and Counter(r['name'] for r in checks) == expected)
if args.suite == 'wait-widths':
    checks = [r for r in rows if r.get('type') == 'check']
    groups = {}
    for row in checks:
        groups.setdefault((row.get('mode'), row.get('width')), []).append(row)
    expected_groups = {('any', 2), ('any', 8), ('any', 64), ('all', 2), ('all', 8)}
    metadata_ok = metadata_ok and set(groups) == expected_groups and len(checks) == 214
    for (mode, count), group in groups.items():
        if (mode, count) not in expected_groups:
            metadata_ok = False
            continue
        if mode == 'any':
            expected = Counter({'initial_timeout': 1, 'drained_timeout': 1, 'late_peer_index': 1,
                'peer_exit': 1, 'late_object_state': 1, 'lowest_ready_index': count,
                'selected_object_state': count, 'release_previous_zero': count // 3})
        else:
            expected = Counter({'blocked_timeout': 1, 'blocked_preserves_ready': count - 1,
                'sentinel_still_empty': 1, 'all_ready_range': 1, 'final_object_state': count,
                'cannot_consume_twice': 1, 'release_previous_zero': (count - 1) // 3})
        # Python 3.9 compares explicit zero entries; this group may have no semaphore.
        expected = Counter({name: value for name, value in expected.items() if value})
        metadata_ok = (metadata_ok and all(type(r.get('name')) is str and r.get('ok') is True
            and type(r.get('actual')) is int and type(r.get('expected')) is int for r in group)
            and Counter(r['name'] for r in group) == expected)
if args.suite == 'matrix':
    record['matrixInventoryUnchanged'] = hashlib.sha256(matrix_inventory_path.read_bytes()).hexdigest() == record['matrixInventorySHA256']
    metadata_ok = (metadata_ok and record['matrixInventoryUnchanged'] and len(completes) == 1
                   and valid_matrix_checks(rows, completes[0], matrix_inventory))
if args.suite == 'nt-extra':
    named_checks = [r for r in rows if r.get('type') == 'check']
    expected_names = Counter({'N.MsgWait.post': 1, 'N.MsgWait.message_payload': 1,
        'N.MsgWait.event_index': 1, 'N.MsgWait.auto_consumed': 1,
        'N.SignalWait.handshake': 1, 'N.SignalWait.peer_exit': 1,
        'N.SignalWait.reply_consumed': 1, 'N.Pulse.initial_signaled': 1, 'N.Pulse.final_reset_only': 1,
        'N.Timeout.native_api': 1, 'N.Timeout.status': 4, 'N.Timeout.nonnegative_clock': 4})
    observations = [r for r in rows if r.get('type') == 'timeout_observation']
    metadata_ok = (metadata_ok and len(named_checks) == 18
        and all(type(r.get('name')) is str and r.get('ok') is True for r in named_checks)
        and Counter(r['name'] for r in named_checks) == expected_names
        and len(observations) == 4
        and all(type(r.get('requested_100ns')) is int and type(r.get('qpc_ticks')) is int
            and r['qpc_ticks'] >= 0 and type(r.get('qpc_frequency')) is int
            and r['qpc_frequency'] > 0 and r.get('precision_gate_applied') is False for r in observations)
        and sorted(r['requested_100ns'] for r in observations) == [0, 10000, 156000, 1000000])
if requires_owned_cleanup:
    record['acceptanceElapsedActiveSeconds'] = time.monotonic() - start
    record['acceptanceElapsedWallSeconds'] = time.time() - wall_start
    record['timingDiscontinuity'] = record['timingDiscontinuity'] or clock_discontinuity(
        record['acceptanceElapsedActiveSeconds'], record['acceptanceElapsedWallSeconds'])
record['passed'] = (record['cliExitCode'] == 0 and metadata_ok and completes[0].get('ok') is True
                    and (not requires_owned_cleanup or record.get('ownedRootCleanup', {}).get('confirmed') is True)
                    and not failed and not reported_errors and not record.get('invalidJSONLine') and not record.get('logTooLargeForAcceptance')
                    and not record.get('statisticsSamplerDidNotStop')
                    and not record.get('sessionBindingNotEstablished')
                    and not record.get('continuousDurationNotEstablished') and not record['timingDiscontinuity'])
record['state'] = 'complete'
result_path.write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps({'result': str(result_path), 'passed': record['passed'], 'cliExitCode': record['cliExitCode'],
                  'seconds': record['seconds'], 'failedChecks': failed,
                  'measurements': [r for r in rows if r.get('type') in ['bench', 'roundtrip', 'wakeup', 'stress', 'resources']]}), flush=True)
sys.exit(0 if record['passed'] else 1)
