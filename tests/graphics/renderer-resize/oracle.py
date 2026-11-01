"""Resize adaptation copied from renderer-owner-transition-fixture-v2/oracle.py.

qualify() consumes one exact session's mixed stdout/native log. It does not
capture PNGs or issue gates. The parent must bind three actual screenshots and
all exact native route retirements and two local-freed owners before gate 4.
Phase 0 is an ordinary-pipe bootstrap, separate from the four render/release phases.
qualify_token() is available to that controller for the unchanged 64-byte ABI.
"""

import json
import re
import struct
import zlib


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


def _dimensions(phase):
    _require(phase in (0, 1, 2, 3, 4), phase)
    return (160, 100) if phase < 2 else (200, 120)


def qualify_token(raw, phase, nonce, identity):
    """Validate ready/gate bytes against qualified actual host/renderer identity.

    identity must be a host record after bootstrap actual owner/tree validation.
    The parent must independently bind that record to its owned session, preserve ready
    bytes, and publish the exact same bytes only after real phase evidence.
    """
    _require(phase in (1, 2, 3, 4), phase)
    _require(re.fullmatch('[0-9a-f]{32}', nonce) is not None, nonce)
    _require(identity['role'] == 'host' and identity['nonce'] == nonce and
             identity['pid'] == identity['host_pid'] > 0 and
             identity['renderer_pid'] > 0 and identity['host_pid'] != identity['renderer_pid'] and
             identity['self_birth'] > 0 and identity['peer_birth'] >= identity['self_birth'] and
             identity['pipe_receives'] >= 1 and all(identity[k] > 0 for k in
             ('root1', 'root2', 'child_a', 'child_b')), identity)
    user_ids = [identity[k] for k in ('root1', 'root2', 'child_a', 'child_b')]
    _require(len(set(user_ids)) == 4 and all(0 < n < 0xffffffff for n in user_ids), identity)
    _require(len(raw) == 64, len(raw))
    words = struct.unpack('<8I2Q4I', raw)
    expected = (0x43465231, 64, 1, phase, _root(identity, phase),
                identity['host_pid'], identity['renderer_pid'], phase if phase < 4 else 0,
                int(nonce[:16], 16), int(nonce[16:], 16),
                identity['child_a'], identity['child_b'], *_dimensions(phase))
    _require(words == expected, (words, expected))
    return dict(phase=phase, pattern=words[7], root=words[4], words=list(words))


def _user_id(value):
    # Only USER handles have zero/sign-extended canonical32 representations.
    # Do not use this function for CID/lease/ticket/generation or process birth.
    _require(isinstance(value, int) and 0 <= value <= 0xffffffffffffffff, value)
    low = value & 0xffffffff
    extended = low | 0xffffffff00000000 if low & 0x80000000 else low
    _require(low not in (0, 0xffffffff) and value in (low, extended), value)
    return low


def _native_user_id(text):
    return _user_id(int(text, 16))


def _full_id(value):
    _require(0 < value <= 0xffffffffffffffff, value)
    return value


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
        _require(sm['phases'] == 4 and sm['pipe_sends'] == sm['pipe_receives'] == 5, sm)
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
            _require((v['image_width'], v['image_height']) == _dimensions(v['phase']), v)
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
             p['local_com_released'] and p['gpu_drained'] and p['com_release_calls'] == 20 and
             p['resize_calls'] == 2 and p['resize_release_calls'] == 6 and p['pixels_read'] == 128000, p)
    _require(all(h[k] == 0 for k in ('swapchains', 'frames', 'readbacks', 'presents', 'com_release_calls',
                                        'resize_calls', 'resize_release_calls', 'pixels_read')), h)

    # Check actual geometry at each *applied* phase, including bootstrap.
    # Expectations alone never authorize the controller to inspect a future tree.
    for role, data in by_role.items():
        brush = _one(data, 'class-borrowed-white-background', 0)
        _require(brush['raw'][0] > 0 and brush['raw'][1:] == [0, 0, 0], brush)
        for phase in range(5):
            phase_data = [v for v in data if v['phase'] == phase]
            clients = _selected(phase_data, 'both-roots-phase-client-size')
            _require({v['raw'][0] for v in clients} == {h['root1'], h['root2']}, (role, phase, clients))
            width, height = (180, 100) if phase < 2 else (220, 120)
            second_x = 210 if phase < 2 else 270
            for v in phase_data:
                raw, label = v['raw'], v['label']
                if label == 'both-roots-phase-client-size':
                    threshold = 2 if raw[0] == h['root2'] else 3
                    _require(raw[1:] == ([540, 300, 0] if phase >= threshold else [440, 240, 0]), v)
                elif label in ('both-roots-active-or-inactive-title', 'only-active-root-visible'):
                    _require(raw[0] in (h['root1'], h['root2']) and
                             raw[1] == (1 if raw[0] == h['root2'] else 0), v)
                    active = int(raw[0] == _root(h, phase))
                    _require(raw[3] == active and raw[2] ==
                             (active if label == 'only-active-root-visible' else
                              56 if active else len('Cider renderer inactive')), v)
                elif label in ('actual-child-phase-window-size', 'actual-bordered-child-client-size',
                               'actual-child-phase-client-slot', 'actual-child-empty-title'):
                    _require(raw[0] in (h['child_a'], h['child_b']), v)
                    index = int(raw[0] == h['child_b'])
                    if label == 'actual-child-phase-window-size':
                        _require(raw[1:] == [width, height, 0], v)
                    elif label == 'actual-bordered-child-client-size':
                        _require(raw[1:] == [width - 2, height - 2, 0], v)
                    elif label == 'actual-child-phase-client-slot':
                        _require(raw[1:] == [second_x if index else 10, 10, 0], v)
                    else:
                        _require(raw[1:] == [0, 0, index], v)
            if phase < 4:
                for label in ('actual-child-phase-window-size', 'actual-bordered-child-client-size',
                              'actual-child-phase-client-slot', 'actual-child-empty-title'):
                    _require({v['raw'][0] for v in _selected(phase_data, label)} ==
                             {h['child_a'], h['child_b']}, (role, phase, label))
            if role == 'host' and phase:
                received = _one(data, 'validate-complete-message', phase)
                observed = [v for v in phase_data if v['event'] in ('window', 'resize-window')]
                _require(observed and min(v['check'] for v in observed) > received['check'], (phase, observed))

    # Controller owns Root2; renderer owns Root1 and both children from creation.
    host_root = _one(host, 'create-host-owned-root2', 0)
    own_intent = _one(renderer, 'renderer-root1-owner-before-create', 0)
    renderer_root = _one(renderer, 'create-renderer-owned-root1', 0)
    child_intent = _selected(renderer, 'renderer-child-owner-before-create', 0)
    child_create = _selected(renderer, 'create-renderer-owned-child', 0)
    _require(host_root['raw'] == [h['root2'], 0, 440, 240] and
             renderer_root['raw'] == [h['root1'], 0, 440, 240] and
             own_intent['raw'] == [p['pid'], p['pid'], h['pid'], 0] and
             own_intent['check'] < renderer_root['check'], (host_root, renderer_root))
    _require(len(child_intent) == len(child_create) == 2 and
             not _selected(host, 'create-renderer-owned-root1') and
             not _selected(host, 'create-renderer-owned-child') and
             not _selected(renderer, 'create-host-owned-root2'), 'window creation roles')
    for index, child in enumerate((h['child_a'], h['child_b'])):
        _require(child_intent[index]['raw'] == [p['pid'], p['pid'], h['root1'], index] and
                 child_create[index]['raw'] == [child, index, p['pid'], 0] and
                 renderer_root['check'] < child_intent[index]['check'] < child_create[index]['check'], child)
    discovered = _one(renderer, 'renderer-discovered-actual-host', 0)
    actual_pi = _one(host, 'PI-handle-actual-renderer-PID', 0)
    envelope = _one(host, 'bootstrap-envelope-bound-to-actual-PI', 0)
    canonical = _one(host, 'bootstrap-distinct-canonical-USER-handles', 0)
    validated = _one(host, 'bootstrap-actual-owner-tree-verified', 0)
    host_boot_done = _one(host, 'bootstrap-echo-sent-before-render-phases', 0)
    boot_echo = _one(renderer, 'bootstrap-reply-exact-echo', 0)
    boot_done = _one(renderer, 'bootstrap-complete-before-D3D', 0)
    _require(discovered['raw'][:2] == [h['pid'], p['pid']] and discovered['raw'][2] and
             discovered['raw'][3] == h['root2'] and
             actual_pi['raw'] == [p['pid'], p['pid'], 0, 0] and
             discovered['check'] < own_intent['check'], discovered)
    _require(envelope['raw'] == [h['pid'], p['pid'], h['pid'], p['pid']] and
             canonical['raw'] == user_ids and
             validated['raw'] == boot_done['raw'] == [h['root1'], h['root2'], p['pid'], h['pid']] and
             host_boot_done['raw'] == boot_echo['raw'] == [0, 32, 80, 0], validated)
    for role, data in by_role.items():
        token = _one(data, 'phase-report-token-root-pattern', 0)
        sent = _one(data, 'write-one-message', 0)
        received = _one(data, 'validate-complete-message', 0)
        _require(token['raw'] == [0, 0, h['root1'], 64] and
                 sent['raw'] == [80, 0, 32, 0] and received['raw'] == [0, 32, 80, 1] and
                 token['check'] < sent['check'] and sent['hr'] == received['hr'] == 0, role)
        _require(all(v['phase'] == 0 and not v['com_release_calls'] and not v['swapchains'] and
                     not v['frames'] for v in data if v['check'] <= received['check']), role)
        if role == 'host':
            _require(actual_pi['check'] < envelope['check'] < canonical['check'] < validated['check'] <
                     received['check'] < token['check'] < sent['check'] < host_boot_done['check'] <
                     _one(host, 'phase-expected-root', 1)['check'], role)
            before, after = canonical['check'], validated['check']
        else:
            _require(child_create[-1]['check'] < token['check'] < sent['check'] < received['check'] <
                     boot_echo['check'] < boot_done['check'], role)
            before, after = child_create[-1]['check'], token['check']
        # Bootstrap claims do not supply owner proof: require the actual API
        # observations between alias adoption/creation and the permitted echo.
        observed = [v for v in data if before < v['check'] < after]
        owners = _selected(observed, 'both-roots-actual-owner', 0)
        child_owners = _selected(observed, 'child-owner-is-renderer', 0)
        _require(all(v['self_birth'] == summaries[role]['self_birth'] and
                     v['peer_birth'] == summaries[role]['peer_birth'] for v in observed), role)
        _require({v['raw'][0] for v in _selected(observed, 'both-roots-IsWindow', 0)} ==
                 {h['root1'], h['root2']}, observed)
        _require({v['raw'][0] for v in owners} == {h['root1'], h['root2']} and
                 all(v['raw'][1:] == [p['pid'] if v['raw'][0] == h['root1'] else h['pid'],
                                      p['pid'] if v['raw'][0] == h['root1'] else h['pid'],
                                      0 if v['raw'][0] == h['root1'] else 1] for v in owners), owners)
        _require({v['raw'][3] for v in child_owners} == {0, 1} and
                 all(v['raw'][:2] == [p['pid'], p['pid']] and v['raw'][2] for v in child_owners) and
                 len(_selected(observed, 'actual-child-IsWindow', 0)) >= 2 and
                 len(_selected(observed, 'actual-child-parent', 0)) >= 2 and
                 len(_selected(observed, 'actual-child-root-ancestor', 0)) >= 2 and
                 _selected(observed, 'unchanged-process-identities', 0), observed)
        for index, child in enumerate((h['child_a'], h['child_b'])):
            valid = _selected(observed, 'actual-child-IsWindow', 0)[index]
            _require(valid['raw'] == [child, h['root1'], index, 0], valid)
            for label in ('actual-child-parent', 'actual-child-root-ancestor'):
                related = [v for v in _selected(observed, label, 0) if v['raw'][2] == index]
                _require(related and all(_user_id(v['raw'][0]) == v['raw'][1] == h['root1'] and
                         v['raw'][3] == 0 for v in related), related)

    device = _one(renderer, 'D3D11CreateDevice-hardware-one-device', 0)
    swaps = _selected(renderer, 'CreateSwapChain-local-child')
    _require(len(swaps) == 2 and [v['raw'][:2] for v in swaps] ==
             [[0, h['child_a']], [1, h['child_b']]] and
             all(v['phase'] == 0 and v['raw'][2:] == [p['pid'], p['pid']] for v in swaps), swaps)
    _require(boot_done['check'] < device['check'] < swaps[0]['check'] < swaps[1]['check'], device)
    owned = _selected(renderer, 'phase-owned-device-and-two-swapchains')
    _require([v['phase'] for v in owned] == [1, 2, 3] and
             all(v['raw'] == owned[0]['raw'] and all(v['raw']) and
                 v['raw'][:2] == device['raw'][2:] and
                 v['com_release_calls'] == (0 if v['phase'] == 1 else 6) for v in owned) and
             owned[0]['raw'][2] != owned[0]['raw'][3], owned)
    releases = [v for v in renderer if v['event'] == 'com-release']
    old_labels = ('resize-old-RTV-Release', 'resize-old-backbuffer-Release', 'resize-old-staging-Release')
    old_releases = [v for v in releases if v['label'] in old_labels]
    final_releases = [v for v in releases if v['label'] != 'all-local-pointers-released-only' and
                      v['label'] not in old_labels]
    _require(len(releases) == 21 and len(old_releases) == 6 and len(final_releases) == 14 and
             all(v['phase'] == 2 for v in old_releases) and all(v['phase'] == 4 for v in final_releases), releases)
    _require([v['label'] for v in old_releases] == list(old_labels) * 2 and
             [v['raw'][2] for v in old_releases] == [0, 0, 0, 1, 1, 1] and
             all(v['raw'][3] > 0 for v in old_releases), old_releases)
    _require([v['label'] for v in final_releases] ==
             ['RTV-Release', 'backbuffer-Release', 'staging-Release', 'swapchain-Release'] * 2 +
             ['event-query-Release', 'factory-Release', 'adapter-Release', 'DXGI-device-Release',
              'context-Release', 'device-Release'], final_releases)
    # Release's actual ULONG return is observed, never required or invented zero.
    actual_releases = old_releases + final_releases
    _require([v['raw'][1] for v in actual_releases] == list(range(1, 21)) and
             all(0 <= v['raw'][0] <= 0xffffffff and v['hr'] == 0 and
                 v['com_release_calls'] == v['raw'][1] for v in actual_releases) and
             [v['resize_release_calls'] for v in old_releases] == list(range(1, 7)) and
             all(v['resize_release_calls'] == 6 for v in final_releases), actual_releases)
    for role, data in by_role.items():
        calls = sum(1 for v in data if v['label'] == 'actual-ResizeBuffers-once-per-swapchain')
        count = returned = 0
        for v in data:
            if v['label'] == 'actual-ResizeBuffers-once-per-swapchain':
                count += 1
            if v['label'] in old_labels:
                returned += 1
            _require(v['resize_calls'] == count and v['resize_release_calls'] == returned, v)
            if role == 'host':
                _require(v['com_release_calls'] == 0 and v['pixels_read'] == 0, v)
        _require(calls == (2 if role == 'renderer' else 0), (role, calls))
    resized = _selected(renderer, 'actual-ResizeBuffers-once-per-swapchain')
    _require(len(resized) == 2, resized)
    before_resize = _one(renderer, 'resize-retained-interface-identities-before', 2)
    after_resize = _one(renderer, 'resize-retained-interface-identities-after', 2)
    drained = _one(renderer, 'resize-actual-completion-before-old-releases', 2)
    unbound = _one(renderer, 'resize-unbind-ClearState', 2)
    returned = _one(renderer, 'resize-all-six-old-target-owners-returned', 2)
    unmapped = _one(renderer, 'resize-old-targets-not-mapped', 2)
    _require(before_resize['raw'] == after_resize['raw'] == owned[0]['raw'] and
             unmapped['raw'] == [0, 0, 0, 0] and returned['raw'] == [6, 6, 0, 0] and
             before_resize['check'] < unmapped['check'] < unbound['check'] < drained['check'] <
             old_releases[0]['check'] < old_releases[-1]['check'] < returned['check'] <
             resized[0]['check'] < resized[1]['check'] < after_resize['check'] < owned[1]['check'], resized)
    completions = _selected(renderer, 'actual-event-completion', 2)
    _require(len(completions) == 3 and unbound['check'] < completions[0]['check'] < drained['check'] and
             completions[0]['hr'] == drained['hr'] == 0 and completions[0]['raw'][1] == 1, completions)
    queries = _selected(renderer, 'retained-event-query-identity')
    _require(len(queries) == 5 and all(v['raw'] == queries[0]['raw'] and
             v['raw'][0] == v['raw'][1] > 0 and v['raw'][2:] == [0, 0] for v in queries) and
             drained['raw'] == [queries[0]['raw'][0], 0, 0, 0], queries)
    _one(renderer, 'completion-query', 0)  # no recreated query
    _require(len(_selected(renderer, 'completion-query')) == 1, 'one retained query')
    for index, call in enumerate(resized):
        pointer = owned[0]['raw'][2 + index]
        _require(call['phase'] == 2 and call['hr'] >= 0 and call['raw'] == [index, pointer, pointer, index + 1] and
                 call['com_release_calls'] == call['resize_release_calls'] == 6, call)
        initial = _selected(renderer, 'actual-swap-description', 0)[index]
        before = _selected(renderer, 'resize-before-swap-description', 2)[index]
        after = _selected(renderer, 'resize-after-swap-description', 2)[index]
        _require(initial['raw'] == before['raw'] == [index, 160, 100, pointer] and
                 after['raw'] == [index, 200, 120, pointer] and
                 initial['hr'] == before['hr'] == after['hr'] == 0 and
                 drained['check'] < before['check'] < old_releases[0]['check'] and
                 call['check'] < after['check'] < after_resize['check'], (initial, before, after))
    for phase in (0, 2):
        desc_labels = ('actual-swap-description',) if phase == 0 else (
            'resize-before-swap-description', 'resize-after-swap-description')
        for label in desc_labels:
            _require(len(_selected(renderer, label, phase)) == 2, (label, phase))
        width, height = _dimensions(phase)
        for label in ('actual-backbuffer-zero', 'actual-RTV', 'actual-CPU-read-staging', 'staging-layout',
                      'actual-BGRA-backbuffer-layout'):
            _require(len(_selected(renderer, label, phase)) == 2, (label, phase))
        layouts = _selected(renderer, 'actual-BGRA-backbuffer-layout', phase)
        _require([v['raw'] for v in layouts] == [[width, height, 87, i] for i in (0, 1)], layouts)
        for index in (0, 1):
            acquired = _selected(renderer, 'actual-backbuffer-zero', phase)[index]
            rtv = _selected(renderer, 'actual-RTV', phase)[index]
            staging = _selected(renderer, 'actual-CPU-read-staging', phase)[index]
            layout = _selected(renderer, 'staging-layout', phase)[index]
            _require(acquired['check'] < layouts[index]['check'] < rtv['check'] < staging['check'] < layout['check'] and
                     layout['raw'] == [index, 3, 131072, 87], (acquired, layout))
            if phase == 2:
                _require(resized[index]['check'] < acquired['check'] < after_resize['check'], acquired)
    _require(all(v['phase'] in (0, 2) for label in
             ('actual-backbuffer-zero', 'actual-RTV', 'actual-CPU-read-staging') for v in _selected(renderer, label)),
             'no extra target rebuild')
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
                     received['raw'] == [phase, flags, 80, phase + 1], (sent, received))
            _require(token['check'] < sent['check'], role)
            if phase < 4:
                _require(token['com_release_calls'] == (6 if role == 'renderer' and phase >= 2 else 0) and
                         not token['local_com_released'], token)
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
                     _user_id(v['raw'][1]) == actual_root and v['raw'][2] == actual_root for v in title), (role, phase))
            _require(_selected(data, 'unchanged-process-identities', phase), (role, phase))
            owners = _selected(data, 'both-roots-actual-owner', phase)
            clients = _selected(data, 'both-roots-phase-client-size', phase)
            parents = _selected(data, 'actual-child-parent', phase)
            ancestors = _selected(data, 'actual-child-root-ancestor', phase)
            child_owners = _selected(data, 'child-owner-is-renderer', phase)
            _require({v['raw'][0] for v in owners} == {h['root1'], h['root2']} and
                     all(v['raw'][1:] == [p['pid'] if v['raw'][0] == h['root1'] else h['pid'],
                                          p['pid'] if v['raw'][0] == h['root1'] else h['pid'],
                                          0 if v['raw'][0] == h['root1'] else 1] for v in owners), owners)
            _require({v['raw'][0] for v in clients} == {h['root1'], h['root2']} and
                     all(v['raw'][1:] == ([540, 300, 0] if
                     phase >= (2 if v['raw'][0] == h['root2'] else 3) else [440, 240, 0]) for v in clients), clients)
            root_owners = _selected(data, 'root-owner', phase)
            expected_pid = h['pid'] if phase == 2 else p['pid']
            _require(root_owners and all(v['raw'][:2] == [expected_pid, expected_pid] and
                     v['raw'][2] for v in root_owners), root_owners)
            if phase == 4:
                continue  # children are destroyed by renderer BEFORE its release report
            _require({v['raw'][2] for v in parents} == {0, 1} and
                     {v['raw'][2] for v in ancestors} == {0, 1} and
                     all(_user_id(v['raw'][0]) == v['raw'][1] == actual_root for v in parents + ancestors), (role, parents))
            _require({v['raw'][3] for v in child_owners} == {0, 1} and
                     all(v['raw'][0] == v['raw'][1] == p['pid'] and v['raw'][2] for v in child_owners), child_owners)
        if phase == 4:
            continue
        echo = _one(renderer, 'reply-exactly-echoes-actual-report', phase)
        done = _one(renderer, 'phase-echo-complete-before-next-root', phase)
        _require(echo['raw'][:2] == [phase, 15] and done['raw'] == [phase, actual_root, phase, 0] and
                 echo['check'] < done['check'] and done['com_release_calls'] == (0 if phase == 1 else 6), done)
        clears = _selected(renderer, 'actual-clear-RTV', phase)
        presents = _selected(renderer, 'actual-Present-HRESULT', phase)
        maps = _selected(renderer, 'actual-Map-READ', phase)
        _require(len(clears) == len(presents) == len(maps) == 2, phase)
        for label in ('frame-backbuffer-actual-dimensions', 'frame-staging-actual-dimensions',
                      'readback-pixel-CRC32', 'map-bounded-CPU-range'):
            _require(len(_selected(renderer, label, phase)) == 2, (phase, label))
        width, height = _dimensions(phase)
        phase_desc = _selected(renderer, 'phase-swap-description', phase)
        _require(len(phase_desc) == 2 and [v['raw'] for v in phase_desc] ==
                 [[i, width, height, owned[0]['raw'][2 + i]] for i in (0, 1)] and
                 all(v['hr'] == 0 and owned[phase - 1]['check'] < v['check'] < clears[0]['check']
                     for v in phase_desc), phase_desc)
        for index, child in enumerate((h['child_a'], h['child_b'])):
            color = 0xffff0000 if phase == 3 or ((phase != 2) == (index == 0)) else 0xff0000ff
            back = _one(renderer, 'readback-' + ('B' if index else 'A') + '-all-expected-pixels-and-Unmap', phase)
            _require(clears[index]['raw'] == [index, color, phase, 2 * (phase - 1) + index + 1], clears)
            expected_total = (0 if phase == 1 else 32000 if phase == 2 else 80000) + (index + 1) * width * height
            _require(back['pixels_read'] == expected_total, back)
            _require(back['raw'][:3] == [color, width * height, 0] and width * 4 <= back['raw'][3] <= 1024 * 1024, back)
            for label in ('frame-backbuffer-actual-dimensions', 'frame-staging-actual-dimensions'):
                layout = _selected(renderer, label, phase)[index]
                _require(layout['raw'] == [index, width, height, 87] and
                         layout['check'] < clears[index]['check'], layout)
            crc = _selected(renderer, 'readback-pixel-CRC32', phase)[index]
            expected_crc = zlib.crc32(struct.pack('<I', color) * (width * height))
            _require(crc['raw'] == [index, expected_crc, width, height] and
                     back['check'] < crc['check'] < presents[index]['check'], crc)
            ranges = _selected(renderer, 'map-bounded-CPU-range', phase)
            bounded = ranges[index]
            _require(bounded['raw'][0] == (height - 1) * back['raw'][3] + width * 4 and
                     bounded['raw'][1] == back['raw'][3] == maps[index]['raw'][1] and
                     bounded['raw'][2] == maps[index]['raw'][2] and
                     bounded['raw'][3] == bounded['raw'][2] + bounded['raw'][0] and
                     maps[index]['check'] < bounded['check'] < back['check'], bounded)
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
        slots = _selected(renderer, 'resized-child-slot-220x120', phase)
        _require(all(len(v) == 2 for v in (before, moved, parents, ancestors, slots)), phase)
        adjusted = _one(renderer, 'resize-root-AdjustWindowRectEx-client-540x300', phase)
        sized = _one(renderer, 'resize-root-SetWindowPos-adjusted-outer', phase)
        measured = _one(renderer, 'resize-root-actual-client-540x300', phase)
        _require(adjusted['raw'][0] == actual_root and adjusted['raw'][1] >= 540 and
                 adjusted['raw'][2] >= 300 and adjusted['raw'][3] == 0 and
                 sized['raw'] == adjusted['raw'] and measured['raw'] == [actual_root, 540, 300, 0] and
                 ancestors[-1]['check'] < adjusted['check'] < sized['check'] < measured['check'] < slots[0]['check'],
                 (adjusted, sized, measured))
        _require([v['raw'] for v in slots] == [[h['child_a'], 10, 10, 0], [h['child_b'], 270, 10, 0]], slots)
        if phase == 2:
            _require(slots[-1]['check'] < before_resize['check'], phase)
        prior_echo = _one(renderer, 'phase-echo-complete-before-next-root', phase - 1)
        _require(prior_echo['check'] < transition[0]['check'], phase)
        for index, child in enumerate((h['child_a'], h['child_b'])):
            _require(before[index]['raw'][0] == moved[index]['raw'][0] == parents[index]['raw'][0] ==
                     ancestors[index]['raw'][0] == slots[index]['raw'][0] == child, child)
            _require(_user_id(before[index]['raw'][1]) == _user_id(moved[index]['raw'][1]) == previous_root and
                     moved[index]['raw'][2] == actual_root and moved[index]['hr'] == 0, moved[index])
            _require(_user_id(parents[index]['raw'][1]) ==
                     _user_id(ancestors[index]['raw'][1]) == actual_root, child)
            _require(transition[-1]['check'] < before[index]['check'] < moved[index]['check'] < parents[index]['check'] <
                     ancestors[index]['check'] < slots[index]['check'] < _one(renderer, 'phase-owned-device-and-two-swapchains', phase)['check'], child)

    _require(not [v for v in host if v['event'] in ('resize-window', 'resize-gpu')], 'host only changes expectations')
    _require([v['phase'] for v in _selected(renderer, 'resize-root-actual-client-540x300')] == [2, 3] and
             len(_selected(renderer, 'resized-child-slot-220x120')) == 4, 'bounded geometry actions')
    release = _one(renderer, 'all-local-pointers-released-only', 4)
    release_echo = _one(renderer, 'release-reply-exact-echo', 4)
    _require(release['local_com_released'] and release['gpu_drained'] and release['raw'] == [20, 1, 1, 0] and
             release['check'] < _one(renderer, 'write-one-message', 4)['check'] < release_echo['check'] and
             release_echo['raw'][:2] == [4, 16], release)
    _require(len(_selected(renderer, 'actual-event-completion')) == 8, 'six frame events, pre-resize drain and final GPU drain')
    retained = _one(renderer, 'renderer-children-destroyed-root1-retained', 4)
    observed = _one(host, 'host-observed-child-absence-root1-live-renderer-alive', 4)
    ack = _one(renderer, 'renderer-final-ACK-before-root1-destroy', 4)
    _require(retained['raw'] == observed['raw'] == [h['root1'], h['child_a'], h['child_b'], p['pid']] and
             ack['raw'] == [4, h['root1'], 5, 5], (retained, observed, ack))
    for index, suffix in enumerate(('A', 'B')):
        label = 'renderer-destroy-owned-child-' + suffix
        destroyed = _one(renderer, label, 4)
        _require(_user_id(destroyed['raw'][0]) == h['child_a' if index == 0 else 'child_b'] and
                 destroyed['raw'][1:] == [1, 0, 0] and destroyed['local_com_released'] and destroyed['gpu_drained'] and
                 release['check'] < destroyed['check'] < retained['check'] <
                 _one(renderer, 'write-one-message', 4)['check'], destroyed)
        _require(not _selected(host, label), label)
    _require(_one(host, 'validate-complete-message', 4)['check'] < observed['check'] <
             _one(host, 'ready-write-token64', 4)['check'], observed)
    for role, data in by_role.items():
        absent = _selected(data, 'child-actually-absent', 4)
        _require({v['raw'][0] for v in absent} == {h['child_a'], h['child_b']}, (role, absent))
        before = retained['check'] if role == 'renderer' else observed['check']
        _require({v['raw'][0] for v in absent if v['check'] < before} ==
                 {h['child_a'], h['child_b']}, (role, absent))
        received = _one(data, 'validate-complete-message', 4)
        boundary = ack if role == 'renderer' else observed
        live_root = [v for v in _selected(data, 'actual-root-IsWindow', 4) if
                     received['check'] < v['check'] < boundary['check']]
        alive = [v for v in _selected(data, 'unchanged-process-identities', 4) if
                 received['check'] < v['check'] < boundary['check']]
        _require(live_root and all(v['raw'] == [h['root1'], 0, 0, 0] for v in live_root) and alive, role)
        _require({v['raw'][0] for v in absent if received['check'] < v['check'] < boundary['check']} ==
                 {h['child_a'], h['child_b']}, role)
        empty = _one(data, 'exactly-five-messages', 4)
        _require(empty['raw'] == [0, 4 if role == 'host' else 5, 5, 0], empty)
        # All actual destruction is guarded by a same-process, same-thread owner
        # query. Host has exactly one owned window; no borrowed destruction.
        guards = _selected(data, 'destroy-slot-actual-self-owner', 4)
        expected_windows = {h['root2']} if role == 'host' else {h['root1'], h['child_a'], h['child_b']}
        _require(len(guards) == len(expected_windows) and
                 {_user_id(v['raw'][0]) for v in guards} == expected_windows and
                 all(v['raw'][1:3] == [summaries[role]['pid']] * 2 and v['raw'][3] for v in guards), guards)
    join = _one(host, 'actual-PI-wait-and-exit', 4)
    _require(join['raw'] == [0, 0, p['pid'], 0] and
             _one(host, 'write-one-message', 4)['check'] < join['check'], join)
    renderer_destroyed = _one(renderer, 'destroy-renderer-owned-root1', 4)
    host_destroyed = _one(host, 'destroy-host-owned-root2', 4)
    _require(_user_id(renderer_destroyed['raw'][0]) == h['root1'] and
             renderer_destroyed['raw'][1:] == [1, 0, 0] and
             release_echo['check'] < ack['check'] < renderer_destroyed['check'] <
             _one(renderer, 'unregister-owned-class', 4)['check'], renderer_destroyed)
    root_gone = _one(host, 'joined-renderer-root1-actually-absent', 4)
    _require(root_gone['raw'] == [h['root1'], 0, p['pid'], 1] and
             _user_id(host_destroyed['raw'][0]) == h['root2'] and host_destroyed['raw'][1:] == [1, 0, 0] and
             join['check'] < root_gone['check'] < host_destroyed['check'] <
             _one(host, 'unregister-owned-class', 4)['check'], host_destroyed)

    # Parse real existing TRACE records only. Six routes = two initial local
    # routes + two foreign routes + two fresh local-return routes. These
    # offline checks cannot establish PNG acceptance or external gate timing.
    number = r'(0x[0-9a-fA-F]+)'
    route_tail = (r' root ' + number + r' lease ' + number + r' ticket ' + number +
                  r' generation ' + number)
    initial = list(re.finditer(r'renderer client active hwnd ' + number + r' context (\d+)' + route_tail, text))
    migrations = list(re.finditer(r'renderer client migrated hwnd ' + number + r' context (\d+)' +
                                 r' previous-root ' + number + route_tail, text))
    retirements = list(re.finditer(r'renderer client route retired hwnd ' + number + r' context (\d+)' +
                                  r' root ' + number + r' lease ' + number + r' ticket ' + number +
                                  r' host-safe (\d+) producer-safe (\d+)', text))
    frees = list(re.finditer(r'renderer client freed hwnd ' + number + r' context0 routes0', text))
    _require(len(initial) == 2 and len(migrations) == 4 and len(retirements) == 6 and len(frees) == 2,
             (len(initial), len(migrations), len(retirements), len(frees)))
    routes, native_retired, active = [], [], []
    for match in initial:
        a, cid, root, lease, ticket, generation = match.groups()
        route = (_native_user_id(a), _full_id(int(cid)), _native_user_id(root),
                 _full_id(int(lease, 16)), _full_id(int(ticket, 16)), _full_id(int(generation, 16)))
        routes.append((match.start(), route))
        active.append(route[:2])
    for match in migrations:
        a, cid, previous, root, lease, ticket, generation = match.groups()
        route = (_native_user_id(a), _full_id(int(cid)), _native_user_id(root),
                 _full_id(int(lease, 16)), _full_id(int(ticket, 16)), _full_id(int(generation, 16)))
        routes.append((match.start(), route))
        previous = _native_user_id(previous)
        _require(previous == (h['root1'] if route[2] == h['root2'] else h['root2']), match.group())
    routes.sort()
    _require({a for a, _ in active} == {h['child_a'], h['child_b']} and
             len({cid for _, cid in active}) == 2, active)
    route_by_key = {}
    for child in (h['child_a'], h['child_b']):
        child_routes = [(position, route) for position, route in routes if route[0] == child]
        _require(len(child_routes) == 3 and [route[2] for _, route in child_routes] ==
                 [h['root1'], h['root2'], h['root1']] and
                 len({route[1] for _, route in child_routes}) == 1 and
                 len({route[5] for _, route in child_routes}) == 3 and
                 len({route[2:5] for _, route in child_routes}) == 3, child_routes)
        for position, route in child_routes:
            key = route[:5]  # exact (child, full CID, root, full lease, full ticket)
            _require(key not in route_by_key, key)
            route_by_key[key] = position
    _require(len(route_by_key) == 6, routes)
    retired_positions = {}
    for match in retirements:
        a, cid, root, lease, ticket, host_safe, producer_safe = match.groups()
        key = (_native_user_id(a), _full_id(int(cid)), _native_user_id(root),
               _full_id(int(lease, 16)), _full_id(int(ticket, 16)))
        # ProducerSAFE is the required release permission. host-safe is an
        # observed boolean, not a forged substitute for producer-safe.
        _require(key in route_by_key and key not in retired_positions and
                 int(host_safe) in (0, 1) and int(producer_safe) == 1 and
                 route_by_key[key] < match.start(), match.group())
        retired_positions[key] = match.start()
        native_retired.append((*key, int(host_safe), int(producer_safe)))
    freed = [_native_user_id(match.group(1)) for match in frees]
    _require(set(retired_positions) == set(route_by_key) and
             set(freed) == {h['child_a'], h['child_b']}, (native_retired, freed))
    for match in frees:
        child = _native_user_id(match.group(1))
        _require(all(position < match.start() for key, position in retired_positions.items() if key[0] == child),
                 match.group())
    _require(exit_code == 0, exit_code)
    return dict(events=events, summaries=summaries, nativeActive=active,
                nativeRoutes=[route for _, route in routes], nativeRouteRetired=native_retired,
                nativeLocalFreed=freed, phases=4, bootstrapMessagesPerDirection=1,
                pipeMessagesPerDirection=5, resizeCalls=2, resizeTargetReleaseCalls=6,
                totalCOMReleaseCalls=20, actualPixelsRead=128000,
                controlledImageDimensions={phase: list(_dimensions(phase)) for phase in range(5)},
                nativeProfile='current-implementation-expectation-not-Windows-CID-contract',
                screenEvidenceRequiredPhases=[1, 2, 3], nativeDrainRequiredPhase=4,
                externalPNGAndNativeGateTimingAccepted=False, physicalDisplayAccepted=False)
