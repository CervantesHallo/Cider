#!/usr/bin/env python3
"""Real packaged CLI -> NSWorkspace -> isolated AppKit fixture -> real bridge."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

from run_native import HERE, REPO, alive, require, run, source_hashes, wait_for


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    evidence = (args.output or HERE / "results" / f"packaged-cli-{timestamp}.json").resolve()
    directory = Path(tempfile.mkdtemp(prefix="cider-packaged-xpc-")).resolve()
    bundle = directory / "ColdLaunchFixture.app"
    binaries = bundle / "Contents/MacOS"
    home = directory / "private-home"
    home.mkdir(mode=0o700)
    (home / "cold-fixture-owned").write_text("private native cold-launch fixture\n")
    environment = {"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "HOME": str(Path.home()), "CIDER_HOME": str(home),
                   "TMPDIR": str(directory), "LANG": "en_US.UTF-8"}
    report = {"kind": "real-packaged-cli-cold-warm-fixture", "createdUTC": timestamp,
              "nativeCasesExecuted": 0, "cases": [], "productGUIOrWineSessions": 0,
              "fixtureHostLaunch": "production CLI NSWorkspace.openApplication", "unitTestsIncluded": 0}
    label = None
    host_pids = []
    broker_pids = []
    snapshots = {}

    def fixture_mode(argument):
        result = subprocess.run([str(binaries / "Cider"), argument], env=environment, capture_output=True, text=True, timeout=8)
        require(result.returncode == 0, f"fixture control failed: {result.stderr}")
        return json.loads(result.stdout) if result.stdout.strip() else None

    def job():
        return run(["/bin/launchctl", "print", f"gui/{os.getuid()}/{label}"], check=False)

    def remember_pids():
        ready = home / "ready.json"
        if ready.exists():
            pid = json.loads(ready.read_text())["hostPID"]
            if pid not in host_pids: host_pids.append(pid)
        if label:
            match = re.search(r"\bpid = (\d+)", job().stdout)
            if match and int(match.group(1)) not in broker_pids: broker_pids.append(int(match.group(1)))

    try:
        before = source_hashes()
        build = run(["/usr/bin/swift", "build", "--skip-update", "--package-path", HERE / "Fixture", "--product", "FixtureAppHost"], timeout=300)
        (directory / "build.log").write_text(build.stdout + build.stderr)
        snapshots = source_hashes()
        require(before == snapshots, "source changed during fixture build; rerun after edits finish")
        build_bin = Path(run(["/usr/bin/swift", "build", "--show-bin-path", "--package-path", HERE / "Fixture"]).stdout.strip())
        actual = REPO / "out/Cider.app/Contents/MacOS"
        # The Bool ACK protocol must match in all three processes; never call an older
        # packaged binary a successful test of today's source.
        newest_ipc = max(path.stat().st_mtime_ns for path in (REPO / "Packages/CiderKit/Sources/CiderIPC").glob("*.swift"))
        sources = {"ciderctl": actual / "ciderctl", "CiderXPCBridge": actual / "CiderXPCBridge"}
        require(all(path.exists() and path.stat().st_mtime_ns >= newest_ipc for path in sources.values()),
                "packaged CLI/bridge predate current IPC source; rebuild out/Cider.app before this native check")
        binaries.mkdir(parents=True)
        report["productionBinaryCopies"] = {}
        for name, source in sources.items():
            original = digest(source)
            shutil.copy2(source, binaries / name)
            require(original == digest(source) == digest(binaries / name), "production binary changed during copy")
            report["productionBinaryCopies"][name] = {"source": str(source), "originalSHA256": original,
                                                       "copiedBytesMatchBeforeFixtureSigning": True}
        shutil.copy2(build_bin / "FixtureAppHost", binaries / "Cider")
        identifier = "org.cider.coldfixture." + uuid.uuid4().hex
        info = {"CFBundleIdentifier": identifier, "CFBundleExecutable": "Cider", "CFBundleName": "Cider Native Cold Fixture",
                "CFBundlePackageType": "APPL", "CFBundleVersion": "1", "LSUIElement": True, "LSMinimumSystemVersion": "14.0"}
        (bundle / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
        for name in ["Cider", "ciderctl", "CiderXPCBridge"]:
            run(["/usr/bin/codesign", "--force", "--sign", "-", "--options", "runtime", "--identifier", identifier + "." + name, binaries / name])
        run(["/usr/bin/codesign", "--force", "--sign", "-", "--options", "runtime", bundle])
        run(["/usr/bin/codesign", "--verify", "--deep", "--strict", bundle])
        report["signatures"] = {}
        for name, path in [("host", bundle), ("client", binaries / "ciderctl"), ("broker", binaries / "CiderXPCBridge")]:
            signed = run(["/usr/bin/codesign", "-d", "--verbose=4", "-r-", path])
            report["signatures"][name] = signed.stdout + signed.stderr
            require("runtime" in report["signatures"][name], "fixture component lacks hardened runtime")
        description = fixture_mode("--describe-fixture")  # Metadata only; does not start NSApplication or register launchd.
        label = description["label"]
        require(Path(description["bundle"]).resolve() == bundle.resolve(), "fixture bundle canonical path escaped")
        require(Path(description["root"]).resolve() == (home / "AppSupport").resolve(), "fixture data root escaped")
        require(Path(description["canonicalBundle"]).resolve() == bundle.resolve(), "canonical fixture bundle escaped")
        expected = "org.cider.ipc." + hashlib.sha256((description["canonicalBundle"] + "\n" + description["root"]).encode()).hexdigest()[:24]
        require(label == expected, "fixture label does not match public canonicalPath")
        require(job().returncode != 0, "cold-launch fixture must start with no registered service")
        require(not (home / "ready.json").exists() and not (home / "executions.jsonl").exists(), "fixture GUI host already ran")
        report["label"] = label
        report["sourceHashes"] = snapshots
        report["sourcesStableDuringBuild"] = True
        for name in ["native.packaged_cli.cold_nsworkspace", "native.packaged_cli.warm_same_host"]:
            started = time.monotonic()
            try:
                result = subprocess.run([str(binaries / "ciderctl"), "engine", "list"], cwd=directory, env=environment,
                                        capture_output=True, text=True, timeout=35 if "cold" in name else 10)
                remember_pids()
                require(result.returncode == 0 and result.stdout == "no engines installed\n" and not result.stderr,
                        f"actual CLI failed: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}")
                ready = json.loads((home / "ready.json").read_text())
                receipts = [json.loads(line) for line in (home / "executions.jsonl").read_text().splitlines()]
                index = 1 if "cold" in name else 2
                require(len(receipts) == index and receipts[-1]["arguments"] == ["engine", "list"], "unexpected host execution count or command")
                require(receipts[-1]["executorPID"] == ready["hostPID"] == host_pids[0] and alive(host_pids[0]), "execution did not belong to the same fixture AppKit host")
                require(Path(receipts[-1]["appSupport"]).resolve() == (home / "AppSupport").resolve(), "actual CLI did not preserve private CIDER_HOME")
                require(len(host_pids) == 1 and len(broker_pids) == 1 and alive(broker_pids[0]), "warm invocation restarted a component")
                if index == 2: require(receipts[0]["requestID"] != receipts[1]["requestID"], "warm call reused a prior request ID")
                report["cases"].append({"name": name, "passed": True, "seconds": time.monotonic() - started,
                    "actualCLIExitCode": result.returncode, "stdout": result.stdout, "stderr": result.stderr,
                    "executorPID": receipts[-1]["executorPID"], "executorPIDEvidence": "independent fixture executor receipt",
                    "hostPID": ready["hostPID"], "brokerPID": broker_pids[0], "receipt": receipts[-1]})
                print("PASS " + name, flush=True)
            except Exception as error:
                report["cases"].append({"name": name, "passed": False, "seconds": time.monotonic() - started, "error": str(error)})
                print("FAIL " + name + ": " + str(error), flush=True)
                break
    except Exception as error:
        report["setupError"] = str(error)
        print("SETUP FAIL: " + str(error), flush=True)
    finally:
        errors = []
        try: remember_pids()
        except Exception as error: errors.append(str(error))
        for pid in host_pids:
            try:
                if alive(pid):
                    command = run(["/bin/ps", "-p", str(pid), "-o", "comm="]).stdout.strip()
                    require(Path(command).resolve() == (binaries / "Cider").resolve(), "refusing to signal a nonfixture AppKit host")
                    os.kill(pid, signal.SIGTERM)
                    try: wait_for(lambda: not alive(pid), timeout=2)
                    except TimeoutError:
                        os.kill(pid, signal.SIGKILL)
                        wait_for(lambda: not alive(pid), timeout=2)
            except Exception as error: errors.append(str(error))
        unregistered = False
        if label:
            try:
                fixture_mode("--unregister-fixture")
                if job().returncode == 0:
                    # A canonicalization regression can change the derived label after root
                    # creation. Clean only the recorded job after verifying its private plist.
                    file = home / "AppSupport/State/XPC" / (label + ".plist")
                    specification = plistlib.loads(file.read_bytes())
                    arguments = specification["ProgramArguments"]
                    require(specification["Label"] == label and Path(arguments[0]).resolve() == (binaries / "CiderXPCBridge").resolve()
                            and Path(arguments[2]).resolve() == (home / "AppSupport").resolve(), "cleanup job does not belong to the fixture")
                    run(["/bin/launchctl", "bootout", f"gui/{os.getuid()}/{label}"], check=False, timeout=8)
                    report["cleanupUsedVerifiedRecordedLabel"] = True
                wait_for(lambda: all(not alive(pid) for pid in broker_pids), timeout=5)
                require(job().returncode != 0, "cold fixture launchd job remains registered")
                unregistered = True
            except Exception as error: errors.append(str(error))
        lsregister = Path("/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister")
        if bundle.exists() and lsregister.exists():
            result = run([lsregister, "-u", bundle], check=False, timeout=10)
            report["launchServicesUnregister"] = {"exitCode": result.returncode, "stdout": result.stdout, "stderr": result.stderr}
        live = [pid for pid in host_pids + broker_pids if alive(pid)]
        if live: errors.append(f"live fixture PIDs remain: {live}")
        report["cleanup"] = {"passed": not errors and (unregistered or label is None), "unregistered": unregistered,
                              "hostPIDs": host_pids, "brokerPIDs": broker_pids, "livePIDs": live, "errors": errors}
        report["sourceChangedDuringRun"] = bool(snapshots and source_hashes() != snapshots)
        report["nativeCasesExecuted"] = len(report["cases"])
        report["nativeCasesPassed"] = sum(case["passed"] for case in report["cases"])
        failure = home / "host-error.txt"
        if failure.exists(): report["hostError"] = failure.read_text()
        report["passed"] = not report.get("setupError") and len(report["cases"]) == 2 and all(case["passed"] for case in report["cases"]) and report["cleanup"]["passed"]
        if not report["cleanup"]["passed"]: report["preservedTemporaryDirectory"] = str(directory)
        evidence.parent.mkdir(parents=True, exist_ok=True)
        evidence.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
        print(f"Packaged CLI native cases: {report['nativeCasesPassed']}/{report['nativeCasesExecuted']}; cleanup {report['cleanup']['passed']}")
        print(f"Evidence: {evidence}")
        if report["cleanup"]["passed"]: shutil.rmtree(directory)
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
