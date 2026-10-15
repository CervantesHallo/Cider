"""Local regression fixtures only: never access the real ledger or a Windows host."""
import contextlib
import copy
from datetime import datetime
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest

SOURCE = Path(__file__).resolve().parents[2] / 'scripts/windows-reference-budget.py'
RID = '00000000-0000-4000-8000-000000000001'


def load(root):
    root = Path(root).resolve()
    # Workers must be confined to this suite's marked temporary directories,
    # including when someone invokes --worker by hand.
    if (not root.name.startswith('cider-budget-fix-')
            or not (root / '.fixture-only').is_file()):
        raise ValueError('Refusing a non-fixture budget directory.')
    spec = importlib.util.spec_from_file_location('budget_fixture', SOURCE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # Module import defines paths only; redirect before invoking any function.
    module.STATE_ROOT = Path(root)
    module.STATE_FILE = Path(root) / 'budget.json'
    class Clock(datetime):
        @classmethod
        def now(cls, tz=None):
            value = datetime.fromisoformat('2026-10-14T04:00:00+00:00')
            return value.astimezone(tz) if tz else value.replace(tzinfo=None)
    module.datetime = Clock
    return module


def invoke(module, argv):
    original = sys.argv
    sys.argv = [str(SOURCE)] + argv
    try:
        with contextlib.redirect_stdout(io.StringIO()) as out:
            module.main()
            return json.loads(out.getvalue())
    finally:
        sys.argv = original


class BudgetRegression(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='cider-budget-fix-')
        self.addCleanup(self.tmp.cleanup)
        (Path(self.tmp.name)/'.fixture-only').touch()
        self.m = load(self.tmp.name)
        self.path = Path(self.tmp.name) / 'receipt.json'
        self.state = self.m.initial_state()
        self.state['reservations'] = [dict(id=RID, phase='preparation', seconds=300,
            label='fixture', status='pending', attended_setup=True)]
        self.m.write_state(self.state)
        self.receipt()

    def receipt(self, **changes):
        value = dict(schema='cider.windows-run-cleanup/v1', reservation_id=RID,
            cleanup_confirmed=True, elapsed_seconds=330.987, cleanup_evidence={'verified': True})
        value.update(changes)
        self.path.write_text(json.dumps(value))

    def args(self, **changes):
        value = dict(id=RID, receipt=self.path, approved_elapsed_seconds=330.987,
            additional_seconds=3600, authorization_id='approved', reason='fixture approval', user_approved=True)
        value.update(changes)
        return types.SimpleNamespace(**value)

    def reconcile(self, **changes):
        with self.m.locked_state() as state:
            return self.m.reconcile_preparation(state, self.m.status(state), self.args(**changes))

    def digest(self):
        return hashlib.sha256(self.m.STATE_FILE.read_bytes()).hexdigest()

    def reject_unchanged(self, fn):
        before = self.digest()
        with self.assertRaises((ValueError, OSError)):
            fn()
        self.assertEqual(before, self.digest())

    def test_v2_charge_and_replay(self):
        first = self.reconcile()
        self.assertEqual(first['schema'], self.m.SCHEMA_V2)
        self.assertEqual(first['remaining_seconds']['preparation'], 6869)
        digest = self.digest()
        self.reconcile()
        self.assertEqual(digest, self.digest())
        state = json.loads(self.m.STATE_FILE.read_text())
        self.assertEqual(state['reservations'][0]['seconds'], 300)
        self.assertEqual(state['reservations'][0]['overrun_reconciliation']['extra_charged_seconds'], 31)
        self.assertEqual(len(state['extensions']), 1)
        self.receipt(elapsed_seconds=3900)
        self.reject_unchanged(lambda: self.reconcile(approved_elapsed_seconds=3900))

    def test_v1_read_only_then_atomic_upgrade(self):
        state = copy.deepcopy(self.state)
        state['schema'] = self.m.SCHEMA_V1
        self.m.STATE_FILE.write_text(json.dumps(state))
        before = self.digest()
        self.assertEqual(invoke(self.m, ['status'])['schema'], self.m.SCHEMA_V1)
        self.assertEqual(before, self.digest())
        self.reconcile()
        self.assertEqual(json.loads(self.m.STATE_FILE.read_text())['schema'], self.m.SCHEMA_V2)

    def test_v1_feature_records_refused(self):
        self.reconcile()
        state = json.loads(self.m.STATE_FILE.read_text())
        state['schema'] = self.m.SCHEMA_V1
        self.m.STATE_FILE.write_text(json.dumps(state))
        self.reject_unchanged(lambda: invoke(self.m, ['status']))

    def test_measurement_conflicts(self):
        for change in [dict(duration_measurement='unmeasured-attended-handoff', elapsed_seconds_upper_bound=300),
                       dict(duration_measurement='unknown'), dict(duration_measurement=[]),
                       dict(elapsed_seconds_upper_bound=300), dict(elapsed_seconds=True),
                       dict(elapsed_seconds=None), dict(elapsed_seconds=10**500),
                       dict(elapsed_seconds=float('nan')), dict(elapsed_seconds=float('inf'))]:
            with self.subTest(change=change):
                self.receipt(**change)
                self.reject_unchanged(self.reconcile)
        self.receipt(duration_measurement=None, elapsed_seconds_upper_bound=None)
        self.reconcile()

    def test_authorization_boundaries(self):
        for changes in [dict(user_approved=False), dict(additional_seconds=0),
                        dict(additional_seconds=3601), dict(additional_seconds=30),
                        dict(approved_elapsed_seconds=330.986), dict(approved_elapsed_seconds=float('nan')),
                        dict(approved_elapsed_seconds=float('inf')), dict(authorization_id='UPPER'),
                        dict(reason=''), dict(reason='x'*513)]:
            with self.subTest(changes=changes):
                self.reject_unchanged(lambda: self.reconcile(**changes))

    def test_close_duration_modes(self):
        self.receipt(elapsed_seconds=1, duration_measurement='unmeasured-attended-handoff',
                     elapsed_seconds_upper_bound=300)
        self.reject_unchanged(lambda: invoke(self.m, ['close', '--id', RID, '--receipt', str(self.path)]))
        self.receipt(elapsed_seconds=None, duration_measurement='unmeasured-attended-handoff',
                     elapsed_seconds_upper_bound=300)
        invoke(self.m, ['close', '--id', RID, '--receipt', str(self.path)])
        with self.m.locked_state() as state:
            self.assertIsNone(state['reservations'][0]['observed_elapsed_seconds'])
        self.reject_unchanged(lambda: invoke(self.m, ['close', '--id', RID, '--receipt', str(self.path)]))

    def test_legacy_numeric_close(self):
        self.receipt(elapsed_seconds=1, cleanup_evidence=None)
        invoke(self.m, ['close', '--id', RID, '--receipt', str(self.path)])
        with self.m.locked_state() as state:
            self.assertEqual(state['reservations'][0]['observed_elapsed_seconds'], 1)

    def test_corrupt_linkage_rejected(self):
        self.reconcile()
        good = json.loads(self.m.STATE_FILE.read_text())
        mutations = [
            lambda s: s['reservations'][0]['overrun_reconciliation'].update(authorization_id=[]),
            lambda s: s['reservations'][0]['overrun_reconciliation'].update(extra_charged_seconds=True),
            lambda s: s['reservations'][0].update(observed_elapsed_seconds=10**500),
            lambda s: s['reservations'][0].update(duration_measurement='unmeasured-attended-handoff'),
            lambda s: s['extensions'][0].update(receipt_sha256='0'*64),
            lambda s: s['extensions'][0].pop('overrun_reservation_id'),
            lambda s: s['reservations'][0].pop('overrun_reconciliation'),
            lambda s: (s['reservations'][0].pop('overrun_reconciliation'), s['extensions'][0].pop('overrun_reservation_id')),
            lambda s: s['reservations'].append(None), lambda s: s['extensions'].append(None),
            lambda s: s['reservations'][0].update(phase=[]),
            lambda s: s['extensions'][0].update(phase=[]),
        ]
        for i, mutation in enumerate(mutations):
            with self.subTest(case=i):
                state = copy.deepcopy(good)
                mutation(state)
                self.m.STATE_FILE.write_text(json.dumps(state))
                self.reject_unchanged(lambda: invoke(self.m, ['status']))

    def test_receipt_descriptor_bounds_and_shape(self):
        for payload in ('[]', '{', '{"cleanup_confirmed":false,"cleanup_confirmed":true}', ' '*65537):
            with self.subTest(payload=payload[:50]):
                self.path.write_text(payload)
                self.reject_unchanged(self.reconcile)
        self.receipt()
        target = self.path.with_suffix('.target')
        self.path.rename(target)
        self.path.symlink_to(target)
        self.reject_unchanged(self.reconcile)
        self.path.unlink()
        os.mkfifo(self.path)
        self.reject_unchanged(self.reconcile)

    def test_size_limit_before_publish(self):
        with self.m.locked_state() as state:
            state['padding'] = 'x'*(self.m.LEDGER_MAX_BYTES-1000)
            self.m.write_state(state)
        self.reject_unchanged(lambda: self.m.write_state({**state, 'padding': 'x'*self.m.LEDGER_MAX_BYTES}))
        self.assertEqual(invoke(self.m, ['status'])['pending'], [RID])

    def test_stop_inserted_during_receipt_read(self):
        original = self.m.read_receipt
        def read_then_stop(*args):
            result = original(*args)
            (self.m.STATE_ROOT/'STOP').touch()
            return result
        self.m.read_receipt = read_then_stop
        self.reject_unchanged(self.reconcile)

    def test_busy_and_stop_still_prevent_reserve(self):
        self.reconcile()
        availability = self.m.STATE_ROOT/'windows-availability.json'
        availability.write_text(json.dumps({'schema':'cider.windows-availability/v1','user_busy':True}))
        self.reject_unchanged(lambda: invoke(self.m, ['reserve','--phase','preparation','--seconds','1',
                                                    '--label','fixture','--attended-setup']))
        availability.unlink()
        (self.m.STATE_ROOT/'STOP').symlink_to(self.m.STATE_ROOT/'absent')
        self.reject_unchanged(lambda: invoke(self.m, ['reserve','--phase','preparation','--seconds','1',
                                                    '--label','fixture','--attended-setup']))

    def test_concurrent_reconciliation(self):
        command = [sys.executable, '-B', str(Path(__file__).resolve()), '--worker', str(self.m.STATE_ROOT)]
        children = [subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) for _ in range(8)]
        for child in children:
            out, error = child.communicate(timeout=20)
            self.assertEqual(child.returncode, 0, error)
            self.assertEqual(json.loads(out)['remaining_seconds']['preparation'], 6869)
        state = json.loads(self.m.STATE_FILE.read_text())
        self.assertEqual(len(state['extensions']), 1)

    def test_concurrent_different_authorizations(self):
        command = [sys.executable, '-B', str(Path(__file__).resolve()), '--worker', str(self.m.STATE_ROOT)]
        children = [subprocess.Popen(command+[f'approved-{i}'], stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE, text=True) for i in range(8)]
        successes = 0
        for child in children:
            out, error = child.communicate(timeout=20)
            if child.returncode == 0:
                successes += 1
                self.assertEqual(json.loads(out)['remaining_seconds']['preparation'], 6869)
            else:
                self.assertIn('refusing reconciliation', error)
        self.assertEqual(successes, 1)
        self.assertEqual(len(json.loads(self.m.STATE_FILE.read_text())['extensions']), 1)

    def test_replace_failure_keeps_pending(self):
        original = self.m.os
        proxy = types.SimpleNamespace(**{name:getattr(os,name) for name in dir(os)})
        self.m.os = proxy
        proxy.replace = lambda *args: (_ for _ in ()).throw(OSError('injected'))
        self.reject_unchanged(self.reconcile)
        self.m.os = original
        self.reconcile()
        self.assertFalse(list(self.m.STATE_ROOT.glob('.budget-*')))

    def test_directory_sync_failure_reports_uncertain_commit(self):
        proxy = types.SimpleNamespace(**{name:getattr(os,name) for name in dir(os)})
        self.m.os = proxy
        calls = 0
        def fsync(fd):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError('injected post-replace sync failure')
            return os.fsync(fd)
        proxy.fsync = fsync
        with self.assertRaises(self.m.CommitUnconfirmedError) as caught:
            self.reconcile()
        self.assertEqual(caught.exception.result['status'], 'commit-unconfirmed')
        self.assertFalse(caught.exception.result['windows_work_permitted'])
        self.assertEqual(len(json.loads(self.m.STATE_FILE.read_text())['extensions']), 1)
        self.m.os = os
        digest = self.digest()
        self.reconcile()
        self.assertEqual(digest, self.digest())

    def test_late_availability_failure_reports_actual_commit(self):
        original = self.m.write_state
        def write_then_corrupt(state):
            original(state)
            (self.m.STATE_ROOT/'windows-availability.json').write_text('{"schema":"unknown"}')
        self.m.write_state = write_then_corrupt
        result = self.reconcile()
        self.assertEqual(result['status'], 'committed')
        self.assertFalse(result['windows_work_permitted'])
        self.assertEqual(result['pending'], [])
        self.assertEqual(len(json.loads(self.m.STATE_FILE.read_text())['extensions']), 1)

    def test_absent_ledger_rejected_close_and_status_do_not_initialize(self):
        self.m.STATE_FILE.unlink()
        with self.assertRaises(ValueError):
            invoke(self.m, ['close','--id',RID,'--receipt',str(self.path)])
        self.assertFalse(self.m.STATE_FILE.exists())
        self.assertEqual(invoke(self.m, ['status'])['pending'], [])
        self.assertFalse(self.m.STATE_FILE.exists())


if __name__ == '__main__':
    if len(sys.argv) in (3, 4) and sys.argv[1] == '--worker':
        m = load(sys.argv[2])
        path = m.STATE_ROOT / 'receipt.json'
        args = types.SimpleNamespace(id=RID, receipt=path, approved_elapsed_seconds=330.987,
            additional_seconds=3600, authorization_id=sys.argv[3] if len(sys.argv) == 4 else 'approved',
            reason='fixture approval', user_approved=True)
        with m.locked_state() as state:
            print(json.dumps(m.reconcile_preparation(state, m.status(state), args)))
    else:
        unittest.main()
