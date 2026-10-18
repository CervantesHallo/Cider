#!/usr/bin/env python3
"""Compile the self-owned Wine fixture using the existing configured Wine tree."""
import argparse
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
for name in ('source', 'build', 'out'):
    setattr(a, name, getattr(a, name).resolve())
a.out.mkdir(parents=True, exist_ok=True)
fixture = Path(__file__).resolve().parent
tool = Path.home() / 'Library/Caches/Cider/toolchain/mingw'
for arch, folder, entry in [('x86_64', 'x86_64', 'DriverEntry'), ('i686', 'i386', '_DriverEntry@8')]:
    cc = tool / f'toolchain-{arch}/bin/{arch}-w64-mingw32-gcc'
    common = [str(cc), '-Wall', '-Wextra', '-Werror', '-O2', '-Wl,--no-insert-timestamp']
    subprocess.run(common + ['-D__WINESRC__', '-fno-stack-protector', '-isystem', str(a.source / 'include'),
        '-isystem', str(a.build / 'include'), '-shared', '-nostdlib', f'-Wl,--entry,{entry}', '-Wl,--subsystem,native',
        str(fixture / 'driver.c'), str(a.build / f'dlls/winecrt0/{folder}-windows/libwinecrt0.a'),
        str(a.build / f'dlls/ntoskrnl.exe/{folder}-windows/libntoskrnl.a'),
        '-o', str(a.out / f'cider-owned-token-{arch}.sys')], check=True)
    subprocess.run(common + ['-std=c11', '-municode', '-static-libgcc', str(fixture / 'client.c'), '-ladvapi32',
        '-o', str(a.out / f'cider-owned-token-{arch}.exe')], check=True)
    subprocess.run(common + ['-std=c11', '-municode', '-static-libgcc', str(fixture / 'policy-child.c'),
        '-o', str(a.out / f'cider-owned-policy-{arch}.exe')], check=True)
print('Both architectures built with warnings as errors')
