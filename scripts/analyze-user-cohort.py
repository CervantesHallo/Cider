#!/usr/bin/env python3
"""Pure stdlib offline analysis of the frozen U cohort; no subprocess/network.

Run: python3 -B scripts/analyze-user-cohort.py --input <cohort> --out <new-folder>
Only analysis.json and REPORT.md inside the new output folder are written. All 90 raw
hashes are checked before output. Original verdicts and receipt rows are retained.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics as stats


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, required=True, help='Frozen U cohort directory')
parser.add_argument('--out', type=Path, required=True, help='New output directory')
options = parser.parse_args()
INPUT = options.input.resolve(strict=True)
HERE = options.out.resolve()
HERE.mkdir(parents=True, exist_ok=False)
PINNED = {
    "performance-protocol-v1.json": "7fddc013514ad58bfd92b8987b8e5c006092a49431f34b0e5678b47ec757b6bc",
    "performance-result-v1.json": "c35a07dfca5cc69ed0d2904d75cbfc869b3a7d67ca4ae170d36776b2e2a4e945",
}
MODES = ("server", "msync")


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def close(a, b):
    return math.isclose(a, b, rel_tol=1e-8, abs_tol=0.002)


def spread(values):
    values = list(values)
    require(bool(values) and all(math.isfinite(v) and v > 0 for v in values), "Invalid spread input")
    median = stats.median(values)
    return dict(values=values, count=len(values), minimum=min(values), maximum=max(values),
                median=median, mean=stats.mean(values), range=max(values)-min(values),
                range_pct_of_median=100*(max(values)-min(values))/median,
                max_over_min=max(values)/min(values))


def pct(after, before):
    return 100*(after/before-1)


def interval(row, row_index):
    workers = row["workers"]
    first = min(w["qpc_begin"] for w in workers)
    last = max(w["qpc_end"] for w in workers)
    ticks = last-first
    scale = 1e9/row["qpc_frequency"]
    units = sum(w["completed"] for w in workers)
    require(len(workers) == row["threads"] and ticks > 0, "Invalid worker envelope")
    require(row["qpc_begin_min"] == first and row["qpc_end_max"] == last
            and row["qpc_envelope_ticks"] == ticks, "Envelope mismatch")
    require(units == row["completed_worker_iterations"], "Iteration mismatch")
    require(close(row["envelope_ns"], ticks*scale)
            and close(row["ns_per_worker_iteration"], ticks*scale/units), "Batch metric mismatch")
    require(sum(w["qpc_ticks"] for w in workers) == row["qpc_worker_ticks_sum"], "Worker sum mismatch")
    derived_workers = []
    for w in workers:
        require(w["qpc_ticks"] == w["qpc_end"]-w["qpc_begin"] > 0, "Worker interval mismatch")
        derived_workers.append(dict(index=w["index"], role=w["role"],
            begin_offset_ns=(w["qpc_begin"]-first)*scale,
            end_offset_ns=(w["qpc_end"]-first)*scale,
            duration_ns=w["qpc_ticks"]*scale,
            wall_ns_per_own_iteration=w["qpc_ticks"]*scale/w["completed"],
            fraction_of_envelope=w["qpc_ticks"]/ticks,
            tail_after_own_end_ns=(last-w["qpc_end"])*scale))
    return dict(receipt_row_index=row_index, batch=row["batch"], phase=row["phase"],
        qpc_begin_min=first, qpc_end_max=last, qpc_envelope_ticks=ticks,
        envelope_ns=ticks*scale, ns_per_worker_iteration=ticks*scale/units,
        completed_worker_iterations=units,
        start_skew_ns=(max(w["qpc_begin"] for w in workers)-first)*scale,
        finish_skew_ns=(last-min(w["qpc_end"] for w in workers))*scale,
        common_intersection_ns=max(0, min(w["qpc_end"] for w in workers)
                                   - max(w["qpc_begin"] for w in workers))*scale,
        sum_worker_wall_ns=sum(w["qpc_ticks"] for w in workers)*scale,
        mean_worker_interval_fraction=sum(w["qpc_ticks"] for w in workers)/(len(workers)*ticks),
        tail_after_own_end_ns_sum=sum((last-w["qpc_end"])*scale for w in workers),
        workers=derived_workers)


def phase_stats(batches, phase):
    selected = [b for b in batches if b["phase"] == phase]
    return dict(batch_ns_per_iteration=spread(b["ns_per_worker_iteration"] for b in selected),
                aggregate_ns_per_iteration=sum(b["envelope_ns"] for b in selected)
                /sum(b["completed_worker_iterations"] for b in selected))


def main():
    manifests, documents = [], {}
    for name, expected in PINNED.items():
        data = (INPUT/name).read_bytes()
        require(digest(data) == expected, "Frozen input hash mismatch: "+name)
        manifests.append(dict(path=str(INPUT/name), sha256=expected, bytes=len(data), verified=True))
        documents[name] = json.loads(data)
    protocol = documents["performance-protocol-v1.json"]
    original = documents["performance-result-v1.json"]
    source_path = INPUT/"performance-inputs-v1/sync-user-bench.c"
    source = source_path.read_bytes()
    source_expected = [v for k, v in protocol["fixedInputs"].items() if k.endswith("/tests/sync/sync-user-bench.c")]
    require(len(source_expected) == 1 and digest(source) == source_expected[0], "Frozen C source hash mismatch")
    manifests.append(dict(path=str(source_path), sha256=digest(source), bytes=len(source), verified=True))
    require(len(original["runs"]) == len(protocol["queue"]) == 90, "Expected complete 90-run cohort")
    require(len(original["cells"]) == 15 and protocol["relativeGate"] == .03, "Original gate/cohort mismatch")
    raw_documents = []
    # Check ALL hashes first. Never import/execute the archived runner or validator.
    for i, run in enumerate(original["runs"]):
        raw_path = Path(run["rawResult"])
        data = raw_path.read_bytes()
        actual = digest(data)
        manifests.append(dict(run_index=i, path=str(raw_path), expected_sha256=run["rawSHA256"],
                              sha256=actual, bytes=len(data), verified=actual == run["rawSHA256"]))
        raw_documents.append(json.loads(data))
    require(all(m["verified"] for m in manifests), "Raw SHA256 mismatch; no analysis output written")
    require(len({r["rawResult"] for r in original["runs"]}) == 90, "Duplicate raw path")
    runs = []
    for i, (queue, run, raw) in enumerate(zip(protocol["queue"], original["runs"], raw_documents)):
        require(all(run[k] == v for k, v in queue.items()), "Queue mismatch at "+str(i))
        require(raw["passed"] is True and raw["timingDiscontinuity"] is False
                and raw["nativeSHA256"] == protocol["expectedNativeSHA256"]
                and raw["exeSHA256"] == protocol["exeSHA256"][run["arch"]], "Receipt identity mismatch")
        mode = raw["sync"]
        require(mode in MODES and (run["bottle"].endswith(" Server")) == (mode == "server"), "Mode mismatch")
        rows = raw["rows"]
        summary = [r for r in rows if r["type"] == "summary"]
        begin = [r for r in rows if r["type"] == "begin"]
        require(len(summary) == len(begin) == 1 and summary[0] == run["summary"], "Summary mismatch")
        require(begin[0]["mode"] == run["kind"] and begin[0]["threads"] == run["threads"], "Begin mismatch")
        batches = [interval(row, j) for j, row in enumerate(rows) if row["type"] == "batch"]
        require([(b["batch"], b["phase"]) for b in batches]
                == [(j, p) for j in range(4) for p in ("warmup", "measurement")], "Missing/reordered batch")
        phases = {p: phase_stats(batches, p) for p in ("warmup", "measurement")}
        require(close(phases["measurement"]["aggregate_ns_per_iteration"],
                      summary[0]["ns_per_worker_iteration"]), "Summary recomputation mismatch")
        comparisons = [dict(batch=j, measurement_vs_warmup_pct=pct(
            batches[2*j+1]["ns_per_worker_iteration"], batches[2*j]["ns_per_worker_iteration"])) for j in range(4)]
        runs.append(dict(run_index=i, raw_path=run["rawResult"], raw_sha256=run["rawSHA256"],
            run_id=raw["runID"], kind=run["kind"], threads=run["threads"], pair=run["pair"], mode=mode,
            wall_start_unix_seconds=raw["wallStartUnixSeconds"],
            original_summary=summary[0], receipt_rows=rows, intervals=batches,
            phases=phases, warmup_comparisons=comparisons,
            measurement_vs_warmup_aggregate_pct=pct(phases["measurement"]["aggregate_ns_per_iteration"],
                                                   phases["warmup"]["aggregate_ns_per_iteration"])))
    require(len({r["run_id"] for r in runs}) == 90, "Duplicate run ID")
    cells = []
    for old in original["cells"]:
        selected = [r for r in runs if (r["kind"], r["threads"]) == (old["kind"], old["threads"])]
        require(len(selected) == 6, "Missing cell run")
        by_mode = {}
        for mode in MODES:
            ms = [r for r in selected if r["mode"] == mode]
            values = [r["original_summary"]["ns_per_worker_iteration"] for r in ms]
            require(len(values) == 3 and values == old["values"][mode], "Cell values mismatch")
            by_mode[mode] = dict(run_indices=[r["run_index"] for r in ms], three_run_spread=spread(values),
                batch_spreads={p: spread(b["ns_per_worker_iteration"] for r in ms for b in r["intervals"]
                                        if b["phase"] == p) for p in ("warmup", "measurement")},
                measurement_vs_warmup_aggregate_pcts=[r["measurement_vs_warmup_aggregate_pct"] for r in ms])
            require(stats.median(values) == old["medians"][mode], "Median mismatch")
        require(old["relative"] == old["medians"]["msync"]/old["medians"]["server"]-1
                and old["within3Percent"] == (abs(old["relative"]) <= .03), "Original verdict mismatch")
        pairs = []
        for pair in range(3):
            ps = [r for r in selected if r["pair"] == pair]
            require(len(ps) == 2, "Missing pair")
            modes = [r["mode"] for r in ps]
            require(modes == (["msync", "server"] if pair == 1 else ["server", "msync"]), "Pair order mismatch")
            costs = {r["mode"]: r["original_summary"]["ns_per_worker_iteration"] for r in ps}
            pairs.append(dict(pair=pair, order="M/S" if pair == 1 else "S/M",
                run_indices=[r["run_index"] for r in ps], ns_per_iteration=costs,
                msync_minus_server_ns=costs["msync"]-costs["server"],
                msync_relative_pct=pct(costs["msync"], costs["server"]),
                adjacent_start_gap_seconds=ps[1]["wall_start_unix_seconds"]-ps[0]["wall_start_unix_seconds"]))
        deltas = [p["msync_relative_pct"] for p in pairs]
        cells.append(dict(kind=old["kind"], threads=old["threads"], original_cell=old,
            backends=by_mode, pairs=pairs,
            order_description=dict(reverse_minus_mean_forward_pp=deltas[1]-(deltas[0]+deltas[2])/2,
                second_forward_minus_first_forward_pp=deltas[2]-deltas[0],
                paired_delta_min_pct=min(deltas), paired_delta_max_pct=max(deltas),
                paired_delta_sign_changes=min(deltas) < 0 < max(deltas),
                same_order_last_vs_first_run_pct={m: pct(pairs[2]["ns_per_iteration"][m],
                                                        pairs[0]["ns_per_iteration"][m]) for m in MODES})))
    outside = sum(not c["original_cell"]["within3Percent"] for c in cells)
    require(outside == 8 and original["attributionAccepted"] is False
            and original["fullM1Accepted"] is False, "Original 8/15 failure verdict changed")
    notes = {
        "causal_attribution_supported": False,
        "reason": "QPC wall intervals combine executing, waiting and off-CPU time; fixed S/M,M/S,S/M order has only one reverse pair and no randomized independent replication. Spread, pair contrasts and warmup comparisons do not identify a mechanism or prove host noise.",
        "missing_measurement_source_point": "Frozen sync-user-bench.c worker_interval lines 366 and 376 bracket each hot loop with QPC only. Missing: same-interval per-worker native-thread CPU runtime and running/blocked/runnable timeline tied to runID/worker/native thread identity, with the timing provider verified as per-thread. GetThreadTimes is absent from these receipts; research85 warns that Wine macOS ThreadTimes may fall back to process CPU. Do not substitute that unverified value.",
        "finish_barrier_boundary": "Lines 379-380 spin at a finish barrier after each worker stops QPC. tail_after_own_end_ns is a wall-time opportunity for post-loop activity while slower peers remain measured, NOT measured barrier residency, CPU, interference, or cause. Actual arrival/exit and native execution state are missing.",
        "metric": "ns per completed worker iteration from min(begin) to max(end); no subtraction. Worker interval sums/intersections are wall interval geometry, not CPU, utilization, isolated API latency, kernel-park proof or wake percentiles.",
        "spread": "range_pct_of_median = 100*(max-min)/median, descriptive only; three whole-run samples per backend, four nested batches per run are not independent replicates.",
        "order": "All paired percentages use 100*(M/S-1), regardless of execution order. reverse_minus_mean_forward_pp is pair1 minus mean(pair0,pair2), in percentage points; same-order drift is pair2 minus pair0. These are descriptive, not corrected verdicts or estimates of an order effect.",
        "warmup": "measurement_vs_warmup_pct compares normalized envelope/iteration costs, not raw durations. Warmup always precedes measurement, uses fewer iterations, reuses workers and resets state. It cannot prove convergence or isolate warming from phase length/contention changes.",
        "preservation": "All 90 receipts' rows are copied with all original field values into receipt_rows, including checks, warmup, measurement, begin, summary and complete; all original cells and run summaries retained. Original file bytes remain unchanged. No run retried, excluded, rewritten or reweighted. No gate change or new benchmark.",
    }
    output = dict(schema="cider.u-offline-attribution/v1", analyzer_sha256=digest(Path(__file__).read_bytes()),
        inputs=manifests, raw_hashes_verified=90, original_verdict={k: original[k] for k in
        ("state", "executionPassed", "attributionAccepted", "fullM1Accepted")},
        counts=dict(runs=90, cells=15, outside_original_3_percent=outside, within_original_3_percent=15-outside,
                    batch_rows=sum(len(r["intervals"]) for r in runs),
                    worker_intervals=sum(len(b["workers"]) for r in runs for b in r["intervals"]),
                    receipt_rows=sum(len(r["receipt_rows"]) for r in runs)),
        notes=notes, cells=cells, runs=runs)
    report = render(output)
    (HERE/"analysis.json").write_text(json.dumps(output, indent=2, allow_nan=False)+"\n", encoding="utf-8")
    (HERE/"REPORT.md").write_text(report, encoding="utf-8")
    print(json.dumps(dict(raw_hashes_verified=90, original_outside_gate=outside,
                          causal_attribution_supported=False, output_directory=str(HERE))))


def render(out):
    lines = ["# Frozen U cohort: offline attribution analysis", "",
        "Reproduce with `python3 -B scripts/analyze-user-cohort.py --input out/sync-user-bench-20261024 --out <new-folder>`. Python standard library only. The script reads pinned protocol/result, frozen C source and their 90 referenced raw JSON receipts, then writes only `analysis.json` and this report beside itself. No archived code is executed.", "",
        "All 90 raw SHA256 values match. Frozen source SHA256: `ae2157e1f665ac82bc36acce4543a9330f54401bfa9257a4a42f90aca9834397`. Protocol/result/source and individual raw hashes are recorded in `analysis.json:inputs`.", "",
        "Original verdict reproduced exactly: **8/15 outside ±3%, 7/15 within; attributionAccepted=false; fullM1Accepted=false**. Execution passed in the original record. Observed variation does not turn any failure into a pass. All 15 cells, 90 runs, 720 batch rows and all worker intervals remain included.", "",
        "## Whole-run variation and existing pairs", "",
        "Costs are ns/completed worker iteration. S/M triplets list backend costs in pair index order 0,1,2; original delta uses ratio of backend medians. Spread columns are 100×(max−min)/median across three same-backend runs. Pair deltas all use 100×(M/S−1). R−F is reverse pair1 minus the mean of forward pairs0,2; F2−F0 is same-order drift, both in percentage points. These contrasts do not adjust the verdict.", "",
        "| Kind | Workers | S costs (0,1,2) | M costs (0,1,2) | S spread % | M spread % | Original Δ % / verdict | Pair Δ % (S/M, M/S, S/M) | R−F pp | F2−F0 pp |",
        "|---|---:|---|---|---:|---:|---|---|---:|---:|"]
    for c in out["cells"]:
        old, order = c["original_cell"], c["order_description"]
        bs = c["backends"]
        triplet = lambda vs: ", ".join(f"{v:.6g}" for v in vs)
        lines.append(f"| {c['kind']} | {c['threads']} | {triplet(old['values']['server'])} | {triplet(old['values']['msync'])} | "
            f"{bs['server']['three_run_spread']['range_pct_of_median']:.2f} | {bs['msync']['three_run_spread']['range_pct_of_median']:.2f} | "
            f"{old['relative']*100:+.2f} / {'within' if old['within3Percent'] else 'OUTSIDE'} | "
            f"{', '.join(format(p['msync_relative_pct'], '+.2f') for p in c['pairs'])} | "
            f"{order['reverse_minus_mean_forward_pp']:+.2f} | {order['second_forward_minus_first_forward_pp']:+.2f} |")
    flips = sum(c["order_description"]["paired_delta_sign_changes"] for c in out["cells"])
    lines += ["", f"Paired delta direction changes across the three records in {flips}/15 cells. The reverse pair appears only once per cell, and thread-count/kind cells run in fixed chronological blocks. Time drift, order, backend and workload history cannot be separated by these records. Large same-backend variation is evidence of variability, not its cause.", "",
        "## Warmup and measurement variation", "",
        "Each backend row below uses every one of its 12 warmup and 12 measurement batches. Ranges are normalized ns/iteration. Three Δ values compare aggregate measurement cost with aggregate warmup cost in each of the three runs. Full per-run four-batch spreads and four matched warmup/measurement comparisons are in `runs[*].phases` and `warmup_comparisons`.", "",
        "| Kind | Workers | Backend | Warmup min..max ns/iter | Measurement min..max ns/iter | Measurement vs warmup % (runs 0,1,2) |",
        "|---|---:|---|---|---|---|"]
    for c in out["cells"]:
        for mode in MODES:
            b = c["backends"][mode]
            w, m = (b["batch_spreads"][p] for p in ("warmup", "measurement"))
            lines.append(f"| {c['kind']} | {c['threads']} | {mode} | {w['minimum']:.6g}..{w['maximum']:.6g} | "
                f"{m['minimum']:.6g}..{m['maximum']:.6g} | "
                f"{', '.join(format(v, '+.2f') for v in b['measurement_vs_warmup_aggregate_pcts'])} |")
    lines += ["", out["notes"]["warmup"], "", "## Interval geometry and limits", "",
        "`runs[*].intervals` recomputes each batch envelope from every worker's raw QPC endpoints, verifies it against the receipt and summary, and retains per-worker offsets, duration, end tail, start/finish skew, common intersection and summed worker wall intervals. `receipt_row_index` points to the unchanged row values in `receipt_rows`.", "",
        out["notes"]["metric"], "",
        "| Phase | Batch envelope min..max ms | Largest worker start skew ms | Largest worker finish skew ms |",
        "|---|---|---:|---:|"]
    for phase in ("warmup", "measurement"):
        bs = [b for r in out["runs"] for b in r["intervals"] if b["phase"] == phase]
        lines.append(f"| {phase} | {min(b['envelope_ns'] for b in bs)/1e6:.6g}..{max(b['envelope_ns'] for b in bs)/1e6:.6g} | "
            f"{max(b['start_skew_ns'] for b in bs)/1e6:.6g} | {max(b['finish_skew_ns'] for b in bs)/1e6:.6g} |")
    measured = [(r, b) for r in out["runs"] for b in r["intervals"] if b["phase"] == "measurement"]
    r, b = max(measured, key=lambda rb: rb[1]["finish_skew_ns"])
    lines += ["", f"Largest measurement finish skew: run {r['run_index']} ({r['kind']}, {r['threads']} workers, {r['mode']}, "
        f"pair {r['pair']}), batch {b['batch']}: {b['finish_skew_ns']/1e6:.6g} ms within a {b['envelope_ns']/1e6:.6g} ms envelope. "
        "This is endpoint geometry, not evidence of the duration or cause of any scheduling delay.", "",
        out["notes"]["finish_barrier_boundary"], "",
        "## Causal conclusion and concrete missing observation", "",
        "**Current data does not support causal attribution.** "+out["notes"]["reason"], "",
        out["notes"]["missing_measurement_source_point"], "",
        "That observation would help distinguish executing work from off-CPU wall time; it would still need a controlled causal comparison. No scheduler/performance change or acceptance-gate change follows from this offline analysis.", "",
        out["notes"]["spread"], "", out["notes"]["preservation"], ""]
    return "\n".join(lines)


if __name__ == "__main__":
    main()
