#include "async.h"
NTSTATUS WINAPI cider_host_async_fixture(void *args)
{
    struct async_frame *f=args;NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<ASYNC_RUN||f->operation>ASYNC_ABORT)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(host_async_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
