/* A/W, new/existing, recursive/trailing and view routing for 0018. Own HKCU nonce only. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned int checks, failures;
static void verify(const char *label, BOOL value)
{
    ++checks;
    if (!value) { ++failures; printf("security-direct-fail check=%u label=%s\n", checks, label); }
}
static int compare_dacl(HKEY key, const BYTE *expected, DWORD length, BOOL protected)
{
    _Alignas(DWORD) BYTE storage[1024] = {0};
    DWORD size = sizeof(storage), revision = 0;
    SECURITY_DESCRIPTOR_CONTROL control = 0;
    PACL acl = NULL;
    BOOL present = FALSE, defaulted = FALSE;
    if (RegGetKeySecurity(key, DACL_SECURITY_INFORMATION, storage, &size) != ERROR_SUCCESS ||
        size > sizeof(storage) || size < SECURITY_DESCRIPTOR_MIN_LENGTH) return -1;
    if (!GetSecurityDescriptorControl(storage, &control, &revision) ||
        !GetSecurityDescriptorDacl(storage, &present, &acl, &defaulted)) return -1;
    if (!present || !acl) return 0;
    if (protected && !(control & SE_DACL_PROTECTED)) return 0;
    if ((BYTE *)acl < storage || (BYTE *)acl > storage + size - sizeof(*acl)) return -1;
    return acl->AclSize == length && length <= (DWORD)(storage + size - (BYTE *)acl) && !memcmp(acl, expected, length);
}
static LONG create(BOOL wide, HKEY root, const char *name, REGSAM view, SECURITY_ATTRIBUTES *attributes,
                   HKEY *key, DWORD *disposition)
{
    WCHAR buffer[128];
    if (!wide) return RegCreateKeyExA(root, name, 0, NULL, 0, KEY_ALL_ACCESS | view, attributes, key, disposition);
    if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, buffer, ARRAYSIZE(buffer))) return ERROR_INVALID_PARAMETER;
    return RegCreateKeyExW(root, buffer, 0, NULL, 0, KEY_ALL_ACCESS | view, attributes, key, disposition);
}
static void one_case(HKEY root, REGSAM view, BOOL wide, const char *name, const BYTE *expected,
                     SECURITY_ATTRIBUTES *attributes, SECURITY_ATTRIBUTES *different)
{
    HKEY key = NULL, reopened = NULL, parent = NULL;
    DWORD disposition = 0, flags = 0;
    LONG status;
    char parent_name[128];
    const char *separator;
    status = create(wide, root, name, view, attributes, &key, &disposition);
    verify("create-new", status == ERROR_SUCCESS && key && disposition == REG_CREATED_NEW_KEY);
    if (status || !key) goto done;
    verify("leaf-dacl", compare_dacl(key, expected, 28, TRUE) == 1);
    verify("new-inherit", GetHandleInformation(key, &flags) && (flags & HANDLE_FLAG_INHERIT));
    status = create(wide, root, name, view, different, &reopened, &disposition);
    verify("open-existing", status == ERROR_SUCCESS && reopened && disposition == REG_OPENED_EXISTING_KEY);
    if (!status && reopened)
    {
        verify("existing-dacl-unchanged", compare_dacl(reopened, expected, 28, TRUE) == 1);
        flags = 0;
        verify("existing-inherit", GetHandleInformation(reopened, &flags) && (flags & HANDLE_FLAG_INHERIT));
    }
    if ((separator = strchr(name, '\\')))
    {
        size_t n = separator - name;
        memcpy(parent_name, name, n); parent_name[n] = 0;
        status = RegOpenKeyExA(root, parent_name, 0, KEY_READ | view, &parent);
        verify("intermediate-open", status == ERROR_SUCCESS && parent);
        if (!status && parent) verify("intermediate-default", compare_dacl(parent, expected, 28, FALSE) == 0);
    }
done:
    if (parent) verify("close-parent", RegCloseKey(parent) == ERROR_SUCCESS);
    if (reopened) verify("close-existing", RegCloseKey(reopened) == ERROR_SUCCESS);
    if (key) verify("close-new", RegCloseKey(key) == ERROR_SUCCESS);
}
int main(int argc, char **argv)
{
    const REGSAM views[] = {0, KEY_WOW64_32KEY, KEY_WOW64_64KEY};
    const char *names[] = {"SingleA", "SingleW", "RecursiveA\\Leaf", "RecursiveW\\Leaf",
                           "TrailingA\\Leaf\\\\", "TrailingW\\Leaf\\\\"};
    _Alignas(DWORD) BYTE acl_bytes[28] = {0};
    ACL *acl = (void *)acl_bytes, empty = {0};
    ACCESS_ALLOWED_ACE *ace = (void *)(acl_bytes + sizeof(ACL));
    SID world = {0};
    SECURITY_DESCRIPTOR sd = {0}, other = {0};
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), &sd, TRUE};
    SECURITY_ATTRIBUTES different = {sizeof(different), &other, TRUE};
    char path[160];
    HKEY root = NULL;
    DWORD disposition;
    LONG status, cleanup;
    unsigned int i, j, cases = 0, owned;
    if (argc != 2 || strlen(argv[1]) != 32) return 2;
    for (i = 0; i < 32; ++i)
        if (!((argv[1][i] >= '0' && argv[1][i] <= '9') || (argv[1][i] >= 'a' && argv[1][i] <= 'f'))) return 3;
    world.Revision = SID_REVISION; world.SubAuthorityCount = 1;
    world.IdentifierAuthority.Value[5] = 1; world.SubAuthority[0] = SECURITY_WORLD_RID;
    acl->AclRevision = ACL_REVISION; acl->AclSize = sizeof(acl_bytes); acl->AceCount = 1;
    ace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE; ace->Header.AceSize = sizeof(acl_bytes) - sizeof(ACL);
    ace->Mask = KEY_ALL_ACCESS; memcpy(&ace->SidStart, &world, sizeof(world));
    sd.Revision = SECURITY_DESCRIPTOR_REVISION; sd.Control = SE_DACL_PRESENT | SE_DACL_PROTECTED; sd.Dacl = acl;
    empty.AclRevision = ACL_REVISION; empty.AclSize = sizeof(empty);
    other.Revision = SECURITY_DESCRIPTOR_REVISION; other.Control = sd.Control; other.Dacl = &empty;
    for (i = 0; i < ARRAYSIZE(views); ++i)
    {
        owned = 0;
        snprintf(path, sizeof(path), "Software\\CiderSecurityDirect\\%s-%u", argv[1], i);
        status = RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS | views[i], NULL, &root, &disposition);
        verify("root-new-owner", status == ERROR_SUCCESS && root && disposition == REG_CREATED_NEW_KEY);
        if (!status && root && disposition == REG_CREATED_NEW_KEY)
        {
            owned = 1;
            verify("root-dacl-precondition", compare_dacl(root, acl_bytes, sizeof(acl_bytes), FALSE) == 0);
            for (j = 0; j < ARRAYSIZE(names); ++j)
            {
                one_case(root, views[i], j & 1, names[j], acl_bytes, &attributes, &different);
                ++cases;
            }
        }
        if (owned)
        {
            cleanup = RegDeleteTreeA(root, NULL);
            verify("owned-children-cleanup", cleanup == ERROR_SUCCESS);
        }
        if (root) { verify("close-root", RegCloseKey(root) == ERROR_SUCCESS); root = NULL; }
        if (owned)
        {
            cleanup = RegDeleteKeyExA(HKEY_CURRENT_USER, path, views[i], 0);
            verify("owned-root-cleanup", cleanup == ERROR_SUCCESS);
        }
    }
    printf("{\"type\":\"registry-security-direct\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"cases\":%u,\"views\":3}\n",
           (unsigned int)(8 * sizeof(void *)), checks, failures, cases);
    return failures || cases != 18 ? 1 : 0;
}
