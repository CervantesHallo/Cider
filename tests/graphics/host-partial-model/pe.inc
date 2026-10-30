/* Include at end of pinned diagnostic remote_watch_pe.c. Existing actual live
 * counters must span watch allocation/free and timer create/delete frames. */
#include "fixture.h"
static SRWLOCK partial_model_control_lock = SRWLOCK_INIT;
NTSTATUS WINAPI cider_partial_model_fixture(void *args)
{
    struct partial_model_fixture *f = args;
    NTSTATUS status;
    if (!f || f->size != sizeof(*f) || f->version != PARTIAL_MODEL_VERSION || f->reserved ||
        f->operation < PARTIAL_MODEL_SETUP || f->operation > PARTIAL_MODEL_FINISH || !f->root || !f->child)
        return STATUS_INVALID_PARAMETER;
    /* No timer/ordinary callback takes this private lock. Native calls can ARM
     * and STOP through the ordinary callback without entering this wrapper. */
    AcquireSRWLockExclusive(&partial_model_control_lock);
    status = MACDRV_CALL(partial_model_fixture, f);
    f->pe_watches = InterlockedCompareExchange(&fixture_live_watches, 0, 0);
    f->pe_timers = InterlockedCompareExchange(&fixture_live_timers, 0, 0);
    if (!status && !f->status && f->operation == PARTIAL_MODEL_CLEANUP && (f->pe_watches || f->pe_timers))
        f->status = ERROR_IO_PENDING; /* actual remaining PE owners; effect may already be applied */
    ReleaseSRWLockExclusive(&partial_model_control_lock);
    return status;
}
