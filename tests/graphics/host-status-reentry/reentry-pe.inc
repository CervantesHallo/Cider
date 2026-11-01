#include "reentry.h"
NTSTATUS WINAPI cider_host_reentry_fixture(void *args)
{
    struct reentry_frame *f=args;
    NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<REENTRY_ARM||f->operation>REENTRY_ABORT)
        return STATUS_INVALID_PARAMETER;
    if(f->operation==REENTRY_RUN_CANCEL)return MACDRV_CALL(host_reentry_fixture,f);
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(host_reentry_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
