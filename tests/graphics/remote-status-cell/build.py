#!/usr/bin/env python3
"""Build the unintegrated NT-section fixture; never launches Wine or downloads tools."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wine-source', type=Path, required=True,
                    help='private Wine 11.18 source with the typed remote packet header')
parser.add_argument('--cc32', type=Path, required=True)
parser.add_argument('--cc64', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True, help='new, empty output directory')
parser.add_argument('--before-wire', type=Path,
                    help='optional original status_wire.h for exact alias comparison')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
fixture = Path(__file__).resolve().parent
component = repo / 'engine/porting/wine-11.18/experiments/render-status-cell'
output = args.out.resolve()
output.mkdir(parents=True, exist_ok=False)
record = dict(passed=False, scope='PE compilation only; no execution or engine integration', operations=[])
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()

def save():
    (output / 'build.json').write_text(json.dumps(record, indent=2) + '\n')

def compile_one(compiler, architecture, name, sources, includes):
    executable = output / (architecture + '-' + name + '.exe')
    command = [str(compiler.resolve()), '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror']
    command += ['-I' + str(path.resolve()) for path in includes]
    command += [str(path.resolve()) for path in sources]
    command += ['-o', str(executable), '-luser32', '-lntdll']
    result = subprocess.run(command, capture_output=True, text=True)
    row = dict(architecture=architecture, name=name, args=command,
               exitCode=result.returncode, stdout=result.stdout, stderr=result.stderr)
    record['operations'].append(row)
    if result.returncode == 0:
        row.update(executable=str(executable), exeSHA256=sha(executable))
    save()
    if result.returncode:
        raise RuntimeError(result.stderr)

save()
try:
    packet = args.wine_source / 'include/wine/macdrv_remote.h'
    inputs = [fixture / 'client.c', fixture / 'alias.c', packet,
              repo / 'tests/graphics/remote-retirement/fixture.h',
              *[component / name for name in ['status_wire.h', 'status_cell.h', 'status_cell.c']]]
    if args.before_wire:
        if args.before_wire.name != 'status_wire.h':
            raise ValueError('--before-wire must name an isolated status_wire.h')
        inputs.append(args.before_wire)
    record['inputs'] = [dict(path=str(path.resolve()), sha256=sha(path)) for path in inputs]
    save()
    for architecture, compiler in [('i386', args.cc32), ('x86_64', args.cc64)]:
        compile_one(compiler, architecture, 'status-cell',
                    [fixture / 'client.c', component / 'status_cell.c'],
                    [component, repo / 'tests/graphics/remote-retirement', args.wine_source / 'include'])
        compile_one(compiler, architecture, 'alias-fixed', [fixture / 'alias.c'], [component])
        if args.before_wire:
            compile_one(compiler, architecture, 'alias-before', [fixture / 'alias.c'],
                        [args.before_wire.parent])
    record['passed'] = True
finally:
    save()
