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
import hashlib
import json
import math
import os
from pathlib import Path
import re
import stat
import sys
import tempfile
import uuid

STATE_ROOT = Path.home() / "Library/Application Support/Cider/WindowsReferenceControl"
STATE_FILE = STATE_ROOT / "budget.json"
LIMITS = {"preparation": 3600, "test": 3600}
LOCAL_TIME = timezone(timedelta(hours=8))
SCHEMA_V1 = "cider.windows-machine-budget/v1"
SCHEMA_V2 = "cider.windows-machine-budget/v2"
LEDGER_MAX_BYTES = 1024 * 1024
RECEIPT_MAX_BYTES = 65536
UNMEASURED = "unmeasured-attended-handoff"


class CommitUnconfirmedError(OSError):
    """Replacement happened; durability is uncertain. Never retry a new reservation."""
    def __init__(self, state, cause):
        super().__init__("Ledger replaced, but directory sync failed; read status before any further action.")
        self.result = {"status": "commit-unconfirmed", "ledger": str(STATE_FILE),
                       "pending": [x["id"] for x in state["reservations"] if x["status"] == "pending"],
                       "windows_work_permitted": False, "error": str(cause)}


def valid_id(value):
    return isinstance(value, str) and re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,63}", value) is not None


def finite_seconds(value):
    # Compare integers before float conversion: untrusted JSON can contain huge ints.
    return (type(value) in (int, float) and 0 <= value <= 4200
            and (type(value) is int or math.isfinite(value)))


def read_json_file(path, limit):
    """Hash and parse the same bounded bytes from a regular, non-symlink file."""
    fd = os.open(str(path), os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise ValueError("Invalid or oversized JSON file.")
        raw = stream.read(limit + 1)
    if len(raw) > limit:
        raise ValueError("JSON file exceeds its size limit.")

    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON field.")
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError("Non-finite JSON number.")

    try:
        data = json.loads(raw, object_pairs_hook=unique_object, parse_constant=invalid_constant)
    except RecursionError as error:
        raise ValueError("JSON nesting exceeds the parser limit.") from error
    if not isinstance(data, dict):
        raise ValueError("Expected a JSON object.")
    return data, raw


def duration(receipt, item, allow_overrun=False, require_cleanup_evidence=True):
    """Legacy numeric/no-mode receipts are measured; explicit modes must agree."""
    elapsed = receipt.get("elapsed_seconds")
    mode = receipt.get("duration_measurement")
    upper = receipt.get("elapsed_seconds_upper_bound")
    if mode == UNMEASURED:
        evidence = receipt.get("cleanup_evidence")
        if (allow_overrun or item.get("attended_setup") is not True or elapsed is not None
                or type(upper) is not int or upper != item["seconds"]
                or (require_cleanup_evidence and
                    (not isinstance(evidence, dict) or evidence.get("verified") is not True))):
            raise ValueError("Inconsistent unmeasured attended duration.")
        return None
    if mode is not None or upper is not None or not finite_seconds(elapsed):
        raise ValueError("Invalid measured duration or contradictory measurement fields.")
    if not allow_overrun and elapsed > item["seconds"]:
        raise ValueError("Over-budget cleanup; keep reservation unresolved.")
    return elapsed


def read_receipt(path, item):
    receipt, raw = read_json_file(path, RECEIPT_MAX_BYTES)
    if (receipt.get("schema") != "cider.windows-run-cleanup/v1"
            or receipt.get("reservation_id") != item["id"]
            or receipt.get("cleanup_confirmed") is not True):
        raise ValueError("Missing or incomplete runner cleanup.")
    return receipt, hashlib.sha256(raw).hexdigest()


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def initial_state():
    return {"schema": SCHEMA_V2, "limits_seconds": dict(LIMITS),
            "created_utc": timestamp(), "reservations": [], "extensions": []}


def effective_limits(state):
    caps = dict(LIMITS)
    for item in state.get("extensions", []):
        caps[item["phase"]] += item["seconds"]
    return caps


def charged_seconds(item):
    return item["seconds"] + item.get("overrun_reconciliation", {}).get("extra_charged_seconds", 0)


def validate(state):
    if (not isinstance(state, dict) or state.get("schema") not in (SCHEMA_V1, SCHEMA_V2)
            or state.get("limits_seconds") != LIMITS):
        raise ValueError("Unknown or changed budget; refusing to reset it.")
    extensions = state.get("extensions", [])
    if not isinstance(extensions, list):
        raise ValueError("Invalid authorization history.")
    authorizations = set()
    additions = {}
    for item in extensions:
        if not isinstance(item, dict):
            raise ValueError("Invalid authorization object.")
        ident = item.get("authorization_id")
        if (not valid_id(ident)
                or ident in authorizations or item.get("user_approved") is not True
                or not isinstance(item.get("phase"), str) or item["phase"] not in LIMITS
                or type(item.get("seconds")) is not int
                or not 1 <= item["seconds"] <= 3600
                or not isinstance(item.get("reason"), str) or not 1 <= len(item["reason"]) <= 512):
            raise ValueError("Invalid or duplicated user budget authorization.")
        authorizations.add(ident)
        additions[ident] = item
    reservations = state.get("reservations")
    if not isinstance(reservations, list):
        raise ValueError("Invalid reservation list.")
    seen = set()
    for item in reservations:
        if not isinstance(item, dict) or not isinstance(item.get("id"), str):
            raise ValueError("Invalid reservation object or identity.")
        ident = str(uuid.UUID(item["id"]))
        if ident != item["id"] or ident in seen:
            raise ValueError("Invalid or duplicated reservation identity.")
        seen.add(ident)
        seconds = item.get("seconds")
        if (type(seconds) is not int or not 1 <= seconds <= 600
                or not isinstance(item.get("phase"), str) or item["phase"] not in LIMITS):
            raise ValueError("Invalid duration or phase.")
        if item.get("status") not in ("pending", "closed"):
            raise ValueError("Invalid reservation status.")
        if (type(item.get("attended_setup", False)) is not bool
                or (item.get("attended_setup") and item["phase"] != "preparation")):
            raise ValueError("Invalid attended preparation reservation.")
        if item["status"] == "closed" and "observed_elapsed_seconds" not in item:
            raise ValueError("Closed reservation is missing its duration record.")
        if "overrun_reconciliation" in item:
            if state["schema"] != SCHEMA_V2:
                raise ValueError("Overrun accounting requires v2; refusing unsafe v1 records.")
            record = item["overrun_reconciliation"]
            if not isinstance(record, dict):
                raise ValueError("Invalid overrun reconciliation.")
            auth_id = record.get("authorization_id")
            if not valid_id(auth_id):
                raise ValueError("Invalid reconciliation authorization identity.")
            addition = additions.get(auth_id)
            elapsed = item.get("observed_elapsed_seconds")
            charge = record.get("extra_charged_seconds")
            digest = record.get("receipt_sha256")
            if (item["status"] != "closed" or item["phase"] != "preparation"
                    or item.get("attended_setup") is not True
                    or not finite_seconds(elapsed)
                    or elapsed <= seconds or type(charge) is not int
                    or charge != math.ceil(elapsed - seconds)
                    or not isinstance(digest, str) or not re.fullmatch(r"[a-f0-9]{64}", digest)
                    or addition is None or addition["phase"] != "preparation"
                    or addition["seconds"] < charge
                    or addition.get("overrun_reservation_id") != ident
                    or addition.get("approved_elapsed_seconds") != elapsed
                    or addition.get("receipt_sha256") != digest):
                raise ValueError("Overrun lacks matching bounded human authorization and receipt.")
            if item.get("duration_measurement") is not None or item.get("elapsed_seconds_upper_bound") is not None:
                raise ValueError("Reconciled overrun must be measured.")
        elif item["status"] == "closed":
            # Legacy ledgers kept the duration, but not the receipt's cleanup
            # object. Receipt acceptance still requires it before close.
            duration({**item, "elapsed_seconds": item.get("observed_elapsed_seconds")}, item,
                     require_cleanup_evidence=False)
        elif any(k in item for k in ("observed_elapsed_seconds", "duration_measurement", "elapsed_seconds_upper_bound")):
            raise ValueError("Pending reservation contains closed duration evidence.")
    for addition in additions.values():
        if any(k in addition for k in ("overrun_reservation_id", "approved_elapsed_seconds", "receipt_sha256")):
            if state["schema"] != SCHEMA_V2:
                raise ValueError("Overrun authorization requires v2.")
            item = next((x for x in reservations if x["id"] == addition["overrun_reservation_id"]), None)
            if (item is None or item.get("overrun_reconciliation", {}).get("authorization_id")
                    != addition["authorization_id"]):
                raise ValueError("Overrun addition has no matching reconciliation.")
    if any(sum(charged_seconds(x) for x in reservations if x["phase"] == phase) > cap
           for phase, cap in effective_limits(state).items()):
        raise ValueError("Ledger exceeds the adopted cumulative budget.")


def write_state(state):
    validate(state)
    # A successful mutation upgrades legacy ledgers under the same lock. An old
    # reader must reject v2, never silently reopen charged time after rollback.
    state["schema"] = SCHEMA_V2
    encoded = (json.dumps(state, ensure_ascii=False, indent=2, allow_nan=False) + "\n").encode("utf-8")
    if len(encoded) > LEDGER_MAX_BYTES:
        raise ValueError("Budget ledger too large; refusing an unreadable write.")
    fd, path = tempfile.mkstemp(prefix=".budget-", dir=str(STATE_ROOT))
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(encoded)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(path, STATE_FILE)
        try:
            directory = os.open(str(STATE_ROOT), os.O_RDONLY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        except OSError as error:
            raise CommitUnconfirmedError(state, error) from error
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
            state, _ = read_json_file(STATE_FILE, LEDGER_MAX_BYTES)
        else:
            state = initial_state()
            # A read or rejected command must not create a ledger. First adopted
            # mutation writes the initial history together with its reservation/grant.
        validate(state)
        yield state


def accounting_status(state):
    remaining = {phase: cap - sum(charged_seconds(x) for x in state["reservations"] if x["phase"] == phase)
                 for phase, cap in effective_limits(state).items()}
    pending = [x["id"] for x in state["reservations"] if x["status"] == "pending"]
    return {"schema": state["schema"], "remaining_seconds": remaining, "pending": pending,
            "effective_limits_seconds": effective_limits(state),
            "accounting": "charge-full-reservation-no-refund", "ledger": str(STATE_FILE)}


def status(state):
    now = datetime.now(LOCAL_TIME)
    window_open = 9 <= now.hour < 17
    window_remaining = max(0, int((now.replace(hour=17, minute=0, second=0, microsecond=0) - now).total_seconds())) if window_open else 0
    availability_path = STATE_ROOT / "windows-availability.json"
    user_busy = False
    if availability_path.exists() or availability_path.is_symlink():
        availability, _ = read_json_file(availability_path, RECEIPT_MAX_BYTES)
        if (availability.get("schema") != "cider.windows-availability/v1"
                or type(availability.get("user_busy")) is not bool):
            raise ValueError("Unknown Windows availability record; refusing work.")
        user_busy = availability["user_busy"]
    return {**accounting_status(state),
            "window_open": window_open, "window_remaining_seconds": window_remaining,
            "window": "09:00-17:00 UTC+08:00 daily",
            "stopped_by_user": os.path.lexists(STATE_ROOT / "STOP"),
            "user_busy": user_busy}


def status_after_commit(state):
    try:
        return status(state)
    except (ValueError, OSError) as error:
        # An availability read cannot undo the acknowledged accounting mutation.
        # Report the commit accurately and block work until availability is valid.
        return {**accounting_status(state), "status": "committed",
                "availability_error": str(error), "windows_work_permitted": False}


def reconcile_preparation(state, current, args):
    """Only after the human approves both the exact old overrun and this bounded addition.

    This is local accounting, not authorization to start Windows work. Existing
    reservation seconds, receipt bytes and base limits are never rewritten.
    """
    if (not args.user_approved or not 1 <= args.additional_seconds <= 3600
            or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,63}", args.authorization_id)
            or not 1 <= len(args.reason) <= 512
            or not finite_seconds(args.approved_elapsed_seconds)):
        raise ValueError("Explicit approval of exact observed duration and a bounded addition is required.")
    item = next((x for x in state["reservations"] if x["id"] == args.id), None)
    if item is None:
        raise ValueError("Unknown reservation.")
    receipt, digest = read_receipt(args.receipt, item)
    elapsed = duration(receipt, item, allow_overrun=True)
    if (item["phase"] != "preparation" or item.get("attended_setup") is not True
            or not isinstance(receipt.get("cleanup_evidence"), dict)
            or receipt["cleanup_evidence"].get("verified") is not True
            or elapsed != args.approved_elapsed_seconds or elapsed <= item["seconds"]):
        raise ValueError("An exact measured attended-preparation overrun with verified cleanup is required.")
    charge = math.ceil(elapsed - item["seconds"])
    if charge > args.additional_seconds:
        raise ValueError("Approved addition does not cover the overrun charge.")
    addition = {"authorization_id": args.authorization_id, "user_approved": True,
                "phase": "preparation", "seconds": args.additional_seconds, "reason": args.reason,
                "overrun_reservation_id": args.id, "approved_elapsed_seconds": elapsed,
                "receipt_sha256": digest}
    reconciliation = {"authorization_id": args.authorization_id,
                      "extra_charged_seconds": charge, "receipt_sha256": digest}
    existing = next((x for x in state.get("extensions", [])
                     if x["authorization_id"] == args.authorization_id), None)
    if existing:
        if (any(existing.get(k) != v for k, v in addition.items())
                or item["status"] != "closed" or item.get("overrun_reconciliation") != reconciliation
                or item.get("observed_elapsed_seconds") != elapsed):
            raise ValueError("Authorization already used for different reconciliation terms.")
        return status(state)
    current = status(state)  # Receipt IO may have crossed a STOP/availability change.
    if current["stopped_by_user"] or current["pending"] != [args.id] or item["status"] != "pending":
        raise ValueError("STOP or other unresolved work; refusing reconciliation and addition.")
    now = timestamp()
    addition["recorded_utc"] = now
    state.setdefault("extensions", []).append(addition)
    item.update(status="closed", closed_utc=now, observed_elapsed_seconds=elapsed,
                overrun_reconciliation=reconciliation)
    state["schema"] = SCHEMA_V2
    if status(state)["stopped_by_user"]:
        raise ValueError("STOP before reconciliation commit.")
    write_state(state)
    return status_after_commit(state)


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
    reconcile = commands.add_parser("reconcile-preparation", help="After explicit human approval only: close one measured attended overrun and charge it within a new addition.")
    reconcile.add_argument("--id", required=True)
    reconcile.add_argument("--receipt", type=Path, required=True)
    reconcile.add_argument("--approved-elapsed-seconds", type=float, required=True)
    reconcile.add_argument("--additional-seconds", type=int, required=True)
    reconcile.add_argument("--authorization-id", required=True)
    reconcile.add_argument("--reason", required=True)
    reconcile.add_argument("--user-approved", action="store_true", required=True)
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
                if status(state)["stopped_by_user"]:
                    raise ValueError("STOP before addition commit.")
                write_state(state)
            result = status_after_commit(state)
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
            current = status(state)
            if (current["stopped_by_user"] or current["user_busy"] or current["pending"]
                    or (not args.attended_setup and (not current["window_open"]
                        or args.seconds > current["window_remaining_seconds"]))):
                raise ValueError("Availability changed before reservation commit.")
            state["reservations"].append(item)
            write_state(state)
            result = item
        elif args.command == "close":
            item = next((x for x in state["reservations"] if x["id"] == args.id), None)
            if item is None or item["status"] != "pending":
                raise ValueError("Unknown or already closed reservation.")
            receipt, _ = read_receipt(args.receipt, item)
            elapsed = duration(receipt, item)
            item.update(status="closed", closed_utc=timestamp(), observed_elapsed_seconds=elapsed)
            if elapsed is None:
                item.update(duration_measurement=receipt["duration_measurement"],
                            elapsed_seconds_upper_bound=item["seconds"])
            write_state(state)
            result = status_after_commit(state)
        else:
            result = reconcile_preparation(state, current, args)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        main()
    except CommitUnconfirmedError as error:
        print(json.dumps(error.result, ensure_ascii=False), file=sys.stderr)
        raise SystemExit(1)
    except (ValueError, KeyError, OSError, json.JSONDecodeError) as error:
        raise SystemExit(str(error))
