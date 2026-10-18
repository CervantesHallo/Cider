#!/usr/bin/env python3
"""Require the reference models to reject deliberate ownership regressions."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--report', type=Path, required=True)
a = p.parse_args()
tests = Path(__file__).resolve().parent
mutations = [
    ('foreign-manager', 'server/device.c', 'if (manager->owner != current->process)', 'if (0)', 'server'),
    ('wrong-primary-owner', 'server/process.c', 'grab_object( process->token )', 'grab_object( current->process->token )', 'server'),
    ('missing-backend-owner', 'server/device.c', '    grab_kernel_object( token_ref );', '    if (token_ref->owned) grab_kernel_object( token_ref );', 'server'),
    ('missing-local-reference', 'dlls/ntoskrnl.exe/ntoskrnl.c', '                                ++header->ref;', '                                /* missing local reference */', 'client'),
    ('missing-process-close', 'dlls/ntoskrnl.exe/ntoskrnl.c', 'state->close_status = NtClose( handle );', 'state->close_status = handle ? 0 : NtClose( handle );', 'client'),
    ('missing-unwind-release', 'dlls/ntoskrnl.exe/ntoskrnl.c', 'if (!normal && state->result)', 'if (0 && state->result)', 'client'),
]
results = []
for label, file, before, after, kind in mutations:
    with tempfile.TemporaryDirectory(prefix='cider-owned-ref-negative-') as tmp:
        root = Path(tmp)
        for name in ['server/device.c', 'server/process.c', 'dlls/ntoskrnl.exe/ntoskrnl.c']:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            text = (a.source / name).read_text()
            if name == file:
                if text.count(before) != 1:
                    raise RuntimeError('Mutation anchor is not unique: ' + label)
                text = text.replace(before, after)
            path.write_text(text)
        source = root if kind == 'server' else root / 'dlls/ntoskrnl.exe/ntoskrnl.c'
        run = subprocess.run([sys.executable, str(tests / ('check-' + kind + '-reference.py')),
            '--source', str(source), '--report', str(root / 'report.json')], capture_output=True, text=True, timeout=30)
        # A compile failure is not evidence that assertions detect bad behavior.
        compiled = 'CalledProcessError' in run.stderr and ('SIGABRT' in run.stderr or 'SIGILL' in run.stderr)
        if run.returncode == 0 or not compiled:
            raise RuntimeError(label + ' was not rejected by the compiled model:\n' + run.stderr)
        results.append({'mutation': label, 'compiledModelRejected': True, 'exitCode': run.returncode,
                        'diagnostic': run.stderr.splitlines()[-1]})
a.report.write_text(json.dumps({'scope': 'six deliberate source mutations; modeled dependencies only', 'results': results}, indent=2) + '\n')
print('PASS: all six deliberate ownership regressions rejected by compiled models')
