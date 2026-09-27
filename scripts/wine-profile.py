#!/usr/bin/env python3
"""Poor man's profiler for Wine processes under Rosetta (docs/plan/02 P-2/P-3).

`sample` cannot unwind translated x86_64 code, but lldb can attach to a Rosetta process and print x86 backtraces.
This script stops the process N times, collects the backtrace of the busiest thread (or all threads), and
prints the hottest leaf functions and call paths. Frames in Windows DLLs are resolved with the mingw addr2line
from Cider's toolchain, using the module ranges from vmmap — Cider's own engines keep DWARF in their PE files.

    scripts/wine-profile.py <pid> [--samples 40] [--interval 0.05] [--thread <tid>|all]
"""
import argparse, collections, os, re, subprocess, sys, tempfile

TOOLCHAIN = os.path.expanduser("~/Library/Caches/Cider/toolchain/mingw")

LLDB_SCRIPT = r'''
import lldb, time, json
def __lldb_init_module(debugger, internal_dict):
    pass
def prof(debugger, command, result, internal_dict):
    samples, interval, want = command.split()
    samples, interval = int(samples), float(interval)
    target = debugger.GetSelectedTarget(); process = target.GetProcess()
    debugger.SetAsync(True)
    out = []
    for i in range(samples):
        process.Continue(); time.sleep(interval); process.Stop()
        # wait until stopped
        for _ in range(100):
            if process.GetState() == lldb.eStateStopped: break
            time.sleep(0.01)
        for t in process:
            if want != "all" and want != "hot" and str(t.GetThreadID()) != want: continue
            frames = []
            for f in t:
                sym = f.GetSymbol(); mod = f.GetModule().GetFileSpec().GetFilename()
                name = sym.GetName() if sym.IsValid() else None
                frames.append([f.GetPC(), name, mod])
            out.append({"tid": t.GetThreadID(), "name": t.GetName(), "frames": frames})
    process.Continue(); process.Detach()
    with open(internal_dict.get("__outfile__", "/tmp/wine-profile.json"), "w") as fh: json.dump(out, fh)
'''

IDLE = re.compile(r"mach_msg|__ulock_wait|__workq_kernreturn|__psynch|kevent|select|poll|__semwait|semaphore_.*wait|nanosleep|__recvmsg|read$|__wait4|__sigsuspend|swtch_pri")


def vmmap_modules(pid):
    """(start, end, path) for mapped files; PE images show up as their .dll/.exe paths."""
    out = subprocess.run(["vmmap", "-wide", str(pid)], capture_output=True, text=True).stdout
    mods = []
    for line in out.splitlines():
        m = re.search(r"([0-9a-f]{9,})-([0-9a-f]{9,})\s.*?(/\S.*\.(?:dll|exe|drv|sys|so))\s*$", line, re.I)
        if m:
            mods.append((int(m.group(1), 16), int(m.group(2), 16), m.group(3)))
    # merge per path: lowest start is the image base
    base = {}
    for s, e, p in mods:
        lo, hi = base.get(p, (s, e))
        base[p] = (min(lo, s), max(hi, e))
    return [(s, e, p) for p, (s, e) in base.items()]


def pe_image_base(path):
    for tool in ("x86_64-w64-mingw32-objdump", "i686-w64-mingw32-objdump"):
        exe = os.path.join(TOOLCHAIN, "toolchain-" + tool.split("-")[0], "bin", tool)
        if not os.path.exists(exe): continue
        out = subprocess.run([exe, "-p", path], capture_output=True, text=True).stdout
        m = re.search(r"ImageBase\s+([0-9a-fA-F]+)", out)
        if m: return int(m.group(1), 16), tool.split("-")[0]
    return None, None


def resolve_pe(addr, modules, cache):
    for s, e, path in modules:
        if s <= addr < e:
            if path.lower().endswith(".so"): return None
            key = (path, addr - s)
            if key in cache: return cache[key]
            base, arch = pe_image_base(path)
            name = "%s+0x%x" % (os.path.basename(path), addr - s)
            if base is not None:
                a2l = os.path.join(TOOLCHAIN, "toolchain-" + arch, "bin", arch + "-w64-mingw32-addr2line")
                r = subprocess.run([a2l, "-f", "-C", "-e", path, hex(base + addr - s)], capture_output=True, text=True).stdout.split("\n")
                if r and r[0] and r[0] != "??": name = "%s!%s" % (os.path.basename(path), r[0])
            cache[key] = name
            return name
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pid", type=int)
    ap.add_argument("--samples", type=int, default=40)
    ap.add_argument("--interval", type=float, default=0.05)
    ap.add_argument("--thread", default="hot", help="thread id, 'all', or 'hot' (busiest non-idle thread)")
    ap.add_argument("--stacks", default="", help="print full call chains of samples containing this text")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        script = os.path.join(tmp, "prof.py"); outfile = os.path.join(tmp, "out.json")
        open(script, "w").write(LLDB_SCRIPT.replace('internal_dict.get("__outfile__", "/tmp/wine-profile.json")', repr(outfile)))
        subprocess.run(["lldb", "--batch", "-p", str(args.pid), "-o", "command script import " + script,
                        "-o", "command script add -f prof.prof prof",
                        "-o", "prof %d %s %s" % (args.samples, args.interval, "all" if args.thread == "hot" else args.thread)],
                       capture_output=True, text=True)
        import json
        data = json.load(open(outfile))

    modules = vmmap_modules(args.pid)
    cache = {}

    def label(frame):
        pc, name, mod = frame
        # PE code lives in memory lldb only knows as the loader's reserved area (or nothing at all).
        if name and name != "__wine_reserve" and not name.startswith("___lldb_unnamed"):
            return "%s!%s" % (mod or "?", name)
        return resolve_pe(pc, modules, cache) or ("%s!%s" % (mod, name) if name else "0x%x" % pc)

    busy = [s for s in data if s["frames"] and not IDLE.search(s["frames"][0][1] or "")]
    if args.thread == "hot" and busy:
        top_tid = collections.Counter(s["tid"] for s in busy).most_common(1)[0][0]
        busy = [s for s in busy if s["tid"] == top_tid]
        print("busiest thread: %s (%s), busy in %d of %d samples" % (top_tid, busy[0]["name"], len(busy), args.samples))
    leaf = collections.Counter(label(s["frames"][0]) for s in busy)
    incl = collections.Counter()
    for s in busy:
        seen = set()
        for f in s["frames"][:40]:
            l = label(f)
            if l not in seen: incl[l] += 1; seen.add(l)
    print("\nleaf functions:")
    for name, n in leaf.most_common(15): print("  %3d  %s" % (n, name))
    print("\ninclusive (top of call paths):")
    for name, n in incl.most_common(25): print("  %3d  %s" % (n, name))
    if args.stacks:
        chains = collections.Counter()
        for s in busy:
            labels = [label(f) for f in s["frames"][:30]]
            if any(args.stacks in l for l in labels): chains[" <- ".join(labels[:18])] += 1
        print("\ncall chains containing %r:" % args.stacks)
        for chain, n in chains.most_common(4): print("  %d x %s\n" % (n, chain))


if __name__ == "__main__":
    main()
