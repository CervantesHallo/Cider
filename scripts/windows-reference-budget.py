#!/usr/bin/env python3
"""Conservative, persistent Windows machine-time reservations (no remote execution).

Reservations are charged in full before a Windows operation and never refunded.
An unresolved reservation prevents further work, including after a disconnect.
This ledger is not a Windows process watchdog: the remote runner must independently
enforce the reserved duration and confirm cleanup before the reservation is closed.
"""
import argparse
from contextlib import contextmanager
from datetime import datetime, timedelta, timezone
import fcntl
import json
import os
from pathlib import Path
import re
import tempfile
import uuid

STATE_ROOT = Path.home() / "Library/Application Support/Cider/WindowsReferenceControl"
STATE_FILE = STATE_ROOT / "budget.json"
LIMITS = {"preparation": 3600, "test": 3600}
LOCAL_TIME = timezone(timedelta(hours=8))


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def initial_state():
    return {"schema": "cider.windows-machine-budget/v1", "limits_seconds": LIMITS,
            "created_utc": timestamp(), "reservations": []}


def validate(state):
    if state.get("schema") != "cider.windows-machine-budget/v1" or state.get("limits_seconds") != LIMITS:
        raise ValueError("Unknown or changed budget; refusing to reset it.")
    reservations = state.get("reservations")
    if not isinstance(reservations, list):
        raise ValueError("Invalid reservation list.")
    seen = set()
    for item in reservations:
        ident = str(uuid.UUID(item["id"]))
        if ident != item["id"] or ident in seen:
            raise ValueError("Invalid or duplicated reservation identity.")
        seen.add(ident)
        seconds = item["seconds"]
        if type(seconds) is not int or not 1 <= seconds <= 600 or item["phase"] not in LIMITS:
            raise ValueError("Invalid duration or phase.")
        if item["status"] not in ("pending", "closed"):
            raise ValueError("Invalid reservation status.")
    if any(sum(x["seconds"] for x in reservations if x["phase"] == phase) > cap
           for phase, cap in LIMITS.items()):
        raise ValueError("Ledger exceeds the adopted cumulative budget.")


def write_state(state):
    validate(state)
    fd, path = tempfile.mkstemp(prefix=".budget-", dir=str(STATE_ROOT))
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(state, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(path, STATE_FILE)
        directory = os.open(str(STATE_ROOT), os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    finally:
        if os.path.exists(path):
            os.unlink(path)


@contextmanager
def locked_state():
    if STATE_ROOT.is_symlink():
        raise ValueError("Budget directory must not be a symlink.")
    STATE_ROOT.mkdir(parents=True, exist_ok=True, mode=0o700)
    if not STATE_ROOT.is_dir() or STATE_FILE.is_symlink():
        raise ValueError("Invalid budget path.")
    lock_path = STATE_ROOT / ".lock"
    fd = os.open(str(lock_path), os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, "r+") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if STATE_FILE.exists():
            if STATE_FILE.stat().st_size > 1024 * 1024:
                raise ValueError("Budget ledger too large.")
            state = json.loads(STATE_FILE.read_text(encoding="utf-8"))
        else:
            state = initial_state()
            write_state(state)
        validate(state)
        yield state


def status(state):
    remaining = {phase: cap - sum(x["seconds"] for x in state["reservations"] if x["phase"] == phase)
                 for phase, cap in LIMITS.items()}
    now = datetime.now(LOCAL_TIME)
    pending = [x["id"] for x in state["reservations"] if x["status"] == "pending"]
    window_open = 9 <= now.hour < 17
    window_remaining = max(0, int((now.replace(hour=17, minute=0, second=0, microsecond=0) - now).total_seconds())) if window_open else 0
    return {"schema": state["schema"], "remaining_seconds": remaining, "pending": pending,
            "window_open": window_open, "window_remaining_seconds": window_remaining,
            "window": "09:00-17:00 UTC+08:00 daily",
            "stopped_by_user": (STATE_ROOT / "STOP").exists(),
            "accounting": "charge-full-reservation-no-refund", "ledger": str(STATE_FILE)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("status")
    reserve = commands.add_parser("reserve")
    reserve.add_argument("--phase", choices=LIMITS, required=True)
    reserve.add_argument("--seconds", type=int, required=True)
    reserve.add_argument("--label", required=True)
    close = commands.add_parser("close")
    close.add_argument("--id", required=True)
    close.add_argument("--receipt", type=Path, required=True,
                       help="Verified runner receipt; not a claim inferred from an SSH exit.")
    args = parser.parse_args()
    with locked_state() as state:
        current = status(state)
        if args.command == "status":
            result = current
        elif args.command == "reserve":
            if not current["window_open"] or current["stopped_by_user"] or current["pending"]:
                raise ValueError("Outside window, stopped, or unresolved cleanup; refusing Windows work.")
            if (not 1 <= args.seconds <= 600
                    or args.seconds > current["remaining_seconds"][args.phase]
                    or args.seconds > current["window_remaining_seconds"]):
                raise ValueError("Reservation exceeds a batch or cumulative limit.")
            if not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,63}", args.label):
                raise ValueError("Label must be a short task ID, without personal or connection data.")
            item = {"id": str(uuid.uuid4()), "phase": args.phase, "seconds": args.seconds,
                    "label": args.label, "status": "pending", "reserved_utc": timestamp()}
            state["reservations"].append(item)
            write_state(state)
            result = item
        else:
            if args.receipt.is_symlink() or not args.receipt.is_file() or args.receipt.stat().st_size > 65536:
                raise ValueError("Invalid runner receipt path.")
            receipt = json.loads(args.receipt.read_text(encoding="utf-8"))
            item = next((x for x in state["reservations"] if x["id"] == args.id), None)
            elapsed = receipt.get("elapsed_seconds")
            if (item is None or item["status"] != "pending"
                    or receipt.get("schema") != "cider.windows-run-cleanup/v1"
                    or receipt.get("reservation_id") != args.id
                    or receipt.get("cleanup_confirmed") is not True
                    or type(elapsed) not in (int, float)
                    or not 0 <= elapsed <= item["seconds"]):
                raise ValueError("Missing, incomplete or over-budget runner cleanup; keep reservation unresolved.")
            item.update(status="closed", closed_utc=timestamp(), observed_elapsed_seconds=elapsed)
            write_state(state)
            result = status(state)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        raise SystemExit(str(error))
