#!/usr/bin/env python3
"""Build the pinned, explicitly scoped probes with an existing Wine PE build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

ORIGINAL = 'd0eeffaef25d80232da52780094da328a4aa08cc90bdb4426ca1172ec5b38b85'
PREPARED = 'd666d6457508d3ebfc73131ac9a1e8e753dba999f884692d75f28a80e902c1c8'


def sha(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wine-source', type=Path, required=True)
    parser.add_argument('--wine-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='A new directory; never overwritten.')
    parser.add_argument('--prepare-only', action='store_true', help='Verify reconstruction and record commands without compiling.')
    args = parser.parse_args()
    source_tree, build = args.wine_source.resolve(), args.wine_build.resolve()
    original = source_tree / 'dlls/mfreadwrite/tests/mfplat.c'
    if sha(original) != ORIGINAL:
        raise SystemExit('This fixture requires the pinned Wine 11.18 test source; port it explicitly.')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    staged = output / 'source/dlls/mfreadwrite/tests/mfplat.c'
    staged.parent.mkdir(parents=True)
    staged.write_bytes(original.read_bytes())
    patch = Path(__file__).with_name('probe.patch').resolve()
    applied = subprocess.run(['/usr/bin/patch', '-p1', '--batch', '--fuzz=0', '-i', str(patch)],
                             cwd=output / 'source', capture_output=True, text=True, timeout=20)
    if applied.returncode or 'offset' in applied.stdout or 'fuzz' in applied.stdout or sha(staged) != PREPARED:
        raise SystemExit('Fixture reconstruction failed: ' + applied.stdout + applied.stderr)
    testlist = output / 'testlist.c'
    testlist.write_text('#define STANDALONE\n#include "windows.h"\n#include "wine/test.h"\n'
                        'extern void func_mfplat(void);\n'
                        'const struct test winetest_testlist[] = {{"mfplat",func_mfplat},{0,0}};\n')
    report = dict(schema='cider.mf-source-reader-probe-build/v1', originalSHA256=ORIGINAL,
                  sourceSHA256=PREPARED, patchSHA256=sha(patch), runnerSHA256=sha(Path(__file__)),
                  makefileSHA256=sha(build / 'Makefile'), preparedOnly=args.prepare_only,
                  runtimePassed=False, rows=[])
    manifest = output / 'build.json'

    def save():
        manifest.write_text(json.dumps(report, indent=2) + '\n')

    save()
    try:
        reader_source = source_tree / 'dlls/mfreadwrite/reader.c'
        for architecture in ['i386', 'x86_64']:
            target = f'dlls/mfreadwrite/{architecture}-windows/reader.o'
            dry = subprocess.run(['make', '-n', '--no-print-directory', '-W', str(reader_source), target],
                                 cwd=build, capture_output=True, text=True, timeout=30)
            if dry.returncode:
                raise RuntimeError(dry.stderr)
            commands = [shlex.split(line) for line in dry.stdout.replace('\\\n', ' ').splitlines()
                        if str(reader_source) in line and ' -c ' in line]
            if len(commands) != 1 or not Path(commands[0][0]).name.endswith('-gcc'):
                raise RuntimeError('Expected one configured Wine PE GCC command.')
            flags = commands[0]
            target_cpu = 'i686' if architecture == 'i386' else architecture
            env = {key: os.environ[key] for key in ['HOME', 'USER', 'LOGNAME', 'TMPDIR'] if key in os.environ}
            env['PATH'] = ':'.join([str(Path(flags[0]).parent), '/usr/bin', '/bin', '/usr/sbin', '/sbin'])
            row = dict(arch=architecture, operations=[])
            report['rows'].append(row)
            objects = []
            for label, file in [('probe', staged), ('testlist', testlist)]:
                object_file = output / (architecture + '-' + label + '.o')
                command = [str(file) if value == str(reader_source) else value for value in flags]
                command[command.index('-o') + 1] = str(object_file)
                if '-Werror' not in command:
                    command.append('-Werror')
                operation = dict(stage=label, args=command)
                row['operations'].append(operation)
                if not args.prepare_only:
                    result = subprocess.run(command, cwd=build, env=env, capture_output=True, text=True, timeout=90)
                    operation.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
                    if result.returncode:
                        raise RuntimeError('Probe compilation failed.')
                    operation['sha256'] = sha(object_file)
                objects.append(object_file)
            executable = output / ('mf-source-reader-' + target_cpu + '.exe')
            command = [str(build / 'tools/winegcc/winegcc'), '-o', str(executable), '--wine-objdir', '.',
                       '--cc-cmd=' + flags[0], '-b', target_cpu + '-w64-mingw32'] + [str(p) for p in objects]
            for library in ['ole32', 'user32', 'd3d11', 'd3d9', 'dxva2', 'mfplat', 'mf', 'mfreadwrite', 'propsys']:
                command.append(f'dlls/{library}/{architecture}-windows/lib{library}.a')
            for library in ['mfuuid', 'uuid', 'winecrt0', 'compiler-rt']:
                command.append(f'libs/{library}/{architecture}-windows/lib{library}.a')
            for library in ['ucrtbase', 'kernel32', 'ntdll']:
                command.append(f'dlls/{library}/{architecture}-windows/lib{library}.a')
            operation = dict(stage='link', args=command)
            row['operations'].append(operation)
            if not args.prepare_only:
                result = subprocess.run(command, cwd=build, env=env, capture_output=True, text=True, timeout=90)
                operation.update(exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
                if result.returncode:
                    raise RuntimeError('Probe link failed.')
                row['exeSHA256'] = sha(executable)
            save()
        report['state'] = 'prepared' if args.prepare_only else 'built'
    except Exception as error:
        report.update(state='failed', error=str(error))
        raise
    finally:
        save()
    print(manifest)


if __name__ == '__main__':
    main()
