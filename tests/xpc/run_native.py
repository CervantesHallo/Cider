#!/usr/bin/env python3
"""Bounded native XPC checks in an ad hoc signed, disposable app namespace."""
import argparse
import base64
import datetime
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
CASES = [
    "native.handshake.no_host", "native.handshake.host", "native.command.engine_list", "native.command.bottle_list",
    "native.output.echo_exit", "native.output.stream", "native.output.over_1mib", "native.input.wrong_root", "native.input.schema",
    "native.input.oversize", "native.input.argument_count", "native.input.malformed", "native.input.nul",
    "native.duplicate.completed", "native.duplicate.active", "native.peer.client_requirement",
    "native.peer.host_requirement", "native.disconnect.unknown_no_replay",
    "native.broker_crash.host_reconnect_no_replay",
    "native.queue.reserved_kill", "native.duplicate.after_256_intervening",
    "native.output.reader_paused_six_seconds", "native.startup.missing_broker_then_register",
]


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(command, *, timeout=20, check=True):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=timeout)
    if check and result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command[0]}\n{result.stdout[-4000:]}\n{result.stderr[-4000:]}")
    return result


def source_hashes():
    files = sorted((REPO / "Packages/CiderKit/Sources").rglob("*.swift"))
    files += sorted((REPO / "Tools/CiderCommands/Sources").rglob("*.swift"))
    files += sorted((HERE / "Fixture/Sources").rglob("*.swift"))
    files += [HERE / "Fixture/Package.swift", HERE / "run_native.py", HERE / "run_packaged_cli.py"]
    return {str(path.relative_to(REPO)): hashlib.sha256(path.read_bytes()).hexdigest() for path in files}


def alive(pid):
    result = run(["/bin/ps", "-p", str(pid), "-o", "stat="], check=False)
    return result.returncode == 0 and not result.stdout.strip().startswith("Z")


def wait_for(predicate, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.03)
    raise TimeoutError("fixture condition timed out")


class NativeFixture:
    def __init__(self, temporary):
        self.temporary = temporary.resolve()
        self.bundle = self.temporary / "NativeFixture.app"
        self.executables = self.bundle / "Contents/MacOS"
        self.root = self.temporary / "support"
        self.root.mkdir(mode=0o700)
        (self.root / "fixture-owned").write_text("native Cider XPC fixture\n")
        self.host_pids = []
        self.broker_pids = []
        self.clients = []
        self.host_pid = None
        self.label = None
        self.registered = False
        self.canonical_root = None
        self.logs = []
        self.snapshots = {}

    def prepare(self):
        self.before_build = source_hashes()
        build = run(["/usr/bin/swift", "build", "--skip-update", "--package-path", HERE / "Fixture"], timeout=300)
        (self.temporary / "build.log").write_text(build.stdout + build.stderr)
        self.snapshots = source_hashes()
        require(self.before_build == self.snapshots, "sources changed during fixture build; rerun after source edits finish")
        binaries = Path(run(["/usr/bin/swift", "build", "--show-bin-path", "--package-path", HERE / "Fixture"]).stdout.strip())
        self.executables.mkdir(parents=True)
        identifier = "org.cider.nativefixture." + uuid.uuid4().hex
        info = {"CFBundleIdentifier": identifier, "CFBundleExecutable": "Cider", "CFBundleName": "Native XPC Fixture",
                "CFBundlePackageType": "APPL", "CFBundleVersion": "1", "LSUIElement": True, "LSMinimumSystemVersion": "14.0"}
        (self.bundle / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
        for name in ["Cider", "ciderctl", "CiderXPCBridge"]:
            shutil.copy2(binaries / name, self.executables / name)
            run(["/usr/bin/codesign", "--force", "--sign", "-", "--options", "runtime", "--identifier", identifier + "." + name, self.executables / name])
        run(["/usr/bin/codesign", "--force", "--sign", "-", "--options", "runtime", self.bundle])
        run(["/usr/bin/codesign", "--verify", "--deep", "--strict", self.bundle])
        self.signatures = {}
        for name, path in [("host", self.bundle), ("client", self.executables / "ciderctl"), ("broker", self.executables / "CiderXPCBridge")]:
            signature = run(["/usr/bin/codesign", "-d", "--verbose=4", "-r-", path])
            self.signatures[name] = (signature.stdout + signature.stderr).strip()
            require("runtime" in self.signatures[name], "fixture component is missing hardened runtime signing")
        description = self.call("describe")
        self.label = description["label"]
        self.canonical_root = description["root"]
        require(Path(description["bundle"]).resolve() == self.bundle.resolve() and Path(description["root"]).resolve() == self.root.resolve(), "canonical fixture paths escaped the owned temporary directory")
        require(Path(description["canonicalBundle"]).resolve() == self.bundle.resolve(), "canonical bundle escaped the fixture")
        expected = "org.cider.ipc." + hashlib.sha256((description["canonicalBundle"] + "\n" + description["root"]).encode()).hexdigest()[:24]
        require(self.label == expected, "service label must derive solely from this fixture bundle and root")
        self.call("register")
        self.registered = True

    def command(self, operation, *arguments, executable="ciderctl"):
        return [self.executables / executable, "--root", self.root, operation, *arguments]

    def call(self, operation, *arguments, executable="ciderctl", expect_success=True, timeout=20):
        result = run(self.command(operation, *arguments, executable=executable), check=False, timeout=timeout)
        value = json.loads(result.stdout)
        if expect_success:
            expected = value.get("exitCode", 0) if operation in ["execute", "execute-slow"] else 0
            require(result.returncode == expected, f"fixture {operation}: code={result.returncode} {result.stderr} error={value.get('error')}")
            if operation in ["execute", "execute-slow"]: value["clientProcessExit"] = result.returncode
        if not expect_success:
            return result.returncode, value
        return value

    def start_host(self, late=False):
        ready = self.root / "ready.json"
        ready.unlink(missing_ok=True)
        launched = self.call("launch-host-late" if late else "launch-host")  # Foundation.Process, never NSWorkspace.
        pid = launched["hostPID"]
        self.host_pid = pid
        self.host_pids.append(pid)
        self.logs.append(Path(launched["log"]))
        wait_for(lambda: ready.exists() or not alive(pid), timeout=10)
        require(ready.exists(), f"fixture host did not register: {self.logs[-1].read_text()}")
        value = json.loads(ready.read_text())
        require(value["pid"] == pid and value["label"] == self.label, "ready record must match the Foundation.Process child")
        if not late:
            hello = self.call("hello")
            require(hello["hostPID"] == pid, "native hello did not identify the host child")
            self.record_broker()

    def record_broker(self):
        if not self.label:
            return
        status = run(["/bin/launchctl", "print", f"gui/{os.getuid()}/{self.label}"], check=False)
        match = re.search(r"\bpid = (\d+)", status.stdout)
        if match:
            pid = int(match.group(1))
            if pid not in self.broker_pids:
                self.broker_pids.append(pid)

    def stop_host(self):
        if self.host_pid and alive(self.host_pid):
            self.stop_owned(self.host_pid, "Cider")
        self.host_pid = None

    def stop_owned(self, pid, component):
        if not alive(pid):
            return
        command = run(["/bin/ps", "-p", str(pid), "-o", "comm="]).stdout.strip()
        require(Path(command).resolve() == (self.executables / component).resolve(), f"refusing to signal PID {pid}: executable is outside this fixture ({command})")
        os.kill(pid, signal.SIGTERM)
        try:
            wait_for(lambda: not alive(pid), timeout=2)
        except TimeoutError:
            command = run(["/bin/ps", "-p", str(pid), "-o", "comm="]).stdout.strip()
            require(Path(command).resolve() == (self.executables / component).resolve(), "PID identity changed before bounded termination")
            os.kill(pid, signal.SIGKILL)
            wait_for(lambda: not alive(pid), timeout=2)

    def request(self, arguments, **overrides):
        value = {"version": 1, "id": str(uuid.uuid4()).upper(), "arguments": arguments,
                 "workingDirectory": self.canonical_root, "appSupport": self.canonical_root}
        value.update(overrides)
        return value

    def request_file(self, value, raw=False):
        path = self.root / ("request-" + uuid.uuid4().hex + (".bin" if raw else ".json"))
        path.write_bytes(value if raw else json.dumps(value).encode())
        return path

    def execute(self, value, timeout=20):
        return self.call("execute", self.request_file(value), timeout=timeout)

    def admissions(self, request_id=None):
        file = self.root / "invocations.jsonl"
        rows = [json.loads(x) for x in file.read_text().splitlines()] if file.exists() else []
        return [x for x in rows if request_id is None or x["id"].upper() == request_id.upper()]

    def raw_rejection(self, value):
        before = len(self.admissions())
        bytes_ = value if isinstance(value, bytes) else json.dumps(value).encode()
        result = self.call("raw", self.request_file(bytes_, raw=True))
        require(result["replyBytes"] == 0, "broker did not reject malformed request bytes")
        require(len(self.admissions()) == before, "invalid input reached the host executor")
        return {"inputBytes": len(bytes_), "replyBytes": result["replyBytes"], "hostAdmissions": 0}

    def start_block(self, request):
        process = subprocess.Popen([str(x) for x in self.command("execute", self.request_file(request))], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.clients.append(process)
        marker = self.root / ("admitted-" + request["id"])
        wait_for(lambda: marker.exists() or process.poll() is not None, timeout=8)
        require(marker.exists(), "blocking request was not admitted")
        return process

    @staticmethod
    def collect(process):
        stdout, stderr = process.communicate(timeout=15)
        value = json.loads(stdout)
        require(process.returncode == value.get("exitCode", 0), f"fixture client failed: code={process.returncode} {stderr} error={value.get('error')}")
        value["clientProcessExit"] = process.returncode
        return value

    def completed(self, response, code):
        require(response["state"] == "completed" and response["exitCode"] == code, f"incorrect native completion: state={response['state']} code={response['exitCode']}")
        require(response["executorPID"] == self.host_pid, "execution must belong to the host PID")
        require(response["clientProcessExit"] == code, "native client process did not preserve the host exit code")
        require(not response["outputIncomplete"], "native output was not fully acknowledged")

    def case(self, name):
        if name == "native.handshake.no_host":
            hello = self.call("hello")
            self.record_broker()
            require(hello.get("hostPID") is None and hello["root"] == self.canonical_root, "unexpected host before launch")
            return hello
        if name == "native.handshake.host":
            hello = self.call("hello")
            require(hello["hostPID"] == self.host_pid, "wrong native host PID")
            return hello
        if name.startswith("native.command."):
            arguments = ["engine", "list"] if name.endswith("engine_list") else ["bottle", "list"]
            response = self.execute(self.request(arguments))
            self.completed(response, 0)
            expected = b"no engines installed\n" if arguments[0] == "engine" else b"no bottles\n"
            require(base64.b64decode(response["stdoutBase64"]) == expected, "readonly library output mismatch")
            require(base64.b64decode(response["stderrBase64"]) == b"", "unexpected readonly stderr")
            return response
        if name in ["native.output.echo_exit", "native.output.stream"]:
            stream = name.endswith("stream")
            response = self.execute(self.request(["fixture-stream"] if stream else ["fixture-echo", "hello", "中文", ""]))
            self.completed(response, 23 if stream else 37)
            stdout = b""; stderr = b""
            if stream:
                for index in range(64):
                    line = (str(index) + ":" + "流🙂" * 500 + "\n").encode()
                    if index % 3 == 0:
                        stderr += line
                    else:
                        stdout += line
            else:
                stdout, stderr = "hello|中文|\n".encode(), b"fixture-stderr\n"
            require(base64.b64decode(response.pop("stdoutBase64")) == stdout, "stdout bytes lost, reordered or changed")
            require(base64.b64decode(response.pop("stderrBase64")) == stderr, "stderr bytes lost, reordered or changed")
            require(response["chunks"] == response["sequenceCount"] and response["chunks"] > 1, "stream envelope/ack count mismatch")
            return dict(response, stdoutBytes=len(stdout), stderrBytes=len(stderr))
        if name == "native.output.over_1mib":
            response = self.execute(self.request(["fixture-long-output"]))
            self.completed(response, 19)
            expected = b"".join(("long-" + str(index) + ":" + "流🙂" * 500 + "\n").encode() for index in range(384))
            require(len(expected) > 1048576, "long-output fixture must exceed 1 MiB of stdout")
            actual = base64.b64decode(response.pop("stdoutBase64"))
            require(actual == expected, f"stdout above 1 MiB changed or truncated: expected {len(expected)}, received {len(actual)}")
            require(base64.b64decode(response.pop("stderrBase64")) == b"", "unexpected long-output stderr")
            return dict(response, stdoutBytes=len(actual), expectedSHA256=hashlib.sha256(expected).hexdigest(), actualSHA256=hashlib.sha256(actual).hexdigest())
        if name.startswith("native.input."):
            value = self.request(["fixture-echo", "must-not-run"])
            kind = name.rsplit(".", 1)[-1]
            if kind == "wrong_root": value["appSupport"] += "-wrong"
            elif kind == "schema": value["version"] = 999
            elif kind == "oversize": value = b"x" * 131073
            elif kind == "argument_count": value["arguments"] = ["x"] * 1025
            elif kind == "malformed": value = b"{bad-json"
            elif kind == "nul": value["arguments"] = ["fixture-echo", "bad\x00argument"]
            return self.raw_rejection(value)
        if name == "native.duplicate.completed":
            request = self.request(["fixture-echo", "once"])
            self.completed(self.execute(request), 37)
            second = self.execute(request)
            require(second["state"] == "rejected" and second["exitCode"] == 64, "completed ID was admitted twice")
            require(len(self.admissions(request["id"])) == 1, "duplicate completed ID reached host twice")
            return {"id": request["id"], "second": second, "hostAdmissions": 1}
        if name == "native.duplicate.active":
            request = self.request(["fixture-block"])
            first = self.start_block(request)
            second = self.execute(request)
            require(second["state"] == "rejected" and second["exitCode"] == 64, "active ID was admitted twice")
            (self.root / ("release-" + request["id"])).touch()
            response = self.collect(first)
            self.completed(response, 0)
            require(len(self.admissions(request["id"])) == 1, "duplicate active ID reached host twice")
            return {"id": request["id"], "first": response, "second": second, "hostAdmissions": 1}
        if name.startswith("native.peer."):
            before = len(self.admissions())
            if name.endswith("client_requirement"):
                code, response = self.call("probe-client", executable="Cider", expect_success=False)
            else:
                # Leave the host slot empty so "another host is already connected" cannot
                # masquerade as successful enforcement of the GUI component requirement.
                self.stop_host()
                wait_for(lambda: self.call("hello").get("hostPID") is None)
                code, response = self.call("probe-host", expect_success=False)
            require(code == 70 and "error" in response, "wrong signed component passed the mutual requirement")
            require(self.call("hello").get("hostPID") == self.host_pid, "unauthorized peer replaced the host")
            require(len(self.admissions()) == before, "unauthorized component reached executor")
            if self.host_pid is None:
                self.start_host()
            return {"processExit": code, "result": response, "hostAdmissions": 0}
        if name == "native.disconnect.unknown_no_replay":
            request = self.request(["fixture-block"])
            client = self.start_block(request)
            previous = self.host_pid
            self.stop_host()
            response = self.collect(client)
            require(response["state"] == "unknown" and response["exitCode"] == 75, "disconnect did not return unknown outcome")
            self.start_host()
            time.sleep(0.2)
            require(len(self.admissions(request["id"])) == 1, "request was replayed after a new host connected")
            require(self.host_pid != previous, "replacement host PID did not change")
            return {"response": response, "previousHostPID": previous, "replacementHostPID": self.host_pid, "hostAdmissions": 1}
        if name == "native.broker_crash.host_reconnect_no_replay":
            request = self.request(["fixture-block-long"])
            client = self.start_block(request)
            original_host = self.host_pid
            self.record_broker()
            original_broker = self.broker_pids[-1]
            command = run(["/bin/ps", "-p", str(original_broker), "-o", "comm="]).stdout.strip()
            require(Path(command).resolve() == (self.executables / "CiderXPCBridge").resolve(), "refusing to crash an unowned broker PID")
            os.kill(original_broker, signal.SIGKILL)
            response = self.collect(client)
            require(response["state"] == "unknown" and response["clientProcessExit"] == 75, "broker crash did not report unknown to the native client")
            require(response.get("resultSource") == "RPCClientTransportFailure", "broker crash must preserve the real client transport-failure evidence")
            require(alive(original_host), "broker crash terminated the host")

            def reconnected():
                try:
                    return self.call("hello").get("hostPID") == original_host
                except (AssertionError, RuntimeError, subprocess.TimeoutExpired):
                    return False

            wait_for(reconnected, timeout=22)
            self.record_broker()
            require(self.broker_pids[-1] != original_broker, "launchd did not replace the crashed broker")
            completed = self.root / ("completed-" + request["id"])
            require(not completed.exists(), "the original wait must remain live across broker recovery")
            require(len(self.admissions(request["id"])) == 1, "broker recovery replayed the admitted wait")
            # Submit a fresh ID only. Never resubmit the live original request after the crash.
            fresh = self.request(["fixture-echo", "fresh-after-broker-crash"])
            self.completed(self.execute(fresh), 37)
            require(len(self.admissions(request["id"])) == 1 and not completed.exists(), "original wait changed while a fresh request ran")
            (self.root / ("release-" + request["id"])).touch()
            wait_for(completed.exists, timeout=3)
            require(completed.read_text() == "0", "the live host wait did not complete after explicit fixture release")
            # The original wait is finished before this intentional duplicate-ID probe.
            replay = self.execute(request)
            require(replay["state"] == "rejected" and replay["clientProcessExit"] == 64
                    and replay.get("executorPID") == original_host, "GUI tombstone did not reject the prior ID after broker restart")
            require(len(self.admissions(request["id"])) == 1, "prior UUID reached the executor again after broker restart")
            return {"response": response, "hostPIDBeforeAndAfter": original_host, "crashedBrokerPID": original_broker,
                    "replacementBrokerPID": self.broker_pids[-1], "hostAdmissionsForOriginalID": 1,
                    "liveOriginalRequestResubmitted": False, "duplicateProbeAfterOriginalCompletion": replay,
                    "freshRequestID": fresh["id"]}
        if name == "native.queue.reserved_kill":
            blocked = []
            responses = []
            try:
                for _ in range(8):
                    request = self.request(["fixture-block-long"])
                    blocked.append((request, self.start_block(request)))
                require(all(not (self.root / ("completed-" + request["id"])).exists() for request, _ in blocked),
                        "all eight ordinary workers must still be blocked")
                control = self.request(["kill", "-b", "fixture"])
                started = time.monotonic()
                result = self.execute(control, timeout=2)
                elapsed = time.monotonic() - started
                # Completed + executorPID proves this is the fixture executor's rejection,
                # not the broker's queue-full rejection (which also uses exit code 64).
                self.completed(result, 64)
                require(elapsed < 2 and len(self.admissions(control["id"])) == 1, "reserved control command did not reach the host promptly")
                require(all(not (self.root / ("completed-" + request["id"])).exists() for request, _ in blocked),
                        "ordinary workers completed before the reserved control command")
            finally:
                for request, _ in blocked:
                    (self.root / ("release-" + request["id"])).touch()
                for request, process in blocked:
                    response = self.collect(process)
                    self.completed(response, 0)
                    require(len(self.admissions(request["id"])) == 1, "blocked command was duplicated")
                    responses.append({"id": request["id"], "state": response["state"], "exitCode": response["exitCode"]})
            return {"ordinarySlotsBlocked": len(blocked), "control": result, "controlSeconds": elapsed,
                    "ordinaryCompletionsAfterExplicitRelease": responses, "wineInvocations": 0}
        if name == "native.duplicate.after_256_intervening":
            original = self.request(["fixture-echo", "retained-tombstone"])
            self.completed(self.execute(original), 37)
            intervening = [self.request(["fixture-echo", str(index)]) for index in range(257)]
            result = self.call("burst", self.request_file(intervening), timeout=20)
            rows = result["responses"]
            require(len(rows) == 257 and len({row["id"] for row in rows}) == 257, "native burst did not complete 257 distinct requests")
            require(all(row["state"] == "completed" and row["exitCode"] == 37 and row.get("executorPID") == self.host_pid
                        and not row["outputIncomplete"] for row in rows), "an intervening request did not complete in the host")
            admitted_ids = {row["id"].upper() for row in self.admissions()}
            require(all(request["id"] in admitted_ids for request in intervening), "intervening requests lack independent host admissions")
            duplicate = self.execute(original)
            require(duplicate["state"] == "rejected" and duplicate["clientProcessExit"] == 64, "prior UUID was evicted after 257 intervening requests")
            require(len(self.admissions(original["id"])) == 1, "retained UUID executed again after the native burst")
            return {"originalID": original["id"], "interveningNativeAdmissions": len(rows), "duplicate": duplicate,
                    "hostAdmissionsForOriginalID": 1, "burstExecutorPID": self.host_pid}
        if name == "native.output.reader_paused_six_seconds":
            request = self.request(["fixture-echo", "slow-reader"])
            started = time.monotonic()
            response = self.call("execute-slow", self.request_file(request), timeout=15)
            elapsed = time.monotonic() - started
            self.completed(response, 37)
            require(elapsed >= 6, "native console did not delay its first ACK for six seconds")
            require(base64.b64decode(response.pop("stdoutBase64")) == b"slow-reader\n"
                    and base64.b64decode(response.pop("stderrBase64")) == b"fixture-stderr\n", "slow reader lost output")
            return {"response": response, "firstConsoleACKDelaySeconds": 6, "elapsedSeconds": elapsed}
        if name == "native.startup.missing_broker_then_register":
            self.stop_host()
            self.call("unregister")
            self.registered = False
            status = run(["/bin/launchctl", "print", f"gui/{os.getuid()}/{self.label}"], check=False)
            require(status.returncode != 0, "initial-start regression must begin with an absent launchd broker")
            self.start_host(late=True)
            initial_host = self.host_pid
            time.sleep(0.75)
            require(alive(initial_host), "initial missing broker terminated the retained host")
            require(run(["/bin/launchctl", "print", f"gui/{os.getuid()}/{self.label}"], check=False).returncode != 0,
                    "fixture registered the broker before the explicit delayed registration")
            self.call("register")
            self.registered = True

            def ready_after_registration():
                try: return self.call("hello").get("hostPID") == initial_host
                except (AssertionError, RuntimeError, subprocess.TimeoutExpired): return False

            wait_for(ready_after_registration, timeout=15)
            self.record_broker()
            self.completed(self.execute(self.request(["fixture-echo", "after-initial-recovery"])), 37)
            return {"sameHostPID": initial_host, "initialBrokerAbsent": True, "registrationDelaySeconds": 0.75,
                    "brokerPID": self.broker_pids[-1], "boundedPublicHostStart": True}
        raise AssertionError("unimplemented native case: " + name)

    def cleanup(self):
        errors = []
        for process in self.clients:
            if process.poll() is None:
                try:
                    self.stop_owned(process.pid, "ciderctl")
                    process.communicate(timeout=3)
                except Exception as error:
                    errors.append(str(error))
        try:
            self.stop_host()
        except Exception as error:
            errors.append(str(error))
        self.record_broker()
        unregistered = False
        if self.label and self.registered:
            try:
                self.call("unregister")
                wait_for(lambda: all(not alive(pid) for pid in self.broker_pids), timeout=5)
                status = run(["/bin/launchctl", "print", f"gui/{os.getuid()}/{self.label}"], check=False)
                require(status.returncode != 0, "owned launchd service remains registered")
                unregistered = True
            except Exception as error:
                errors.append(str(error))
        live = [pid for pid in self.host_pids + self.broker_pids if alive(pid)]
        if live: errors.append(f"live fixture PIDs remain: {live}")
        return {"unregistered": unregistered, "hostPIDs": self.host_pids, "brokerPIDs": self.broker_pids,
                "livePIDs": live, "errors": errors, "passed": not errors and (unregistered or not self.registered)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--filter", action="append", default=[], help="case-name glob; repeat to select a union")
    parser.add_argument("--list", action="store_true", help="list native cases without building or registering anything")
    parser.add_argument("--output", type=Path, help="JSON evidence path (default: tests/xpc/results/native-TIMESTAMP.json)")
    args = parser.parse_args()
    selected = [name for name in CASES if not args.filter or any(fnmatch.fnmatchcase(name, pattern) for pattern in args.filter)]
    if args.list:
        print("\n".join(selected)); return 0
    if not selected:
        parser.error("filters selected no native cases")
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = (args.output or HERE / "results" / f"native-{timestamp}.json").resolve()
    report = {"kind": "native-macos-xpc-fixture", "createdUTC": timestamp, "platform": platform.platform(),
              "filters": args.filter, "selected": selected, "skipped": [name for name in CASES if name not in selected],
              "cases": [], "unitTestsIncluded": 0, "productGUIOrWineSessions": 0}
    directory = tempfile.mkdtemp(prefix="cider-native-xpc-")
    try:
        fixture = NativeFixture(Path(directory))
        try:
            fixture.prepare()
            report["signatures"] = fixture.signatures
            report["label"] = fixture.label
            report["sourceHashes"] = fixture.snapshots
            report["sourcesStableDuringBuild"] = True
            for name in selected:
                if name not in ["native.handshake.no_host", "native.startup.missing_broker_then_register"] and fixture.host_pid is None:
                    fixture.start_host()
                started = time.monotonic()
                try:
                    evidence = fixture.case(name)
                    report["cases"].append({"name": name, "passed": True, "seconds": time.monotonic() - started, "evidence": evidence})
                    print("PASS " + name, flush=True)
                except Exception as error:
                    report["cases"].append({"name": name, "passed": False, "seconds": time.monotonic() - started, "error": str(error)})
                    print("FAIL " + name + ": " + str(error), flush=True)
                    # Stop on a platform/transport failure; do not create a retry loop or weaken identity checks.
                    break
        except Exception as error:
            report["setupError"] = str(error)
            print("SETUP FAIL: " + str(error), flush=True)
        finally:
            report["cleanup"] = fixture.cleanup()
            report["sourceChangedDuringRun"] = bool(fixture.snapshots and fixture.snapshots != source_hashes())
            report["hostLogs"] = [{"name": path.name, "text": path.read_text(errors="replace")[-12000:]} for path in fixture.logs if path.exists()]
            report["nativeCasesExecuted"] = len(report["cases"])
            report["nativeCasesPassed"] = sum(case["passed"] for case in report["cases"])
            report["passed"] = (not report.get("setupError") and len(report["cases"]) == len(selected)
                                and all(case["passed"] for case in report["cases"]) and report["cleanup"]["passed"])
            if not report["cleanup"]["passed"]:
                report["preservedTemporaryDirectory"] = directory
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
            print(f"Native cases: {report['nativeCasesPassed']}/{report['nativeCasesExecuted']} passed; selected {len(selected)}; cleanup {report['cleanup']['passed']}")
            print(f"Evidence: {output}")
    finally:
        if report.get("cleanup", {}).get("passed"):
            shutil.rmtree(directory)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
