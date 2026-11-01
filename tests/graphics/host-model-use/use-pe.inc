#include "use.h"
NTSTATUS WINAPI cider_model_use_fixture(void *args)
{
    struct use_frame *f=args;
    NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<USE_UPDATE||f->operation>USE_REMOVE)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(model_use_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
