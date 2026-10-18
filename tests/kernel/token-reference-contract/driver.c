/* Own Wine requestors only. No caller-selected PID, handle, address or rights.
 * The matching client refuses native Windows. This is a Wine runtime fixture. */
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
#include "wine/exception.h"
#include "protocol.h"

static PACCESS_TOKEN held_token;
static POBJECT_TYPE token_type;
static LUID held_id;
static PDEVICE_OBJECT owned_device;
static void check(TOKEN_RESULT *r, BOOLEAN value, ULONG line, NTSTATUS status)
{
    ++r->checks;
    if (!value) {
        if (!r->failures) { r->first_failure = line; r->first_status = status; }
        ++r->failures;
    }
}
#define CHECK(r, value, status) check(r, !!(value), __LINE__, status)
static BOOLEAN same_id(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
static NTSTATUS token_id(PACCESS_TOKEN token, LUID *id)
{
    TOKEN_STATISTICS stats;
    HANDLE handle = NULL;
    ULONG bytes;
    NTSTATUS status, close_status;
    status = ObOpenObjectByPointer(token, 0, NULL, TOKEN_QUERY, token_type, KernelMode, &handle);
    if (status) return status;
    status = NtQueryInformationToken(handle, TokenStatistics, &stats, sizeof(stats), &bytes);
    close_status = ZwClose(handle);
    if (!status && !close_status) *id = stats.TokenId;
    return status ? status : close_status;
}
static NTSTATUS complete(PIRP irp, NTSTATUS status, ULONG_PTR bytes)
{
    irp->IoStatus.Status = status; irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT); return status;
}
typedef struct TOKEN_WORK {
    PIO_WORKITEM item;
    PEPROCESS process;
    PACCESS_TOKEN token;
    PKEVENT start;
    KEVENT ready, completed;
    LUID expected;
    TOKEN_RESULT result;
    BOOLEAN submitted, inject_failure, injection_reached;
} TOKEN_WORK;
static void release_work_token(TOKEN_WORK *work)
{
    if (work->token) {
        PACCESS_TOKEN token = work->token;
        work->token = NULL;
        __TRY { PsDereferencePrimaryToken(token); }
        __EXCEPT_ALL { CHECK(&work->result, FALSE, GetExceptionCode()); }
        __ENDTRY
    }
}
static void WINAPI token_worker(PDEVICE_OBJECT device, void *context)
{
    TOKEN_WORK *work = context;
    ULONG i;
    UNREFERENCED_PARAMETER(device);
    __TRY {
        PIO_WORKITEM item = work->item;
        work->item = NULL;
        IoFreeWorkItem(item);
        KeSetEvent(&work->ready, IO_NO_INCREMENT, FALSE);
        KeWaitForSingleObject(work->start, Executive, KernelMode, FALSE, NULL);
        for (i = 0; i < 32; ++i) {
            LUID id = {0};
            NTSTATUS status;
            work->token = PsReferencePrimaryToken(work->process);
            if (!work->token) RtlRaiseStatus(STATUS_NO_TOKEN);
            if (work->inject_failure) {
                work->injection_reached = TRUE;
                RtlRaiseStatus(STATUS_NO_MEMORY);
            }
            status = token_id(work->token, &id);
            CHECK(&work->result, status == STATUS_SUCCESS && same_id(id, work->expected), status);
            release_work_token(work);
            ++work->result.concurrent_rounds;
        }
    }
    __EXCEPT_ALL {
        if (work->injection_reached && work->token && (NTSTATUS)GetExceptionCode() == STATUS_NO_MEMORY) ++work->result.expected_faults;
        else CHECK(&work->result, FALSE, GetExceptionCode());
    }
    __ENDTRY
    release_work_token(work);
    /* Even an early callback exception must unblock the dispatch join. */
    KeSetEvent(&work->ready, IO_NO_INCREMENT, FALSE);
    KeSetEvent(&work->completed, IO_NO_INCREMENT, FALSE);
}
static void run_concurrent(TOKEN_RESULT *r, PEPROCESS process, ULONG inject_failure)
{
    TOKEN_WORK work[2];
    KEVENT start;
    ULONG i;
    volatile BOOLEAN queue_injection_reached = FALSE;
    RtlZeroMemory(work, sizeof(work));
    KeInitializeEvent(&start, NotificationEvent, FALSE);
    for (i = 0; i < 2; ++i) {
        work[i].process = process; work[i].start = &start; work[i].expected = r->target_id;
        work[i].inject_failure = inject_failure == 1 && i == 0;
        KeInitializeEvent(&work[i].ready, NotificationEvent, FALSE);
        KeInitializeEvent(&work[i].completed, NotificationEvent, FALSE);
    }
    __TRY {
        for (i = 0; i < 2; ++i) {
            work[i].item = IoAllocateWorkItem(owned_device);
            CHECK(r, work[i].item != NULL, STATUS_NO_MEMORY);
            if (!work[i].item) break;
            /* The checked IoQueueWorkItem throws only if submission failed;
             * normal return transfers item ownership to its callback. */
            if (inject_failure == 2 && i == 1) {
                queue_injection_reached = TRUE;
                RtlRaiseStatus(STATUS_NO_MEMORY);
            }
            IoQueueWorkItem(work[i].item, token_worker, DelayedWorkQueue, &work[i]);
            work[i].submitted = TRUE;
        }
        for (i = 0; i < 2; ++i) if (work[i].submitted) {
            LARGE_INTEGER timeout;
            NTSTATUS status;
            timeout.QuadPart = -50000000;
            status = KeWaitForSingleObject(&work[i].ready, Executive, KernelMode, FALSE, &timeout);
            CHECK(r, status == STATUS_SUCCESS, status);
        }
    }
    __EXCEPT_ALL {
        if (queue_injection_reached && (NTSTATUS)GetExceptionCode() == STATUS_NO_MEMORY) ++r->expected_faults;
        else CHECK(r, FALSE, GetExceptionCode());
    }
    __ENDTRY
    /* Runs after normal submission, allocation failure and queue exception.
     * Keep this IRP/stack alive until every actually submitted callback exits. */
    KeSetEvent(&start, IO_NO_INCREMENT, FALSE);
    for (i = 0; i < 2; ++i) {
        if (work[i].submitted) {
            LARGE_INTEGER timeout;
            NTSTATUS status;
            timeout.QuadPart = -100000000;
            status = KeWaitForSingleObject(&work[i].completed, Executive, KernelMode, FALSE, &timeout);
            CHECK(r, status == STATUS_SUCCESS, status);
            if (status) KeWaitForSingleObject(&work[i].completed, Executive, KernelMode, FALSE, NULL);
            CHECK(r, work[i].token == NULL, STATUS_UNSUCCESSFUL);
            r->expected_faults += work[i].result.expected_faults;
            r->checks += work[i].result.checks;
            if (work[i].result.failures && !r->failures) {
                r->first_failure = work[i].result.first_failure;
                r->first_status = work[i].result.first_status;
            }
            r->failures += work[i].result.failures;
            r->concurrent_rounds += work[i].result.concurrent_rounds;
        } else if (work[i].item) {
            PIO_WORKITEM item = work[i].item;
            work[i].item = NULL;
            IoFreeWorkItem(item);
        }
    }
}
static NTSTATUS WINAPI dispatch(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    TOKEN_RESULT *r;
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode, i;
    PACCESS_TOKEN a, b;
    HANDLE handle;
    TOKEN_STATISTICS stats;
    NTSTATUS status;
    ULONG bytes;
    UNREFERENCED_PARAMETER(device);
    if (stack->MajorFunction == IRP_MJ_CREATE || stack->MajorFunction == IRP_MJ_CLOSE)
        return complete(irp, STATUS_SUCCESS, 0);
    if (stack->MajorFunction != IRP_MJ_DEVICE_CONTROL ||
        (code != TOKEN_CHECK && code != TOKEN_HOLD && code != TOKEN_RELEASE))
        return complete(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    if (stack->Parameters.DeviceIoControl.InputBufferLength ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(*r) || !irp->AssociatedIrp.SystemBuffer)
        return complete(irp, STATUS_INVALID_BUFFER_SIZE, 0);
    r = irp->AssociatedIrp.SystemBuffer;
    RtlZeroMemory(r, sizeof(*r)); r->version = 2; r->bytes = sizeof(*r);
    if (code == TOKEN_RELEASE) {
        CHECK(r, held_token != NULL, STATUS_INVALID_DEVICE_STATE);
        if (held_token) {
            status = token_id(held_token, &r->held_id);
            CHECK(r, status == STATUS_SUCCESS && same_id(held_id, r->held_id), status);
            a = held_token; held_token = NULL; PsDereferencePrimaryToken(a);
        }
        return complete(irp, STATUS_SUCCESS, sizeof(*r));
    }
    a = PsReferencePrimaryToken(IoGetRequestorProcess(irp));
    CHECK(r, a != NULL, STATUS_NO_TOKEN);
    if (!a) return complete(irp, STATUS_SUCCESS, sizeof(*r));
    status = token_id(a, &r->target_id);
    CHECK(r, status == STATUS_SUCCESS, status);
    /* NtCurrentProcess resolves to the actual Wine driver host here. Its
     * different TokenId detects accidental caller/host substitution. */
    status = NtOpenProcessToken((HANDLE)(LONG_PTR)-1, TOKEN_QUERY, &handle);
    CHECK(r, status == STATUS_SUCCESS, status);
    if (!status) {
        status = NtQueryInformationToken(handle, TokenStatistics, &stats, sizeof(stats), &bytes);
        CHECK(r, status == STATUS_SUCCESS, status);
        if (!status) r->host_id = stats.TokenId;
        status = ZwClose(handle); CHECK(r, status == STATUS_SUCCESS, status);
        CHECK(r, !same_id(r->target_id, r->host_id), STATUS_UNSUCCESSFUL);
    }
    if (code == TOKEN_HOLD) {
        CHECK(r, held_token == NULL, STATUS_INVALID_DEVICE_STATE);
        if (!held_token) { held_token = a; held_id = r->target_id; a = NULL; }
    } else for (i = 0; i < 64; ++i) {
        b = PsReferencePrimaryToken(IoGetRequestorProcess(irp));
        CHECK(r, b == a, STATUS_UNSUCCESSFUL);
        status = token_id(b, &r->held_id);
        CHECK(r, status == STATUS_SUCCESS && same_id(r->target_id, r->held_id), status);
        PsDereferencePrimaryToken(b);
        status = token_id(a, &r->held_id);
        CHECK(r, status == STATUS_SUCCESS && same_id(r->target_id, r->held_id), status);
        /* Cross zero local references while the requestor process still owns
         * its primary token; reacquisition must adopt the same real object. */
        PsDereferencePrimaryToken(a);
        a = PsReferencePrimaryToken(IoGetRequestorProcess(irp));
        CHECK(r, a != NULL, STATUS_NO_TOKEN);
        ++r->rounds;
    }
    if (code == TOKEN_CHECK) {
        run_concurrent(r, IoGetRequestorProcess(irp), 0);
        for (i = 1; i <= 2; ++i) {
            TOKEN_RESULT fault = {0};
            fault.target_id = r->target_id;
            run_concurrent(&fault, IoGetRequestorProcess(irp), i);
            CHECK(r, fault.expected_faults == 1 && fault.concurrent_rounds == 32, STATUS_UNSUCCESSFUL);
            if (fault.failures && !r->failures) { r->first_failure = fault.first_failure; r->first_status = fault.first_status; }
            r->failures += fault.failures; r->checks += fault.checks; r->expected_faults += fault.expected_faults;
        }
    }
    if (a) PsDereferencePrimaryToken(a);
    return complete(irp, STATUS_SUCCESS, sizeof(*r));
}
static void WINAPI unload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING link;
    if (held_token) { PACCESS_TOKEN a = held_token; held_token = NULL; PsDereferencePrimaryToken(a); }
    RtlInitUnicodeString(&link, TOKEN_LINK); IoDeleteSymbolicLink(&link);
    if (driver->DeviceObject) IoDeleteDevice(driver->DeviceObject);
}
NTSTATUS WINAPI DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry)
{
    PDEVICE_OBJECT device;
    POBJECT_TYPE *type_export;
    UNICODE_STRING name, link;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(registry);
    RtlInitUnicodeString(&name, L"SeTokenObjectType");
    type_export = MmGetSystemRoutineAddress(&name);
    if (!type_export || !*type_export) return STATUS_PROCEDURE_NOT_FOUND;
    token_type = *type_export;
    RtlInitUnicodeString(&name, TOKEN_DEVICE);
    status = IoCreateDevice(driver, 0, &name, 0x834a, 0, TRUE, &device);
    if (status) return status;
    owned_device = device;
    RtlInitUnicodeString(&link, TOKEN_LINK); status = IoCreateSymbolicLink(&link, &name);
    if (status) { IoDeleteDevice(device); return status; }
    driver->MajorFunction[IRP_MJ_CREATE] = dispatch;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch;
    driver->DriverUnload = unload; device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
