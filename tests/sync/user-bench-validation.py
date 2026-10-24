"""Validate complete raw U-group receipts; never infer benchmark gates here."""
from collections import Counter
import math


def close(actual, expected):
    return (type(actual) in (int, float) and math.isfinite(actual)
            and abs(actual - expected) <= max(.002, abs(expected) * 1e-8))


def valid_user_bench(rows, arch, kind, iterations, threads, batches):
    try:
        return _validate(rows, arch, kind, iterations, threads, batches)
    except (KeyError, TypeError, ValueError, ZeroDivisionError):
        return False


def _validate(rows, arch, kind, iterations, threads, batches):
    def only(t):
        values = [r for r in rows if r.get('type') == t]
        if len(values) != 1:
            raise ValueError('Missing or duplicate ' + t)
        return values[0]
    begin, summary, complete = only('begin'), only('summary'), only('complete')
    bits = 32 if arch == 'i686' else 64
    warmup = min(iterations, dict(cs=32768, srw=32768, cv=512, address=1024, once=262144)[kind])
    if not (begin['benchmark'] == 'sync_user' and begin['mode'] == kind
            and begin['pointer_bits'] == bits and begin['threads'] == threads
            and begin['batches_requested'] == batches
            and begin['iterations_per_worker_per_batch'] == iterations
            and begin['warmup_iterations_per_worker_per_batch'] == warmup
            and begin['worker_zero_is_main_thread'] is True
            and begin['warmup_reuses_measured_workers'] is True
            and begin['latency_percentiles'] is False and begin['subtraction'] is False
            and type(begin['qpc_frequency']) is int and begin['qpc_frequency'] > 0):
        return False
    checks = [r for r in rows if r.get('type') == 'check']
    expected = Counter({'QPC.frequency': 1, 'worker.exit': batches * (threads - 1),
                        'phase.absolute_deadline': 2 * batches,
                        'phase.not_aborted': 2 * batches,
                        'worker.error_stage': 2 * batches * threads,
                        'worker.completed': 2 * batches * threads})
    if kind in ('cs', 'cv'):
        expected['setup.initialize_cs'] = batches
    if kind in ('cs', 'srw'):
        expected.update({'locks.final_sequence': 2 * batches, 'locks.final_checkword': 2 * batches})
    elif kind == 'cv':
        expected.update({name: 2 * batches for name in ['cv.produced', 'cv.consumed', 'cv.empty_after_join']})
    elif kind == 'address':
        expected.update({'address.apis_available_no_skip': 1, 'address.final_turn': batches * threads,
                         'address.final_sequence': batches * threads, 'address.wait_api_exercised': 2 * batches})
    else:
        expected.update({'setup.once_initialized_before_warmup': batches, 'once.callback_exactly_once': 2 * batches})
    if not (all(type(r.get('name')) is str and r.get('ok') is True
                and type(r.get('actual')) is int and type(r.get('expected')) is int for r in checks)
            and Counter(r['name'] for r in checks) == expected
            and type(complete['checks']) is int and complete['checks'] == len(checks)
            and complete['failures'] == 0 and complete['ok'] is True
            and complete['output_error'] is False and complete['worker_cleanup'] == 'joined'
            and complete['batches_completed'] == batches):
        return False
    batch_rows = [r for r in rows if r.get('type') == 'batch']
    if [(r.get('batch'), r.get('phase')) for r in batch_rows] != [
            (i, phase) for i in range(batches) for phase in ['warmup', 'measurement']]:
        return False
    total_ticks = total_units = total_calls = 0
    for row in batch_rows:
        count = warmup if row['phase'] == 'warmup' else iterations
        workers = row['workers']
        if (row['ok'] is not True or row['threads'] != threads or len(workers) != threads
                or row['iterations_per_worker'] != count or row['qpc_frequency'] != begin['qpc_frequency']
                or row['timing_scope'] != 'worker_loop_envelope_excludes_gates_and_joins'
                or row['phase_completion_tick_ms'] >= row['phase_deadline_tick_ms']):
            return False
        for i, w in enumerate(workers):
            if not (w['index'] == i and w['completed'] == count and w['error_stage'] == 0
                    and w['win32_error'] == 0 and w['containment_wake_calls'] == 0
                    and type(w['qpc_begin']) is int and type(w['qpc_end']) is int
                    and w['qpc_end'] > w['qpc_begin']
                    and w['qpc_ticks'] == w['qpc_end'] - w['qpc_begin']):
                return False
            expected_calls = (count if kind in ['cs', 'srw', 'cv'] else 0)
            if w['acquire_calls'] != expected_calls or w['release_calls'] != expected_calls:
                return False
            if w['once_calls'] != (count if kind == 'once' else 0):
                return False
            if w['wake_calls'] != (count if kind in ['cv', 'address'] else 0):
                return False
            if any(type(w[k]) is not int or w[k] < 0 for k in ['acquire_calls', 'release_calls',
                    'once_calls', 'wake_calls', 'wait_calls', 'timeout_returns']):
                return False
            if w['timeout_returns'] > w['wait_calls']:
                return False
            if kind not in ['cv', 'address'] and (w['wait_calls'] or w['timeout_returns']):
                return False
        first, last = min(w['qpc_begin'] for w in workers), max(w['qpc_end'] for w in workers)
        ticks, units = last - first, threads * count
        calls = sum(w[k] for w in workers for k in ['acquire_calls', 'release_calls', 'wait_calls', 'wake_calls', 'once_calls'])
        ns = ticks * 1e9 / begin['qpc_frequency']
        if not (row['qpc_begin_min'] == first and row['qpc_end_max'] == last
                and row['qpc_envelope_ticks'] == ticks
                and row['qpc_worker_ticks_sum'] == sum(w['qpc_ticks'] for w in workers)
                and row['completed_worker_iterations'] == units and row['primitive_api_calls'] == calls
                and close(row['envelope_ns'], ns) and close(row['ns_per_worker_iteration'], ns / units)
                and close(row['ns_per_primitive_api_call'], ns / calls)):
            return False
        if any(row[key] != sum(w[worker_key] for w in workers) for key, worker_key in [
                ('predicate_wait_calls', 'wait_calls'), ('wake_api_calls', 'wake_calls'),
                ('timeout_returns', 'timeout_returns')]):
            return False
        sequence = count * (threads if kind == 'cs' else threads // 2) if kind in ['cs', 'srw'] else 0
        transfers = count * (threads // 2) if kind in ['cv', 'address'] else 0
        produced = count * (threads // 2) if kind == 'cv' else 0
        if not (row['protected_sequence'] == sequence
                and row['transfer_or_roundtrip_count'] == transfers
                and row['produced'] == row['consumed'] == produced and row['slot_full'] == 0
                and row['once_callback_count'] == (1 if kind == 'once' else 0)
                and type(row['cs_peer_switches']) is int
                and (0 < row['cs_peer_switches'] < units if kind == 'cs' else row['cs_peer_switches'] == 0)):
            return False
        pairs = row['address_pairs']
        if kind == 'address':
            if (row['predicate_wait_calls'] <= 0 or len(pairs) != threads // 2
                    or any(p['index'] != i or p['sequence'] != 2 * count or p['turn'] != 0
                           for i, p in enumerate(pairs))):
                return False
        elif pairs != []:
            return False
        if row['phase'] == 'measurement':
            total_ticks += ticks
            total_units += units
            total_calls += calls
    return (summary['ok'] is True and summary['mode'] == kind
            and summary['batches_requested'] == summary['batches_completed'] == batches
            and summary['all_measurement_rows_retained'] is True
            and summary['qpc_frequency'] == begin['qpc_frequency']
            and summary['measurement_envelope_ticks_sum'] == total_ticks
            and summary['completed_worker_iterations'] == total_units
            and summary['primitive_api_calls'] == total_calls
            and close(summary['sum_of_measurement_envelopes_ns'], total_ticks * 1e9 / begin['qpc_frequency'])
            and close(summary['ns_per_worker_iteration'], total_ticks * 1e9 / begin['qpc_frequency'] / total_units)
            and close(summary['ns_per_primitive_api_call'], total_ticks * 1e9 / begin['qpc_frequency'] / total_calls))
