#!/usr/bin/env python3
"""Extract exact candidate0120 source for native helpers with explicit stubs.

Select --part frame --source <applied-Wine>/dlls/ntdll/unix/sync.c, or
--part event --source <applied-Wine>/dlls/ntdll/unix/msync.c. The existing
--output-dir receives the header and a source/hash record. No build or execution
is performed. Compile the corresponding msync-ready-{frame,event}.c with this
output directory and the exact applied Wine include directory on its search path.
Run helpers only in a fresh owned process session with an outer deadline.
These helpers do not establish Wine/API/signal-handler/Windows equivalence.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile


FUNCTIONS = (
    "inproc_frame_enter",
    "inproc_frame_enter_ready",
    "inproc_frame_mask",
    "inproc_prepare_kernel",
    "inproc_frame_promote",
    "inproc_frame_leave",
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def identity(stat):
    return (stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)


def stable_read(path):
    with path.open("rb") as stream:
        before = os.fstat(stream.fileno())
        data = stream.read()
        after = os.fstat(stream.fileno())
    if identity(before) != identity(after) or identity(after) != identity(path.stat()):
        raise RuntimeError(f"source changed during extraction: {path}; prepare again")
    return data


def atomic_write(path, data):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=f".{path.name}.", dir=path.parent,
                                         delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def extract_closed_functions(source, data, output, directory, part):
    """Extract exact, disjoint definitions; surrounding APIs are doubles."""
    blocks, records = [], []
    if part == "set-event":
        specifications = (("signal_all", rb"static inline int"),
                          ("coord_finish", rb"static void"),
                          ("msync_try_set_event_obj", rb"NTSTATUS"))
        stem, harness = "set-event-extracted", "msync-set-event.c"
    else:
        specifications = (("msync_wait_single", rb"static inline NTSTATUS"),
                          ("prepare_payload_park", rb"static int"),
                          ("verify_single_wait", rb"static void"),
                          ("single_wait_remove", rb"static void"))
        stem, harness = "late-extracted", "msync-late-park.c"
        contexts = list(re.finditer(
            rb"(?m)^struct msync_payload_park[ \t\r\n]*\{[^{}]*^\};[ \t]*\r?$", data))
        if len(contexts) != 1:
            raise RuntimeError("expected one exact payload-park context declaration")
        context = contexts[0]
        blocks.append(context.group(0) + b"\n"
                      b"static int prepare_payload_park(struct msync_payload_park *,int,void *);\n"
                      b"static void verify_single_wait(int,void *);\n")
        records.append({"declaration": "msync_payload_park",
                        "start_byte": context.start(), "end_byte_exclusive": context.end(),
                        "block_sha256": digest(context.group(0))})
    for name, declaration in specifications:
        # A later definition may have a forward declaration with the same name.
        pattern = (rb"(?m)^" + declaration + rb"[ \t]+" + name.encode()
                   + rb"[ \t]*\([^;{}]*\)[ \t\r\n]*\{")
        matches = list(re.finditer(pattern, data))
        if len(matches) != 1:
            raise RuntimeError(f"expected one definition of {name}")
        start = matches[0].start()
        # These selected Wine functions have their closing brace at column zero.
        end_match = re.search(rb"(?m)^\}[ \t]*\r?$", data[start:])
        if end_match is None:
            raise RuntimeError(f"missing function end for {name}")
        end = start + end_match.end()
        block = data[start:end]
        if not re.search(rb"\)[ \t\r\n]*\{", block):
            raise RuntimeError(f"selected declaration is not a definition: {name}")
        line = data[:start].count(b"\n") + 1
        blocks.append(f"#line {line} {json.dumps(str(source))}\n".encode("ascii") + block + b"\n")
        records.append({"function": name, "start_byte": start,
                        "end_byte_exclusive": end, "start_line": line,
                        "block_sha256": digest(block)})
    header = b"/* Exact production functions WITH EXPLICIT NATIVE DOUBLES. */\n" + b"\n".join(blocks)
    path = output / (stem + ".h")
    record = {"kind": "native-exact-" + part + "-with-stubs",
              "scope": "not Wine/API/signal-handler/Windows equivalence",
              "source": str(source), "source_sha256": digest(data),
              "functions": records, "header": str(path),
              "header_sha256": digest(header),
              "harness_sha256": digest(stable_read(directory / harness)),
              "extractor_sha256": digest(stable_read(Path(__file__).resolve()))}
    atomic_write(path, header)
    atomic_write(output / (stem + ".sha256.json"),
                 (json.dumps(record, indent=2, sort_keys=True) + "\n").encode())
    json.dump(record, sys.stdout, sort_keys=True)
    sys.stdout.write("\n")
    sys.stdout.flush()


def main():
    directory = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=directory,
                        help="existing directory for the header and source-hash record")
    parser.add_argument("--source", type=Path, required=True, help="explicit applied Wine source file")
    parser.add_argument("--part", choices=["frame", "event", "set-event", "late-park"], default="frame")
    args = parser.parse_args()
    output = args.output_dir.resolve(strict=True)
    if not output.is_dir():
        parser.error("--output-dir must be a directory")

    # Read the selected production source; do not rewrite a modeled substitute.
    source = args.source.resolve(strict=True)
    data = stable_read(source)
    if args.part in ("set-event", "late-park"):
        extract_closed_functions(source, data, output, directory, args.part)
        return
    if args.part == "frame":
        start_pattern = rb"(?m)^struct inproc_frame[ \t]*\r?\n\{"
        end_pattern = rb"(?m)^void inproc_thread_cleanup[ \t]*\([ \t]*void[ \t]*\)"
        functions, declaration = FUNCTIONS, rb"(?:static[ \t]+)?void"
    else:
        start_pattern = rb"(?m)^NTSTATUS msync_try_wait_ready_event[ \t]*\("
        end_pattern = rb"(?m)^NTSTATUS msync_wait_objs[ \t]*\("
        functions, declaration = ("msync_try_wait_ready_event",), rb"NTSTATUS"
    starts = list(re.finditer(start_pattern, data))
    if len(starts) != 1:
        raise RuntimeError("expected one source start boundary")
    start = starts[0].start()
    # A later non-Apple empty cleanup definition is outside the selected block.
    ending = re.search(end_pattern, data[start:])
    if ending is None:
        raise RuntimeError("missing source end boundary after selected start")
    end = start + ending.start()
    block = data[start:end]
    for name in functions:
        definitions = re.findall(rb"(?m)^" + declaration + rb"[ \t]+" + name.encode()
                                 + rb"[ \t]*\([^;{}]*\)[ \t\r\n]*\{", block)
        if len(definitions) != 1:
            raise RuntimeError(f"expected exactly one extracted definition of {name}")

    source_sha, block_sha = digest(data), digest(block)
    has_spin_budget = False
    if args.part == "event":
        signature = re.search(rb"NTSTATUS msync_try_wait_ready_event[ \t]*\(([^)]*)\)", block)
        if signature is None:
            raise RuntimeError("missing ready-helper signature")
        has_spin_budget = b"," in signature.group(1)
        if has_spin_budget and not re.fullmatch(
                rb"[ \t\r\n]*int[ \t]+obj[ \t]*,[ \t\r\n]*unsigned[ \t]+int[ \t]+max_spins[ \t\r\n]*",
                signature.group(1)):
            raise RuntimeError("unrecognized ready-helper argument contract")
    first_line = data[:start].count(b"\n") + 1
    stem = "frame-extracted" if args.part == "frame" else "ready-event-extracted"
    header_path = output / (stem + ".h")
    record_path = output / (stem + ".sha256.json")
    tag = "FRAME_FIXTURE" if args.part == "frame" else "READY_FIXTURE"
    prefix = (
        "/* GENERATED: native exact-frame helper WITH STUBS; prepare before each build.\n"
        " * The block below is byte-for-byte sync.c, not a reimplemented model. */\n"
        f"#ifndef CIDER_{tag}_EXTRACTED_H\n"
        f"#define CIDER_{tag}_EXTRACTED_H\n"
        f"#define {tag}_EXTRACT_VERSION 1\n"
        f"#define READY_FIXTURE_HAS_SPIN_BUDGET {int(has_spin_budget)}\n"
        f"#define {tag}_SOURCE_PATH {json.dumps(str(source))}\n"
        f'#define {tag}_SOURCE_SHA256 "{source_sha}"\n'
        f'#define {tag}_BLOCK_SHA256 "{block_sha}"\n'
        f"#line {first_line} {json.dumps(str(source))}\n"
    ).encode("ascii")
    suffix = b'\n#line 1 "frame-extracted.h:generated-end"\n#endif\n'
    header = prefix + block + suffix
    record = {
        "kind": "native-exact-frame-helper-with-stubs",
        "scope": "not Wine/API/signal-handler/Windows equivalence",
        "source": str(source),
        "source_sha256": source_sha,
        "block_sha256": block_sha,
        "block_start_byte": start,
        "block_end_byte_exclusive": end,
        "block_start_line": first_line,
        "block_end_line_exclusive": data[:end].count(b"\n") + 1,
        "functions": functions,
        "header": str(header_path),
        "header_sha256": digest(header),
        "harness_sha256": digest(stable_read(directory / ("msync-ready-frame.c" if args.part == "frame" else "msync-ready-event.c"))),
        "extractor_sha256": digest(stable_read(Path(__file__).resolve())),
    }
    # The record is published last and hashes the complete generated header.
    atomic_write(header_path, header)
    atomic_write(record_path, (json.dumps(record, indent=2, sort_keys=True) + "\n").encode())
    json.dump(record, sys.stdout, sort_keys=True)
    sys.stdout.write("\n")
    sys.stdout.flush()  # A broken output stream must fail preparation.


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        print(f"frame extraction failed: {error}", file=sys.stderr, flush=True)
        raise SystemExit(1)
