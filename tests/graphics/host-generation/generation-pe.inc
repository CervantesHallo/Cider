#include "generation.h"
NTSTATUS WINAPI cider_generation_fixture(void *args)
{
    struct generation_frame *f=args;NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<GENERATION_RUN||f->operation>GENERATION_CLEANUP)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(generation_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
