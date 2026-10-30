#include "fixture.h"
static SRWLOCK live_host_control_lock=SRWLOCK_INIT;
NTSTATUS WINAPI cider_live_host_fixture(void *args)
{
    struct live_host_fixture *f=args;
    NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<LIVE_SETUP||f->operation>LIVE_CLEANUP)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(live_host_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
