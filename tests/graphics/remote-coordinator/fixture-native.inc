/* DIAGNOSTIC ONLY. Included after the exact coordinator implementation.
 * Real Cocoa roots and CAContexts; two explicit local fault points. No game or
 * producer-transport acceptance, no extra renderer/resource lifetime pin. */
#include <stdio.h>
#include "coordinator-fixture.h"
extern void macdrv_fixture_snapshot(WineWindow *, unsigned int, struct coordinator_snapshot *);
extern void macdrv_fixture_fail_next_host(void);
extern unsigned int macdrv_fixture_allocation_failures(void);
C_ASSERT(sizeof(struct coordinator_fixture) == 80);
C_ASSERT(sizeof(struct coordinator_snapshot) == 88);
static void fixture_check(struct coordinator_fixture *frame, const char *label, BOOL accepted)
{
    ++frame->checks;
    if (!accepted) { ++frame->failures; fprintf(stderr, "coordinator-fail check=%u label=%s\n", frame->checks, label); }
}
static LONG fixture_root_refs(HWND hwnd)
{
    struct remote_root *root;
    LONG refs;
    pthread_mutex_lock(&remote_lock);
    root = root_lookup_locked(hwnd);
    refs = root ? root->refs : 0;
    pthread_mutex_unlock(&remote_lock);
    return refs;
}
static void fixture_snapshot(struct coordinator_fixture *frame, WineWindow *window, UINT context,
                             struct coordinator_snapshot *snapshot)
{
    memset(snapshot, 0, sizeof(*snapshot));
    macdrv_fixture_snapshot(window, context, snapshot);
    ++frame->snapshots;
}
/* Preserve a real job's own host pin until it is deliberately delivered late. */
static struct update_job *fixture_hold_job(UINT64 ticket)
{
    struct update_job *job = calloc(1, sizeof(*job));
    struct remote_host *host;
    if (!job) return NULL;
    pthread_mutex_lock(&remote_lock);
    host = host_lookup_locked(ticket);
    if (host && host->state == HOST_INSTALLED)
    {
        job->host = host;
        InterlockedIncrement(&host->refs);
        job->revision = host->root->revision;
        job->serial = identity_locked();
        host->update_serial = job->serial;
    }
    pthread_mutex_unlock(&remote_lock);
    if (!job->host) { free(job); return NULL; }
    return job;
}
static void fixture_deliver_job(struct update_job *job)
{
    struct remote_host *host = job->host;
    macdrv_window_update_remote_host_checked(host->root->window, host->context_id, host->ticket,
        CGRectMake(9876, 9876, 1, 1), CGRectMake(9876, 9876, 1, 1), 9999, true,
        update_validate, update_dispose, job);
}
NTSTATUS macdrv_remote_fixture_probe(void *args)
{
    struct coordinator_fixture *frame = args;
    struct macdrv_win_data *data;
    struct coordinator_snapshot snapshot;
    struct update_job *job = NULL;
    id_WineMetalSwapChain contexts[3] = {0};
    UINT ids[3] = {0};
    HWND hwnd, child;
    WineWindow *window;
    UINT64 epoch, reopened, a = 0, b = 0, c = 0;
    RECT client;
    POINT origin = {0}, offset;
    double scale;
    unsigned int i;
    if (!frame || frame->magic != COORDINATOR_FIXTURE_MAGIC || frame->version != 1) return STATUS_INVALID_PARAMETER;
    hwnd = UlongToHandle(frame->root); child = UlongToHandle(frame->child);
    if (!(data = get_win_data(hwnd))) return STATUS_NOT_FOUND;
    window = macdrv_legacy_retain_object(data->cocoa_window);
    offset.x = data->rects.client.left - data->rects.visible.left;
    offset.y = data->rects.client.top - data->rects.visible.top;
    release_win_data(data);
    if (!window) return STATUS_NOT_FOUND;
    /* Advance without reusing any identity, then acquire a real new root
     * epoch with nonzero high32bits. The PE32 result path must retain them. */
    macdrv_remote_root_close(hwnd, window);
    pthread_mutex_lock(&remote_lock);
    if (next_identity < ((UINT64)1 << 33)) next_identity = (UINT64)1 << 33;
    pthread_mutex_unlock(&remote_lock);
    epoch = macdrv_remote_root_open(hwnd, window);
    frame->epoch = epoch;
    fixture_check(frame, "root-epoch", !!epoch); /*1*/
    if (!NtUserGetClientRect(child, &client, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI))) goto done;
    for (i = 0; i < 3; ++i)
    {
        contexts[i] = macdrv_create_exported_metal_context(CGRectMake(0, 0, client.right, client.bottom), NULL);
        if (contexts[i]) ids[i] = macdrv_exported_metal_context_id(contexts[i]);
        fixture_check(frame, "real-context", contexts[i] && ids[i] && (i == 0 || ids[i] != ids[i - 1])); /*2..4*/
        if (!contexts[i] || !ids[i]) goto done;
    }
    a = macdrv_remote_host_install(hwnd, epoch, child, ids[0]);
    frame->tickets[0] = a;
    fixture_check(frame, "first-install", !!a); /*5*/
    fixture_snapshot(frame, window, ids[0], &snapshot);
    fixture_check(frame, "first-model", snapshot.found && snapshot.count == 1 && snapshot.generation == a); /*6*/
    if (!NtUserGetClientRect(child, &client, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI))) goto done;
    NtUserMapWindowPoints(child, hwnd, &origin, 1, NtUserGetWinMonitorDpi(hwnd, MDT_RAW_DPI));
    scale = retina_on ? 2.0 : 1.0;
    fixture_check(frame, "client-frame-and-full-source", snapshot.frame[0] == (origin.x + offset.x) / scale &&
        snapshot.frame[1] == (origin.y + offset.y) / scale && snapshot.frame[2] == client.right / scale &&
        snapshot.frame[3] == client.bottom / scale && snapshot.source[0] == 0 && snapshot.source[1] == 0 &&
        snapshot.source[2] == client.right / scale && snapshot.source[3] == client.bottom / scale); /*7*/
    fixture_check(frame, "matching-flipped-geometry", snapshot.flipped); /*8*/
    macdrv_fixture_fail_next_host();
    fixture_check(frame, "failed-replacement", !macdrv_remote_host_install(hwnd, epoch, child, ids[1])); /*9*/
    frame->allocation_faults = macdrv_fixture_allocation_failures();
    fixture_snapshot(frame, window, ids[0], &snapshot);
    fixture_check(frame, "old-host-preserved", snapshot.found && snapshot.count == 1 && snapshot.generation == a); /*10*/
    fixture_check(frame, "old-ticket-still-live", macdrv_remote_host_refresh(a)); /*11*/
    fixture_snapshot(frame, window, ids[0], &snapshot);
    fixture_check(frame, "update-pin-disposed", fixture_root_refs(hwnd) == 2); /*12*/
    b = macdrv_remote_host_install(hwnd, epoch, child, ids[1]);
    frame->tickets[1] = b;
    fixture_check(frame, "replacement-install", b && b != a); /*13*/
    fixture_snapshot(frame, window, ids[1], &snapshot);
    fixture_check(frame, "replacement-model", snapshot.found && snapshot.count == 1 && snapshot.generation == b); /*14*/
    fixture_snapshot(frame, window, ids[0], &snapshot);
    fixture_check(frame, "predecessor-retired", !snapshot.found && snapshot.count == 1); /*15*/
    fixture_check(frame, "old-refresh-rejected", !macdrv_remote_host_refresh(a)); /*16*/
    macdrv_remote_host_release(a);
    fixture_snapshot(frame, window, ids[1], &snapshot);
    fixture_check(frame, "old-release-keeps-new", snapshot.found && snapshot.count == 1 && snapshot.generation == b); /*17*/
    if (!(job = fixture_hold_job(b))) goto done;
    macdrv_remote_host_release(b);
    fixture_check(frame, "late-update-rejected", !update_validate(job)); /*18*/
    fixture_deliver_job(job); job = NULL;
    fixture_snapshot(frame, window, ids[1], &snapshot);
    fixture_check(frame, "released-model-empty", !snapshot.found && !snapshot.count); /*19*/
    fixture_check(frame, "all-old-pins-disposed", fixture_root_refs(hwnd) == 1); /*20*/
    macdrv_remote_root_close(hwnd, window);
    fixture_check(frame, "root-close-revokes", !macdrv_remote_root_epoch(hwnd)); /*21*/
    fixture_check(frame, "closed-epoch-rejected", !macdrv_remote_host_install(hwnd, epoch, child, ids[2])); /*22*/
    reopened = macdrv_remote_root_open(hwnd, window);
    fixture_check(frame, "new-root-epoch", reopened && reopened != epoch); /*23*/
    fixture_check(frame, "old-epoch-stays-rejected", !macdrv_remote_host_install(hwnd, epoch, child, ids[2])); /*24*/
    c = macdrv_remote_host_install(hwnd, reopened, child, ids[2]);
    frame->tickets[2] = c;
    fixture_check(frame, "reopened-install", !!c); /*25*/
    fixture_snapshot(frame, window, ids[2], &snapshot);
    fixture_check(frame, "reopened-model", snapshot.found && snapshot.count == 1 && snapshot.generation == c); /*26*/
    if (!(job = fixture_hold_job(c))) goto done;
    pthread_mutex_lock(&remote_lock);
    next_identity = ~(UINT64)0; /* Diagnostic-only namespace exhaustion, never reset. */
    pthread_mutex_unlock(&remote_lock);
    frame->exhaustion_faults = 1;
    macdrv_remote_root_changed(hwnd);
    fixture_check(frame, "exhaustion-revokes-old-job", !update_validate(job)); /*27*/
    fixture_check(frame, "exhaustion-revokes-epoch", !macdrv_remote_root_epoch(hwnd)); /*28*/
    fixture_deliver_job(job); job = NULL;
    fixture_snapshot(frame, window, ids[2], &snapshot);
    fixture_check(frame, "exhaustion-model-empty", !snapshot.found && !snapshot.count); /*29*/
    fixture_check(frame, "exhaustion-pins-disposed", fixture_root_refs(hwnd) == 1); /*30*/
    fixture_check(frame, "exhaustion-no-admission", !macdrv_remote_host_install(hwnd, reopened, child, ids[2])); /*31*/
    macdrv_remote_host_release(c);
    fixture_check(frame, "duplicate-release-stable", fixture_root_refs(hwnd) == 1); /*32*/
done:
    if (job) update_dispose(job);
    if (a) macdrv_remote_host_release(a);
    if (b) macdrv_remote_host_release(b);
    if (c) macdrv_remote_host_release(c);
    for (i = 0; i < 3; ++i) if (contexts[i]) macdrv_destroy_swapchain(contexts[i]);
    /* Flush the actual main-thread release queue before recording cleanup. */
    macdrv_fixture_snapshot(window, 0, &snapshot);
    frame->cleanup = !snapshot.count && fixture_root_refs(hwnd) == 1;
    macdrv_legacy_release_object(window);
    return STATUS_SUCCESS;
}
