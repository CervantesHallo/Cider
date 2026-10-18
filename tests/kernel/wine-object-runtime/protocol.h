#ifndef CIDER_OWNED_OBJECT_RUNTIME_H
#define CIDER_OWNED_OBJECT_RUNTIME_H
#define OWNED_IOCTL CTL_CODE(0x8349, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS)
#define OWNED_DEVICE L"\\Device\\CiderOwnedObjectRuntime"
#define OWNED_LINK L"\\DosDevices\\CiderOwnedObjectRuntime"
#define OWNED_WIN32 L"\\\\.\\CiderOwnedObjectRuntime"
#define OWNED_SERVICE L"CiderOwnedObjectRuntime"
typedef struct OWNED_RESULT {
    ULONG version, bytes, checks, failures, first_failure;
    LONG first_status;
    ULONG event_rounds, callbacks;
    LONG unsupported_info_status;
    ULONG concurrent_references, concurrent_closed_handles;
} OWNED_RESULT;
#endif
