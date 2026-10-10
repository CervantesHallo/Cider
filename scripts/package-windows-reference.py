#!/usr/bin/env python3
"""Package only self-owned reference sources, with exact per-file provenance."""
import argparse
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
source = root / "Tools/windows-reference"
git = ["git", "-C", str(root)]
dirty = subprocess.check_output(git + ["status", "--porcelain", "--", "Tools/windows-reference",
                                      "scripts/package-windows-reference.py"], text=True)
if dirty.strip():
    raise SystemExit("Commit reference sources and packaging script before producing a provenance bundle.")
commit = subprocess.check_output(git + ["rev-parse", "HEAD"], text=True).strip()
if len(commit) != 40 or any(c not in "0123456789abcdef" for c in commit):
    raise SystemExit("Unexpected source commit.")
payload = {}
for path in sorted(source.rglob("*")):
    relative = path.relative_to(source)
    if any(part.startswith(".") or part in ("out", "results") for part in relative.parts):
        continue
    if path.is_symlink():
        raise SystemExit("Reference source symlinks are not supported.")
    if path.is_file():
        payload[relative.as_posix()] = path.read_bytes()
payload["shared/source-version.h"] = (
    "/* Generated source-bundle provenance. */\n#define CR_SOURCE_COMMIT \"" + commit + "\"\n"
).encode("ascii")
payload["SOURCE_COMMIT.txt"] = (commit + "\n").encode("ascii")
manifest = {
    "schema": "cider.windows-reference-source-bundle/v1",
    "source_commit": commit,
    "source_tree": "Tools/windows-reference",
    "generated_files": ["shared/source-version.h", "SOURCE_COMMIT.txt"],
    "files": {name: {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
              for name, data in sorted(payload.items())},
}
payload["source-manifest.json"] = (json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode()
args.output.parent.mkdir(parents=True, exist_ok=True)
if args.output.exists():
    raise SystemExit("Refusing to replace an existing source bundle.")
with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
    for name, data in sorted(payload.items()):
        info = zipfile.ZipInfo("windows-reference/" + name, date_time=(2026, 10, 10, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, data)
digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
receipt = args.output.with_suffix(args.output.suffix + ".sha256")
receipt.write_text(digest + "  " + args.output.name + "\n", encoding="ascii")
print(json.dumps({"bundle": str(args.output), "bytes": args.output.stat().st_size,
                  "sha256": digest, "source_commit": commit}, ensure_ascii=False))
