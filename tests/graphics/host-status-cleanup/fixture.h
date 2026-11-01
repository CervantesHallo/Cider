#ifndef CIDER_HOST_STATUS_CLEANUP_FIXTURE_H
#define CIDER_HOST_STATUS_CLEANUP_FIXTURE_H
#include <stddef.h>
#include <stdint.h>
#define HOST_CLEANUP_VERSION 1u
#define HOST_CLEANUP_HIGH (UINT64_C(1) << 33)
/* Opaque reservation metadata ONLY: never imported, not a real CAContext ID. */
#define HOST_CLEANUP_METADATA_ID UINT32_C(0x6d657461)
enum host_cleanup_operation
{
    HOST_CLEANUP_BIND = 1, HOST_CLEANUP_SNAPSHOT, HOST_CLEANUP_ARM,
    HOST_CLEANUP_RUN_CANCEL, HOST_CLEANUP_FINISH, HOST_CLEANUP_DISARM
};
enum host_cleanup_flag { HOST_CLEANUP_BOUND = 1u, HOST_CLEANUP_FINISHED = 2u };
/* Private normal Unix slot7/count8, no native pointers. writer_handle and
 * event_handle are explicit process-local Wine HANDLE32 values, not pointers
 * or duplicated owners. Client retains the real event until worker exit.
 * decision_local_closed is the actual local variable, NOT an NT result unless
 * decision_attempted==1. Unavailable observations/statuses are UINT32_MAX.
 */
struct host_cleanup_fixture
{
    uint32_t size, version, operation, status;
    uint32_t root, context_id, flags, writer_handle;
    uint64_t root_epoch, lease, ticket, generation, control_sequence, producer_birth;
    uint64_t hold_start, hold_frequency, decision_serial, root_birth;
    uint32_t active, watched, finalized, request_inflight;
    uint32_t host_resources, host_owners, registered_count, host_registered;
    uint32_t cell_present, cell_matches, cell_ready, status_busy;
    uint32_t status_controls, status_resource_done, status_terminal, status_queued;
    uint32_t model_jobs, model_possible, retirement_pending, host_state;
    uint32_t close_seen, close_attempts, close_status, close_partial;
    uint32_t hold_armed, hold_active, hold_done, hold_wait_status;
    uint32_t decision_seen, decision_controls, decision_resource_done, decision_busy;
    uint32_t decision_owns_cell, decision_attempted, decision_local_closed, decision_removed;
    uint32_t decision_cell_present, decision_cell_matches, decision_registered, decision_host_owners;
    uint32_t decision_registry_count, decision_queued, pe_watches, pe_timers;
    uint32_t native_cleanup, native_setups, observer_owned, reserved;
    uint32_t event_handle, control_returned, control_error, native_watches;
    uint32_t close_successes, last_close_status, last_close_partial, hold_timely;
};
_Static_assert(sizeof(struct host_cleanup_fixture) == 336, "host cleanup frame336");
_Static_assert(offsetof(struct host_cleanup_fixture, root_epoch) == 32, "host cleanup identity");
_Static_assert(offsetof(struct host_cleanup_fixture, active) == 112, "host cleanup counts");
_Static_assert(offsetof(struct host_cleanup_fixture, decision_seen) == 224, "host cleanup decision");
#endif
