#ifndef CIDER_RENDER_STATUS_CELL_H
#define CIDER_RENDER_STATUS_CELL_H
#ifdef _WIN32
#define WIN32_NO_STATUS
#include <windows.h>
#include <winternl.h>
#undef WIN32_NO_STATUS
#include <ntstatus.h>
/* Native APIs omitted by the installed MinGW public header. */
NTSTATUS NTAPI NtCreateSection(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PLARGE_INTEGER,ULONG,ULONG,HANDLE);
NTSTATUS NTAPI NtOpenSection(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES);
NTSTATUS NTAPI NtMapViewOfSection(HANDLE,HANDLE,PVOID *,ULONG_PTR,SIZE_T,PLARGE_INTEGER,PSIZE_T,int,ULONG,ULONG);
NTSTATUS NTAPI NtUnmapViewOfSection(HANDLE,PVOID);
#else
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winternl.h"
#endif
#include "status_wire.h"
struct macdrv_status_cell;
/* Create/open/close require a Wine thread. Publish/read perform no NT calls.
 * Objects are externally serialized and pinned by the caller across each use.
 * out retains partial resources after ANY failed NT call; always close non-NULL
 * out. A failed close retains the object/remaining handle or view for retry.
 * Close only after callback/publication users have drained; no concurrent free.
 */
NTSTATUS macdrv_status_cell_create(const struct macdrv_status_identity *,const struct macdrv_status_values *,struct macdrv_status_cell **out);
NTSTATUS macdrv_status_cell_open(const struct macdrv_status_identity *,struct macdrv_status_cell **out);
enum macdrv_status_publish_result macdrv_status_cell_publish(struct macdrv_status_cell *,const struct macdrv_status_values *,int safe);
enum macdrv_status_read_result macdrv_status_cell_read(const struct macdrv_status_cell *,struct macdrv_status_snapshot *);
NTSTATUS macdrv_status_cell_close_handle(struct macdrv_status_cell *); /* mapped view remains owned */
NTSTATUS macdrv_status_cell_close(struct macdrv_status_cell **owner);
const void *macdrv_status_cell_view(const struct macdrv_status_cell *,SIZE_T *size);
HANDLE macdrv_status_cell_handle(const struct macdrv_status_cell *); /* borrowed, no ownership transfer */
#endif
