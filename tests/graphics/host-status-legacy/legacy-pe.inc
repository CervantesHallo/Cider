#include "legacy.h"
NTSTATUS WINAPI cider_host_legacy_fixture(void *args)
{
    struct legacy_frame *f=args;
    NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<LEGACY_CREATE||f->operation>LEGACY_CLEANUP)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(host_legacy_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
