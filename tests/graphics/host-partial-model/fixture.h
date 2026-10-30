#ifndef CIDER_HOST_PARTIAL_MODEL_FIXTURE_H
#define CIDER_HOST_PARTIAL_MODEL_FIXTURE_H
#include <stddef.h>
#include <stdint.h>

#define PARTIAL_MODEL_VERSION 1u
#define PARTIAL_MODEL_HIGH (UINT64_C(1) << 33)
#define PARTIAL_MODEL_EXPECTED_CHECKS 30u
#define PARTIAL_MODEL_PRIMARY_CHECK 19u
enum partial_model_operation
{
    PARTIAL_MODEL_SETUP = 1, PARTIAL_MODEL_REVOKE, PARTIAL_MODEL_SNAPSHOT,
    PARTIAL_MODEL_SUBMIT_HELD, PARTIAL_MODEL_CLEANUP, PARTIAL_MODEL_FINISH
};
enum partial_model_flag
{
    PARTIAL_MODEL_INITIALIZED = 1u, PARTIAL_MODEL_SETUP_RETURNED = 2u,
    PARTIAL_MODEL_HOLD_ARMED = 4u, PARTIAL_MODEL_HELD_ONCE = 8u,
    PARTIAL_MODEL_REVOKE_CALLED = 16u, PARTIAL_MODEL_SUBMIT_CALLED = 32u,
    PARTIAL_MODEL_CONTEXT_RELEASED = 64u, PARTIAL_MODEL_FINISHED = 128u
};
/* Private synchronous slot7/count8 ABI, unrelated to USER96. Unavailable
 * producer/Cocoa observations are UINT32_MAX with their valid field zero.
 * witness_* is frozen by the actual checked Cocoa retirement, immediately
 * before its real completion callback; live observations are separate samples.
 * install_result is the ACTUAL host_install return, not fixture status.
 */
struct partial_model_fixture
{
    uint32_t size, version, operation, status;
    uint32_t root, child, context_id, flags;
    uint64_t root_epoch, lease, ticket, generation, install_result, witness_ticket;
    uint64_t producer_birth, witness_serial;
    uint32_t producer_valid, active, watched, finalized;
    uint32_t completion_seen, process_open, cleanup_inflight, request_inflight;
    uint32_t pending, retirement_total, native_producers, native_watches;
    uint32_t native_setups, native_cleanup, model_jobs, held;
    uint32_t cocoa_valid, attached, indexed, ledger;
    uint32_t attach_faults, rollback_faults, witness_seen, witness_removed;
    uint32_t witness_valid, witness_attached, witness_indexed, witness_ledger;
    uint32_t witness_faults, witness_rollback_faults, pe_watches, pe_timers;
    uint32_t context_owned, window_owned, producer_owned, release_attempted;
    uint32_t revoke_error, cocoa_error, witness_error, reserved;
};
_Static_assert(sizeof(struct partial_model_fixture) == 256, "partial frame256");
_Static_assert(offsetof(struct partial_model_fixture, root_epoch) == 32, "partial identity offset");
_Static_assert(offsetof(struct partial_model_fixture, producer_valid) == 96, "partial counts offset");
_Static_assert(offsetof(struct partial_model_fixture, witness_seen) == 184, "partial witness offset");

#endif
