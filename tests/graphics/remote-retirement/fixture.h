#ifndef CIDER_RETIREMENT_FIXTURE_H
#define CIDER_RETIREMENT_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

#define RETIREMENT_FIXTURE_VERSION 1u

enum retirement_fixture_operation
{
    RETIREMENT_FIXTURE_SETUP = 1,
    RETIREMENT_FIXTURE_REVOKE_HELD = 2,
    RETIREMENT_FIXTURE_SNAPSHOT = 3,
    RETIREMENT_FIXTURE_SUBMIT_HELD = 4,
    RETIREMENT_FIXTURE_CLEANUP = 5
};

/* Private synchronous cider_retirement_fixture ABI, identical in PE32/PE64/native.
 * The export returns NTSTATUS; status is the actual Win32 operation result.
 * Only CLEANUP permits ERROR_IO_PENDING. SNAPSHOT must return success.
 * CLEANUP must also release a held retirement after any partial failure, and
 * retain the real CAContext until native quiescence; callers always attempt it.
 * This frame contains IDs and counts, never native pointers or owned handles.
 */
struct retirement_fixture
{
    uint32_t size;
    uint32_t version;
    uint32_t operation;
    uint32_t status;
    uint32_t root;
    uint32_t child;
    uint32_t context_id;
    uint32_t model_found;
    uint64_t lease;
    uint64_t ticket;
    uint32_t pending;
    uint32_t watched;
    uint32_t finalized;
    uint32_t completion_seen;
    uint32_t retirement_total;
    uint32_t pe_watches;
    uint32_t pe_timers;
    uint32_t reserved;
};

_Static_assert(sizeof(struct retirement_fixture) == 80, "retirement fixture must be 80 bytes");
_Static_assert(offsetof(struct retirement_fixture, lease) == 32, "retirement lease offset");
_Static_assert(offsetof(struct retirement_fixture, pending) == 48, "retirement pending offset");
_Static_assert(offsetof(struct retirement_fixture, reserved) == 76, "retirement reserved offset");

#endif
