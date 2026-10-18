#!/bin/bash
# Add GCC's PE C++ frontend in an independent prefix for Engine R v1.
# Uses the pinned GCC15.2 archive and mingw CRT from toolchain.sh.
# Does not replace the proven C toolchains or install a new target C++ runtime;
# Wine builds its own libc++/C++ABI. Output is adopted only after configure.
# Usage: engine/pe-cxx-toolchain.sh [i686|x86_64 ...]
set -euo pipefail

TASK_TC=${CIDER_TOOLCHAIN:-$HOME/Library/Caches/Cider/toolchain}
TASK_STAGE="$TASK_TC/mingw-cxx17"
TASK_WORK="$TASK_TC/build/mingw-cxx17"
TASK_HOST="$TASK_TC/host"
TASK_SOURCE="$TASK_WORK/source-gcc-15.2.0"
TASK_ARCHIVE="$TASK_TC/src/gcc-15.2.0.tar.xz"
TASK_GCC_SHA=438fd996826b0c82485a29da03a72d71d6e3541a83ec702df4271f6fe025d24e
TASK_JOBS=${CIDER_CXX_JOBS:-2}
[[ "$TASK_JOBS" =~ ^[1-4]$ ]] || { echo 'CIDER_CXX_JOBS must be 1..4' >&2; exit 2; }
[ -f "$TASK_ARCHIVE" ] || { echo 'Build the existing pinned C toolchain first' >&2; exit 1; }
echo "$TASK_GCC_SHA  $TASK_ARCHIVE" | shasum -a 256 -c --quiet -
[ ! -L "$TASK_STAGE" ] && [ ! -L "$TASK_WORK" ] || { echo 'Refusing linked staging roots' >&2; exit 1; }
mkdir -p "$TASK_STAGE" "$TASK_WORK"
TASK_LOCK="$TASK_STAGE/.build-lock"
mkdir "$TASK_LOCK" 2>/dev/null || { echo 'Another C++ build or interrupted lock requires inspection' >&2; exit 1; }
trap 'rmdir "$TASK_LOCK"' EXIT

export SDKROOT=${CIDER_SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk}
python3 - "$SDKROOT/SDKSettings.json" <<'PY'
import json,pathlib,sys
sdk=json.loads(pathlib.Path(sys.argv[1]).read_text())
if not sdk['Version'].startswith('15.'):
    raise SystemExit('A macOS15 SDK is required')
PY
export MACOSX_DEPLOYMENT_TARGET=14.0
export MAKEINFO=true
unset CC CXX CFLAGS CXXFLAGS LDFLAGS CPPFLAGS PKG_CONFIG_PATH MAKEFLAGS GCC_EXEC_PREFIX COMPILER_PATH LIBRARY_PATH CPATH CPLUS_INCLUDE_PATH C_INCLUDE_PATH DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH

# Start with the archive's actual source, not an unbound source/build cache.
# One source tree serves both independent out-of-tree builds. Compare every
# archive file on reuse; a stale stamp alone cannot establish source identity.
if [ ! -e "$TASK_SOURCE" ]; then
    mkdir "$TASK_SOURCE"
    tar -xf "$TASK_ARCHIVE" -C "$TASK_SOURCE" --strip-components 1
fi
[ ! -L "$TASK_SOURCE" ] || { echo 'Refusing linked GCC source' >&2; exit 1; }
python3 - "$TASK_ARCHIVE" "$TASK_SOURCE" <<'PY'
import hashlib,pathlib,sys,tarfile,unicodedata
root=pathlib.Path(sys.argv[2]);count=0;expected=set()
def digest(f):
    result=hashlib.sha256()
    for block in iter(lambda:f.read(1024*1024),b''): result.update(block)
    return result.digest()
with tarfile.open(sys.argv[1],'r|xz') as archive:
    for member in archive:
        parts=pathlib.PurePosixPath(member.name).parts
        if not parts or parts[0]!='gcc-15.2.0' or '..' in parts:
            raise SystemExit('Unexpected GCC archive path')
        path=root.joinpath(*parts[1:])
        if len(parts)>1:
            relative=pathlib.PurePosixPath(*parts[1:])
            expected.add(str(relative))
            expected.update(str(parent) for parent in relative.parents if str(parent)!='.')
        if member.isfile():
            if path.is_symlink() or not path.is_file(): raise SystemExit('GCC source file missing or linked')
            with path.open('rb') as actual:
                if digest(archive.extractfile(member))!=digest(actual): raise SystemExit('GCC source differs: '+str(path))
            count+=1
        elif member.issym():
            if not path.is_symlink() or str(path.readlink())!=member.linkname:
                raise SystemExit('GCC source link differs')
        elif member.isdir():
            if path.is_symlink() or not path.is_dir(): raise SystemExit('GCC source directory differs')
        else: raise SystemExit('Unexpected GCC archive member')
actual={str(path.relative_to(root)) for path in root.rglob('*')}
# APFS may return decomposed names for NFC archive paths. Reject real extras
# and normalization collisions, while accepting the same filesystem name.
normalized=lambda paths:{unicodedata.normalize('NFC',path) for path in paths}
actual_names=normalized(actual);expected_names=normalized(expected)
if len(actual_names)!=len(actual) or len(expected_names)!=len(expected):
    raise SystemExit('Ambiguous Unicode paths in GCC source')
if actual_names!=expected_names:
    raise SystemExit('Unexpected GCC source paths: '+repr(sorted(actual^expected)[:10]))
print('Pinned GCC source files verified:',count)
PY

TASK_ARCHS=("$@")
[ "${#TASK_ARCHS[@]}" -gt 0 ] || TASK_ARCHS=(i686 x86_64)
for TASK_ARCH in "${TASK_ARCHS[@]}"; do
    [[ "$TASK_ARCH" = i686 || "$TASK_ARCH" = x86_64 ]] || { echo 'Expected i686 or x86_64' >&2; exit 2; }
    TASK_TARGET="$TASK_ARCH-w64-mingw32"
    TASK_ORIGINAL="$TASK_TC/mingw/toolchain-$TASK_ARCH"
    TASK_PREFIX="$TASK_STAGE/toolchain-$TASK_ARCH"
    TASK_BUILD="$TASK_WORK/$TASK_ARCH/pinned-build"
    TASK_MARKER="$TASK_STAGE/$TASK_ARCH.json"
    [ -x "$TASK_ORIGINAL/bin/$TASK_TARGET-gcc" ] || {
        echo "Existing C toolchain missing: $TASK_ARCH" >&2; exit 1;
    }
    [ ! -L "$TASK_PREFIX" ] && [ ! -L "$TASK_BUILD" ] || { echo 'Refusing linked build destinations' >&2; exit 1; }
    TASK_OLD_HASH=$(shasum -a 256 "$TASK_ORIGINAL/bin/$TASK_TARGET-gcc" | cut -d ' ' -f1)
    # Only the target CRT/libgcc/binutils are cloned; native build caches are not.
    [ -e "$TASK_PREFIX" ] || /bin/cp -cR "$TASK_ORIGINAL" "$TASK_PREFIX"
    mkdir -p "$TASK_BUILD"
    python3 - "$TASK_BUILD" "$TASK_PREFIX" "$TASK_TARGET" "$TASK_GCC_SHA" "$TASK_ORIGINAL" "$SDKROOT" "$TASK_HOST" <<'PY'
import hashlib,json,pathlib,subprocess,sys
build,prefix,target,source,original,sdk,host=sys.argv[1:]
def sha(path): return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
tools={name:sha(pathlib.Path(prefix)/'bin'/(target+'-'+name)) for name in ('as','ld')}
if tools!={name:sha(pathlib.Path(original)/'bin'/(target+'-'+name)) for name in ('as','ld')}:
    raise SystemExit('Independent binutils differ from the proven C toolchain')
data={'schema':'cider.pe-cxx-build-inputs/v2','target':target,'prefix':prefix,'sysroot':prefix,
      'gccSourceArchiveSHA256':source,'sdk':sdk,'sdkSettingsSHA256':sha(pathlib.Path(sdk)/'SDKSettings.json'),
      'deploymentTarget':'14.0','nativeCXX':subprocess.check_output(['/usr/bin/clang++','--version'],text=True),
      'languages':'c,c++','targetCppRuntimeInstalled':False,
      'originalCDriverSHA256':sha(pathlib.Path(original)/'bin'/(target+'-gcc')),
      'binutilsSHA256':tools,
      'hostDependenciesSHA256':{name:sha(pathlib.Path(host)/'lib'/('lib'+name+'.a')) for name in ('gmp','mpfr','mpc')}}
path=pathlib.Path(build)/'build-inputs.json'
if path.exists():
    if json.loads(path.read_text())!=data: raise SystemExit('Build inputs changed; use a fresh independent C++ build directory')
else: path.write_text(json.dumps(data,indent=2)+'\n')
PY
    if [ -f "$TASK_MARKER" ]; then
        python3 - "$TASK_MARKER" "$TASK_PREFIX" "$TASK_TARGET" "$TASK_BUILD" <<'PY'
import hashlib,json,os,pathlib,subprocess,sys
data=json.loads(pathlib.Path(sys.argv[1]).read_text())
prefix=pathlib.Path(sys.argv[2]);target=sys.argv[3];build=pathlib.Path(sys.argv[4])
if data['schema']!='cider.pe-cxx-frontend/v2' or data['inputs']!=json.loads((build/'build-inputs.json').read_text()):
    raise SystemExit('C++ completion marker inputs differ')
driver=prefix/'bin'/(target+'-g++')
if subprocess.check_output([str(driver),'-dumpmachine'],text=True).strip()!=target: raise SystemExit('C++ target differs')
backend=pathlib.Path(subprocess.check_output([str(driver),'-print-prog-name=cc1plus'],text=True).strip()).resolve()
if not backend.is_relative_to(prefix.resolve()) or str(backend)!=data['cc1plusPath']: raise SystemExit('C++ backend path differs')
for path,expected in [(driver,data['cxxDriverSHA256']),(backend,data['cc1plusSHA256'])]:
    if not os.access(path,os.X_OK) or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
        raise SystemExit('C++ executable missing or changed')
obj=build/'cxx17-capability.o'
if hashlib.sha256(obj.read_bytes()).hexdigest()!=data['capabilityObjectSHA256']: raise SystemExit('C++ capability object changed')
PY
        echo "C++ frontend already built: $TASK_TARGET"
        continue
    fi
    # Compiler source and native objects are independent of the C-only cache.
    export PATH="$TASK_PREFIX/bin:$TASK_HOST/bin:/usr/bin:/bin:/usr/sbin:/sbin"
    (
        cd "$TASK_BUILD"
        "$TASK_SOURCE/configure" --target="$TASK_TARGET" --prefix="$TASK_PREFIX" --with-sysroot="$TASK_PREFIX" \
            --enable-languages=c,c++ --disable-multilib --disable-nls --enable-threads=win32 \
            --with-gmp="$TASK_HOST" --with-mpfr="$TASK_HOST" --with-mpc="$TASK_HOST" \
            --without-isl --without-zstd --with-system-zlib \
            --with-ld="$TASK_PREFIX/bin/$TASK_TARGET-ld" --with-as="$TASK_PREFIX/bin/$TASK_TARGET-as" \
            --disable-libssp --disable-libquadmath --disable-libgomp --disable-libatomic \
            --disable-shared --with-pkgversion='Cider PE C++ frontend' \
            CC=/usr/bin/clang CXX=/usr/bin/clang++ > cxx-configure.log 2>&1
        make -j"$TASK_JOBS" all-gcc > cxx-build.log 2>&1
        # Do not install a configure cache which still resolves old binutils.
        python3 - gcc/config.cache "$TASK_PREFIX" "$TASK_TARGET" <<'PY'
import pathlib,sys
cache=pathlib.Path(sys.argv[1]).read_text()
for tool in ('as','ld'):
    expected='gcc_cv_'+tool+'=${gcc_cv_'+tool+'='+sys.argv[2]+'/bin/'+sys.argv[3]+'-'+tool+'}'
    if expected not in cache: raise SystemExit('Unexpected configured '+tool+' path; not installing')
PY
        make install-gcc > cxx-install.log 2>&1
    )
    # Compiler capability check only: compile an object, never execute a PE.
    printf '%s\n' 'template<class... T> constexpr auto total(T... v) { return (v + ...); }' \
        'static_assert(total(1,2,3)==6); int value() { if constexpr(sizeof(void*)==8) return 64; else return 32; }' \
        | "$TASK_PREFIX/bin/$TASK_TARGET-g++" -x c++ -std=gnu++17 -c -o "$TASK_BUILD/cxx17-capability.o" -
    [ "$TASK_OLD_HASH" = "$(shasum -a 256 "$TASK_ORIGINAL/bin/$TASK_TARGET-gcc" | cut -d ' ' -f1)" ] || {
        echo 'Original C compiler changed unexpectedly' >&2; exit 1;
    }
    python3 - "$TASK_MARKER" "$TASK_PREFIX" "$TASK_TARGET" "$TASK_BUILD" <<'PY'
import hashlib,json,pathlib,struct,subprocess,sys
marker,prefix,target,build=sys.argv[1:];prefix=pathlib.Path(prefix);build=pathlib.Path(build)
driver=prefix/'bin'/(target+'-g++')
backend=pathlib.Path(subprocess.check_output([str(driver),'-print-prog-name=cc1plus'],text=True).strip()).resolve()
if not backend.is_relative_to(prefix.resolve()): raise SystemExit('C++ backend outside independent prefix')
if subprocess.check_output([str(driver),'-dumpmachine'],text=True).strip()!=target: raise SystemExit('C++ target differs')
obj=(build/'cxx17-capability.o').read_bytes();machine=struct.unpack_from('<H',obj)[0]
if machine!=({'i686-w64-mingw32':0x14c,'x86_64-w64-mingw32':0x8664}[target]): raise SystemExit('Unexpected COFF machine')
data={'schema':'cider.pe-cxx-frontend/v2','inputs':json.loads((build/'build-inputs.json').read_text()),
      'cxxDriverSHA256':hashlib.sha256(driver.read_bytes()).hexdigest(),
      'cc1plusPath':str(backend),'cc1plusSHA256':hashlib.sha256(backend.read_bytes()).hexdigest(),
      'capabilityObjectSHA256':hashlib.sha256(obj).hexdigest(),'coffMachine':machine,
      'originalCDriverUnchanged':True,'targetCppRuntimeInstalled':False,'scope':'Wine PE C++ frontend; C++17 object compiled, no PE execution'}
path=pathlib.Path(marker);stage=path.with_suffix('.tmp');stage.write_text(json.dumps(data,indent=2)+'\n');stage.replace(path)
PY
    echo "Built C++ frontend: $TASK_TARGET"
done
echo "CIDER_PE_CXX_TOOLCHAIN=$TASK_STAGE"
