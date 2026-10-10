/* Independent observation driver. No hooks, memory access interfaces, token
 * mutation, process attachment, background work, games or third-party drivers.
 */
#include <ntifs.h>
#include <wdmsec.h>
#include "../shared/protocol.h"

static const GUID reference_class =
    {0x2535cdad, 0x3ea1, 0x490b, {0x98, 0x30, 0x15, 0x53, 0x45, 0xa9, 0x11, 0x65}};

typedef struct CR_PRIVILEGE_SET {
    ULONG count;
    ULONG control;
    LUID_AND_ATTRIBUTES values[CR_PRIVILEGE_COUNT];
} CR_PRIVILEGE_SET;

C_ASSERT(FIELD_OFFSET(CR_PRIVILEGE_SET, values) == FIELD_OFFSET(PRIVILEGE_SET, Privilege));
C_ASSERT(sizeof(CR_REQUEST) == 32);
C_ASSERT(sizeof(CR_SET_OBSERVATION) == 40);
C_ASSERT(sizeof(CR_RESPONSE) == 300);

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD reference_unload;
DRIVER_DISPATCH reference_create;
DRIVER_DISPATCH reference_close;
DRIVER_DISPATCH reference_control;

static NTSTATUS complete(PIRP irp, NTSTATUS status, ULONG_PTR bytes)
{
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = NT_SUCCESS(status) ? bytes : 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static BOOLEAN same_luid(LUID a, LUID b)
{
    return a.LowPart == b.LowPart && a.HighPart == b.HighPart;
}

static NTSTATUS observe(PIRP irp, const CR_REQUEST *request, CR_RESPONSE *response)
{
    SECURITY_SUBJECT_CONTEXT subject;
    PACCESS_TOKEN primary, client;
    PTOKEN_PRIVILEGES privileges = NULL;
    BOOLEAN copy_on_open = FALSE, effective_only = FALSE;
    SECURITY_IMPERSONATION_LEVEL level = SecurityAnonymous;
    NTSTATUS status;
    ULONG i, j;

    /* All observations are synchronous, on the original requestor's context. */
    if (KeGetCurrentIrql() != PASSIVE_LEVEL || irp->RequestorMode != UserMode ||
        IoGetRequestorProcess(irp) != PsGetCurrentProcess() ||
        irp->Tail.Overlay.Thread != PsGetCurrentThread())
        return STATUS_INVALID_DEVICE_STATE;

    RtlZeroMemory(response, sizeof(*response));
    response->version = CR_PROTOCOL_VERSION;
    response->size = sizeof(*response);
    response->irql = KeGetCurrentIrql();
    response->requestor_mode = irp->RequestorMode;
    response->current_process_is_requestor = 1;
    response->current_thread_is_requestor = 1;
    /* SinglePrivilegeCheck captures the current thread independently. Do not
     * call it while holding locks on an already-captured subject. */
    for (i = 0; i < CR_PRIVILEGE_COUNT; ++i) {
        response->single_user[i] = SeSinglePrivilegeCheck(request->privileges[i], UserMode);
        response->single_kernel[i] = SeSinglePrivilegeCheck(request->privileges[i], KernelMode);
    }

    RtlZeroMemory(&subject, sizeof(subject));
    SeCaptureSubjectContext(&subject);
    response->context_capture_calls = 1;
    if (!subject.PrimaryToken) {
        SeReleaseSubjectContext(&subject);
        return STATUS_NOT_SUPPORTED;
    }
    response->captured_client_present = subject.ClientToken != NULL;
    if (subject.ClientToken)
        response->captured_impersonation_level = subject.ImpersonationLevel;

    primary = PsReferencePrimaryToken(PsGetCurrentProcess());
    response->primary_reference_calls = 1;
    response->primary_reference_matches_capture = primary == subject.PrimaryToken;
    if (primary) {
        PsDereferencePrimaryToken(primary);
        response->primary_dereference_calls = 1;
    }
    client = PsReferenceImpersonationToken(PsGetCurrentThread(), &copy_on_open, &effective_only, &level);
    response->client_reference_calls = 1;
    response->client_reference_present = client != NULL;
    response->client_reference_matches_capture = client == subject.ClientToken;
    if (client) {
        response->client_reference_level = level;
        response->client_copy_on_open = copy_on_open;
        response->client_effective_only = effective_only;
        PsDereferenceImpersonationToken(client);
        response->client_dereference_calls = 1;
    }

    SeLockSubjectContext(&subject);
    response->context_lock_calls = 1;
    status = SeQueryInformationToken(SeQuerySubjectContextToken(&subject), TokenPrivileges,
                                    (PVOID *)&privileges);
    response->token_query_status = status;
    if (NT_SUCCESS(status)) {
        for (i = 0; i < CR_PRIVILEGE_COUNT; ++i)
            for (j = 0; j < privileges->PrivilegeCount; ++j)
                if (same_luid(request->privileges[i], privileges->Privileges[j].Luid)) {
                    response->privileges_present[i] = 1;
                    response->privilege_attributes[i] = privileges->Privileges[j].Attributes;
                    break;
                }
    }

    for (i = 0; i < CR_SET_COUNT; ++i) {
        CR_PRIVILEGE_SET required;
        CR_SET_OBSERVATION *row = &response->sets[i];
        KPROCESSOR_MODE mode = i < 2 ? UserMode : KernelMode;
        RtlZeroMemory(&required, sizeof(required));
        required.count = CR_PRIVILEGE_COUNT;
        required.control = (i & 1) ? PRIVILEGE_SET_ALL_NECESSARY : 0;
        for (j = 0; j < CR_PRIVILEGE_COUNT; ++j)
            required.values[j].Luid = request->privileges[j];
        row->mode = mode;
        row->control = required.control;
        row->count = required.count;
        row->result = SePrivilegeCheck((PPRIVILEGE_SET)&required, &subject, mode);
        for (j = 0; j < CR_PRIVILEGE_COUNT; ++j) {
            row->attributes_before[j] = 0;
            row->attributes_after[j] = required.values[j].Attributes;
        }
    }
    if (privileges) ExFreePool(privileges);
    SeUnlockSubjectContext(&subject);
    response->context_unlock_calls = 1;
    SeReleaseSubjectContext(&subject);
    response->context_release_calls = 1;
    /* Query failure remains visible in the response, never becomes a fabricated
     * privilege list. IOCTL transport success is distinct from that API status. */
    return STATUS_SUCCESS;
}

NTSTATUS reference_create(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    UNREFERENCED_PARAMETER(device);
    /* A single control object; no namespace below it. */
    return complete(irp, stack->FileObject->FileName.Length ? STATUS_OBJECT_NAME_NOT_FOUND : STATUS_SUCCESS, 0);
}

NTSTATUS reference_close(PDEVICE_OBJECT device, PIRP irp)
{
    UNREFERENCED_PARAMETER(device);
    return complete(irp, STATUS_SUCCESS, 0);
}

NTSTATUS reference_control(PDEVICE_OBJECT device, PIRP irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    CR_REQUEST request;
    CR_RESPONSE response;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(device);
    if (stack->Parameters.DeviceIoControl.IoControlCode != CR_IOCTL_OBSERVE)
        return complete(irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    if (stack->Parameters.DeviceIoControl.InputBufferLength != sizeof(request) ||
        stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(response) ||
        !irp->AssociatedIrp.SystemBuffer)
        return complete(irp, STATUS_INVALID_BUFFER_SIZE, 0);
    RtlCopyMemory(&request, irp->AssociatedIrp.SystemBuffer, sizeof(request));
    if (request.version != CR_PROTOCOL_VERSION || request.size != sizeof(request))
        return complete(irp, STATUS_REVISION_MISMATCH, 0);
    status = observe(irp, &request, &response);
    if (!NT_SUCCESS(status)) return complete(irp, status, 0);
    RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, &response, sizeof(response));
    return complete(irp, STATUS_SUCCESS, sizeof(response));
}

VOID reference_unload(PDRIVER_OBJECT driver)
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(CR_SYMBOLIC_NAME);
    IoDeleteSymbolicLink(&name);
    if (driver->DeviceObject) IoDeleteDevice(driver->DeviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path)
{
    UNICODE_STRING name = RTL_CONSTANT_STRING(CR_DEVICE_NAME);
    UNICODE_STRING link = RTL_CONSTANT_STRING(CR_SYMBOLIC_NAME);
    UNICODE_STRING sddl = RTL_CONSTANT_STRING(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    PDEVICE_OBJECT device;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(registry_path);
    status = IoCreateDeviceSecure(driver, 0, &name, CR_DEVICE_TYPE, FILE_DEVICE_SECURE_OPEN,
                                  FALSE, &sddl, &reference_class, &device);
    if (!NT_SUCCESS(status)) return status;
    driver->MajorFunction[IRP_MJ_CREATE] = reference_create;
    driver->MajorFunction[IRP_MJ_CLOSE] = reference_close;
    driver->MajorFunction[IRP_MJ_CLEANUP] = reference_close;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = reference_control;
    driver->DriverUnload = reference_unload;
    status = IoCreateSymbolicLink(&link, &name);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(device);
        return status;
    }
    device->Flags |= DO_BUFFERED_IO;
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
