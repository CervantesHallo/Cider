#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winternl.h>
#include "fixture.h"
typedef NTSTATUS (WINAPI *probe_fn)(struct legacy_fixture *);
int main(int argc, char **argv)
{
    HMODULE module;
    probe_fn probe;
    struct legacy_fixture result = {0};
    const UINT32 expected_calls[6] = {0, 3, 1, 2, 5, 7};
    NTSTATUS status;
    LONG cleanup = -1, error;
    HKEY control = NULL;
    DWORD disposition, sentinel = 0x50726f62u, readback = 0, size, type;
    char path[128];
    unsigned int i, negative = 0, control_owned = 0, cleanup_performed = 0;
    int success = 1;
    if ((argc != 2 && argc != 3) || strlen(argv[1]) != 32) return 2;
    for (i = 0; i < 32; ++i)
        if (!((argv[1][i] >= '0' && argv[1][i] <= '9') || (argv[1][i] >= 'a' && argv[1][i] <= 'f'))) return 3;
    if (argc == 3)
    {
        if (!strcmp(argv[2], "--existing")) negative = 1;
        else if (!strcmp(argv[2], "--reject")) negative = 2;
        else return 3;
    }
    snprintf(path, sizeof(path), "Software\\CiderLegacyProbe\\%s", argv[1]);
    result.magic = LEGACY_FIXTURE_MAGIC;
    result.version = negative == 2 ? 0 : 1;
    memcpy(result.nonce, argv[1], 32);
    if (!(module = LoadLibraryW(L"winemac.drv"))) return 4;
    if (!(probe = (probe_fn)(void *)GetProcAddress(module, "cider_registry_probe"))) return 5;
    if (negative)
    {
        error = RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &control, &disposition);
        if (error || !control || disposition != REG_CREATED_NEW_KEY)
        {
            if (control) RegCloseKey(control);
            return 6; /* No ownership: do not write or delete an existing tree. */
        }
        control_owned = 1;
        error = RegSetValueExA(control, "Sentinel", 0, REG_DWORD, (BYTE *)&sentinel, sizeof(sentinel));
        if (error) success = 0;
        RegCloseKey(control); control = NULL;
    }
    status = probe(&result);
    if (negative)
    {
        success = success && !result.created_tree && !result.consumed_injections && !result.security_cases &&
                  !result.pending_after_open_failure && !result.pending_after_create_failure && !result.pending_final;
        if (negative == 2)
        {
            success = success && status == (NTSTATUS)0xc000000d && !result.checks && !result.failures && !result.value_bytes;
            for (i = 0; i < 6; ++i) success = success && !result.calls[i] && !result.peer_calls[i];
        }
        else
        {
            const UINT32 expected_existing[6] = {0, 0, 0, 0, 1, 1};
            success = success && !status && result.checks == 4 && result.failures == 1 && result.value_bytes == 131073;
            for (i = 0; i < 6; ++i)
                success = success && result.calls[i] == expected_existing[i] && result.peer_calls[i] == expected_existing[i];
        }
        error = RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &control);
        success = success && !error && !!control;
        if (control)
        {
            size = sizeof(readback); type = 0;
            error = RegQueryValueExA(control, "Sentinel", NULL, &type, (BYTE *)&readback, &size);
            success = success && !error && type == REG_DWORD && size == sizeof(readback) && readback == sentinel;
            size = 0;
            error = RegQueryValueExA(control, "Large", NULL, NULL, NULL, &size);
            success = success && error == ERROR_FILE_NOT_FOUND;
            RegCloseKey(control); control = NULL;
        }
    }
    else
    {
        success = !status && result.created_tree == 1 && result.checks == 33 && !result.failures &&
                  result.pending_after_open_failure == 1 && result.pending_after_create_failure == 1 &&
                  !result.pending_final && result.value_bytes == 131073 && result.security_cases == 3 &&
                  result.consumed_injections == 2;
        for (i = 0; i < 6; ++i)
            success = success && result.calls[i] == expected_calls[i] && result.peer_calls[i] == expected_calls[i];
    }
    if (control_owned || (!negative && result.created_tree))
    {
        cleanup = RegDeleteTreeA(HKEY_CURRENT_USER, path);
        cleanup_performed = 1;
    }
    printf("{\"type\":\"legacy-registry\",\"bits\":%u,\"negative\":%u,\"success\":%u,\"status\":%lu,\"checks\":%u,\"failures\":%u,\"owned\":%u,\"injections\":%u,\"pending_open\":%u,\"pending_create\":%u,\"pending_final\":%u,\"value_bytes\":%u,\"security_cases\":%u,\"cleanup_performed\":%u,\"cleanup\":%ld,\"calls\":[%u,%u,%u,%u,%u],\"peer_calls\":[%u,%u,%u,%u,%u]}\n",
           (unsigned int)(8 * sizeof(void *)), negative, !!success, (unsigned long)status, result.checks, result.failures,
           result.created_tree, result.consumed_injections, result.pending_after_open_failure,
           result.pending_after_create_failure, result.pending_final, result.value_bytes, result.security_cases,
           cleanup_performed, cleanup, result.calls[1], result.calls[2], result.calls[3], result.calls[4], result.calls[5],
           result.peer_calls[1], result.peer_calls[2], result.peer_calls[3], result.peer_calls[4], result.peer_calls[5]);
    FreeLibrary(module);
    return success && cleanup_performed && cleanup == ERROR_SUCCESS ? 0 : 1;
}
