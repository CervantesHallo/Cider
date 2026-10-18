#!/usr/bin/env python3
"""Real packaged CLI, one private App URL, two marked roots; no product GUI/Wine.

Baseline: --phase baseline --fixture-revision 731267a
Final (only after the parent rebuilds the package): --phase final
Only FixtureAppHost is built, in a disposable source/build snapshot. Production
CLI/Bridge bytes are copied from --app; neither is built by this runner.
"""
import argparse
import datetime
import hashlib
import io
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
LSREGISTER = Path('/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister')


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(command, *, timeout=15, check=True, **kwargs):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=timeout, **kwargs)
    if check and result.returncode:
        raise RuntimeError(f'{command[0]} exited {result.returncode}: {result.stderr[-3000:]} {result.stdout[-3000:]}')
    return result


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def processes():
    result = run(['/bin/ps', '-axo', 'pid=,comm='])
    return {int(fields[0]): str(Path(fields[1]).resolve()) for line in result.stdout.splitlines()
            if len(fields := line.strip().split(None, 1)) == 2}


def source_hashes():
    # Local to this runner: do not alter the existing native harness's helper.
    files = sorted((REPO / 'Packages/CiderKit/Sources').rglob('*.swift'))
    files += sorted((REPO / 'Tools/CiderCommands/Sources').rglob('*.swift'))
    files += sorted((HERE / 'Fixture/Sources').rglob('*.swift'))
    files += [REPO / 'Tools/ciderctl/Sources/ciderctl/ciderctl.swift', REPO / 'Tools/ciderctl/Package.swift',
              REPO / 'App/Sources/Cider/CommandHost.swift', HERE / 'run_data_roots.py']
    return {str(path.relative_to(REPO)): digest(path) for path in files}


def source_snapshot(destination, revision):
    paths = ['Packages/CiderKit', 'Tools/CiderCommands', 'tests/xpc/Fixture']
    if revision:
        archive = subprocess.run(['/usr/bin/git', 'archive', revision, *paths], cwd=REPO,
                                 capture_output=True, check=True).stdout
        with tarfile.open(fileobj=io.BytesIO(archive)) as contents:
            for member in contents.getmembers():
                require(not Path(member.name).is_absolute() and '..' not in Path(member.name).parts
                        and (member.isfile() or member.isdir()), 'unexpected archive path/type')
            contents.extractall(destination)
    else:
        for relative in paths:
            shutil.copytree(REPO / relative, destination / relative,
                            ignore=shutil.ignore_patterns('.build', '.git', '.swiftpm'))
    return {str(path.relative_to(destination)): digest(path)
            for path in sorted(destination.rglob('*')) if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=['baseline', 'final'], required=True)
    parser.add_argument('--fixture-revision', help='Freeze only fixture/dependency sources at this Git revision')
    parser.add_argument('--fixture-host', type=Path, help='Use an existing matching FixtureAppHost; do not build anything')
    parser.add_argument('--app', type=Path, default=REPO / 'out/Cider.app')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    evidence = (args.output or HERE / 'results' / f'data-roots-{args.phase}-{timestamp}.json').resolve()
    require(not evidence.exists(), 'result files must be new')
    directory = Path(tempfile.mkdtemp(prefix='cider-data-roots-')).resolve()
    bundle = directory / 'ReadonlyDataRoots.app'
    binaries = bundle / 'Contents/MacOS'
    identifier = 'org.cider.datarootsfixture.' + uuid.uuid4().hex
    roots = {}
    children = []
    owned_pids = {}
    report = {'kind': 'real-packaged-cli-two-data-roots', 'phase': args.phase, 'createdUTC': timestamp,
              'cases': [], 'productGUIOrWineSessions': 0, 'unitTestsIncluded': 0,
              'bundle': str(bundle), 'bundleIdentifier': identifier, 'roots': {},
              'fixtureSourceRevision': args.fixture_revision or 'working-tree-snapshot',
              'productionBuildPerformed': False}
    report['sourceHashes'] = source_hashes()

    def job(root):
        return run(['/bin/launchctl', 'print', f'gui/{os.getuid()}/{root["label"]}'], check=False)

    def observe():
        current = processes()
        allowed = {str(binaries / name) for name in ['Cider', 'ciderctl', 'CiderXPCBridge']}
        for pid, executable in current.items():
            if executable in allowed:
                owned_pids[pid] = executable
        for root in roots.values():
            match = re.search(r'\bpid = (\d+)', job(root).stdout)
            if match:
                pid = int(match.group(1))
                if current.get(pid) == str(binaries / 'CiderXPCBridge'):
                    root['brokerPIDs'].add(pid)
            ready = root['home'] / 'ready.json'
            if ready.exists():
                value = json.loads(ready.read_text())
                require(value['label'] == root['label'] and value['root'] == root['root'], 'host ready root/label mismatch')
                root['hostPIDs'].add(value['hostPID'])
        return current

    def receipts(root):
        path = root['home'] / 'executions.jsonl'
        return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []

    def invoke(root, count, name):
        started = time.monotonic()
        calls = []
        for _ in range(count):
            process = subprocess.Popen([str(binaries / 'ciderctl'), 'engine', 'list'], cwd=directory,
                                       env=root['environment'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            children.append(process)
            calls.append(process)
        deadline = started + 36
        while any(process.poll() is None for process in calls):
            observe()
            require(time.monotonic() < deadline, f'{name}: bounded CLI deadline exceeded')
            time.sleep(0.08)
        observe()
        results = []
        for process in calls:
            stdout, stderr = process.communicate(timeout=2)
            results.append({'pid': process.pid, 'exitCode': process.returncode, 'stdout': stdout, 'stderr': stderr})
        case = {'name': name, 'seconds': time.monotonic() - started, 'cliResults': results,
                'receipts': receipts(root), 'hostPIDs': sorted(root['hostPIDs']), 'brokerPIDs': sorted(root['brokerPIDs'])}
        case['passed'] = all(result['exitCode'] == 0 and result['stdout'] == 'no engines installed\n'
                             and not result['stderr'] for result in results)
        report['cases'].append(case)
        print(('PASS ' if case['passed'] else 'FAIL ') + name, flush=True)
        return case

    def verify_root(root, expected_count, expected_pid=None):
        entries = receipts(root)
        require(len(entries) == expected_count, f'{root["name"]}: expected {expected_count} receipts, got {len(entries)}')
        require(len(root['hostPIDs']) == 1 and len(root['brokerPIDs']) == 1, f'{root["name"]}: duplicate/missing root components')
        pid = next(iter(root['hostPIDs']))
        require(expected_pid is None or pid == expected_pid, 'warm call replaced the original host')
        require(all(entry['executorPID'] == pid and entry['appSupport'] == root['root']
                    and entry['arguments'] == ['engine', 'list'] and entry['workingDirectory'] == str(directory)
                    for entry in entries), 'receipt belongs to another host/root/command/cwd')
        require(len({entry['requestID'] for entry in entries}) == len(entries), 'request IDs repeated')
        require(processes().get(pid) == str(binaries / 'Cider'), 'owned host is no longer alive')
        return pid

    try:
        if args.fixture_host:
            fixture_host = args.fixture_host.resolve()
            report['fixtureBuild'] = {'performed': False, 'cachedFixtureHost': str(fixture_host)}
        else:
            snapshot = directory / 'source'
            report['fixtureBuildSourceHashes'] = source_snapshot(snapshot, args.fixture_revision)
            package = snapshot / 'tests/xpc/Fixture'
            build_args = ['/usr/bin/swift', 'build', '--skip-update', '--package-path', package,
                          '--scratch-path', directory / 'fixture-build']
            build = run([*build_args, '--product', 'FixtureAppHost'], timeout=300)
            report['fixtureBuild'] = {'performed': True, 'exitCode': build.returncode, 'logTail': (build.stdout + build.stderr)[-4000:]}
            fixture_host = Path(run([*build_args, '--show-bin-path']).stdout.strip()) / 'FixtureAppHost'
        report['fixtureHostSHA256BeforeSigning'] = digest(fixture_host)
        binaries.mkdir(parents=True)
        report['productionBinaryCopies'] = {}
        for name in ['ciderctl', 'CiderXPCBridge']:
            source = args.app.resolve() / 'Contents/MacOS' / name
            if args.phase == 'final':
                inputs = list((REPO / 'Packages/CiderKit/Sources/CiderIPC').glob('*.swift'))
                if name == 'ciderctl':
                    inputs.append(REPO / 'Tools/ciderctl/Sources/ciderctl/ciderctl.swift')
                require(source.stat().st_mtime_ns >= max(path.stat().st_mtime_ns for path in inputs),
                        'final packaged binary predates its current source; wait for the parent release build')
            original = digest(source)
            shutil.copy2(source, binaries / name)
            require(original == digest(source) == digest(binaries / name), 'production binary changed during copy')
            report['productionBinaryCopies'][name] = {'source': str(source), 'originalSHA256': original,
                                                      'copiedBytesMatchBeforeFixtureSigning': True}
        shutil.copy2(fixture_host, binaries / 'Cider')
        info = {'CFBundleIdentifier': identifier, 'CFBundleExecutable': 'Cider', 'CFBundleName': 'Cider Readonly Data Roots Fixture',
                'CFBundlePackageType': 'APPL', 'CFBundleVersion': '1', 'LSUIElement': True, 'LSMinimumSystemVersion': '14.0'}
        (bundle / 'Contents/Info.plist').write_bytes(plistlib.dumps(info))
        for name in ['Cider', 'ciderctl', 'CiderXPCBridge']:
            run(['/usr/bin/codesign', '--force', '--sign', '-', '--options', 'runtime', '--identifier', identifier + '.' + name, binaries / name])
        run(['/usr/bin/codesign', '--force', '--sign', '-', '--options', 'runtime', bundle])
        run(['/usr/bin/codesign', '--verify', '--deep', '--strict', bundle])
        report['signatures'] = {}
        for name, path in [('host', bundle), ('client', binaries / 'ciderctl'), ('broker', binaries / 'CiderXPCBridge')]:
            signed = run(['/usr/bin/codesign', '-d', '--verbose=4', '-r-', path])
            report['signatures'][name] = signed.stdout + signed.stderr
            require('runtime' in report['signatures'][name], 'missing hardened runtime')
        for name in ['A', 'B']:
            home = directory / ('marked-root-' + name)
            home.mkdir(mode=0o700)
            (home / 'cold-fixture-owned').write_text('private readonly data-root fixture ' + name + '\n')
            environment = {'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'HOME': str(Path.home()),
                           'CIDER_HOME': str(home), 'TMPDIR': str(directory), 'LANG': 'en_US.UTF-8'}
            description = json.loads(run([binaries / 'Cider', '--describe-fixture'], env=environment).stdout)
            expected_root = str(home / 'AppSupport')
            expected_label = 'org.cider.ipc.' + hashlib.sha256((str(bundle) + '\n' + expected_root).encode()).hexdigest()[:24]
            require(description['canonicalBundle'] == str(bundle) and description['root'] == expected_root
                    and description['label'] == expected_label, 'fixture description escaped its app/root namespace')
            root = {'name': name, 'home': home, 'root': expected_root, 'label': expected_label,
                    'environment': environment, 'hostPIDs': set(), 'brokerPIDs': set()}
            roots[name] = root
            report['roots'][name] = description
            require(job(root).returncode != 0, 'fixture job unexpectedly exists before cold call')
        require(roots['A']['label'] != roots['B']['label'], 'two roots share a service label')
        first = invoke(roots['A'], 4, 'root_A.concurrent_cold_4')
        require(first['passed'], 'root A cold calls failed')
        original_a = verify_root(roots['A'], 4)
        second = invoke(roots['B'], 3, 'root_B.concurrent_cold_3_while_A_runs')
        require(verify_root(roots['A'], 4, original_a) == original_a, 'B cold call disturbed A')
        if second['passed']:
            original_b = verify_root(roots['B'], 3)
            require(original_a != original_b, 'different roots reused one host')
            require(invoke(roots['B'], 1, 'root_B.warm_same_host')['passed'], 'B warm call failed')
            verify_root(roots['B'], 4, original_b)
        else:
            report['baselineFailureConfirmed'] = (
                args.phase == 'baseline' and not receipts(roots['B']) and not roots['B']['hostPIDs']
                and second['seconds'] >= 14
                and all(result['exitCode'] == 75 and 'Cider未连接到当前资料目录' in result['stderr']
                        for result in second['cliResults']))
        require(invoke(roots['A'], 1, 'root_A.warm_after_B')['passed'], 'A warm call after B failed')
        verify_root(roots['A'], 5, original_a)
        hosts = {pid for pid, executable in owned_pids.items() if executable == str(binaries / 'Cider')}
        expected_hosts = set().union(*(root['hostPIDs'] for root in roots.values()))
        require(hosts == expected_hosts, 'unrecorded/duplicate fixture GUI instance detected')
        require(len(hosts) == (2 if second['passed'] else 1), 'wrong number of root hosts')
        report['singleHostPerRootVerified'] = True
    except Exception as error:
        report['error'] = str(error)
        print('RUN ERROR: ' + str(error), flush=True)
    finally:
        errors = []
        try:
            observe()
        except Exception as error:
            errors.append('observe: ' + str(error))
        # Children are direct owned subprocesses; all other signals require an exact
        # private executable-path match immediately before signalling (PID reuse safe).
        for child in children:
            if child.poll() is None:
                child.kill()
            child.communicate(timeout=3)
        for pid, executable in list(owned_pids.items()):
            if executable != str(binaries / 'Cider'):
                continue
            try:
                if processes().get(pid) == executable:
                    os.kill(pid, signal.SIGTERM)
                    deadline = time.monotonic() + 2
                    while processes().get(pid) == executable and time.monotonic() < deadline:
                        time.sleep(0.03)
                    if processes().get(pid) == executable:
                        os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            except Exception as error:
                errors.append('host cleanup: ' + str(error))
        jobs = {}
        for name, root in roots.items():
            try:
                if job(root).returncode == 0:
                    file = root['home'] / 'AppSupport/State/XPC' / (root['label'] + '.plist')
                    specification = plistlib.loads(file.read_bytes())
                    arguments = specification['ProgramArguments']
                    require(specification['Label'] == root['label']
                            and Path(arguments[0]).resolve() == binaries / 'CiderXPCBridge'
                            and arguments[1:] == ['--root', root['root'], '--label', root['label']],
                            'refusing to bootout a job without exact private ownership')
                    run(['/bin/launchctl', 'bootout', f'gui/{os.getuid()}/{root["label"]}'], check=False)
                jobs[name] = job(root).returncode != 0
                require(jobs[name], 'owned job remains registered')
            except Exception as error:
                errors.append('job cleanup: ' + str(error))
        deadline = time.monotonic() + 5
        live = {}
        while True:
            current = processes()
            live = {pid: executable for pid, executable in current.items()
                    if executable in {str(binaries / name) for name in ['Cider', 'ciderctl', 'CiderXPCBridge']}}
            if not live or time.monotonic() >= deadline:
                break
            time.sleep(0.05)
        if live:
            errors.append('owned fixture processes remain alive')
        ls_absent = False
        if bundle.exists():
            try:
                unregister = run([LSREGISTER, '-u', bundle])
                dump = run([LSREGISTER, '-dump'], timeout=25).stdout
                ls_absent = str(bundle) not in dump and identifier not in dump
                report['launchServicesCleanup'] = {'exitCode': unregister.returncode,
                    'stdout': unregister.stdout, 'stderr': unregister.stderr, 'privateBundleAndIdentifierAbsentFromDump': ls_absent}
                require(ls_absent, 'private App still appears in LaunchServices dump after unregister')
            except Exception as error:
                errors.append('LaunchServices cleanup: ' + str(error))
        else:
            ls_absent = True
        report['cleanup'] = {'passed': not errors and not live and all(jobs.values()) and ls_absent,
                             'jobsAbsent': jobs, 'observedOwnedPIDs': owned_pids, 'liveOwnedPIDs': live, 'errors': errors}
        report['nativeCasesExecuted'] = len(report['cases'])
        report['sourceHashesAfterRun'] = source_hashes()
        report['sourceChangedDuringRun'] = report['sourceHashes'] != report['sourceHashesAfterRun']
        if args.phase == 'final' and report['sourceChangedDuringRun']:
            report['error'] = 'source changed during final verification; do not attribute this run to final source'
        report['nativeCasesPassed'] = sum(case['passed'] for case in report['cases'])
        report['passed'] = (not report.get('error') and report['cleanup']['passed']
                            and report.get('singleHostPerRootVerified', False)
                            and all(case['passed'] for case in report['cases']) and len(report['cases']) == 4)
        report['baselineReproducedAndCleaned'] = (report.get('baselineFailureConfirmed', False)
                                                  and not report.get('error') and report['cleanup']['passed'])
        if not report['cleanup']['passed']:
            report['preservedTemporaryDirectory'] = str(directory)
        evidence.parent.mkdir(parents=True, exist_ok=True)
        evidence.write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n')
        print(f"Native cases {report['nativeCasesPassed']}/{report['nativeCasesExecuted']}; cleanup {report['cleanup']['passed']}")
        print('Evidence: ' + str(evidence), flush=True)
        if report['cleanup']['passed']:
            shutil.rmtree(directory)
    return 0 if report['passed'] or report['baselineReproducedAndCleaned'] else 1


if __name__ == '__main__':
    sys.exit(main())
