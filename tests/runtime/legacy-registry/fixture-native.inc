/* Diagnostic-only code, absent from the ordinary candidate. */
#include "fixture.h"
#include <stdio.h>

static UINT32 pending_count(void)
{
    struct registry_mapping_owner *owner;
    UINT32 count = 0;
    pthread_mutex_lock(&mapping_lock);
    for (owner = pending_mappings; owner; owner = owner->next) ++count;
    pthread_mutex_unlock(&mapping_lock);
    return count;
}

static BOOL valid_dacl(HKEY key, const BYTE *expected, ULONG expected_size)
{
    _Alignas(DWORD) BYTE buffer[1024] = {0};
    SECURITY_DESCRIPTOR_RELATIVE *descriptor = (void *)buffer;
    ACL *acl;
    ULONG required = 0;
    NTSTATUS status;
    status = NtQuerySecurityObject(key, DACL_SECURITY_INFORMATION, descriptor, sizeof(buffer), &required);
    if (status || required < sizeof(*descriptor) || required > sizeof(buffer) || !(descriptor->Control & SE_SELF_RELATIVE) ||
        !(descriptor->Control & SE_DACL_PRESENT) || !(descriptor->Control & SE_DACL_PROTECTED) || descriptor->Dacl < sizeof(*descriptor) ||
        descriptor->Dacl > required - sizeof(*acl)) return FALSE;
    acl = (void *)(buffer + descriptor->Dacl);
    return acl->AclSize == expected_size && expected_size <= required - descriptor->Dacl &&
           !memcmp(acl, expected, expected_size);
}

static BOOL inherited_handle(HKEY key, BOOL expected)
{
    OBJECT_HANDLE_FLAG_INFORMATION flags = {0};
    ULONG returned = 0;
    NTSTATUS status;
    flags.Inherit = !expected;
    status = NtQueryObject(key, ObjectHandleFlagInformation, &flags, sizeof(flags), &returned);
    return !status && returned == sizeof(flags) && !!flags.Inherit == !!expected;
}

#define VERIFY(value) do { ++result->checks; if (!(value)) ++result->failures; } while (0)

NTSTATUS macdrv_legacy_registry_probe(void *args)
{
    struct legacy_fixture *result = args;
    HKEY root = NULL, child = NULL;
    BYTE *bytes = NULL, *received = NULL, small[7];
    _Alignas(DWORD) BYTE acl_bytes[28] = {0}, relative_bytes[48] = {0};
    ACL *acl = (void *)acl_bytes;
    ACCESS_ALLOWED_ACE *ace = (void *)(acl_bytes + sizeof(ACL));
    SID world = {0};
    SECURITY_DESCRIPTOR absolute = {0};
    SECURITY_DESCRIPTOR_RELATIVE *relative = (void *)relative_bytes;
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), NULL, TRUE};
    char path[128];
    DWORD count, type, disposition, old_calls;
    UINT32 i;
    LONG status;

    if (!result || result->magic != LEGACY_FIXTURE_MAGIC || result->version != 1) return STATUS_INVALID_PARAMETER;
    for (i = 0; i < 32; ++i)
        if (!((result->nonce[i] >= '0' && result->nonce[i] <= '9') ||
              (result->nonce[i] >= 'a' && result->nonce[i] <= 'f'))) return STATUS_INVALID_PARAMETER;
    if (InterlockedCompareExchange(&diag_active, 1, 0)) return STATUS_DEVICE_BUSY;
    diag_thread = NtCurrentTeb()->ClientId.UniqueThread;
    memset(diag_calls, 0, sizeof(diag_calls));
    result->checks = result->failures = result->security_cases = 0;
    result->created_tree = result->consumed_injections = 0;
    InterlockedExchange(&diag_consumed, 0);
    result->value_bytes = 131073;
    VERIFY(pending_count() == 0);
    snprintf(path, sizeof(path), "Software\\CiderLegacyProbe\\%.*s", 32, result->nonce);
    status = macdrv_legacy_RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &root, &disposition);
    VERIFY(status == ERROR_SUCCESS && root && disposition == REG_CREATED_NEW_KEY);
    if (status || !root || disposition != REG_CREATED_NEW_KEY) goto done;
    result->created_tree = 1;
    bytes = malloc(result->value_bytes);
    received = malloc(result->value_bytes + 17);
    VERIFY(bytes && received);
    if (!bytes || !received) goto done;
    memset(received, 0xcc, result->value_bytes + 17);
    for (i = 0; i < result->value_bytes; ++i) bytes[i] = (BYTE)(i * 37u + 11u);
    status = macdrv_legacy_RegSetValueExA(root, "Large", 0, REG_BINARY, bytes, result->value_bytes);
    VERIFY(status == ERROR_SUCCESS);
    count = 0xfeedffff;
    type = 0xfeedffff;
    status = macdrv_legacy_RegQueryValueExA(root, "Large", NULL, &type, NULL, &count);
    VERIFY(status == ERROR_SUCCESS && type == REG_BINARY && count == result->value_bytes);
    count = sizeof(small);
    status = macdrv_legacy_RegQueryValueExA(root, "Large", NULL, NULL, small, &count);
    VERIFY(status == ERROR_MORE_DATA && count == result->value_bytes);
    count = result->value_bytes + 17;
    status = macdrv_legacy_RegQueryValueExA(root, "Large", NULL, &type, received, &count);
    VERIFY(status == ERROR_SUCCESS && type == REG_BINARY && count == result->value_bytes);
    VERIFY(!memcmp(bytes, received, result->value_bytes));

    old_calls = diag_calls[MACDRV_LEGACY_REG_OPEN];
    InterlockedExchange(&diag_fail_cleanup, 1);
    status = macdrv_legacy_RegOpenKeyExA(root, "", 0, KEY_ALL_ACCESS, &child);
    result->pending_after_open_failure = pending_count();
    VERIFY(status == ERROR_SUCCESS && child && result->pending_after_open_failure == 1);
    VERIFY(diag_calls[MACDRV_LEGACY_REG_OPEN] == old_calls + 1);
    if (child) { VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS); child = NULL; }
    VERIFY(pending_count() == 0);
    VERIFY(diag_calls[MACDRV_LEGACY_REG_OPEN] == old_calls + 1);
    old_calls = diag_calls[MACDRV_LEGACY_REG_CREATE];
    InterlockedExchange(&diag_fail_cleanup, 1);
    status = macdrv_legacy_RegCreateKeyExA(root, "CleanupFailure", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &child, &disposition);
    result->pending_after_create_failure = pending_count();
    VERIFY(status == ERROR_SUCCESS && child && disposition == REG_CREATED_NEW_KEY && result->pending_after_create_failure == 1);
    VERIFY(diag_calls[MACDRV_LEGACY_REG_CREATE] == old_calls + 1);
    if (child) { VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS); child = NULL; }
    VERIFY(pending_count() == 0);
    VERIFY(diag_calls[MACDRV_LEGACY_REG_CREATE] == old_calls + 1);
    status = macdrv_legacy_RegOpenKeyExA(root, "CleanupFailure", 0, KEY_READ, &child);
    VERIFY(status == ERROR_SUCCESS && child);
    if (child) { VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS); child = NULL; }

    status = macdrv_legacy_RegCreateKeyExA(root, "NullDescriptor", 0, NULL, 0, KEY_ALL_ACCESS, &attributes, &child, NULL);
    VERIFY(status == ERROR_SUCCESS && child);
    if (child)
    {
        VERIFY(inherited_handle(child, TRUE));
        VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS); child = NULL;
        ++result->security_cases;
    }
    world.Revision = SID_REVISION;
    world.SubAuthorityCount = 1;
    world.IdentifierAuthority.Value[5] = 1;
    world.SubAuthority[0] = SECURITY_WORLD_RID;
    acl->AclRevision = ACL_REVISION;
    acl->AclSize = sizeof(acl_bytes);
    acl->AceCount = 1;
    ace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    ace->Header.AceSize = sizeof(acl_bytes) - sizeof(ACL);
    ace->Mask = KEY_ALL_ACCESS;
    memcpy(&ace->SidStart, &world, sizeof(world));
    absolute.Revision = SECURITY_DESCRIPTOR_REVISION;
    absolute.Control = SE_DACL_PRESENT | SE_DACL_PROTECTED;
    absolute.Dacl = acl;
    attributes.lpSecurityDescriptor = &absolute;
    attributes.bInheritHandle = FALSE;
    status = macdrv_legacy_RegCreateKeyExA(root, "AbsoluteDescriptor", 0, NULL, 0, KEY_ALL_ACCESS, &attributes, &child, NULL);
    VERIFY(status == ERROR_SUCCESS && child);
    if (child)
    {
        VERIFY(inherited_handle(child, FALSE));
        VERIFY(valid_dacl(child, acl_bytes, sizeof(acl_bytes)));
        VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS); child = NULL;
        ++result->security_cases;
    }
    relative->Revision = SECURITY_DESCRIPTOR_REVISION;
    relative->Control = absolute.Control | SE_SELF_RELATIVE;
    relative->Dacl = sizeof(*relative);
    memcpy(relative_bytes + relative->Dacl, acl_bytes, sizeof(acl_bytes));
    attributes.lpSecurityDescriptor = relative;
    attributes.bInheritHandle = TRUE;
    status = macdrv_legacy_RegCreateKeyExA(root, "RelativeDescriptor", 0, NULL, 0, KEY_ALL_ACCESS, &attributes, &child, NULL);
    VERIFY(status == ERROR_SUCCESS && child);
    if (child)
    {
        VERIFY(inherited_handle(child, TRUE));
        VERIFY(valid_dacl(child, acl_bytes, sizeof(acl_bytes)));
        VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS); child = NULL;
        ++result->security_cases;
    }
done:
    InterlockedExchange(&diag_fail_cleanup, 0);
    if (child) VERIFY(macdrv_legacy_RegCloseKey(child) == ERROR_SUCCESS);
    if (root) VERIFY(macdrv_legacy_RegCloseKey(root) == ERROR_SUCCESS);
    free(received);
    free(bytes);
    for (i = 0; i < 8 && pending_count(); ++i) retry_pending_mapping();
    result->consumed_injections = InterlockedCompareExchange(&diag_consumed, 0, 0);
    result->pending_final = pending_count();
    VERIFY(!result->pending_final);
    for (i = 0; i < 6; ++i) result->calls[i] = diag_calls[i];
    diag_thread = NULL;
    InterlockedExchange(&diag_active, 0);
    return STATUS_SUCCESS;
}
