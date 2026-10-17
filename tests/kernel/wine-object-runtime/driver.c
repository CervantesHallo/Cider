/* Self-owned event/reference/work-item fixture for Wine's user-space driver
 * host. No token APIs, third-party drivers, memory interfaces or hooks. */
#include <stdarg.h>
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/ntddk.h"
#include "ddk/ntifs.h"
#include "ddk/wdm.h"
#include "protocol.h"

static PDRIVER_OBJECT owned_driver;
static PDEVICE_OBJECT owned_device;

static void check(OWNED_RESULT *r, BOOLEAN condition, ULONG line, NTSTATUS status)
{
    ++r->checks;
    if (!condition) {
        if (!r->failures) { r->first_failure = line; r->first_status = status; }
        ++r->failures;
    }
}
#define CHECK(r, condition, status) check(r, !!(condition), __LINE__, status)

typedef struct OWNED_WORK {
    KEVENT start, pinned, release, completed;
    PIO_WORKITEM item;
    HANDLE event;
    POBJECT_TYPE event_type;
    NTSTATUS reference_status;
    BOOLEAN hold_reference;
    volatile LONG called;
} OWNED_WORK;

static void WINAPI work_callback(PDEVICE_OBJECT device, void *context)
{
    OWNED_WORK *work = context;
    UNREFERENCED_PARAMETER(device);
    /* K3 must capture its device before a callback frees the work item. */
    IoFreeWorkItem(work->item);
    KeWaitForSingleObject(&work->start, Executive, KernelMode, FALSE, NULL);
    {
        void *object = NULL;
        work->reference_status = ObReferenceObjectByHandle(work->event, 0, work->event_type,
            KernelMode, &object, NULL);
        if (work->hold_reference) {
            KeSetEvent(&work->pinned, IO_NO_INCREMENT, FALSE);
            KeWaitForSingleObject(&work->release, Executive, KernelMode, FALSE, NULL);
        }
        if (work->reference_status == STATUS_SUCCESS) {
            /* The original handle may already be closed. A successful owned
             * reference must keep the returned event usable until release. */
            KeReadStateEvent(object);
            ObDereferenceObject(object);
        }
    }
    InterlockedIncrement(&work->called);
    KeSetEvent(&work->completed, IO_NO_INCREMENT, FALSE);
}

static NTSTATUS complete(PIRP irp, NTSTATUS status, ULONG_PTR bytes)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS WINAPI dispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    OWNED_RESULT *r;
    UNICODE_STRING exported;
    POBJECT_TYPE *event_type, *driver_type;
    NTSTATUS status;
    ULONG i;
    UNREFERENCED_PARAMETER(device);
    if (stack->MajorFunction == IRP_MJ_CREATE || stack->MajorFunction == IRP_MJ_CLOSE)
        return complete(irp, STATUS_SUCCESS, 0);
    if (stack->MajorFunction != IRP_MJ_DEVICE_CONTROL ||
        stack->Parameters.DeviceIoControl.IoControlCode != OWNED_IOCTL)
        return complete(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    if (stack->Parameters.DeviceIoControl.InputBufferLength ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(*r) || !irp->AssociatedIrp.SystemBuffer)
        return complete(irp, STATUS_INVALID_BUFFER_SIZE, 0);
    r = irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(r, sizeof(*r)); r->version = 2; r->bytes = sizeof(*r);
    RtlInitUnicodeString(&exported, L"ExEventObjectType");
    event_type = MmGetSystemRoutineAddress(&exported);
    RtlInitUnicodeString(&exported, L"IoDriverObjectType");
    driver_type = MmGetSystemRoutineAddress(&exported);
    CHECK(r, event_type != NULL && driver_type != NULL, STATUS_PROCEDURE_NOT_FOUND);
    if (!event_type || !driver_type) return complete(irp, STATUS_SUCCESS, sizeof(*r));

    for (i = 0; i < 256; ++i) {
        HANDLE event = NULL;
        void *object = NULL, *output = (void *)(ULONG_PTR)0x1234;
        LARGE_INTEGER timeout;
        status = ZwCreateEvent(&event, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);
        CHECK(r, status == STATUS_SUCCESS, status);
        if (status) break;
        /* This optional output is explicitly unsupported by the candidate.
         * Its status also distinguishes the repaired path from the old body. */
        status = ObReferenceObjectByHandle(event, 0, *event_type, KernelMode,
            &output, (POBJECT_HANDLE_INFORMATION)&timeout);
        r->unsupported_info_status = status;
        CHECK(r, status == STATUS_NOT_IMPLEMENTED && output == (void *)(ULONG_PTR)0x1234, status);
        if (status == STATUS_SUCCESS && output != (void *)(ULONG_PTR)0x1234) ObDereferenceObject(output);
        status = ObReferenceObjectByHandle(event, 0, *event_type, KernelMode, &object, NULL);
        CHECK(r, status == STATUS_SUCCESS && object != NULL, status);
        ZwClose(event);
        if (!status && object) {
            CHECK(r, KeReadStateEvent(object) == 0, STATUS_UNSUCCESSFUL);
            KeSetEvent(object, IO_NO_INCREMENT, FALSE);
            timeout.QuadPart = -1000000;
            status = KeWaitForSingleObject(object, Executive, KernelMode, FALSE, &timeout);
            CHECK(r, status == STATUS_SUCCESS, status);
            ObDereferenceObject(object);
            ++r->event_rounds;
        }
    }
    {
        void *object = NULL;
        status = ObReferenceObjectByName(&owned_driver->DriverName, OBJ_CASE_INSENSITIVE,
            NULL, 0, *driver_type, KernelMode, NULL, &object);
        CHECK(r, status == STATUS_SUCCESS && object == owned_driver, status);
        if (!status && object) ObDereferenceObject(object);
    }
    for (i = 0; i < 64; ++i) {
        OWNED_WORK work;
        LARGE_INTEGER timeout;
        RtlZeroMemory(&work, sizeof(work));
        status = ZwCreateEvent(&work.event, EVENT_ALL_ACCESS, NULL, NotificationEvent, FALSE);
        CHECK(r, status == STATUS_SUCCESS, status);
        if (status) break;
        work.event_type = *event_type;
        work.hold_reference = i % 3 == 0;
        KeInitializeEvent(&work.start, NotificationEvent, FALSE);
        KeInitializeEvent(&work.pinned, NotificationEvent, FALSE);
        KeInitializeEvent(&work.release, NotificationEvent, FALSE);
        KeInitializeEvent(&work.completed, NotificationEvent, FALSE);
        work.item = IoAllocateWorkItem(owned_device);
        CHECK(r, work.item != NULL, STATUS_NO_MEMORY);
        if (!work.item) { ZwClose(work.event); break; }
        IoQueueWorkItem(work.item, work_callback, DelayedWorkQueue, &work);
        if (i % 3 == 1) ZwClose(work.event);
        KeSetEvent(&work.start, IO_NO_INCREMENT, FALSE);
        if (work.hold_reference) {
            timeout.QuadPart = -100000000;
            status = KeWaitForSingleObject(&work.pinned, Executive, KernelMode, FALSE, &timeout);
            CHECK(r, status == STATUS_SUCCESS && work.reference_status == STATUS_SUCCESS, status);
        }
        /* Ordered positive/closed controls and real close/pin races all use
         * an actual callback thread; no inner backend operation is mocked. */
        if (i % 3 != 1) ZwClose(work.event);
        KeSetEvent(&work.release, IO_NO_INCREMENT, FALSE);
        timeout.QuadPart = -100000000;
        status = KeWaitForSingleObject(&work.completed, Executive, KernelMode, FALSE, &timeout);
        CHECK(r, status == STATUS_SUCCESS && work.called == 1, status);
        if (status != STATUS_SUCCESS) {
            /* Keep stack context alive until the callback really exits; the
             * external owned-prefix deadline stops a genuinely stuck host. */
            KeWaitForSingleObject(&work.completed, Executive, KernelMode, FALSE, NULL);
            break;
        }
        CHECK(r, work.reference_status == STATUS_SUCCESS || work.reference_status == STATUS_INVALID_HANDLE
            || work.reference_status == STATUS_OBJECT_TYPE_MISMATCH, work.reference_status);
        if (work.reference_status == STATUS_SUCCESS) ++r->concurrent_references;
        else ++r->concurrent_closed_handles;
        ++r->callbacks;
    }
    return complete(irp, STATUS_SUCCESS, sizeof(*r));
}

static void WINAPI unload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, OWNED_LINK); IoDeleteSymbolicLink(&name);
    if (driver->DeviceObject) IoDeleteDevice(driver->DeviceObject);
}

NTSTATUS WINAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    UNICODE_STRING name, link;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(registry);
    owned_driver = driver;
    RtlInitUnicodeString(&name, OWNED_DEVICE);
    status = IoCreateDevice(driver, 0, &name, 0x8349, 0, FALSE, &owned_device);
    if (status) return status;
    RtlInitUnicodeString(&link, OWNED_LINK);
    status = IoCreateSymbolicLink(&link, &name);
    if (status) { IoDeleteDevice(owned_device); return status; }
    driver->MajorFunction[IRP_MJ_CREATE] = dispatch;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    driver->DriverUnload = unload;
    owned_device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
