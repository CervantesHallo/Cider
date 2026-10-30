#ifndef CIDER_RECOVERY_EPOCH_FIXTURE_H
#define CIDER_RECOVERY_EPOCH_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

#define RECOVERY_EPOCH_FIXTURE_VERSION 1u
#define RECOVERY_EPOCH_HIGH (UINT64_C(1) << 33)
#define RECOVERY_EPOCH_EXPECTED_CHECKS 29u
#define RECOVERY_EPOCH_PRIMARY_CHECK 20u

enum recovery_epoch_operation
{
    RECOVERY_EPOCH_ARM_EMPTY = 1,
    RECOVERY_EPOCH_ACQUIRE = 2,
    RECOVERY_EPOCH_SNAPSHOT = 3,
    RECOVERY_EPOCH_RELEASE_TICK = 4,
    RECOVERY_EPOCH_CLEAR = 5,
    RECOVERY_EPOCH_CLEANUP = 6
};
enum recovery_epoch_native_flag
{
    RECOVERY_EPOCH_EMPTY_RETURNED = 0x0001u,
    RECOVERY_EPOCH_ACQUIRE_RETURNED = 0x0002u,
    RECOVERY_EPOCH_OWNER_CAPTURED = 0x0004u,
    RECOVERY_EPOCH_OWNER_PRESENT = 0x0008u,
    RECOVERY_EPOCH_TARGET_ROOT_PRESENT = 0x0010u,
    RECOVERY_EPOCH_OWNER_ACTIVE = 0x0020u,
    RECOVERY_EPOCH_OWNER_BUSY = 0x0040u,
    RECOVERY_EPOCH_OWNER_QUEUED = 0x0080u,
    RECOVERY_EPOCH_THREAD_OPEN = 0x0100u,
    RECOVERY_EPOCH_PROCESS_OPEN = 0x0200u,
    RECOVERY_EPOCH_CLEARED = 0x0400u,
    RECOVERY_EPOCH_QUERY_ATTEMPTED = 0x0800u,
    RECOVERY_EPOCH_SET_ATTEMPTED = 0x1000u,
    RECOVERY_EPOCH_WAS_PROTECTED = 0x2000u,
    RECOVERY_EPOCH_LAST_CLOSE_VALID = 0x4000u
};
enum recovery_epoch_pe_flag
{
    RECOVERY_EPOCH_GATE_ARMED = 0x0001u,
    RECOVERY_EPOCH_SELECTED = 0x0002u,
    RECOVERY_EPOCH_HELD = 0x0004u,
    RECOVERY_EPOCH_RELEASED = 0x0008u,
    RECOVERY_EPOCH_WAIT_FINISHED = 0x0010u,
    RECOVERY_EPOCH_HOOK_DONE = 0x0020u,
    RECOVERY_EPOCH_EVENT_CLOSED = 0x0040u,
    RECOVERY_EPOCH_LIVE_NEEDED = 0x0080u,
    RECOVERY_EPOCH_LIVE_TIMER = 0x0100u,
    RECOVERY_EPOCH_WAIT_TIMELY = 0x0200u
};

/* Private synchronous slot7/count8. Native and PE each fill their own fields;
 * the two snapshots are not one cross-domain transaction. decision_* plus
 * selected_epoch/recover_* are a frozen, single selected callback observation
 * taken immediately after the ordinary decision, under the same watch_lock.
 * No native pointer or borrowed HANDLE crosses this frame. Unattempted NT/PE
 * status observations are UINT32_MAX. CLEAR's ERROR_IO_PENDING is nonacting;
 * CLEANUP may signal the gate before returning IO_PENDING for its close owner.
 * status is an actual operation error, never owner readiness.
 */
struct recovery_epoch_fixture
{
    uint32_t size, version, operation, status;
    uint32_t root, flags, acquire_error, arm_error;
    uint64_t root_epoch, empty_setup_id, owner_id, returned_lease;
    uint64_t selected_epoch, decision_epoch, live_epoch;
    uint64_t wait_start, wait_end, wait_frequency;
    uint32_t producers, watches, setups, cleanup, models, hosts, owner_busy, last_close;
    uint32_t query_status, set_status, pe_watches, pe_timers;
    uint32_t pe_state, recover_status, recover_pending, wait_result;
    uint32_t decision_seen, decision_needed, decision_timer, signal_error;
    uint32_t event_owners, hook_users, control_error, reserved;
};
_Static_assert(sizeof(struct recovery_epoch_fixture) == 208, "recovery epoch frame208");
_Static_assert(offsetof(struct recovery_epoch_fixture, root_epoch) == 32, "epoch offset");
_Static_assert(offsetof(struct recovery_epoch_fixture, producers) == 112, "counts offset");
_Static_assert(offsetof(struct recovery_epoch_fixture, decision_seen) == 176, "decision offset");

/* Existing slot6/version2 diagnostic probe declaration, kept for the pinned
 * 107 Native/PE probe code. This client does not need to invoke it. */
struct producer_startup_owner_probe
{
    uint32_t size, version, roots, producers, watch_pins, hosts, pe_watches, cleanup_pins;
    uint64_t target_lease;
    uint32_t target_active, target_watched;
};
_Static_assert(sizeof(struct producer_startup_owner_probe) == 48, "existing probe48");

#endif
