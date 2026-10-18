#!/usr/bin/env python3
"""Exact release wrapper delegation; dependency behavior is a host model."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--report', type=Path, required=True)
args = parser.parse_args()
source = args.source.read_text()
match = re.search(r'^void WINAPI PsDereferencePrimaryToken\([^;]*?\n\{\n.*?^\}', source, re.M | re.S)
if not match:
    raise RuntimeError('exact release definition missing')
body = match.group()
prelude = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
typedef void *PACCESS_TOKEN;
#define WINAPI
#define TRACE(...) ((void)0)
static void *expected;
static int references, calls, raise_error;
static jmp_buf frame;
static void ObDereferenceObject(void *object) {
    assert(object == expected); ++calls;
    if (raise_error) longjmp(frame,42);
    assert(references > 0); --references;
}
'''
main = r'''
int main(void) {
    int token;
    expected = &token;
    references = 2; calls = 0; PsDereferencePrimaryToken(&token);
    assert(calls == 1 && references == 1);
    calls = 0; PsDereferencePrimaryToken(&token);
    assert(calls == 1 && references == 0);
    calls = 0; references = 1; raise_error = 1;
    if (!setjmp(frame)) { PsDereferencePrimaryToken(&token); assert(0); }
    assert(calls == 1 && references == 1);
    puts("PASS: exact pointer and one release, final release, underlying failure propagation");
}
'''
with tempfile.TemporaryDirectory(prefix='cider-token-release-wrapper-') as tmp:
    tmp = Path(tmp); file = tmp/'check.c'; binary = tmp/'check'
    file.write_text(prelude+body+main)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(file),'-o',str(binary)],check=True)
    result = subprocess.run([str(binary)],capture_output=True,text=True,timeout=10,check=True)
report = {'scope':'exact wrapper with modeled generic dereference; not actual token backend or native Windows',
          'extractedSHA256':hashlib.sha256(body.encode()).hexdigest(),'checks':3,'exitCode':result.returncode,
          'output':result.stdout.strip(),'primaryLookupImplemented':False}
args.report.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
