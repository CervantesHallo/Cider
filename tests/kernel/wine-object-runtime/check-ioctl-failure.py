#!/usr/bin/env python3
"""Exact dispatch_ioctl ownership paths with controlled IRP allocation failure."""
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
#include <stdlib.h>
#include <string.h>
typedef int32_t NTSTATUS;
typedef uint32_t ULONG;
typedef struct DEVICE_OBJECT { int StackSize; } DEVICE_OBJECT;
typedef struct FILE_OBJECT { DEVICE_OBJECT *DeviceObject; } FILE_OBJECT;
typedef struct IO_STACK_LOCATION { FILE_OBJECT *FileObject; } IO_STACK_LOCATION;
typedef struct IRP {
    struct { struct { FILE_OBJECT *OriginalFileObject; } Overlay; } Tail;
    int RequestorMode;
    struct { void *SystemBuffer; } AssociatedIrp;
    void *UserBuffer;
    unsigned int Flags;
} IRP;
struct dispatch_context {
    struct { struct { void *file; ULONG out_size,code; } ioctl; } params;
    ULONG in_size;
    void *in_buff;
};
#define TRACE(...) ((void)0)
#define FALSE 0
#define UserMode 1
#define METHOD_BUFFERED 0
#define HEAP_REALLOC_IN_PLACE_ONLY 1
#define IRP_DEALLOCATE_BUFFER 4
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xc0000010)
#define STATUS_NO_MEMORY ((NTSTATUS)0xc0000017)
static IRP irp;
static IO_STACK_LOCATION stack;
static int fail_irp, balance, forwarded;
static void *wine_server_get_ptr(void *p) { return p; }
static DEVICE_OBJECT *IoGetAttachedDevice(DEVICE_OBJECT *p) { return p; }
static void *GetProcessHeap(void) { return NULL; }
static void *HeapAlloc(void *heap,int flags,ULONG size) { (void)heap; (void)flags; ++balance; return malloc(size ? size : 1); }
static int HeapFree(void *heap,int flags,void *p) { (void)heap; (void)flags; if(p) { --balance; free(p); } return 1; }
static void *HeapReAlloc(void *heap,int flags,void *p,ULONG size) {
    (void)heap; (void)flags; (void)size; return p; /* in-place shrink only */
}
static IRP *IoBuildDeviceIoControlRequest(ULONG code,DEVICE_OBJECT *device,void *input,ULONG input_size,
                                         void *output,ULONG output_size,int internal,void *event,void *iosb) {
    (void)code; (void)device; (void)input; (void)input_size; (void)output_size;
    (void)internal; (void)event; (void)iosb;
    irp.UserBuffer = output;
    return fail_irp ? NULL : &irp;
}
static IO_STACK_LOCATION *IoGetNextIrpStackLocation(IRP *p) { (void)p; return &stack; }
static NTSTATUS dispatch_irp(DEVICE_OBJECT *d, IRP *p, struct dispatch_context *c) {
    assert(d && p == &irp && c); ++forwarded; return 0;
}
'''
MAIN = r'''
int main(void) {
    DEVICE_OBJECT device = {1}; FILE_OBJECT file = {&device};
    unsigned int mode, size, pass;
    for (pass = 0; pass < 2; ++pass) for (mode = 0; mode < 2; ++mode) for (size = 0; size < 3; ++size) {
        struct dispatch_context context;
        NTSTATUS status;
        memset(&context,0,sizeof(context)); memset(&irp,0,sizeof(irp));
        context.params.ioctl.file = &file; context.params.ioctl.code = mode;
        context.params.ioctl.out_size = size == 0 ? 0 : size == 1 ? 8 : (mode == 0 ? 32 : 16);
        context.in_size = 16; context.in_buff = HeapAlloc(NULL,0,16); memset(context.in_buff,0x37,16);
        fail_irp = !pass;
        status = dispatch_ioctl(&context);
        assert(status == (fail_irp ? STATUS_NO_MEMORY : 0));
        if (fail_irp) {
            assert(context.in_buff != NULL); /* dispatch loop still owns it */
            HeapFree(NULL,0,context.in_buff);
        } else {
            assert(context.in_buff == NULL && stack.FileObject == &file);
            if (irp.UserBuffer != irp.AssociatedIrp.SystemBuffer) HeapFree(NULL,0,irp.UserBuffer);
            HeapFree(NULL,0,irp.AssociatedIrp.SystemBuffer);
        }
        assert(balance == 0);
    }
    assert(forwarded == 6);
    puts("PASS: 12 exact IOCTL paths, all buffers freed once; alias/expanded/separate/empty outputs");
}
'''

def extract(path):
    match = re.search(r'^static NTSTATUS dispatch_ioctl\([^;]*?\n\{\n.*?^\}', path.read_text(), re.M | re.S)
    if not match:
        raise RuntimeError('exact dispatch_ioctl definition missing')
    return match.group()

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--old-source', type=Path, required=True)
parser.add_argument('--report', type=Path, required=True)
args = parser.parse_args()
report = {'scope':'exact C body, host allocation/IRP mocks; ASan/UBSan, not native Windows', 'cases':[]}
with tempfile.TemporaryDirectory(prefix='cider-ioctl-buffers-') as temp:
    temp = Path(temp)
    for label, path in [('repaired',args.source),('old',args.old_source)]:
        body = extract(path); source = temp/(label+'.c'); binary = temp/label
        source.write_text(PRELUDE+body+MAIN)
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        '-fno-omit-frame-pointer',str(source),'-o',str(binary)],check=True)
        result = subprocess.run([str(binary)],text=True,capture_output=True,timeout=10)
        valid = result.returncode == 0 if label == 'repaired' else result.returncode != 0 and 'double-free' in result.stderr
        if not valid:
            raise RuntimeError(label+': '+result.stdout+result.stderr)
        report['cases'].append({'source':label,'extractedSHA256':hashlib.sha256(body.encode()).hexdigest(),
            'exitCode':result.returncode,'stdout':result.stdout.strip(),'oldASanDoubleFreeReproduced':label=='old'})
args.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
