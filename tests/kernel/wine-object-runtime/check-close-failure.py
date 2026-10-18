#!/usr/bin/env python3
"""Exercise the exact dispatch_close body; only IRP allocation/dispatch are mocked."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

PRELUDE = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int32_t NTSTATUS;
typedef void *PETHREAD;
typedef struct DEVICE_OBJECT { int StackSize; } DEVICE_OBJECT;
typedef struct FILE_OBJECT { DEVICE_OBJECT *DeviceObject; int local_refs; } FILE_OBJECT;
typedef struct IO_STACK_LOCATION { int MajorFunction; FILE_OBJECT *FileObject; } IO_STACK_LOCATION;
typedef struct IRP {
    struct { struct { FILE_OBJECT *OriginalFileObject; void *Thread; } Overlay; } Tail;
    int RequestorMode;
    struct { void *SystemBuffer; } AssociatedIrp;
    void *UserBuffer, *UserIosb, *UserEvent;
    unsigned int Flags;
} IRP;
struct dispatch_context { struct { struct { void *file; } close; } params; };
#define TRACE(...) ((void)0)
#define FALSE 0
#define UserMode 1
#define IRP_MJ_CLOSE 2
#define IRP_CLOSE_OPERATION 4
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#define STATUS_NO_MEMORY ((NTSTATUS)0xc0000017)
static IRP irp;
static IO_STACK_LOCATION stack;
static int fail_allocation, forwarded;
static void *wine_server_get_ptr(void *p) { return p; }
static DEVICE_OBJECT *IoGetAttachedDevice(DEVICE_OBJECT *p) { return p; }
static IRP *IoAllocateIrp(int size,int quota) { (void)size; (void)quota; return fail_allocation ? NULL : &irp; }
static IO_STACK_LOCATION *IoGetNextIrpStackLocation(IRP *p) { (void)p; return &stack; }
static void *KeGetCurrentThread(void) { return (void *)(uintptr_t)0x99; }
static void ObDereferenceObject(FILE_OBJECT *p) { assert(p->local_refs > 0); --p->local_refs; }
static NTSTATUS dispatch_irp(DEVICE_OBJECT *d, IRP *p, struct dispatch_context *c) {
    assert(d && p == &irp && c); ++forwarded; return 0;
}
'''
MAIN = r'''
int main(void) {
    DEVICE_OBJECT device = {1};
    FILE_OBJECT file = {&device,0};
    struct dispatch_context context = {{{&file}}};
    fail_allocation = 1;
    assert(dispatch_close(&context) == STATUS_NO_MEMORY);
    assert(file.local_refs == 0 && forwarded == 0);
    fail_allocation = 0;
    assert(dispatch_close(&context) == 0);
    assert(file.local_refs == 0 && forwarded == 1 && stack.FileObject == &file);
    assert(irp.Tail.Overlay.OriginalFileObject == &file && irp.RequestorMode == UserMode);
    context.params.close.file = NULL;
    assert(dispatch_close(&context) == STATUS_INVALID_HANDLE && forwarded == 1);
    puts("PASS: allocation failure, normal dispatch, null file; borrowed local ref remains zero");
}
'''

def extract(path):
    text = path.read_text()
    match = re.search(r'^static NTSTATUS dispatch_close\([^;]*?\n\{\n.*?^\}', text, re.M | re.S)
    if not match:
        raise RuntimeError('exact dispatch_close definition missing')
    return match.group()

args = argparse.ArgumentParser(description=__doc__)
args.add_argument('--source', type=Path, required=True)
args.add_argument('--old-source', type=Path, required=True)
args.add_argument('--report', type=Path, required=True)
a = args.parse_args()
report = {'scope': 'exact C body, mocked allocation/dispatch, ASan/UBSan; not native Windows', 'cases': []}
with tempfile.TemporaryDirectory(prefix='cider-close-ownership-') as temp:
    temp = Path(temp)
    for label, path in [('repaired', a.source), ('old', a.old_source)]:
        body = extract(path)
        source = temp / (label + '.c')
        source.write_text(PRELUDE + body + MAIN)
        binary = temp / label
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(source), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], text=True, capture_output=True, timeout=10)
        valid = result.returncode == 0 if label == 'repaired' else result.returncode != 0 and 'local_refs > 0' in result.stderr
        if not valid:
            raise RuntimeError(label + ': ' + result.stdout + result.stderr)
        report['cases'].append({'source': label, 'extractedSHA256': hashlib.sha256(body.encode()).hexdigest(),
                                'exitCode': result.returncode, 'stdout': result.stdout.strip(),
                                'oldUnmatchedDereferenceReproduced': label == 'old'})
a.report.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
