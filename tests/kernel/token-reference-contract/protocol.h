#ifndef CIDER_OWNED_TOKEN_RUNTIME_H
#define CIDER_OWNED_TOKEN_RUNTIME_H
#define TOKEN_CHECK CTL_CODE(0x834a, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define TOKEN_HOLD CTL_CODE(0x834a, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)
#define TOKEN_RELEASE CTL_CODE(0x834a, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
#define TOKEN_DEVICE L"\\Device\\CiderOwnedTokenRuntime"
#define TOKEN_LINK L"\\DosDevices\\CiderOwnedTokenRuntime"
#define TOKEN_WIN32 L"\\\\.\\CiderOwnedTokenRuntime"
#define TOKEN_SERVICE L"CiderOwnedTokenRuntime"
typedef struct TOKEN_RESULT {
    ULONG version, bytes, checks, failures, first_failure;
    LONG first_status;
    LUID target_id, host_id, held_id;
    ULONG rounds, concurrent_rounds, expected_faults;
} TOKEN_RESULT;
#endif
