#include "use.h"
extern void macdrv_model_use_cocoa(void *,unsigned int,uint64_t,struct use_frame *);
NTSTATUS macdrv_model_use_fixture(void *args)
{
    struct use_frame *f=args;
    struct macdrv_status_snapshot snapshot={0};
    if(!f||f->size!=sizeof(*f)||f->version!=1||f->operation<USE_UPDATE||f->operation>USE_REMOVE||f->reserved)
        return STATUS_INVALID_PARAMETER;
    f->status=USE_BAD_STATE;
    if(!live_host.install_sent||!live_host.window||!live_host.context||!live_host.host_reader||
       wine_server_ptr_handle(f->root)!=live_host.root||wine_server_ptr_handle(f->child)!=live_host.child)
        return STATUS_SUCCESS;
    f->context=live_host.context_id;f->epoch=live_host.epoch;f->lease=live_host.lease;
    f->ticket=live_host.ticket;f->sequence=live_host.sequence;
    macdrv_model_use_cocoa(live_host.window,live_host.context_id,live_host.ticket,f);
    f->context_owned=live_host.context!=NULL;
    f->host_read=macdrv_status_cell_read(live_host.host_reader,&snapshot);
    f->host_valid=snapshot.values_valid;
    return STATUS_SUCCESS;
}
