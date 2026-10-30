#include "status_cell.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define STATUS_CURRENT_PROCESS ((HANDLE)(LONG_PTR)-1)
_Static_assert(sizeof(WCHAR)==2,"NT Unicode width16");
struct macdrv_status_cell
{
    HANDLE section;
    struct macdrv_status_wire *view;
    SIZE_T size;
    struct macdrv_status_identity identity;
    int writer,ready,closing;
};
struct status_name
{
    WCHAR buffer[240];
    UNICODE_STRING text;
    OBJECT_ATTRIBUTES attributes;
};
static NTSTATUS make_name(const struct macdrv_status_identity *id,struct status_name *name)
{
    char ascii[240];DWORD session;size_t i;int length;NTSTATUS status;
    if(!macdrv_status_identity_valid(id))return STATUS_INVALID_PARAMETER;
    status=NtQueryInformationProcess(STATUS_CURRENT_PROCESS,ProcessSessionInformation,&session,sizeof(session),NULL);
    if(status)return status;
    if(session!=id->session_id)return STATUS_INVALID_PARAMETER;
    length=snprintf(ascii,sizeof(ascii),"\\Sessions\\%u\\BaseNamedObjects\\Cider.RenderStatus.%u.%08x.%016llx.%016llx.%016llx",
        (unsigned)session,(unsigned)id->kind,(unsigned)id->root_pid,
        (unsigned long long)id->root_birth,(unsigned long long)id->lease,(unsigned long long)id->ticket);
    if(length<0||(size_t)length>=sizeof(ascii))return STATUS_NAME_TOO_LONG;
    for(i=0;i<=(size_t)length;++i)name->buffer[i]=(WCHAR)(unsigned char)ascii[i];
    name->text.Buffer=name->buffer;name->text.Length=(USHORT)(length*sizeof(WCHAR));
    name->text.MaximumLength=(USHORT)((length+1)*sizeof(WCHAR));
    memset(&name->attributes,0,sizeof(name->attributes));name->attributes.Length=sizeof(name->attributes);
    name->attributes.ObjectName=&name->text;name->attributes.Attributes=OBJ_CASE_INSENSITIVE;
    return STATUS_SUCCESS;
}
static NTSTATUS allocate_cell(const struct macdrv_status_identity *id,struct macdrv_status_cell **out)
{
    struct macdrv_status_cell *cell;
    if(!out)return STATUS_INVALID_PARAMETER;
    *out=NULL;
    if(!macdrv_status_identity_valid(id))return STATUS_INVALID_PARAMETER;
    cell=calloc(1,sizeof(*cell));if(!cell)return STATUS_NO_MEMORY;
    cell->identity=*id;*out=cell;return STATUS_SUCCESS;
}
NTSTATUS macdrv_status_cell_create(const struct macdrv_status_identity *id,const struct macdrv_status_values *values,
                                  struct macdrv_status_cell **out)
{
    struct macdrv_status_cell *cell;struct status_name name;LARGE_INTEGER maximum;
    PROCESS_BASIC_INFORMATION basic;KERNEL_USER_TIMES times;NTSTATUS status;uint64_t birth;
    if(!out)return STATUS_INVALID_PARAMETER;
    *out=NULL;
    if(!macdrv_status_identity_valid(id)||!macdrv_status_values_valid(values))return STATUS_INVALID_PARAMETER;
    status=NtQueryInformationProcess(STATUS_CURRENT_PROCESS,ProcessBasicInformation,&basic,sizeof(basic),NULL);
    if(status)return status;
    status=NtQueryInformationProcess(STATUS_CURRENT_PROCESS,ProcessTimes,&times,sizeof(times),NULL);
    if(status)return status;
#ifdef _WIN32
    birth=(uint64_t)times.CreateTime.dwLowDateTime|((uint64_t)times.CreateTime.dwHighDateTime<<32);
#else
    birth=(uint64_t)times.CreateTime.QuadPart;
#endif
    if((ULONG_PTR)basic.UniqueProcessId!=id->root_pid||birth!=id->root_birth)
        return STATUS_INVALID_PARAMETER;
    status=make_name(id,&name);if(status)return status;
    status=allocate_cell(id,out);if(status)return status;cell=*out;cell->writer=1;maximum.QuadPart=4096;
    /* No OBJ_OPENIF: an existing name must fail, never overwrite another page. */
    status=NtCreateSection(&cell->section,SECTION_MAP_READ|SECTION_MAP_WRITE|SECTION_QUERY,
        &name.attributes,&maximum,PAGE_READWRITE,SEC_COMMIT,NULL);if(status)return status;
    status=NtMapViewOfSection(cell->section,STATUS_CURRENT_PROCESS,(void **)&cell->view,0,0,NULL,&cell->size,2,0,PAGE_READWRITE);
    if(status)return status;
    if(!cell->view||cell->size<sizeof(*cell->view))return STATUS_BUFFER_TOO_SMALL;
    if(!macdrv_status_initialize(cell->view,id,values))return STATUS_DATA_ERROR;
    cell->ready=1;return STATUS_SUCCESS;
}
NTSTATUS macdrv_status_cell_open(const struct macdrv_status_identity *id,struct macdrv_status_cell **out)
{
    struct macdrv_status_cell *cell;struct status_name name;NTSTATUS status;
    if(!out)return STATUS_INVALID_PARAMETER;
    *out=NULL;status=make_name(id,&name);if(status)return status;
    status=allocate_cell(id,out);if(status)return status;cell=*out;
    status=NtOpenSection(&cell->section,SECTION_MAP_READ|SECTION_QUERY,&name.attributes);if(status)return status;
    status=NtMapViewOfSection(cell->section,STATUS_CURRENT_PROCESS,(void **)&cell->view,0,0,NULL,&cell->size,2,0,PAGE_READONLY);
    if(status)return status;
    if(!cell->view||cell->size<sizeof(*cell->view))return STATUS_BUFFER_TOO_SMALL;
    cell->ready=1;return STATUS_SUCCESS;
}
enum macdrv_status_publish_result macdrv_status_cell_publish(struct macdrv_status_cell *cell,
    const struct macdrv_status_values *values,int safe)
{
    if(!cell||!cell->ready||!cell->writer||cell->closing||!cell->view)return MACDRV_STATUS_PUBLISH_INVALID;
    return macdrv_status_publish_wire(cell->view,values,safe);
}
enum macdrv_status_read_result macdrv_status_cell_read(const struct macdrv_status_cell *cell,struct macdrv_status_snapshot *out)
{
    if(!cell||!cell->ready||cell->closing||!cell->view)return MACDRV_STATUS_READ_INVALID;
    return macdrv_status_read_wire(cell->view,&cell->identity,out);
}
NTSTATUS macdrv_status_cell_close_handle(struct macdrv_status_cell *cell)
{
    NTSTATUS status;
    if(!cell||cell->writer)return STATUS_INVALID_PARAMETER;
    if(!cell->section)return STATUS_SUCCESS;
    status=NtClose(cell->section);if(!status)cell->section=NULL;
    return status;
}
NTSTATUS macdrv_status_cell_close(struct macdrv_status_cell **owner)
{
    struct macdrv_status_cell *cell;NTSTATUS status;
    if(!owner)return STATUS_INVALID_PARAMETER;
    if(!(cell=*owner))return STATUS_SUCCESS;
    cell->closing=1;cell->ready=0;
    if(cell->view)
    {
        status=NtUnmapViewOfSection(STATUS_CURRENT_PROCESS,cell->view);if(status)return status;
        cell->view=NULL;cell->size=0;
    }
    if(cell->section)
    {
        status=NtClose(cell->section);if(status)return status;
        cell->section=NULL;
    }
    free(cell);*owner=NULL;return STATUS_SUCCESS;
}
const void *macdrv_status_cell_view(const struct macdrv_status_cell *cell,SIZE_T *size)
{
    if(size)*size=cell?cell->size:0;
    return cell&&!cell->closing?cell->view:NULL;
}
HANDLE macdrv_status_cell_handle(const struct macdrv_status_cell *cell)
{
    return cell?cell->section:NULL;
}
