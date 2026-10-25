/* DIAGNOSTIC ONLY: model-state observation, not screen/compositor pixels. */
void macdrv_fixture_snapshot(WineWindow *window, unsigned int context_id, struct coordinator_snapshot *out)
{
    memset(out, 0, sizeof(*out));
    OnMainThread(^{
        NSView *content = [window contentView];
        if ([content isKindOfClass:[WineContentView class]])
            [(WineContentView *)content coordinatorFixtureSnapshot:context_id into:out];
    });
}
void macdrv_fixture_fail_next_host(void)
{
    OnMainThread(^{ coordinator_fixture_fail_next = true; });
}
unsigned int macdrv_fixture_allocation_failures(void)
{
    __block unsigned int count;
    OnMainThread(^{ count = coordinator_fixture_alloc_failures; });
    return count;
}
