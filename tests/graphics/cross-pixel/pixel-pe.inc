#include "pixel.h"
NTSTATUS WINAPI cider_pixel_fixture(void *args)
{
    struct pixel_frame *f=args;NTSTATUS status;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<PIXEL_RED||f->operation>PIXEL_DRAIN)
        return STATUS_INVALID_PARAMETER;
    AcquireSRWLockExclusive(&live_host_control_lock);
    status=MACDRV_CALL(pixel_fixture,f);
    ReleaseSRWLockExclusive(&live_host_control_lock);
    return status;
}
