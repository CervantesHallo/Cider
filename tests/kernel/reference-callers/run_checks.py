#!/usr/bin/env python3
"""Apply K3 in a private temporary tree; compile/run exact function bodies with mocks."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PATCH = ROOT / 'engine/patches/candidates/0012-reference-callers.patch'
FILES = ('ntoskrnl.c', 'ntoskrnl_private.h', 'sync.c', 'pnp.c')
FUNCTIONS = {
    'ntoskrnl.c': (
        'close_constructor_handle', 'create_file_object', 'create_process_object', 'create_thread_object',
        'ObReferenceObjectByHandle', 'PsLookupProcessByProcessId', 'PsLookupThreadByThreadId',
        'KeGetCurrentThread', 'ObReferenceObjectByName', 'ObOpenObjectByName',
        'dereference_work_item_device', 'run_work_item_worker', 'IoQueueWorkItem',
        'ObReferenceObject', 'ObfReferenceObject', 'IoGetAttachedDeviceReference',
    ),
    'sync.c': ('create_event_object', 'IoCreateSynchronizationEvent', 'IoCreateNotificationEvent'),
    'pnp.c': ('dereference_function_driver', 'load_function_driver'),
}


def body(source, name):
    # These definitions have unindented closing braces; declarations are skipped.
    match = re.search(r'^[\w *]+\b' + re.escape(name) + r'\([^;]*?\n\{\n.*?^\}',
                      source, re.M | re.S)
    if not match:
        raise RuntimeError(f'Cannot extract exact definition: {name}')
    return match.group(0)


def run(command, **kwargs):
    result = subprocess.run(command, text=True, capture_output=True, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{command[0]} exit {result.returncode}:\n{result.stdout}{result.stderr}')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='Read-only Wine source with 0009/0010 applied')
    parser.add_argument('--already-applied', action='store_true', help='Validate exact combined source; do not apply 0012 again')
    parser.add_argument('--build', type=Path, help='Read-only configured Wine build; also compile i686/x64 PE objects')
    parser.add_argument('--x64-cc', default='x86_64-w64-mingw32-gcc')
    parser.add_argument('--i686-cc', default='i686-w64-mingw32-gcc')
    parser.add_argument('--report', type=Path, help='Write machine-readable evidence here')
    args = parser.parse_args()
    report = {'patch_sha256': hashlib.sha256(PATCH.read_bytes()).hexdigest(),
              'source_sha256': {}, 'host_only_execution': True, 'native_parity': 'pending'}
    with tempfile.TemporaryDirectory(prefix='cider-k3-checks-') as tmp:
        tmp = Path(tmp)
        target = tmp / 'dlls/ntoskrnl.exe'
        target.mkdir(parents=True)
        for name in FILES:
            original = args.source / 'dlls/ntoskrnl.exe' / name
            report['source_sha256'][name] = hashlib.sha256(original.read_bytes()).hexdigest()
            shutil.copyfile(original, target / name)
        if not args.already_applied:
            applied = run(['patch', '-p1', '--fuzz=0', '--batch', '-i', str(PATCH)], cwd=tmp)
            if 'fuzz' in applied.stdout or 'offset' in applied.stdout:
                raise RuntimeError('Patch must apply without fuzz or offsets:\n' + applied.stdout)
        assert 'status = type->constructor( handle, &obj );' in (target / 'ntoskrnl.c').read_text()
        assert 'NTSTATUS (*constructor)(HANDLE, void **)' in (target / 'ntoskrnl_private.h').read_text()
        extracted = '\n\n'.join(body((target / name).read_text(), fn)
                                 for name, names in FUNCTIONS.items() for fn in names)
        if args.already_applied:
            extracted = body((target / 'ntoskrnl.c').read_text(), 'close_reference_handle') + '\n\n' + extracted
        (tmp / 'extracted.h').write_text(extracted + '\n')
        report['extracted_sha256'] = hashlib.sha256(extracted.encode()).hexdigest()
        report['functions_exercised'] = [fn for names in FUNCTIONS.values() for fn in names]
        command = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                   '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g',
                   '-I', str(tmp), str(HERE / 'mock.c'), '-o', str(tmp / 'checks')]
        if args.already_applied:
            command.insert(1, '-DCOMBINED_LIFETIME_FIX')
        run(command)
        result = run([str(tmp / 'checks')])
        report['mock_checks'] = result.stdout.strip()
        print(result.stdout, end='')
        mutations = {
            'premature named output': (
                'status = reference_kernel_object_checked( &driver->driver_obj );\n'
                '        if (!status) *Object = &driver->driver_obj;',
                '*Object = &driver->driver_obj;\n'
                '        status = reference_kernel_object_checked( &driver->driver_obj );'),
            'missing submission rollback': (
                'status = RtlGetLastNtStatus();\n        ObDereferenceObject( device );',
                'status = RtlGetLastNtStatus();'),
            'erased constructor status': (
                'if ((status = NtQueryInformationProcess( handle, ProcessBasicInformation, &info, sizeof(info), NULL )))\n'
                '        return status;',
                'if ((status = NtQueryInformationProcess( handle, ProcessBasicInformation, &info, sizeof(info), NULL )))\n'
                '        return STATUS_NO_MEMORY;'),
            'missing temporary handle close': (
                'NtClose( *(HANDLE *)handle );', '(void)handle;'),
            'missing AddDevice unwind cleanup': (
                'ObDereferenceObject( driver );', '(void)driver;'),
        }
        mutations['missing nested-constructor unwind cleanup'] = (
            'static void CALLBACK close_constructor_handle( BOOL normal, void *handle )\n{\n    NtClose( *(HANDLE *)handle );\n}',
            'static void CALLBACK close_constructor_handle( BOOL normal, void *handle )\n{\n    (void)handle;\n}')
        for label, (before, after) in mutations.items():
            assert extracted.count(before) >= 1, label
            (tmp / 'extracted.h').write_text(extracted.replace(before, after, 1))
            run(command)
            result = subprocess.run([str(tmp / 'checks')], text=True, capture_output=True)
            if result.returncode == 0 or 'Assertion failed' not in result.stderr:
                raise RuntimeError('Regression mutation was not rejected by an assertion: ' + label)
        report['regression_mutations_rejected'] = list(mutations)
        print(f'Regression sensitivity: {len(mutations)} injected regressions rejected')
        (tmp / 'extracted.h').write_text(extracted + '\n')
        if args.build:
            build = args.build.resolve()
            source = args.source.resolve()
            objects = []
            for arch, compiler in (('x86_64', args.x64_cc), ('i386', args.i686_cc)):
                for name in ('ntoskrnl.c', 'sync.c', 'pnp.c'):
                    output = tmp / (arch + '-' + name + '.o')
                    command = [compiler, '-c', '-o', str(output), str(target / name),
                               '-I' + str(target), '-I' + str(build / 'dlls/ntoskrnl.exe'),
                               '-I' + str(source / 'dlls/ntoskrnl.exe'), '-I' + str(build / 'include'),
                               '-I' + str(source / 'include'), '-I' + str(source / 'include/msvcrt'),
                               '-D_MSVCR_VER=0', '-D__WINESRC__', '-D__WINE_PE_BUILD', '-Wall',
                               '-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types',
                               '-fno-strict-aliasing', '-Wdeclaration-after-statement', '-O2', '-g']
                    result = run(command)
                    objects.append({'arch': arch, 'source': name,
                                    'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
                                    'diagnostics': result.stderr})
            report['pe_compile_only'] = objects
            print('PE object compile: x86_64 + i386, ntoskrnl.c + sync.c + pnp.c: PASS')
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
