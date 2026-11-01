/* The qualified producer is deliberately compiled without changing its source. */
#define main two_pixel_main
#include "client.c"
#undef main

/* Test-only three-process schedule, GPU-only unless the host is given
 * --capture-images for two parent WindowServer single-window image gates.
 * Those images are independent of Space and do not prove physical display
 * visibility. Each child has its own actual
 * process, pipes, Native observer, identity and monotonically increasing probe
 * sequence. Selecting the legacy globals is only for validation and logging;
 * none of the old host's cleanup/ownership state is used by this host. */
struct three_lane
{
    const char *name;
    HWND child;
    UINT32 child_value,pid;
    UINT64 birth;
    PROCESS_INFORMATION pi;
    HANDLE request_read,request_write,reply_read,reply_write,stdout_child,stderr_child;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes;
    BOOL attributes_ready,create_attempted,created,opposite_bits;
    BOOL setup_attempted,setup_ack,setup_ok,identity_saved,bound,last_absent;
    BOOL finish_attempted,native_finished,pending,probe_ready,cleanup_reported;
    BOOL kernel_joined,exit_known,exit_zero,wait_error,forced,terminate_returned;
    BOOL complete,local_cleanup;
    DWORD child_exit,last_wait,create_error,terminate_error;
    unsigned received_phase,replied_phase,reply_attempts,check_count,failure_count,call_count;
    DWORD reply_bytes;
    UINT64 previous_native_sequence,last_probe_sequence;
    struct live_host_fixture setup,identity,last_frame,last_probe;
    NTSTATUS last_nt,probe_nt;
    BOOL last_nt_valid,probe_nt_valid;
    struct cross_message request;
    struct budget reply_budget;
};

static const char *three_bool(BOOL value)
{
    return value?"true":"false";
}

static void three_select(const struct three_lane *lane)
{
    child_value=lane->child_value;producer_pid=lane->pid;producer_birth=lane->birth;
    identity_saved=lane->identity_saved;identity=lane->identity;
    wire_sequence=lane->pending?lane->request.sequence:lane->replied_phase;
}

static BOOL three_check(struct three_lane *lane,BOOL ok,const char *name)
{
    char qualified[192];
    int n;
    DWORD error=GetLastError();
    if(!lane)return check(ok,name);
    ++lane->check_count;
    if(!ok)++lane->failure_count;
    n=snprintf(qualified,sizeof(qualified),"%s-%s",lane->name,name);
    if(n<0||(size_t)n>=sizeof(qualified)){io_error=TRUE;return FALSE;}
    three_select(lane);
    SetLastError(error);
    return check(ok,qualified);
}

static BOOL three_close(struct three_lane *lane,HANDLE *handle,const char *name)
{
    BOOL ok;
    if(!*handle)return TRUE;
    ok=CloseHandle(*handle);
    if(ok)*handle=NULL;
    if(!three_check(lane,ok,name)){lane->local_cleanup=FALSE;return FALSE;}
    return TRUE;
}

static BOOL three_envelope(const struct three_lane *lane,const struct live_host_fixture *f,unsigned op)
{
    return f->size==160&&f->version==2&&f->operation==op&&
           f->root==root_value&&f->child==lane->child_value;
}

static BOOL three_observation(const struct three_lane *lane,const struct live_host_fixture *f)
{
    return lane->identity_saved&&f->role==LIVE_ROLE_HOST&&same_identity(f,&lane->identity)&&
           f->probe_bound==1&&f->cocoa_valid==1&&!f->context_owned&&
           !f->host_reader_owned&&!f->producer_reader_owned&&!f->flags&&
           !f->close_status&&!f->reply_status&&!f->host_values_valid&&!f->producer_values_valid&&
           f->host_read==MACDRV_STATUS_READ_INVALID&&f->producer_read==MACDRV_STATUS_READ_INVALID;
}

static BOOL three_pending_room(const struct three_lane lanes[2])
{
    unsigned i;
    if(!room(NULL))return FALSE;
    if(cleaning)return TRUE;
    for(i=0;i<2;++i)
        if(lanes[i].pending&&!room(&lanes[i].reply_budget))return FALSE;
    return !io_error&&!pipe_error&&!quit_seen;
}

/* Decode only real Native acknowledgements. This runs before output and before
 * all post-call clock checks, including for a late or partial setup/finish. */
static void three_observe(struct three_lane *lane,unsigned op,
                          const struct live_host_fixture *f,NTSTATUS nt,UINT64 requested_probe)
{
    if(nt||!three_envelope(lane,f,op)||f->role!=LIVE_ROLE_HOST||f->root_pid!=root_pid)return;
    if(op==LIVE_HOST_SETUP)
    {
        lane->setup_ack=TRUE; /* status may report a partially completed setup. */
        lane->setup_ok=!f->status&&f->root_birth==root_birth&&root_birth&&f->epoch&&
            f->window_owned==1&&!f->context_id&&!f->ticket&&!f->lease&&!f->producer_pid&&
            !f->birth&&!f->probe_bound&&!f->probe_sequence&&!f->context_owned&&
            !f->host_reader_owned&&!f->producer_reader_owned;
    }
    if(f->status)return;
    if(op==LIVE_HOST_FINISH&&!lane->created&&lane->setup_ack&&!lane->bound&&!lane->identity_saved&&
       !f->window_owned&&!f->producer_pid&&!f->birth&&!f->lease&&!f->ticket&&!f->context_id&&
       !f->context_owned&&!f->host_reader_owned&&!f->producer_reader_owned&&
       !f->probe_bound&&!f->probe_sequence&&!f->close_status&&!f->reply_status)
        lane->native_finished=TRUE; /* Real unbound release, not model absence or SAFE. */
    if((op==LIVE_HOST_PROBE||op==LIVE_HOST_FINISH)&&three_observation(lane,f))
    {
        lane->bound=TRUE;
        if(op==LIVE_HOST_PROBE&&requested_probe==lane->last_probe_sequence+1&&
           f->probe_sequence==requested_probe&&f->window_owned==1)
        {
            lane->last_probe_sequence=f->probe_sequence;
            lane->last_absent=models(f,0);
            lane->last_probe=*f;lane->probe_nt=nt;lane->probe_nt_valid=TRUE;
        }
        if(op==LIVE_HOST_FINISH&&f->probe_sequence==lane->last_probe_sequence&&
           models(f,0)&&!f->window_owned)
        {lane->last_absent=TRUE;lane->native_finished=TRUE;}
    }
}

static BOOL three_native(struct three_lane lanes[2],struct three_lane *lane,unsigned op,
                         struct live_host_fixture *f)
{
    struct budget call_budget;
    UINT64 requested_probe=op==LIVE_HOST_PROBE?f->probe_sequence:0;
    NTSTATUS nt;
    BOOL call_in_time,outer_in_time;
    lane->last_nt_valid=FALSE;
    if(!fixture||!three_pending_room(lanes))return FALSE;
    if(op==LIVE_HOST_SETUP&&lane->setup_attempted)return FALSE;
    if(op==LIVE_HOST_PROBE&&(!lane->pending||!lane->identity_saved||
       requested_probe!=lane->received_phase||requested_probe!=lane->last_probe_sequence+1))return FALSE;
    if(op==LIVE_HOST_FINISH&&(lane->finish_attempted||lane->native_finished||!lane->setup_ack))return FALSE;
    if(op!=LIVE_HOST_SETUP&&op!=LIVE_HOST_PROBE&&op!=LIVE_HOST_FINISH)return FALSE;
    if(!begin_budget(&call_budget,4)||!room(&call_budget))return FALSE;
    three_select(lane);
    f->size=160;f->version=2;f->operation=op;f->status=ERROR_INVALID_DATA;
    f->root=root_value;f->child=lane->child_value;
    if(op==LIVE_HOST_SETUP)lane->setup_attempted=TRUE;
    if(op==LIVE_HOST_FINISH)lane->finish_attempted=TRUE;
    nt=fixture(f);
    ++native_calls;++lane->call_count;
    lane->last_frame=*f;lane->last_nt=nt;lane->last_nt_valid=TRUE;
    if(op==LIVE_HOST_SETUP)lane->setup=*f;
    three_observe(lane,op,f,nt,requested_probe);
    frame_record("frame",f,TRUE,nt,0);
    record("three-native","\"lane\":\"%s\",\"child_pid\":%u,\"child\":%u,"
           "\"native_call\":%u,\"operation\":%u,\"probe_sequence\":%llu,"
           "\"setup_ack\":%s,\"native_finished\":%s",
           lane->name,(unsigned)lane->pid,(unsigned)lane->child_value,native_calls,op,
           (unsigned long long)f->probe_sequence,three_bool(lane->setup_ack),three_bool(lane->native_finished));
    call_in_time=room(&call_budget);outer_in_time=three_pending_room(lanes);
    return !nt&&three_envelope(lane,f,op)&&call_in_time&&outer_in_time;
}

/* These are newly allocated copies for this child alone. Both pipe pairs and
 * the HANDLE_LIST are independent of the other lane. */
static BOOL three_inheritance_cleanup(struct three_lane *lane)
{
    BOOL ok=TRUE;
    if(!three_close(lane,&lane->request_write,"close-host-copy-child-request-end"))ok=FALSE;
    if(!three_close(lane,&lane->reply_read,"close-host-copy-child-reply-end"))ok=FALSE;
    if(!three_close(lane,&lane->stdout_child,"close-host-copy-child-stdout"))ok=FALSE;
    if(!three_close(lane,&lane->stderr_child,"close-host-copy-child-stderr"))ok=FALSE;
    if(lane->attributes_ready)
    {DeleteProcThreadAttributeList(lane->attributes);lane->attributes_ready=FALSE;}
    if(lane->attributes)
    {
        BOOL freed=HeapFree(GetProcessHeap(),0,lane->attributes);
        if(freed)lane->attributes=NULL;
        if(!three_check(lane,freed,"free-owned-inheritance-list"))
        {ok=FALSE;lane->local_cleanup=FALSE;}
    }
    return ok;
}

static BOOL three_spawn(struct three_lane lanes[2],struct three_lane *lane,
                        const WCHAR *peer,const char *peer_ascii,const char *nonce_ascii,BOOL host_wow)
{
    SECURITY_ATTRIBUTES security={sizeof(security),NULL,TRUE};
    STARTUPINFOEXW startup;
    PROCESS_INFORMATION actual_pi;
    HANDLE inherited[4];
    SIZE_T attribute_bytes=0;
    WCHAR command[1024];
    char ascii[1024];
    BOOL created,copies_closed,child_wow=FALSE,got_birth,got_bits;
    unsigned i;
    int n;
    memset(&startup,0,sizeof(startup));memset(&actual_pi,0,sizeof(actual_pi));
    if(!three_check(lane,CreatePipe(&lane->request_read,&lane->request_write,&security,4096)&&
       SetHandleInformation(lane->request_read,HANDLE_FLAG_INHERIT,0),"request-pipe-only-child-end-inheritable"))return FALSE;
    if(!three_check(lane,CreatePipe(&lane->reply_read,&lane->reply_write,&security,4096)&&
       SetHandleInformation(lane->reply_write,HANDLE_FLAG_INHERIT,0),"reply-pipe-only-child-end-inheritable"))return FALSE;
    if(!three_check(lane,child_pipe_width(lane->reply_read)&&child_pipe_width(lane->request_write),
       "actual-pipe-values-fit-both-bitnesses"))return FALSE;
    if(!three_check(lane,duplicate_output(STD_OUTPUT_HANDLE,&lane->stdout_child)&&
       duplicate_output(STD_ERROR_HANDLE,&lane->stderr_child),"owned-inheritable-output-copies"))return FALSE;
    (void)InitializeProcThreadAttributeList(NULL,1,0,&attribute_bytes);
    if(!three_check(lane,attribute_bytes!=0,"inheritance-list-size"))return FALSE;
    lane->attributes=HeapAlloc(GetProcessHeap(),0,attribute_bytes);
    if(!three_check(lane,lane->attributes!=NULL,"allocate-inheritance-list"))return FALSE;
    lane->attributes_ready=InitializeProcThreadAttributeList(lane->attributes,1,0,&attribute_bytes);
    if(!three_check(lane,lane->attributes_ready,"initialize-inheritance-list"))return FALSE;
    inherited[0]=lane->reply_read;inherited[1]=lane->request_write;
    inherited[2]=lane->stdout_child;inherited[3]=lane->stderr_child;
    if(!three_check(lane,UpdateProcThreadAttribute(lane->attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
       inherited,sizeof(inherited),NULL,NULL),"restricted-four-handle-list-for-this-child-only"))return FALSE;
    startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput=lane->reply_read;startup.StartupInfo.hStdOutput=lane->stdout_child;
    startup.StartupInfo.hStdError=lane->stderr_child;startup.lpAttributeList=lane->attributes;
    n=snprintf(ascii,sizeof(ascii),"\"%s\" --producer %08x %08x %s %llx %llx --gpu-only",
       peer_ascii,(unsigned)root_value,(unsigned)lane->child_value,nonce_ascii,
       (unsigned long long)(uintptr_t)lane->reply_read,(unsigned long long)(uintptr_t)lane->request_write);
    if(!three_check(lane,n>0&&(size_t)n<sizeof(ascii),"bounded-quoted-ascii-producer-command"))return FALSE;
    for(i=0;i<=(unsigned)n;++i)command[i]=(WCHAR)(unsigned char)ascii[i];
    if(!three_check(lane,three_pending_room(lanes),"watchdog-before-ordinary-create-process"))return FALSE;
    lane->create_attempted=TRUE;
    created=CreateProcessW(peer,command,NULL,NULL,TRUE,EXTENDED_STARTUPINFO_PRESENT,
                           NULL,NULL,&startup.StartupInfo,&actual_pi);
    lane->create_error=created?ERROR_SUCCESS:GetLastError();
    /* Save the real PI ownership before clocks, logging, or fallible queries. */
    lane->created=created;
    if(created){lane->pi=actual_pi;lane->pid=actual_pi.dwProcessId;}
    copies_closed=three_inheritance_cleanup(lane);
    if(!three_close(lane,&lane->pi.hThread,"close-created-primary-thread"))copies_closed=FALSE;
    SetLastError(lane->create_error);
    if(!three_check(lane,created,"ordinary-create-process-owned-producer"))return FALSE;
    got_birth=process_birth(lane->pi.hProcess,&lane->birth);
    got_bits=IsWow64Process(lane->pi.hProcess,&child_wow);
    lane->opposite_bits=got_bits&&child_wow!=host_wow;
    three_select(lane);
    record("three-create","\"lane\":\"%s\",\"child_pid\":%u,\"child_birth\":%llu,"
           "\"child\":%u,\"created\":%s,\"win32_error\":%u,\"opposite_bits\":%s,"
           "\"inherited_handle_count\":4,\"host_child_copies_closed\":%s",
           lane->name,(unsigned)lane->pid,(unsigned long long)lane->birth,(unsigned)lane->child_value,
           three_bool(lane->created),(unsigned)lane->create_error,three_bool(lane->opposite_bits),three_bool(copies_closed));
    return three_check(lane,lane->pi.hProcess&&lane->pid&&lane->pid!=root_pid&&got_birth&&
                       lane->opposite_bits&&copies_closed&&three_pending_room(lanes),
                       "actual-created-pid-birth-opposite-bitness-and-post-create-clock");
}

static BOOL three_receive(struct three_lane lanes[2],struct three_lane *lane,unsigned phase)
{
    struct three_lane *other=lane==&lanes[0]?&lanes[1]:&lanes[0];
    const struct budget *held=other->pending?&other->reply_budget:NULL;
    BOOL valid;
    if(!three_check(lane,!lane->pending&&phase==lane->received_phase+1&&
       lane->received_phase==lane->replied_phase&&phase<=4&&three_pending_room(lanes),
       "receive-next-phase-on-this-lane-only"))return FALSE;
    three_select(lane);
    if(!three_check(lane,read_message(lane->request_read,&lane->request,lane->pi.hProcess,held),
       "bounded-pumped-complete-200-byte-request"))return FALSE;
    lane->pending=TRUE;lane->probe_ready=FALSE;lane->probe_nt_valid=FALSE;lane->reply_bytes=0;
    /* This starts on actual receipt; the child independently bounds its whole
     * exchange. B's phase 2 remains under that real 64s producer budget. */
    if(!three_check(lane,begin_budget(&lane->reply_budget,phase<=2?64u:4u),"reply-budget-64s-or-4s"))return FALSE;
    three_select(lane);
    message_record("pipe-request","receive",&lane->request);
    /* reply_status is the preserved control reply, which may legitimately be
     * IO_PENDING after CANCEL/RELEASE even when this fresh SNAPSHOT is SAFE.
     * The qualified validator distinguishes it from both actual statuses. */
    valid=host_request(&lane->request,phase,&lane->setup,lane->previous_native_sequence);
    if(!three_check(lane,valid,"strict-wire-header-nonce-phase-native-frame-root-child-pid-birth-ticket-sequence-status"))return FALSE;
    lane->received_phase=phase;lane->previous_native_sequence=lane->request.frame.sequence;
    if(!lane->identity_saved){lane->identity=lane->request.frame;lane->identity_saved=TRUE;}
    if(phase==4)lane->cleanup_reported=TRUE; /* Real flags63/noowners report, not a kernel join. */
    three_select(lane);
    record("three-receive","\"lane\":\"%s\",\"child_pid\":%u,\"phase\":%u,"
           "\"cleanup_reported\":%s,\"native_sequence\":%llu,\"probed\":false,\"replied\":false",
           lane->name,(unsigned)lane->pid,phase,three_bool(lane->cleanup_reported),
           (unsigned long long)lane->previous_native_sequence);
    return three_check(lane,three_pending_room(lanes),"post-request-validation-clock");
}

static BOOL three_probe(struct three_lane lanes[2],struct three_lane *lane)
{
    struct live_host_fixture observed=lane->request.frame;
    BOOL called;
    observed.probe_sequence=lane->request.sequence;
    called=three_native(lanes,lane,LIVE_HOST_PROBE,&observed);
    lane->probe_ready=called&&!observed.status&&three_observation(lane,&observed)&&
        observed.probe_sequence==lane->received_phase&&observed.window_owned==1&&
        models(&observed,lane->received_phase<=2?1u:0u)&&lane->probe_nt_valid&&
        lane->last_probe_sequence==lane->received_phase;
    return three_check(lane,lane->probe_ready,"fresh-native-probe-actual-models-and-matching-per-lane-sequence");
}

/* Record a full write as an actual effect before its post-clock check. There
 * are exactly four bounded replies per fresh 4096-byte pipe, so no peer can
 * cause a synchronous write to wait for pipe capacity. No ACK is replayed. */
static BOOL three_reply(struct three_lane lanes[2],struct three_lane *lane)
{
    struct cross_message reply;
    unsigned phase=lane->received_phase;
    BOOL ok=TRUE;
    if(!three_check(lane,lane->pending&&lane->probe_ready&&lane->probe_nt_valid&&
       !lane->probe_nt&&lane->last_probe_sequence==phase&&
       !(lane->reply_attempts&(1u<<phase))&&three_pending_room(lanes),"actual-probe-ready-for-single-reply"))return FALSE;
    three_select(lane);
    make_message(&reply,lane->request.operation,lane->request.sequence,&lane->last_probe,lane->probe_nt);
    message_record("pipe-reply","send-attempt",&reply);
    pair_record(&lane->request.frame,&lane->last_probe);
    lane->reply_attempts|=1u<<phase;
    while(lane->reply_bytes<(DWORD)sizeof(reply))
    {
        DWORD written=0,want=(DWORD)sizeof(reply)-lane->reply_bytes;
        pump();
        if(!three_pending_room(lanes))return FALSE;
        ok=WriteFile(lane->reply_write,(const unsigned char *)&reply+lane->reply_bytes,want,&written,NULL);
        if(written<=want)lane->reply_bytes+=written;else ok=FALSE;
        if(lane->reply_bytes==(DWORD)sizeof(reply))
        {lane->replied_phase=phase;lane->pending=FALSE;lane->probe_ready=FALSE;}
        if(!ok||!written){pipe_error=TRUE;break;}
        if(!three_pending_room(lanes)){ok=FALSE;break;}
    }
    three_select(lane);
    record("three-reply","\"lane\":\"%s\",\"child_pid\":%u,\"phase\":%u,"
           "\"actual_written_bytes\":%u,\"complete_write\":%s,\"probe_sequence\":%llu",
           lane->name,(unsigned)lane->pid,phase,(unsigned)lane->reply_bytes,
           three_bool(lane->replied_phase==phase),(unsigned long long)lane->last_probe_sequence);
    return three_check(lane,ok&&lane->replied_phase==phase&&room(&lane->reply_budget)&&
                       three_pending_room(lanes),"exact-actual-native-frame-reply-written-in-time");
}

/* WAIT_OBJECT_0 on this actual PI is the only way to set kernel_joined. The
 * result and exit query are latched before logging or any post-clock test. */
static BOOL three_poll_join(struct three_lane *lane)
{
    DWORD wait,error=ERROR_SUCCESS;
    BOOL queried;
    if(!lane->created)return TRUE;
    if(lane->kernel_joined)return lane->exit_known;
    wait=WaitForSingleObject(lane->pi.hProcess,0);
    lane->last_wait=wait;
    if(wait==WAIT_TIMEOUT)return TRUE;
    if(wait==WAIT_OBJECT_0)
    {
        lane->kernel_joined=TRUE;
        queried=GetExitCodeProcess(lane->pi.hProcess,&lane->child_exit);
        error=queried?ERROR_SUCCESS:GetLastError();
        lane->exit_known=queried;lane->exit_zero=queried&&lane->child_exit==0;
    }
    else
    {lane->wait_error=TRUE;error=wait==WAIT_FAILED?GetLastError():ERROR_INVALID_DATA;}
    three_select(lane);
    record("three-child-join","\"lane\":\"%s\",\"child_pid\":%u,\"child_birth\":%llu,"
           "\"kernel_wait\":%u,\"kernel_joined\":%s,\"exit_known\":%s,\"child_exit\":%u,"
           "\"exit_zero\":%s,\"forced\":%s,\"win32_error\":%u",
           lane->name,(unsigned)lane->pid,(unsigned long long)lane->birth,(unsigned)wait,
           three_bool(lane->kernel_joined),three_bool(lane->exit_known),(unsigned)lane->child_exit,
           three_bool(lane->exit_zero),three_bool(lane->forced),(unsigned)error);
    SetLastError(error);
    return three_check(lane,wait==WAIT_OBJECT_0&&lane->exit_known,"actual-kernel-wait-and-exit-query");
}

static BOOL three_join(struct three_lane lanes[2],struct three_lane *lane)
{
    struct budget b;
    if(!three_check(lane,begin_budget(&b,4),"normal-kernel-join-budget"))return FALSE;
    while(three_pending_room(lanes)&&room(&b))
    {
        pump();
        if(!three_poll_join(lane))return FALSE;
        if(lane->kernel_joined)
            return three_check(lane,lane->exit_known&&lane->exit_zero&&!lane->forced&&
                               three_pending_room(lanes)&&room(&b),"normal-kernel-join-exit-zero");
        Sleep(2);
    }
    return three_check(lane,FALSE,"normal-kernel-join-within-budget");
}

static BOOL three_finish(struct three_lane lanes[2],struct three_lane *lane)
{
    struct live_host_fixture f;
    BOOL called;
    if(lane->created)
    {
        if(!three_check(lane,lane->bound&&lane->last_absent&&lane->cleanup_reported&&
           lane->kernel_joined,"real-cleanup-report-and-kernel-join-before-observer-finish"))return FALSE;
        f=lane->last_probe;f.probe_sequence=lane->last_probe_sequence;
    }
    else memset(&f,0,sizeof(f));
    called=three_native(lanes,lane,LIVE_HOST_FINISH,&f);
    return three_check(lane,called&&!f.status&&lane->native_finished,
                       lane->created?"fresh-native-absence-and-observer-release":"actual-unbound-partial-or-full-setup-release");
}

static BOOL three_distinct(const struct three_lane lanes[2])
{
    const struct live_host_fixture *a=&lanes[0].identity,*b=&lanes[1].identity;
    return lanes[0].identity_saved&&lanes[1].identity_saved&&a->root==b->root&&
        a->root==root_value&&a->root_pid==b->root_pid&&a->root_pid==root_pid&&
        a->root_birth==b->root_birth&&a->root_birth==root_birth&&a->epoch&&a->epoch==b->epoch&&
        a->child!=b->child&&a->producer_pid!=b->producer_pid&&a->producer_pid==lanes[0].pid&&
        b->producer_pid==lanes[1].pid&&a->birth==lanes[0].birth&&b->birth==lanes[1].birth&&
        a->birth&&b->birth&&a->birth!=b->birth&&
        a->context_id&&b->context_id&&a->context_id!=b->context_id&&
        a->lease&&b->lease&&a->lease!=b->lease&&a->ticket&&b->ticket&&a->ticket!=b->ticket;
}

static BOOL three_pair_evidence(struct three_lane lanes[2],const char *event)
{
    char a[640],b[640];
    if(!frame_words(&lanes[0].last_frame,a,sizeof(a))||
       !frame_words(&lanes[1].last_frame,b,sizeof(b)))return FALSE;
    record(event,"\"lane_a_pid\":%u,\"lane_b_pid\":%u,\"root\":%u,"
           "\"lane_a_frame_words\":[%s],\"lane_b_frame_words\":[%s],"
           "\"a_kernel_joined\":%s,\"a_exit_zero\":%s,\"a_native_finished\":%s,"
           "\"b_phase2_replied\":%s,\"screenAccepted\":false",
           (unsigned)lanes[0].pid,(unsigned)lanes[1].pid,(unsigned)root_value,a,b,
           three_bool(lanes[0].kernel_joined),three_bool(lanes[0].exit_zero),
           three_bool(lanes[0].native_finished),three_bool(lanes[1].replied_phase>=2));
    return !io_error&&three_pending_room(lanes);
}

static BOOL three_both_joined(const struct three_lane lanes[2])
{
    return (!lanes[0].created||lanes[0].kernel_joined)&&
           (!lanes[1].created||lanes[1].kernel_joined);
}

/* Closing the host endpoints requests cooperative failure cleanup without
 * inventing a reply or cancelling a ticket on a producer's behalf. Both
 * children get the same pumped grace; force all remaining owned children
 * before joining either, so one stuck child cannot consume the other's time. */
static void three_failure_stop(struct three_lane lanes[2])
{
    struct budget grace;
    unsigned i;
    BOOL started;
    for(i=0;i<2;++i)
    {
        (void)three_close(&lanes[i],&lanes[i].reply_write,"failure-close-reply-for-cooperative-cleanup");
        (void)three_close(&lanes[i],&lanes[i].request_read,"failure-close-request-for-cooperative-cleanup");
        (void)three_inheritance_cleanup(&lanes[i]);
    }
    record("three-stop-note","\"note\":\"TEST ONLY shared pumped cooperative grace up to 8s, cut off at elapsed 170s; reserve 10s within total180 for at most one TerminateProcess per actual unjoined child and real kernel joins; force and unknown internal cleanup cannot pass\"");
    started=begin_budget(&grace,8);
    while((started||clock_error)&&room(NULL)&&room(&grace)&&!three_both_joined(lanes))
    {
        ULONGLONG now=GetTickCount64();
        if(now<entry_tick){clock_error=TRUE;break;}
        if(now-entry_tick>=170000u)break;
        pump();
        for(i=0;i<2;++i)
            if(lanes[i].created&&!lanes[i].kernel_joined&&!lanes[i].wait_error)(void)three_poll_join(&lanes[i]);
        if(!three_both_joined(lanes))Sleep(2);
    }
    for(i=0;i<2;++i)
    {
        struct three_lane *lane=&lanes[i];
        if(lane->created&&!lane->kernel_joined&&!lane->forced&&room(NULL))
        {
            /* One last actual wait can avoid force if the child just exited. */
            if(!lane->wait_error)(void)three_poll_join(lane);
            if(lane->kernel_joined)continue;
            lane->forced=TRUE;
            lane->terminate_returned=TerminateProcess(lane->pi.hProcess,1);
            lane->terminate_error=lane->terminate_returned?ERROR_SUCCESS:GetLastError();
            three_select(lane);
            record("three-child-terminate","\"lane\":\"%s\",\"child_pid\":%u,\"child_birth\":%llu,"
                   "\"attempted\":true,\"returned\":%s,\"win32_error\":%u",
                   lane->name,(unsigned)lane->pid,(unsigned long long)lane->birth,
                   three_bool(lane->terminate_returned),(unsigned)lane->terminate_error);
        }
    }
    while(room(NULL)&&!three_both_joined(lanes))
    {
        BOOL waitable=FALSE;
        pump();
        for(i=0;i<2;++i)
            if(lanes[i].created&&!lanes[i].kernel_joined&&!lanes[i].wait_error)
            {waitable=TRUE;(void)three_poll_join(&lanes[i]);}
        if(!waitable)break;
        if(!three_both_joined(lanes))Sleep(2);
    }
}

static void three_lane_summary(struct three_lane *lane)
{
    three_select(lane);
    record("three-lane-summary","\"lane\":\"%s\",\"root\":%u,\"child\":%u,"
           "\"child_pid\":%u,\"child_birth\":%llu,\"epoch\":%llu,\"context_id\":%u,"
           "\"lease\":%llu,\"ticket\":%llu,\"complete\":%s,\"checks\":%u,\"failures\":%u,"
           "\"native_calls\":%u,\"setup_attempted\":%s,\"setup_ack\":%s,\"setup_ok\":%s,"
           "\"create_attempted\":%s,\"created\":%s,\"opposite_bits\":%s,\"identity_saved\":%s,"
           "\"received_phase\":%u,\"replied_phase\":%u,\"probe_sequence\":%llu,"
           "\"cleanup_reported\":%s,\"kernel_wait\":%u,\"kernel_joined\":%s,\"exit_known\":%s,"
           "\"child_exit\":%u,\"exit_zero\":%s,\"forced\":%s,\"terminate_returned\":%s,"
           "\"bound\":%s,\"last_absent\":%s,\"finish_attempted\":%s,\"native_finished\":%s,"
           "\"local_cleanup\":%s,\"process_handle_retained\":%s,\"unknown_internal_cleanup\":%s",
           lane->name,(unsigned)root_value,(unsigned)lane->child_value,(unsigned)lane->pid,
           (unsigned long long)lane->birth,(unsigned long long)lane->identity.epoch,
           (unsigned)lane->identity.context_id,(unsigned long long)lane->identity.lease,
           (unsigned long long)lane->identity.ticket,three_bool(lane->complete),lane->check_count,
           lane->failure_count,lane->call_count,three_bool(lane->setup_attempted),three_bool(lane->setup_ack),
           three_bool(lane->setup_ok),three_bool(lane->create_attempted),three_bool(lane->created),
           three_bool(lane->opposite_bits),three_bool(lane->identity_saved),lane->received_phase,lane->replied_phase,
           (unsigned long long)lane->last_probe_sequence,three_bool(lane->cleanup_reported),
           (unsigned)lane->last_wait,three_bool(lane->kernel_joined),three_bool(lane->exit_known),
           (unsigned)lane->child_exit,three_bool(lane->exit_zero),three_bool(lane->forced),
           three_bool(lane->terminate_returned),three_bool(lane->bound),three_bool(lane->last_absent),
           three_bool(lane->finish_attempted),three_bool(lane->native_finished),three_bool(lane->local_cleanup),
           three_bool(lane->pi.hProcess!=NULL),three_bool(lane->created&&!lane->cleanup_reported));
}

static int three_host_main(int argc,char **argv)
{
    static const WCHAR class_name[]=L"CiderThreePixelHostV1";
    struct three_lane lanes[2];
    WNDCLASSW wc;
    HWND root=NULL;
    HMODULE module=NULL;
    ATOM atom=0;
    WCHAR peer[MAX_PATH];
    struct live_host_fixture f;
    struct budget capture_budget;
    DWORD binary_type=0;
    BOOL host_wow=FALSE,complete=FALSE,local_cleanup=TRUE,slots_released,cleanup_ok;
    BOOL coexistence=FALSE,independence=FALSE;
    BOOL capture_images=argc==5&&!strcmp(argv[4],"--capture-images");
    unsigned i;
    memset(lanes,0,sizeof(lanes));memset(&wc,0,sizeof(wc));
    lanes[0].name="A";lanes[1].name="B";
    for(i=0;i<2;++i)
    {lanes[i].local_cleanup=TRUE;lanes[i].child_exit=STILL_ACTIVE;lanes[i].last_wait=WAIT_FAILED;}
    role_name="triple-host";screen_required=capture_images;root_pid=GetCurrentProcessId();
    if(!check(argc==4||capture_images,"triple-host-argc4-gpu-only-or-argc5-capture-images"))goto done;
    if(!check(initialize_clock(),"qpc-total180-work160-native4-external210-test-only"))goto done;
    if(!check(peer_path(argv[2],peer)&&parse_nonce(argv[3]),"bounded-owned-ascii-peer-and-batch-nonce"))goto done;
    if(!check(process_birth(GetCurrentProcess(),&root_birth),"actual-host-process-birth"))goto done;
    if(!check(IsWow64Process(GetCurrentProcess(),&host_wow)&&host_wow==(sizeof(void *)==4)&&
       GetBinaryTypeW(peer,&binary_type)&&binary_type==(sizeof(void *)==8?SCS_32BIT_BINARY:SCS_64BIT_BINARY),
       "actual-host-and-peer-image-have-opposite-bitness"))goto done;
    wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=class_name;
    atom=RegisterClassW(&wc);
    if(!check(atom!=0,"register-common-host-window-class"))goto done;
    root=CreateWindowW(class_name,capture_images?L"Cider three-process host images":L"Cider three-process GPU-only host",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                       20,20,440,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root!=NULL&&window_value(root,&root_value),"actual-single-common-host-root"))goto done;
    for(i=0;i<2;++i)
    {
        DWORD owner=0,root_owner=0;
        lanes[i].child=CreateWindowW(class_name,i?L"host-owned slot B":L"host-owned slot A",
            WS_CHILD|WS_VISIBLE|WS_BORDER,10+(int)i*200,10,180,100,root,NULL,wc.hInstance,NULL);
        if(!three_check(&lanes[i],lanes[i].child!=NULL&&window_value(lanes[i].child,&lanes[i].child_value)&&
           lanes[i].child_value!=root_value&&GetAncestor(lanes[i].child,GA_ROOT)==root&&
           GetParent(lanes[i].child)==root&&GetWindowThreadProcessId(lanes[i].child,&owner)!=0&&
           GetWindowThreadProcessId(root,&root_owner)!=0&&owner==root_pid&&root_owner==root_pid,
           "actual-host-owned-child-slot-in-common-root"))goto done;
    }
    {
        RECT a,b;
        if(!check(lanes[0].child_value!=lanes[1].child_value&&
           GetWindowRect(lanes[0].child,&a)&&GetWindowRect(lanes[1].child,&b)&&
           a.left<a.right&&b.left<b.right&&a.top<a.bottom&&b.top<b.bottom&&a.right<=b.left,
           "two-distinct-actual-child-slots-at-disjoint-positions"))goto done;
    }
    if(!load_fixture(&module))goto done;
    for(i=0;i<2;++i)
    {
        memset(&f,0,sizeof(f));
        if(!three_check(&lanes[i],three_native(lanes,&lanes[i],LIVE_HOST_SETUP,&f)&&
           !f.status&&lanes[i].setup_ok,"actual-native-observer-setup-for-this-root-and-child"))goto done;
    }
    if(!check(lanes[0].setup.root==lanes[1].setup.root&&
       lanes[0].setup.root_birth==lanes[1].setup.root_birth&&
       lanes[0].setup.epoch==lanes[1].setup.epoch&&lanes[0].setup.epoch&&
       lanes[0].setup.child!=lanes[1].setup.child,"two-native-observers-share-root-birth-epoch-only"))goto done;
    for(i=0;i<2;++i)
        if(!three_spawn(lanes,&lanes[i],peer,argv[2],argv[3],host_wow))goto done;
    if(!check(lanes[0].pid!=lanes[1].pid&&lanes[0].pi.hProcess!=lanes[1].pi.hProcess&&
       lanes[0].request_read!=lanes[1].request_read&&lanes[0].reply_write!=lanes[1].reply_write,
       "two-distinct-created-processes-and-host-pipe-ends"))goto done;

    /* Neither child can retire its active model until its real first reply. */
    if(!three_receive(lanes,&lanes[0],1)||!three_probe(lanes,&lanes[0]))goto done;
    if(!three_receive(lanes,&lanes[1],1)||!three_probe(lanes,&lanes[1]))goto done;
    if(!check(three_distinct(lanes)&&lanes[0].last_probe_sequence==1&&lanes[1].last_probe_sequence==1&&
       models(&lanes[0].last_probe,1)&&models(&lanes[1].last_probe,1)&&
       WaitForSingleObject(lanes[0].pi.hProcess,0)==WAIT_TIMEOUT&&
       WaitForSingleObject(lanes[1].pi.hProcess,0)==WAIT_TIMEOUT&&three_pending_room(lanes),
       "actual-simultaneous-two-context-models-distinct-pids-cids-leases-tickets"))goto done;
    if(!three_pair_evidence(lanes,"three-coexistence"))goto done;
    coexistence=TRUE;
    if(capture_images)
    {
        /* Both producers still hold their real red frames until A1/B1. The
         * pixel helper selects the shared root by A's exact CID/lease/ticket;
         * it never overwrites either lane's saved Native probe or NTSTATUS. */
        three_select(&lanes[0]);
        if(!check(begin_budget(&capture_budget,60)&&
           capture_gate(1,&lanes[0].request,&lanes[0].reply_budget,&capture_budget),
           "host-image-both-red-fresh60-gate-before-either-first-reply"))
        {capture_error=TRUE;goto done;}
    }
    if(!three_reply(lanes,&lanes[0])||!three_reply(lanes,&lanes[1]))goto done;

    /* Receive both fresh blue snapshots. B2 remains buffered and unprobed. */
    if(!three_receive(lanes,&lanes[0],2)||!three_receive(lanes,&lanes[1],2))goto done;
    if(!three_probe(lanes,&lanes[0]))goto done;
    if(capture_images)
    {
        /* Both real blue requests are held. A2 is probed, B2 stays buffered;
         * A's earlier 64s reply budget also bounds the matching B2 hold. */
        three_select(&lanes[0]);
        if(!check(begin_budget(&capture_budget,60)&&
           capture_gate(2,&lanes[0].request,&lanes[0].reply_budget,&capture_budget),
           "host-image-both-blue-fresh60-gate-after-a2-probe-before-a2-reply"))
        {capture_error=TRUE;goto done;}
    }
    if(!three_reply(lanes,&lanes[0]))goto done;
    if(!three_receive(lanes,&lanes[0],3)||!three_probe(lanes,&lanes[0])||!three_reply(lanes,&lanes[0]))goto done;
    if(!three_receive(lanes,&lanes[0],4)||!three_probe(lanes,&lanes[0])||!three_reply(lanes,&lanes[0]))goto done;
    if(!three_join(lanes,&lanes[0])||!three_finish(lanes,&lanes[0]))goto done;
    lanes[0].complete=TRUE;

    /* This is B's first and only Native sequence2. A's fresh FINISH frame
     * precedes it; there is no duplicate probe or invented delayed ACK. */
    if(!three_check(&lanes[1],lanes[0].native_finished&&lanes[0].kernel_joined&&lanes[0].exit_zero&&
       !lanes[0].forced&&lanes[1].pending&&lanes[1].received_phase==2&&
       lanes[1].replied_phase==1&&lanes[1].last_probe_sequence==1&&three_distinct(lanes),
       "a-finished-before-first-native-probe-of-buffered-b2"))goto done;
    if(!three_probe(lanes,&lanes[1]))goto done;
    if(!check(lanes[0].native_finished&&!lanes[0].last_frame.window_owned&&models(&lanes[0].last_frame,0)&&
       lanes[0].last_frame.operation==LIVE_HOST_FINISH&&lanes[1].last_frame.operation==LIVE_HOST_PROBE&&
       lanes[1].last_probe_sequence==2&&models(&lanes[1].last_probe,1)&&
       lanes[1].request.frame.context_owned==1&&lanes[1].request.frame.host_reader_owned==1&&
       lanes[1].request.frame.producer_reader_owned==1&&
       WaitForSingleObject(lanes[1].pi.hProcess,0)==WAIT_TIMEOUT&&three_pending_room(lanes),
       "independent-b-model-active-with-context-and-readers-after-a-finish-and-exit-zero"))goto done;
    if(!three_pair_evidence(lanes,"three-independence"))goto done;
    independence=TRUE;
    if(!three_reply(lanes,&lanes[1]))goto done;
    if(!three_receive(lanes,&lanes[1],3)||!three_probe(lanes,&lanes[1])||!three_reply(lanes,&lanes[1]))goto done;
    if(!three_receive(lanes,&lanes[1],4)||!three_probe(lanes,&lanes[1])||!three_reply(lanes,&lanes[1]))goto done;
    if(!three_join(lanes,&lanes[1])||!three_finish(lanes,&lanes[1]))goto done;
    lanes[1].complete=TRUE;
    /* Only capture_gate issues host pixel calls, always PIXEL_HOST_WINDOW (3).
     * GPU drawing, present/readback and draining remain producer-only. */
    if(!check(coexistence&&independence&&screen_required==capture_images&&!capture_error&&
       pixel_calls==(capture_images?2u:0u)&&capture_acks==(capture_images?2u:0u)&&
       !pixel_attempted&&!red_results&&!blue_results&&pixel_clean&&
       lanes[0].replied_phase==4&&lanes[1].replied_phase==4&&
       lanes[0].last_probe_sequence==4&&lanes[1].last_probe_sequence==4&&three_pending_room(lanes),
       capture_images?"two-real-four-phase-lanes-two-host-window-image-gates-no-host-gpu-drawing":
                      "two-real-four-phase-lanes-gpu-only-no-host-pixel-or-capture-ack"))goto done;
    complete=TRUE;
done:
    cleaning=TRUE;
    if(!three_both_joined(lanes))three_failure_stop(lanes);
    if(!close_owned(&ready_file,"cleanup-host-owned-capture-ready-file"))
    {capture_error=TRUE;local_cleanup=FALSE;}
    if(!close_owned(&gate_file,"cleanup-host-owned-capture-gate-file"))
    {capture_error=TRUE;local_cleanup=FALSE;}
    /* A process exit never promotes unknown producer cleanup to known. An
     * attempted setup without a real acknowledgement also stays retained. */
    for(i=0;i<2;++i)
    {
        struct three_lane *lane=&lanes[i];
        if(!lane->native_finished&&!lane->finish_attempted&&lane->setup_ack&&fixture&&clock_ready&&room(NULL)&&
           ((!lane->created&&!lane->identity_saved&&!lane->bound)||
            (lane->created&&lane->cleanup_reported&&lane->kernel_joined&&lane->bound&&lane->last_absent)))
            (void)three_finish(lanes,lane);
        (void)three_inheritance_cleanup(lane);
        (void)three_close(lane,&lane->pi.hThread,"cleanup-owned-primary-thread");
        (void)three_close(lane,&lane->request_read,"close-owned-host-request-end");
        (void)three_close(lane,&lane->reply_write,"close-owned-host-reply-end");
        if(!lane->created||lane->kernel_joined)
            (void)three_close(lane,&lane->pi.hProcess,"close-actually-joined-process-handle");
    }
    slots_released=(!lanes[0].setup_attempted||lanes[0].native_finished)&&
                   (!lanes[1].setup_attempted||lanes[1].native_finished);
    if(slots_released)
    {
        for(i=0;i<2;++i)
        {
            if(lanes[i].child)
            {
                BOOL destroyed=IsWindow(lanes[i].child)&&DestroyWindow(lanes[i].child);
                if(destroyed)lanes[i].child=NULL;
                if(!three_check(&lanes[i],destroyed,"destroy-owned-child-after-both-native-slots-released"))
                    lanes[i].local_cleanup=FALSE;
            }
        }
        if(root&&!lanes[0].child&&!lanes[1].child)
        {
            BOOL destroyed=IsWindow(root)&&DestroyWindow(root);
            if(destroyed)root=NULL;
            if(!check(destroyed,"destroy-common-root-after-both-observer-releases"))local_cleanup=FALSE;
        }
        if(module&&!root&&!lanes[0].child&&!lanes[1].child)
        {
            BOOL freed=FreeLibrary(module);
            if(freed){module=NULL;fixture=NULL;pixel_fixture=NULL;}
            if(!check(freed,"free-module-after-both-observer-releases-and-window-destruction"))local_cleanup=FALSE;
        }
        if(atom&&!root&&!lanes[0].child&&!lanes[1].child)
        {
            BOOL unregistered=UnregisterClassW(class_name,wc.hInstance);
            if(unregistered)atom=0;
            if(!check(unregistered,"unregister-common-host-window-class"))local_cleanup=FALSE;
        }
    }
    else local_cleanup=FALSE;
    if(clock_ready&&!room(NULL))local_cleanup=FALSE;
    cleanup_ok=local_cleanup&&!capture_error&&!ready_file&&!gate_file&&
        slots_released&&lanes[0].local_cleanup&&lanes[1].local_cleanup&&
        three_both_joined(lanes)&&(!lanes[0].created||lanes[0].cleanup_reported)&&
        (!lanes[1].created||lanes[1].cleanup_reported)&&!root&&!lanes[0].child&&!lanes[1].child&&!module&&!atom;
    for(i=0;i<2;++i)three_lane_summary(&lanes[i]);
    wire_sequence=0;
    record("three-summary","\"complete\":%s,\"cleanup\":%s,\"checks\":%u,\"failures\":%u,"
           "\"native_calls\":%u,\"host_pid\":%u,\"host_birth\":%llu,\"root\":%u,"
           "\"coexistence\":%s,\"independence\":%s,\"native_slots_released\":%s,"
           "\"lanes\":[{\"lane\":\"A\",\"pid\":%u,\"birth\":%llu,\"child\":%u,\"epoch\":%llu,"
           "\"cid\":%u,\"lease\":%llu,\"ticket\":%llu,\"complete\":%s,\"cleanup_reported\":%s,"
           "\"kernel_joined\":%s,\"exit_known\":%s,\"exit\":%u,\"exit_zero\":%s,\"forced\":%s,"
           "\"setup_attempted\":%s,\"setup_ack\":%s,\"created\":%s,\"native_finished\":%s,\"local_cleanup\":%s},"
           "{\"lane\":\"B\",\"pid\":%u,\"birth\":%llu,\"child\":%u,\"epoch\":%llu,"
           "\"cid\":%u,\"lease\":%llu,\"ticket\":%llu,\"complete\":%s,\"cleanup_reported\":%s,"
           "\"kernel_joined\":%s,\"exit_known\":%s,\"exit\":%u,\"exit_zero\":%s,\"forced\":%s,"
           "\"setup_attempted\":%s,\"setup_ack\":%s,\"created\":%s,\"native_finished\":%s,\"local_cleanup\":%s}],"
           "\"io_error\":%s,\"pipe_error\":%s,\"clock_error\":%s,\"timeout\":%s,\"quit_seen\":%s,"
           "\"elapsed_ms\":%llu,\"screenAccepted\":false,\"gpu_only\":%s,\"capture_images\":%s,"
           "\"capture_error\":%s,\"pixel_calls\":%u,\"capture_acks\":%u,"
           "\"ready_handle_owned\":%s,\"gate_handle_owned\":%s,"
           "\"note\":\"TEST ONLY work160 total180 Native4 external210; Native synchronous calls checked before/after, not preempted; four actual probes per slot; producers remain GPU-only; optional host image gates use fresh60 and A reply64 budgets, WindowServer single-window images independent of Space, no physical display or current Space proof; host pixel operations only HOST_WINDOW(3), no host GPU drawing; process exit is not resource SAFE; unknown observers/windows/module, capture handles and unjoined process handles retained\"",
           three_bool(complete),three_bool(cleanup_ok),checks,failures,native_calls,(unsigned)root_pid,
           (unsigned long long)root_birth,(unsigned)root_value,three_bool(coexistence),three_bool(independence),
           three_bool(slots_released),
           (unsigned)lanes[0].pid,(unsigned long long)lanes[0].birth,(unsigned)lanes[0].child_value,
           (unsigned long long)lanes[0].identity.epoch,(unsigned)lanes[0].identity.context_id,
           (unsigned long long)lanes[0].identity.lease,(unsigned long long)lanes[0].identity.ticket,
           three_bool(lanes[0].complete),three_bool(lanes[0].cleanup_reported),three_bool(lanes[0].kernel_joined),
           three_bool(lanes[0].exit_known),(unsigned)lanes[0].child_exit,three_bool(lanes[0].exit_zero),
           three_bool(lanes[0].forced),three_bool(lanes[0].setup_attempted),three_bool(lanes[0].setup_ack),
           three_bool(lanes[0].created),three_bool(lanes[0].native_finished),three_bool(lanes[0].local_cleanup),
           (unsigned)lanes[1].pid,(unsigned long long)lanes[1].birth,(unsigned)lanes[1].child_value,
           (unsigned long long)lanes[1].identity.epoch,(unsigned)lanes[1].identity.context_id,
           (unsigned long long)lanes[1].identity.lease,(unsigned long long)lanes[1].identity.ticket,
           three_bool(lanes[1].complete),three_bool(lanes[1].cleanup_reported),three_bool(lanes[1].kernel_joined),
           three_bool(lanes[1].exit_known),(unsigned)lanes[1].child_exit,three_bool(lanes[1].exit_zero),
           three_bool(lanes[1].forced),three_bool(lanes[1].setup_attempted),three_bool(lanes[1].setup_ack),
           three_bool(lanes[1].created),three_bool(lanes[1].native_finished),three_bool(lanes[1].local_cleanup),
           three_bool(io_error),three_bool(pipe_error),three_bool(clock_error),three_bool(timed_out),
           three_bool(quit_seen),(unsigned long long)(GetTickCount64()-entry_tick),
           three_bool(!capture_images),three_bool(capture_images),three_bool(capture_error),pixel_calls,capture_acks,
           three_bool(ready_file!=NULL),three_bool(gate_file!=NULL));
    return !complete||!cleanup_ok||failures||!lanes[0].complete||!lanes[1].complete||
        !lanes[0].exit_zero||!lanes[1].exit_zero||lanes[0].forced||lanes[1].forced||
        capture_error||io_error||pipe_error||clock_error||timed_out||quit_seen?1:0;
}

int main(int argc,char **argv)
{
    entry_tick=GetTickCount64();
    if(setvbuf(stdout,NULL,_IONBF,0)!=0)io_error=TRUE;
    if(argc>1&&!strcmp(argv[1],"--producer"))
    {
        if(argc==8&&!strcmp(argv[7],"--gpu-only"))return producer_main(argc,argv);
        screen_required=FALSE;
        (void)check(FALSE,"three-process-producer-requires-explicit-gpu-only-argc8");
        return 1;
    }
    if(argc>1&&!strcmp(argv[1],"--triple-host"))return three_host_main(argc,argv);
    screen_required=FALSE;
    (void)check(FALSE,"expected-triple-host-argc4-gpu-only-or-argc5-capture-images-or-gpu-only-producer-argc8");
    record("three-summary","\"complete\":false,\"cleanup\":true,\"checks\":%u,\"failures\":%u,"
           "\"io_error\":%s,\"screenAccepted\":false,\"gpu_only\":true",checks,failures,three_bool(io_error));
    return 1;
}
