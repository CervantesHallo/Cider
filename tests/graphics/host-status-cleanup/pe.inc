/* Include at end of pinned diagnostic remote_watch_pe.c. No global wrapper
 * serialization: SNAPSHOT must run concurrently with a held RUN_CANCEL. */
#include "fixture.h"
NTSTATUS WINAPI cider_host_cleanup_fixture(void *args)
{
    struct host_cleanup_fixture *f = args;
    NTSTATUS status;
    if (!f || f->size != sizeof(*f) || f->version != HOST_CLEANUP_VERSION || f->reserved ||
        f->operation < HOST_CLEANUP_BIND || f->operation > HOST_CLEANUP_DISARM) return STATUS_INVALID_PARAMETER;
    status = MACDRV_CALL(host_cleanup_fixture, f);
    f->pe_watches = InterlockedCompareExchange(&fixture_live_watches, 0, 0);
    f->pe_timers = InterlockedCompareExchange(&fixture_live_timers, 0, 0);
    return status;
}
