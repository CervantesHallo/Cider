/* Fixed-size, pointer-free protocol for Cider's independent native reference.
 * The sole IOCTL observes the calling thread. It accepts no process IDs,
 * addresses, handles, paths or requested access masks.
 */
#ifndef CIDER_REFERENCE_PROTOCOL_H
#define CIDER_REFERENCE_PROTOCOL_H

#define CR_PROTOCOL_VERSION 1u
#define CR_PRIVILEGE_COUNT 3u
#define CR_SET_COUNT 4u
#define CR_DEVICE_TYPE 0x8337u
#define CR_IOCTL_OBSERVE CTL_CODE(CR_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define CR_DEVICE_NAME L"\\Device\\CiderContractReference"
#define CR_SYMBOLIC_NAME L"\\DosDevices\\CiderContractReference"
#define CR_WIN32_NAME L"\\\\.\\CiderContractReference"

typedef struct CR_REQUEST {
    ULONG version;
    ULONG size;
    LUID privileges[CR_PRIVILEGE_COUNT];
} CR_REQUEST;

typedef struct CR_SET_OBSERVATION {
    ULONG mode;                         /* Actual KPROCESSOR_MODE passed. */
    ULONG control;
    ULONG count;
    ULONG result;
    ULONG attributes_before[CR_PRIVILEGE_COUNT];
    ULONG attributes_after[CR_PRIVILEGE_COUNT];
} CR_SET_OBSERVATION;

typedef struct CR_RESPONSE {
    ULONG version;
    ULONG size;
    LONG token_query_status;
    ULONG irql;
    ULONG requestor_mode;
    ULONG current_process_is_requestor;
    ULONG current_thread_is_requestor;
    ULONG captured_client_present;
    ULONG captured_impersonation_level;
    ULONG primary_reference_matches_capture;
    ULONG client_reference_matches_capture;
    ULONG client_reference_present;
    ULONG client_reference_level;
    ULONG client_copy_on_open;
    ULONG client_effective_only;
    ULONG context_capture_calls;
    ULONG context_lock_calls;
    ULONG context_unlock_calls;
    ULONG context_release_calls;
    ULONG primary_reference_calls;
    ULONG primary_dereference_calls;
    ULONG client_reference_calls;
    ULONG client_dereference_calls;
    ULONG privileges_present[CR_PRIVILEGE_COUNT];
    ULONG privilege_attributes[CR_PRIVILEGE_COUNT];
    ULONG single_user[CR_PRIVILEGE_COUNT];
    ULONG single_kernel[CR_PRIVILEGE_COUNT];
    CR_SET_OBSERVATION sets[CR_SET_COUNT];
} CR_RESPONSE;

#endif
