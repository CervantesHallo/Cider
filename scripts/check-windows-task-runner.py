#!/usr/bin/env python3
"""Explicit local validation in an existing account-free Cider experiment bottle.

No Windows network access, no user bottle cloning, no reference baseline rerun.
Build the three task-runner binaries first; the lab-state file is private metadata
from preparing an isolated lab under Cider's windows-reference cache.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import uuid


def main():
    if sys.platform != "darwin":
        raise RuntimeError("This driver measures an isolated Mac/Cider experiment, not native Windows.")
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path.home() / "Library/Caches/Cider/windows-reference"
    parser.add_argument("--lab-state", type=Path, default=root / "task-runner-lab-20261011.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    state = json.loads(args.lab_state.read_text())
    home, lab = Path(state["home"]).resolve(), Path(state["lab"]).resolve()
    home.relative_to(root.resolve())
    if not home.name.startswith("wine-lab-") or not re.fullmatch(r"win32-reference-[a-f0-9-]{36}", state["bottle"]):
        raise ValueError("Only the separately created Win32 reference lab is accepted.")
    prefix = home / "AppSupport/Bottles" / state["bottle"] / "prefix/drive_c"
    relative = lab.relative_to(prefix.resolve())
    if len(relative.parts) != 1 or not re.fullmatch(r"CiderRunnerLab-[a-f0-9]{10}", relative.name):
        raise ValueError("Invalid dedicated runner fixture directory.")
    win = "C:\\" + relative.name
    if state["windows_root"] != win:
        raise ValueError("Windows/Mac experiment directory mismatch.")
    cli = repo / "Tools/ciderctl/.build/debug/ciderctl"
    env = os.environ.copy()
    env["CIDER_HOME"] = str(home)
    names = ["cider-task-runner.exe", "cider-task-fixture.exe", "cider-task-deadline-fixture.exe"]
    for name in names:
        shutil.copy2(repo / "out" / name, lab / name)
    records = []
    cleanup = False

    def run(case, child_args, seconds="20", ident=None, receipt=None, binary=names[0]):
        ident = ident or str(uuid.uuid4())
        receipt = receipt or str(uuid.uuid4()) + ".json"
        command = [str(cli), "run", "-b", state["bottle"], "--wait", "--debug", "quiet",
                   win + "\\" + binary, ident, seconds, win + "\\" + receipt,
                   win + "\\STOP", win + "\\" + names[1], *child_args]
        begin = time.monotonic()
        completed = subprocess.run(command, env=env, capture_output=True, text=True, timeout=35)
        path = lab / receipt
        value = json.loads(path.read_text()) if path.exists() else None
        partial = json.loads(Path(str(path) + ".partial").read_text()) if Path(str(path) + ".partial").exists() else None
        match = re.search(r" · log (.+)", completed.stdout)
        echo = []
        if match:
            log = Path(match.group(1).strip())
            if log.stat().st_size > 4 * 1024 * 1024:
                raise ValueError("Unexpectedly large synthetic fixture output.")
            echo = [line for line in log.read_text(errors="replace").splitlines()
                    if re.fullmatch(r"\d+:[0-9a-f]*", line)]
        row = {"case": case, "cli_exit": completed.returncode,
               "local_wall_seconds": round(time.monotonic() - begin, 3),
               "receipt": value, "partial_receipt": partial, "echo_lines": echo}
        records.append(row)
        print(case, "exit", completed.returncode, flush=True)
        return row, ident, receipt

    try:
        # A prior failed fixture may leave only this lab's own STOP sentinel.
        (lab / "STOP").unlink(missing_ok=True)
        row, ident, path = run("quoted-argv", ["echo", "", "two words", 'a"b', "C:\\tail\\", "中文"])
        row["checks_passed"] = row["cli_exit"] == 0 and row["receipt"]["cleanup_confirmed"] and row["echo_lines"] == [
            "0:", "9:00740077006f00200077006f007200640073", "3:006100220062",
            "8:0043003a005c007400610069006c005c", "2:4e2d6587"]
        old = row["receipt"]
        row, _, _ = run("stale-receipt", ["stop", win + "\\STOP"], ident=ident, receipt=path)
        row["checks_passed"] = row["cli_exit"] != 0 and row["receipt"] == old and not (lab / "STOP").exists()
        row, _, _ = run("nonzero", ["exit37"])
        row["checks_passed"] = row["cli_exit"] != 0 and row["receipt"]["cleanup_confirmed"] and row["receipt"]["primary_exit_code"] == 37
        for case, mode, total in [("timeout", "sleep", 1), ("descendant-after-parent-exit", "spawn", 2)]:
            row, _, _ = run(case, [mode], seconds="10")
            value = row["receipt"]
            row["checks_passed"] = row["cli_exit"] != 0 and value["cleanup_confirmed"] and value["status"] == "timeout" and value["job_total_processes"] == total and value["elapsed_seconds"] <= 10
            if mode == "spawn": row["checks_passed"] &= value["primary_exit_code"] == 0
        row, _, _ = run("breakaway-denied", ["breakaway"])
        row["checks_passed"] = row["cli_exit"] == 0 and row["receipt"]["cleanup_confirmed"] and row["receipt"]["job_total_processes"] == 1
        row, _, _ = run("stop-file", ["stop", win + "\\STOP"])
        row["checks_passed"] = row["cli_exit"] != 0 and row["receipt"]["cleanup_confirmed"] and row["receipt"]["status"] == "stopped"
        (lab / "STOP").unlink(missing_ok=True)
        row, _, _ = run("short-reservation-denied", ["stop", win + "\\STOP"], seconds="1")
        row["checks_passed"] = row["cli_exit"] != 0 and row["receipt"] is None and not (lab / "STOP").exists()
        row, _, _ = run("blocked-main-thread-hard-deadline", ["sleep"], seconds="10", binary=names[2])
        # Wine's Unix exit status contains the low byte of ERROR_TIMEOUT (1460).
        row["checks_passed"] = row["cli_exit"] == (1460 & 255) and row["receipt"] is None and row["partial_receipt"]["cleanup_confirmed"] is False and row["partial_receipt"]["child_started"] is None
    finally:
        subprocess.run([str(cli), "kill", "-b", state["bottle"]], env=env, capture_output=True, timeout=15, check=True)
        ps = subprocess.run([str(cli), "ps", "--all"], env=env, capture_output=True, text=True, timeout=5, check=True)
        cleanup = not any(len(line.split("\t")) >= 2 and line.split("\t")[1] == state["bottle"] for line in ps.stdout.splitlines())
        digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
        result = {"schema": "cider.windows-cli-runner-local-validation/v1", "date": "2026-10-11",
                  "environment": "isolated Cider r2 on macOS; not native Windows",
                  "engine_id": "cider-cx26.3-r2-x86_64", "windows_machine_used": False,
                  "runner_source_sha256": digest(repo / "Tools/windows-task-runner/runner.c"),
                  "fixture_source_sha256": digest(repo / "Tools/windows-task-runner/fixture.c"),
                  "binaries": {name: digest(repo / "out" / name) for name in names},
                  "deadline_fixture_definition": "CIDER_RUNNER_DEADLINE_FIXTURE (separate binary only)",
                  "cases": records, "experiment_bottle_stopped": cleanup,
                  "all_checks_passed": len(records) == 9 and cleanup and all(row.get("checks_passed") for row in records),
                  "native_windows_verified": False}
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    return 0 if result["all_checks_passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
