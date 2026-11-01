#include "overlap.h"
/* RUN is deliberately independent of live_host_control_lock. The shared
 * overlap state has its own remote_lock discipline in the native helper. */
NTSTATUS WINAPI cider_host_overlap_fixture(void *args)
{
    struct overlap_frame *f=args;
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<OVERLAP_ARM||f->operation>OVERLAP_FINISH)
        return STATUS_INVALID_PARAMETER;
    return MACDRV_CALL(host_overlap_fixture,f);
}
