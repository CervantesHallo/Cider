#ifndef CIDER_PRODUCER_STARTUP_FAILURE_FIXTURE_H
#define CIDER_PRODUCER_STARTUP_FAILURE_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

#define PRODUCER_STARTUP_FIXTURE_VERSION 1u
#define PRODUCER_STARTUP_GENERATION (UINT64_C(1) << 33)

enum producer_startup_operation
{
    PRODUCER_STARTUP_RUN = 1,
    PRODUCER_STARTUP_SNAPSHOT = 2,
    PRODUCER_STARTUP_CLEAR = 3
};
enum producer_startup_mode
{
    PRODUCER_STARTUP_SECTION_FAILURE = 1,
    PRODUCER_STARTUP_THREAD_FAILURE = 2
};
enum producer_startup_flag
{
    PRODUCER_STARTUP_RUN_RETURNED = 0x0001u,
    PRODUCER_STARTUP_OWNER_CAPTURED = 0x0002u,
    PRODUCER_STARTUP_OWNER_PRESENT = 0x0004u,
    PRODUCER_STARTUP_CLEARED = 0x0008u,
    PRODUCER_STARTUP_OWNER_ACTIVE = 0x0010u,
    PRODUCER_STARTUP_OWNER_BUSY = 0x0020u,
    PRODUCER_STARTUP_CLEANUP_QUEUED = 0x0040u,
    PRODUCER_STARTUP_STATUS_OWNER = 0x0080u,
    PRODUCER_STARTUP_STATUS_CLEANUP = 0x0100u,
    PRODUCER_STARTUP_PROCESS_OPEN = 0x0200u,
    PRODUCER_STARTUP_WATCH_STARTED = 0x0400u,
    PRODUCER_STARTUP_COMPLETION_SEEN = 0x0800u,
    PRODUCER_STARTUP_LAST_CLOSE_VALID = 0x1000u,
    PRODUCER_STARTUP_QUERY_ATTEMPTED = 0x2000u,
    PRODUCER_STARTUP_SET_ATTEMPTED = 0x4000u,
    PRODUCER_STARTUP_WAS_PROTECTED = 0x8000u,
    PRODUCER_STARTUP_TARGET_ROOT_PRESENT = 0x10000u,
    PRODUCER_STARTUP_RETRY_UNDER_CLEAR_PIN = 0x20000u
};

/* Private synchronous slot7/count8 ABI. PE wrapper returns the actual Native
 * NTSTATUS and fills pe_watches/pe_timers with actual live-owner counts AFTER
 * MACDRV_CALL(producer_startup_failure_fixture,...). It must not drive cleanup
 * or turn SNAPSHOT into a cleanup operation. RUN's acquire_error and
 * returned_lease are the actual ordinary acquisition result; status describes
 * the fixture operation. Only an explicitly non-acting CLEAR may return
 * ERROR_IO_PENDING. last_close is meaningful only with LAST_CLOSE_VALID;
 * query_status/set_status require their corresponding ATTEMPTED flags.
 * No native pointer or raw owned handle crosses this frame.
 */
struct producer_startup_failure_fixture
{
    uint32_t size, version, operation, status;
    uint32_t mode, root, acquire_error, flags;
    uint64_t owner_id, returned_lease;
    uint32_t producers, watches, setups, cleanup;
    uint32_t models, owner_inflight, pe_watches, pe_timers;
    uint32_t last_close, query_status, set_status, reserved;
};
_Static_assert(sizeof(struct producer_startup_failure_fixture) == 96, "startup fixture96");
_Static_assert(offsetof(struct producer_startup_failure_fixture, owner_id) == 32, "startup owner offset");
_Static_assert(offsetof(struct producer_startup_failure_fixture, pe_watches) == 72, "startup PE offset");

/* Byte-identical layout to existing read-only transport probe v2, slot6.
 * cleanup_pins is the existing PE timer-owner field; frame.cleanup separately
 * counts the Native producer/setup cleanup FIFOs. This declaration avoids a
 * new probe.h dependency or a change to the existing diagnostic operation.
 */
struct producer_startup_owner_probe
{
    uint32_t size, version, roots, producers, watch_pins, hosts, pe_watches, cleanup_pins;
    uint64_t target_lease;
    uint32_t target_active, target_watched;
};
_Static_assert(sizeof(struct producer_startup_owner_probe) == 48, "startup probe48");

#endif
