#ifndef CIDER_LEGACY_REGISTRY_FIXTURE_H
#define CIDER_LEGACY_REGISTRY_FIXTURE_H
#define LEGACY_FIXTURE_MAGIC 0x31524743u
struct legacy_fixture
{
    UINT32 magic, version;
    char nonce[32];
    UINT32 checks, failures;
    UINT32 calls[6];
    UINT32 pending_after_open_failure, pending_after_create_failure, pending_final;
    UINT32 value_bytes, security_cases;
    UINT32 created_tree, consumed_injections;
    UINT32 peer_calls[6];
};
C_ASSERT(sizeof(struct legacy_fixture) == 124);
void legacy_fixture_pe_begin(void);
void legacy_fixture_pe_end(UINT32 *counts);
#endif
