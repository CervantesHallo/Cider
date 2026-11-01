"""Four-phase adaptation of renderer_consumer_oracle_v1.py.

qualify() consumes one exact session's mixed stdout/native log. It does not
capture PNGs or issue gates. The parent must bind three actual screenshots and
the native-local-freed evidence before copying each ready token to its gate.
qualify_token() is available to that controller for the unchanged 64-byte ABI.
"""

import json
import re
import struct


def _require(condition, detail):
    # Keep qualification effective under python -O as well.
    if not condition:
        raise AssertionError(detail)


def _records(text):
    decoder = json.JSONDecoder()
    events = []
    for line in text.splitlines():
        if not line.startswith('{"type":"renderer-consumer"'):
            continue
        rest = line
        while rest:
            value, end = decoder.raw_decode(rest)
            _require(value['type'] == 'renderer-consumer', value)
            _require(value['role'] in ('host', 'renderer'), value)
            events.append(value)
            rest = rest[end:].lstrip()
    return events


def _selected(data, label, phase=None):
    return [v for v in data if v['label'] == label and
            (phase is None or v['phase'] == phase)]


def _one(data, label, phase=None):
    found = _selected(data, label, phase)
    _require(len(found) == 1, (label, phase, found))
    return found[0]


def _root(identity, phase):
    return identity['root2'] if phase == 2 else identity['root1']


def qualify_token(raw, phase, nonce, identity):
    """Validate ready/gate bytes against qualified actual host/renderer identity.

    identity may be a host record after its renderer-birth check. The parent
    must independently bind that record to its owned session, preserve ready
    bytes, and publish the exact same bytes only after real phase evidence.
    """
    _require(phase in (1, 2, 3, 4), phase)
    _require(re.fullmatch('[0-9a-f]{32}', nonce) is not None, nonce)
    _require(identity['role'] == 'host' and identity['nonce'] == nonce and
             identity['pid'] == identity['host_pid'] > 0 and
             identity['renderer_pid'] > 0 and identity['host_pid'] != identity['renderer_pid'] and
             identity['self_birth'] > 0 and identity['peer_birth'] >= identity['self_birth'], identity)
    user_ids = [identity[k] for k in ('root1', 'root2', 'child_a', 'child_b')]
    _require(len(set(user_ids)) == 4 and all(0 < n < 0xffffffff for n in user_ids), identity)
    _require(len(raw) == 64, len(raw))
    words = struct.unpack('<8I2Q4I', raw)
    expected = (0x43465231, 64, 1, phase, _root(identity, phase),
                identity['host_pid'], identity['renderer_pid'], phase if phase < 4 else 0,
                int(nonce[:16], 16), int(nonce[16:], 16),
                identity['child_a'], identity['child_b'], 160, 100)
    _require(words == expected, (words, expected))
    return dict(phase=phase, pattern=words[7], root=words[4], words=list(words))


def _native_user_id(text):
    value = int(text, 16)
    low = value & 0xffffffff
    extended = low | 0xffffffff00000000 if low & 0x80000000 else low
    _require(low not in (0, 0xffffffff) and value in (low, extended), text)
    return low


def qualify(text, arch, peer, nonce, exit_code):
    _require({arch, peer} == {'x86_64', 'i386'}, (arch, peer))
    _require(re.fullmatch('[0-9a-f]{32}', nonce) is not None, nonce)
    events = _records(text)
    summaries, by_role = {}, {}
    for role, architecture in (('host', arch), ('renderer', peer)):
        data = [v for v in events if v['role'] == role]
        summary = [v for v in data if v['event'] == 'summary']
        _require(len(summary) == 1 and data[-1] == summary[0], (role, summary))
        sm = summary[0]
        summaries[role], by_role[role] = sm, data
        _require(sm['ok'] is True and all(v['ok'] is True and v['failures'] == 0 for v in data), data)
        _require([v['check'] for v in data] == list(range(1, len(data) + 1)), role)
        _require(all(v['checks'] == v['check'] for v in data) and sm['checks'] == len(data), role)
        _require(all(v['nonce'] == nonce and v['pid'] == sm['pid'] and
                     v['bits'] == (64 if architecture == 'x86_64' else 32) for v in data), role)
        _require(sm['phases'] == sm['pipe_sends'] == sm['pipe_receives'] == 4, sm)
        _require(all(not v[k] for v in data for k in
                     ('io_error', 'clock_error', 'timeout', 'force_attempted', 'force_succeeded')), role)
        _require([v['elapsed_ms'] for v in data] == sorted(v['elapsed_ms'] for v in data) and
                 sm['elapsed_ms'] < 180000, role)
        _require([v['phase'] for v in data] == sorted(v['phase'] for v in data) and
                 all(v['phase'] in (0, 1, 2, 3, 4) for v in data), role)

    h, p = summaries['host'], summaries['renderer']
    host, renderer = by_role['host'], by_role['renderer']
    _require(h['pid'] == h['host_pid'] == p['host_pid'] and
             p['pid'] == p['renderer_pid'] == h['renderer_pid'] and h['pid'] != p['pid'], (h, p))
    _require(0 < h['self_birth'] == p['peer_birth'] <= p['self_birth'] == h['peer_birth'], (h, p))
    identity_keys = ('root1', 'root2', 'child_a', 'child_b', 'host_pid', 'renderer_pid')
    _require(all(h[k] == p[k] for k in identity_keys), (h, p))
    user_ids = [h[k] for k in ('root1', 'root2', 'child_a', 'child_b')]
    _require(len(set(user_ids)) == 4 and all(0 < n < 0xffffffff for n in user_ids), user_ids)
    for role, data in by_role.items():
        sm = summaries[role]
        for v in data:
            _require(all(v[k] in (0, h[k]) for k in identity_keys), v)
            _require(v['self_birth'] in (0, sm['self_birth']) and
                     v['peer_birth'] in (0, sm['peer_birth']), v)
            if v['phase']:
                _require(v['root'] == _root(h, v['phase']) and
                         all(v[k] == h[k] for k in identity_keys) and
                         v['self_birth'] == sm['self_birth'] and v['peer_birth'] == sm['peer_birth'], v)
            if v['label'] == 'unchanged-process-identities':
                _require(v['raw'] == [sm['self_birth'], sm['peer_birth'],
                         p['pid'] if role == 'host' else h['pid'],
                         p['pid'] if role == 'host' else h['pid']], v)
    _require(h['root'] == p['root'] == h['root1'], (h, p))
    _require(h['peer_frame_reports'] == 3 and
             all(h[k] == 6 for k in ('peer_frames_reported', 'peer_readbacks_reported', 'peer_presents_reported')) and
             h['peer_local_release_reported'] and h['kernel_signaled'] and h['joined_zero'] and h['exit_code'] == 0, h)
    _require(p['swapchains'] == 2 and all(p[k] == 6 for k in ('frames', 'readbacks', 'presents')) and
             p['local_com_released'] and p['gpu_drained'] and p['com_release_calls'] == 14, p)

    device = _one(renderer, 'D3D11CreateDevice-hardware-one-device', 0)
    swaps = _selected(renderer, 'CreateSwapChain-foreign-child')
    _require(len(swaps) == 2 and [v['raw'][:2] for v in swaps] ==
             [[0, h['child_a']], [1, h['child_b']]] and all(v['phase'] == 0 for v in swaps), swaps)
    owned = _selected(renderer, 'phase-owned-device-and-two-swapchains')
    _require([v['phase'] for v in owned] == [1, 2, 3] and
             all(v['raw'] == owned[0]['raw'] and all(v['raw']) and
                 v['raw'][:2] == device['raw'][2:] and v['com_release_calls'] == 0 for v in owned) and
             owned[0]['raw'][2] != owned[0]['raw'][3], owned)
    releases = [v for v in renderer if v['event'] == 'com-release']
    _require(len(releases) == 15 and all(v['phase'] == 4 for v in releases), releases)
    _require([v['phase'] for v in _selected(host, 'screen-ready')] == [1, 2, 3] and
             [v['phase'] for v in _selected(host, 'native-drain-ready')] == [4], 'three screen readies and one drain ready')

    for phase in (1, 2, 3, 4):
        actual_root, pattern = _root(h, phase), phase if phase < 4 else 0
        flags = 15 if phase < 4 else 16
        expected_root = _one(host, 'phase-expected-root', phase)
        previous_root = h['root2'] if phase == 3 else h['root1']
        _require(expected_root['raw'] == [phase, previous_root, actual_root, 1 if phase == 2 else 0], expected_root)
        for role, data in by_role.items():
            token = _one(data, 'phase-report-token-root-pattern', phase)
            sent = _one(data, 'write-one-message', phase)
            received = _one(data, 'validate-complete-message', phase)
            _require(token['raw'] == [phase, pattern, actual_root, 64], token)
            _require(sent['raw'] == [80, phase, flags, 0] and
                     received['raw'] == [phase, flags, 80, phase], (sent, received))
            _require(token['check'] < sent['check'], role)
            if phase < 4:
                _require(token['com_release_calls'] == 0 and not token['local_com_released'], token)
        gate = _one(host, 'parent-token-accepted', phase)
        ready = _one(host, 'ready-write-token64', phase)
        announced = _one(host, 'screen-ready' if phase < 4 else 'native-drain-ready', phase)
        _require(announced['raw'] == [phase, pattern, actual_root, 64], announced)
        _require(ready['raw'] == [64, 64, 0, phase] and
                 gate['raw'][:2] == [phase, pattern] and gate['raw'][2] < 45000 and gate['raw'][3] == 64, gate)
        _require(expected_root['check'] < _one(host, 'validate-complete-message', phase)['check'] <
                 ready['check'] < announced['check'] < gate['check'] < _one(host, 'write-one-message', phase)['check'], phase)
        host_done = _one(host, 'phase-echo-sent-before-next-root', phase)
        _require(host_done['raw'] == [phase, actual_root, phase, 0] and
                 _one(host, 'write-one-message', phase)['check'] < host_done['check'], host_done)
        if phase > 1:
            _require(_one(host, 'phase-echo-sent-before-next-root', phase - 1)['check'] < expected_root['check'], phase)
        _require(_one(host, 'validate-complete-message', phase)['hr'] ==
                 _one(renderer, 'validate-complete-message', phase)['hr'], phase)
        for role, data in by_role.items():
            title = _selected(data, 'unique-canonical-capture-title', phase)
            _require(title and all(v['raw'][0] == 1 and
                     (v['raw'][1] & 0xffffffff) == actual_root and v['raw'][2] == actual_root for v in title), (role, phase))
            _require(_selected(data, 'unchanged-process-identities', phase), (role, phase))
            owners = _selected(data, 'both-roots-owner-is-host', phase)
            clients = _selected(data, 'both-roots-client-440x240', phase)
            parents = _selected(data, 'actual-child-parent', phase)
            ancestors = _selected(data, 'actual-child-root-ancestor', phase)
            child_owners = _selected(data, 'child-owner-is-host', phase)
            _require({v['raw'][0] for v in owners} == {h['root1'], h['root2']} and
                     all(v['raw'][1:3] == [h['pid'], h['pid']] for v in owners), owners)
            _require({v['raw'][0] for v in clients} == {h['root1'], h['root2']} and
                     all(v['raw'][1:] == [440, 240, 0] for v in clients), clients)
            _require({v['raw'][2] for v in parents} == {0, 1} and
                     {v['raw'][2] for v in ancestors} == {0, 1} and
                     all((v['raw'][0] & 0xffffffff) == v['raw'][1] == actual_root for v in parents + ancestors), (role, parents))
            _require({v['raw'][3] for v in child_owners} == {0, 1} and
                     all(v['raw'][0] == v['raw'][1] == h['pid'] and v['raw'][2] for v in child_owners), child_owners)
        if phase == 4:
            continue
        echo = _one(renderer, 'reply-exactly-echoes-actual-report', phase)
        done = _one(renderer, 'phase-echo-complete-before-next-root', phase)
        _require(echo['raw'][:2] == [phase, 15] and done['raw'] == [phase, actual_root, phase, 0] and
                 echo['check'] < done['check'] and done['com_release_calls'] == 0, done)
        clears = _selected(renderer, 'actual-clear-RTV', phase)
        presents = _selected(renderer, 'actual-Present-HRESULT', phase)
        maps = _selected(renderer, 'actual-Map-READ', phase)
        _require(len(clears) == len(presents) == len(maps) == 2, phase)
        for index, child in enumerate((h['child_a'], h['child_b'])):
            color = 0xffff0000 if phase == 3 or ((phase != 2) == (index == 0)) else 0xff0000ff
            back = _one(renderer, 'readback-' + ('B' if index else 'A') + '-all-16000-and-Unmap', phase)
            _require(clears[index]['raw'] == [index, color, phase, 2 * (phase - 1) + index + 1], clears)
            _require(back['raw'][:3] == [color, 16000, 0] and 640 <= back['raw'][3] <= 1024 * 1024, back)
            _require(maps[index]['raw'][0] == index and maps[index]['raw'][2] != 0 and maps[index]['raw'][3] == 1, maps)
            _require(presents[index]['raw'][:2] == [index, phase] and presents[index]['hr'] >= 0 and
                     clears[index]['check'] < maps[index]['check'] < back['check'] < presents[index]['check'] <
                     _one(renderer, 'write-one-message', phase)['check'] < echo['check'], child)

    _require(len(_selected(renderer, 'ordinary-SetParent-returned-old-parent')) == 4 and
             not _selected(host, 'ordinary-SetParent-returned-old-parent'), 'exactly four renderer SetParent calls')
    for phase, previous_root in ((2, h['root1']), (3, h['root2'])):
        actual_root = _root(h, phase)
        labels = ('phase-expected-root', 'old-root-remove-canonical-title',
                  'old-root-title-removed-before-new-title', 'old-root-ShowWindow-hide-return',
                  'old-root-actually-hidden', 'new-root-assign-canonical-title',
                  'new-root-ShowWindow-show-return', 'new-root-actually-visible')
        transition = [_one(renderer, label, phase) for label in labels]
        _require([v['check'] for v in transition] == sorted(v['check'] for v in transition), transition)
        _require(transition[0]['raw'] == [phase, previous_root, actual_root, 1 if phase == 2 else 0], transition)
        before = _selected(renderer, 'before-SetParent-actual-old-parent', phase)
        moved = _selected(renderer, 'ordinary-SetParent-returned-old-parent', phase)
        parents = _selected(renderer, 'after-SetParent-actual-new-parent', phase)
        ancestors = _selected(renderer, 'after-SetParent-actual-new-ancestor', phase)
        slots = _selected(renderer, 'fixed-child-slot-without-resize', phase)
        _require(all(len(v) == 2 for v in (before, moved, parents, ancestors, slots)), phase)
        prior_echo = _one(renderer, 'phase-echo-complete-before-next-root', phase - 1)
        _require(prior_echo['check'] < transition[0]['check'], phase)
        for index, child in enumerate((h['child_a'], h['child_b'])):
            _require(before[index]['raw'][0] == moved[index]['raw'][0] == parents[index]['raw'][0] ==
                     ancestors[index]['raw'][0] == slots[index]['raw'][0] == child, child)
            _require((before[index]['raw'][1] & 0xffffffff) == (moved[index]['raw'][1] & 0xffffffff) == previous_root and
                     moved[index]['raw'][2] == actual_root and moved[index]['hr'] == 0, moved[index])
            _require((parents[index]['raw'][1] & 0xffffffff) ==
                     (ancestors[index]['raw'][1] & 0xffffffff) == actual_root, child)
            _require(transition[-1]['check'] < before[index]['check'] < moved[index]['check'] < parents[index]['check'] <
                     ancestors[index]['check'] < slots[index]['check'] < _one(renderer, 'phase-owned-device-and-two-swapchains', phase)['check'], child)

    release = _one(renderer, 'all-local-pointers-released-only', 4)
    release_echo = _one(renderer, 'release-reply-exact-echo', 4)
    _require(release['local_com_released'] and release['gpu_drained'] and release['raw'] == [14, 1, 1, 0] and
             release['check'] < _one(renderer, 'write-one-message', 4)['check'] < release_echo['check'] and
             release_echo['raw'][:2] == [4, 16], release)
    _require(len(_selected(renderer, 'actual-event-completion')) == 7, 'six frame events plus final GPU drain')
    for index, suffix in enumerate(('A', 'B')):
        destroyed = _one(host, 'destroy-child-' + suffix + '-while-renderer-alive', 4)
        _require(destroyed['raw'] == [h['child_a' if index == 0 else 'child_b'], 1, 0, 0] and
                 destroyed['peer_local_release_reported'] and
                 _one(host, 'validate-complete-message', 4)['check'] < destroyed['check'] <
                 _one(host, 'ready-write-token64', 4)['check'], destroyed)
    for role, data in by_role.items():
        absent = _selected(data, 'child-actually-absent', 4)
        _require({v['raw'][0] for v in absent} == {h['child_a'], h['child_b']}, (role, absent))
        empty = _one(data, 'exactly-four-messages', 4)
        _require(empty['raw'] == [0, 3 if role == 'host' else 4, 4, 0], empty)
    join = _one(host, 'actual-PI-wait-and-exit', 4)
    _require(join['raw'] == [0, 0, p['pid'], 0] and
             _one(host, 'write-one-message', 4)['check'] < join['check'], join)
    for number in (1, 2):
        destroyed = _one(host, 'destroy-owned-root' + str(number), 4)
        _require(destroyed['raw'] == [h['root' + str(number)], 1, 0, 0] and
                 destroyed['check'] > join['check'], destroyed)

    active = [(_native_user_id(a), int(b)) for a, b in re.findall(
        r'renderer client active hwnd (0x[0-9a-fA-F]+) context (\d+)', text)]
    freed = [_native_user_id(a) for a in re.findall(
        r'renderer client freed hwnd (0x[0-9a-fA-F]+) context0 routes0', text)]
    _require(len(active) == 2 and {a for a, _ in active} == {h['child_a'], h['child_b']} and
             len({b for _, b in active}) == 2 and all(b > 0 for _, b in active), active)
    _require(len(freed) == 2 and set(freed) == {h['child_a'], h['child_b']}, freed)
    _require(exit_code == 0, exit_code)
    return dict(events=events, summaries=summaries, nativeActive=active, nativeLocalFreed=freed,
                phases=4, screenEvidenceRequiredPhases=[1, 2, 3], nativeDrainRequiredPhase=4,
                physicalDisplayAccepted=False)
