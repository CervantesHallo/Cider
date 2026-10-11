#!/usr/bin/env python3
"""Conservative, persistent Windows machine-time reservations (no remote execution).

Reservations are charged in full before a Windows operation and never refunded.
An unresolved reservation prevents further work, including after a disconnect.
This ledger is not a Windows process watchdog: the remote runner must independently
enforce the reserved duration and confirm cleanup before the reservation is closed.
Explicit user-approved additions are append-only; they never reset reservations.
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
            "created_utc": timestamp(), "reservations": [], "extensions": []}


def effective_limits(state):
    caps = dict(LIMITS)
    for item in state.get("extensions", []):
        caps[item["phase"]] += item["seconds"]
    return caps


def validate(state):
    if state.get("schema") != "cider.windows-machine-budget/v1" or state.get("limits_seconds") != LIMITS:
        raise ValueError("Unknown or changed budget; refusing to reset it.")
    extensions = state.get("extensions", [])
    if not isinstance(extensions, list):
        raise ValueError("Invalid authorization history.")
    authorizations = set()
    for item in extensions:
        ident = item["authorization_id"]
        if (not isinstance(ident, str) or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,63}", ident)
                or ident in authorizations or item.get("user_approved") is not True
                or item["phase"] not in LIMITS or type(item["seconds"]) is not int
                or not 1 <= item["seconds"] <= 3600
                or not isinstance(item.get("reason"), str) or not 1 <= len(item["reason"]) <= 512):
            raise ValueError("Invalid or duplicated user budget authorization.")
        authorizations.add(ident)
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
        if (type(item.get("attended_setup", False)) is not bool
                or (item.get("attended_setup") and item["phase"] != "preparation")):
            raise ValueError("Invalid attended preparation reservation.")
    if any(sum(x["seconds"] for x in reservations if x["phase"] == phase) > cap
           for phase, cap in effective_limits(state).items()):
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
                 for phase, cap in effective_limits(state).items()}
    now = datetime.now(LOCAL_TIME)
    pending = [x["id"] for x in state["reservations"] if x["status"] == "pending"]
    window_open = 9 <= now.hour < 17
    window_remaining = max(0, int((now.replace(hour=17, minute=0, second=0, microsecond=0) - now).total_seconds())) if window_open else 0
    availability_path = STATE_ROOT / "windows-availability.json"
    user_busy = False
    if availability_path.exists() or availability_path.is_symlink():
        if (availability_path.is_symlink() or not availability_path.is_file()
                or availability_path.stat().st_size > 65536):
            raise ValueError("Invalid Windows availability record; refusing work.")
        availability = json.loads(availability_path.read_text(encoding="utf-8"))
        if (availability.get("schema") != "cider.windows-availability/v1"
                or type(availability.get("user_busy")) is not bool):
            raise ValueError("Unknown Windows availability record; refusing work.")
        user_busy = availability["user_busy"]
    return {"schema": state["schema"], "remaining_seconds": remaining, "pending": pending,
            "effective_limits_seconds": effective_limits(state),
            "window_open": window_open, "window_remaining_seconds": window_remaining,
            "window": "09:00-17:00 UTC+08:00 daily",
            "stopped_by_user": (STATE_ROOT / "STOP").exists(),
            "user_busy": user_busy,
            "accounting": "charge-full-reservation-no-refund", "ledger": str(STATE_FILE)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("status")
    extend = commands.add_parser("extend", help="Apply one explicitly approved user addition; never use automatically.")
    extend.add_argument("--phase", choices=LIMITS, required=True)
    extend.add_argument("--seconds", type=int, required=True)
    extend.add_argument("--authorization-id", required=True, help="Stable ID for this exact human approval; reuse on retry.")
    extend.add_argument("--reason", required=True, help="Approval summary without connection data or credentials.")
    extend.add_argument("--user-approved", action="store_true", required=True)
    reserve = commands.add_parser("reserve")
    reserve.add_argument("--phase", choices=LIMITS, required=True)
    reserve.add_argument("--seconds", type=int, required=True)
    reserve.add_argument("--label", required=True)
    reserve.add_argument("--attended-setup", action="store_true",
                         help="Only for a current human-operated setup handoff, never a background test.")
    close = commands.add_parser("close")
    close.add_argument("--id", required=True)
    close.add_argument("--receipt", type=Path, required=True,
                       help="Verified runner receipt; not a claim inferred from an SSH exit.")
    args = parser.parse_args()
    with locked_state() as state:
        current = status(state)
        if args.command == "status":
            result = current
        elif args.command == "extend":
            if (not args.user_approved or not 1 <= args.seconds <= 3600
                    or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,63}", args.authorization_id)
                    or not 1 <= len(args.reason) <= 512):
                raise ValueError("Explicit, bounded user approval is required.")
            existing = next((x for x in state.get("extensions", [])
                             if x["authorization_id"] == args.authorization_id), None)
            if existing:
                if (existing["phase"], existing["seconds"], existing["reason"]) != (args.phase, args.seconds, args.reason):
                    raise ValueError("Authorization already used for different terms.")
            else:
                if current["pending"] or current["stopped_by_user"]:
                    raise ValueError("Unresolved cleanup or STOP; refusing a new addition.")
                state.setdefault("extensions", []).append({
                    "authorization_id": args.authorization_id, "user_approved": True,
                    "phase": args.phase, "seconds": args.seconds,
                    "reason": args.reason, "recorded_utc": timestamp()})
                write_state(state)
            result = status(state)
        elif args.command == "reserve":
            if args.attended_setup and args.phase != "preparation":
                raise ValueError("Attended handoff is only a preparation reservation.")
            if ((not current["window_open"] and not args.attended_setup)
                    or current["stopped_by_user"] or current["pending"] or current["user_busy"]):
                raise ValueError("Outside window, stopped, user busy, or unresolved cleanup; refusing Windows work.")
            if (not 1 <= args.seconds <= 600
                    or args.seconds > current["remaining_seconds"][args.phase]
                    or (not args.attended_setup and args.seconds > current["window_remaining_seconds"])):
                raise ValueError("Reservation exceeds a batch or cumulative limit.")
            if not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,63}", args.label):
                raise ValueError("Label must be a short task ID, without personal or connection data.")
            item = {"id": str(uuid.uuid4()), "phase": args.phase, "seconds": args.seconds,
                    "label": args.label, "status": "pending", "reserved_utc": timestamp()}
            if args.attended_setup:
                item["attended_setup"] = True
            state["reservations"].append(item)
            write_state(state)
            result = item
        else:
            if args.receipt.is_symlink() or not args.receipt.is_file() or args.receipt.stat().st_size > 65536:
                raise ValueError("Invalid runner receipt path.")
            receipt = json.loads(args.receipt.read_text(encoding="utf-8"))
            item = next((x for x in state["reservations"] if x["id"] == args.id), None)
            elapsed = receipt.get("elapsed_seconds")
            measured = type(elapsed) in (int, float) and item is not None and 0 <= elapsed <= item["seconds"]
            attended_bound = (item is not None and item.get("attended_setup") is True
                              and elapsed is None
                              and receipt.get("duration_measurement") == "unmeasured-attended-handoff"
                              and receipt.get("elapsed_seconds_upper_bound") == item["seconds"]
                              and isinstance(receipt.get("cleanup_evidence"), dict)
                              and receipt["cleanup_evidence"].get("verified") is True)
            if (item is None or item["status"] != "pending"
                    or receipt.get("schema") != "cider.windows-run-cleanup/v1"
                    or receipt.get("reservation_id") != args.id
                    or receipt.get("cleanup_confirmed") is not True
                    or not (measured or attended_bound)):
                raise ValueError("Missing, incomplete or over-budget runner cleanup; keep reservation unresolved.")
            item.update(status="closed", closed_utc=timestamp(), observed_elapsed_seconds=elapsed)
            if attended_bound:
                item.update(duration_measurement=receipt["duration_measurement"],
                            elapsed_seconds_upper_bound=item["seconds"])
            write_state(state)
            result = status(state)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        raise SystemExit(str(error))
