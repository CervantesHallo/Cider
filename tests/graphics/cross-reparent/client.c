#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include "status_cell.h"
#include "protocol.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Host owns both roots, the child slot and one created guest process. Producer alone
 * owns the exported context and RO mappings. Reply.status is the exact Native
 * NTSTATUS; frame.status is the distinct, unmodified operation result. */
typedef NTSTATUS (WINAPI *fixture_fn)(void *);
struct budget
{
    LONGLONG counter,ticks;
    ULONGLONG tick;
    unsigned seconds;
};
struct tree_change
{
    BOOL attempted,returned,effect_observed,verified;
    HWND before_parent,before_ancestor,previous_parent,actual_parent,actual_ancestor;
    DWORD win32_error,root1_pid,root2_pid,child_pid;
    DWORD root1_thread,root2_thread,child_thread;
};
static const char *role_name="invalid";
static unsigned checks,failures,native_calls;
static BOOL io_error,pipe_error,clock_error,timed_out,quit_seen,cleaning,clock_ready;
static BOOL identity_saved,ever_context,ever_lease,host_safe,producer_safe;
static BOOL native_clean=TRUE,host_bound,last_absent,host_finished,host_setup_ok,host_spawned,host_setup_acknowledged;
static UINT32 root_value,child_value,root_pid,producer_pid,attempts;
static UINT64 root_birth,producer_birth,wire_sequence,nonce[2];
static LONGLONG frequency;
static ULONGLONG entry_tick;
static struct budget total_budget,work_budget;
static struct live_host_fixture identity,last_frame;
static NTSTATUS last_nt;
static BOOL last_nt_valid;
static fixture_fn fixture;

static void record(const char *event,const char *format,...)
{
    char line[4096];
    size_t used;
    int n;
    va_list args;
    n=snprintf(line,sizeof(line),"{\"type\":\"cross-reparent\",\"role\":\"%s\","
               "\"event\":\"%s\",\"bits\":%u,\"pid\":%u,\"sequence\":%llu,"
               "\"nonce\":[%llu,%llu],",role_name,event,
               (unsigned)(sizeof(void *)*CHAR_BIT),(unsigned)GetCurrentProcessId(),
               (unsigned long long)wire_sequence,(unsigned long long)nonce[0],(unsigned long long)nonce[1]);
    if(n<0||(size_t)n>=sizeof(line)){io_error=TRUE;return;}
    used=(size_t)n;
    va_start(args,format);
    n=vsnprintf(line+used,sizeof(line)-used,format,args);
    va_end(args);
    if(n<0||(size_t)n>=sizeof(line)-used){io_error=TRUE;return;}
    used+=(size_t)n;
    if(sizeof(line)-used<3){io_error=TRUE;return;}
    line[used++]='}';line[used++]='\n';
    /* One complete, bounded record per stdio write on the shared stdout.
     * No subsequent success can clear a short write, fflush, or ferror. */
    if(fwrite(line,1,used,stdout)!=used||fflush(stdout)==EOF||ferror(stdout))io_error=TRUE;
}

static BOOL check(BOOL ok,const char *name)
{
    DWORD error=GetLastError();
    ++checks;
    if(!ok)++failures;
    record("check","\"check\":%u,\"name\":\"%s\",\"ok\":%s,\"last_error\":%u",
           checks,name,ok?"true":"false",ok?0u:(unsigned)error);
    return ok;
}

static BOOL frame_words(const struct live_host_fixture *f,char *raw,size_t capacity)
{
    UINT32 words[40];
    size_t used=0;
    unsigned i;
    memcpy(words,f,sizeof(words));
    for(i=0;i<40;++i)
    {
        int n=snprintf(raw+used,capacity-used,"%s%u",i?",":"",(unsigned)words[i]);
        if(n<0||(size_t)n>=capacity-used){io_error=TRUE;return FALSE;}
        used+=(size_t)n;
    }
    return TRUE;
}

static void frame_record(const char *event,const struct live_host_fixture *f,
                         BOOL nt_valid,NTSTATUS nt,unsigned wire_op)
{
    char raw[640];
    if(!frame_words(f,raw,sizeof(raw)))return;
    record(event,"\"wire_op\":%u,\"native_call\":%u,\"ntstatus_valid\":%s,"
           "\"ntstatus\":%u,\"status\":%u,\"operation\":%u,\"frame_role\":%u,"
           "\"root_pid\":%u,\"producer_pid\":%u,\"cocoa_valid\":%u,"
           "\"attached\":%u,\"ledger\":%u,\"indexed\":%u,\"frame_words\":[%s]",
           wire_op,native_calls,nt_valid?"true":"false",(unsigned)(UINT32)nt,
           (unsigned)f->status,(unsigned)f->operation,(unsigned)f->role,
           (unsigned)f->root_pid,(unsigned)f->producer_pid,(unsigned)f->cocoa_valid,
           (unsigned)f->attached,(unsigned)f->ledger,(unsigned)f->indexed,raw);
}

static void pair_record(const struct live_host_fixture *producer,const struct live_host_fixture *host)
{
    char p[640],h[640];
    if(!frame_words(producer,p,sizeof(p))||!frame_words(host,h,sizeof(h)))return;
    record("pipe-pair","\"producer_frame_words\":[%s],\"host_frame_words\":[%s]",p,h);
}

static void message_record(const char *event,const char *direction,const struct cross_message *m)
{
    char raw[640];
    if(!frame_words(&m->frame,raw,sizeof(raw)))return;
    record(event,"\"direction\":\"%s\",\"wire_size\":%u,\"wire_version\":%u,\"wire_op\":%u,"
           "\"wire_sequence\":%llu,\"wire_nonce\":[%llu,%llu],\"ntstatus\":%u,"
           "\"operation\":%u,\"status\":%u,\"frame_words\":[%s]",direction,
           (unsigned)m->size,(unsigned)m->version,(unsigned)m->operation,
           (unsigned long long)m->sequence,(unsigned long long)m->nonce[0],
           (unsigned long long)m->nonce[1],(unsigned)m->status,
           (unsigned)m->frame.operation,(unsigned)m->frame.status,raw);
}

static BOOL counter_now(LONGLONG *out)
{
    LARGE_INTEGER value;
    if(!QueryPerformanceCounter(&value)||value.QuadPart<0){clock_error=TRUE;return FALSE;}
    *out=value.QuadPart;
    return TRUE;
}

static BOOL begin_budget(struct budget *b,unsigned seconds)
{
    b->tick=GetTickCount64();b->seconds=seconds;b->counter=0;b->ticks=0;
    if(!clock_ready||!seconds||seconds>25u||frequency>INT64_MAX/(LONGLONG)seconds||
       !counter_now(&b->counter))return FALSE;
    b->ticks=frequency*(LONGLONG)seconds;
    return TRUE;
}

static BOOL budget_alive(const struct budget *b)
{
    ULONGLONG tick=GetTickCount64();
    LONGLONG value=0;
    BOOL qpc_ok;
    if(tick<b->tick){clock_error=TRUE;return FALSE;}
    if(tick-b->tick>=(ULONGLONG)b->seconds*1000u){timed_out=TRUE;return FALSE;}
    qpc_ok=b->ticks>0&&counter_now(&value);
    if(qpc_ok&&value<b->counter){clock_error=TRUE;qpc_ok=FALSE;}
    if(qpc_ok&&value-b->counter>=b->ticks){timed_out=TRUE;return FALSE;}
    /* Independent monotonic ticks still bound failure cleanup after a broken
     * QPC. The sticky clock fault prevents a passing result. */
    return qpc_ok||cleaning;
}

static BOOL room(const struct budget *b)
{
    return budget_alive(&total_budget)&&(cleaning||budget_alive(&work_budget))&&(!b||budget_alive(b));
}

static BOOL initialize_clock(void)
{
    LARGE_INTEGER f;
    if(!QueryPerformanceFrequency(&f)||f.QuadPart<=0||f.QuadPart>INT64_MAX/25)
    {clock_error=TRUE;return FALSE;}
    frequency=f.QuadPart;clock_ready=TRUE;
    if(!begin_budget(&total_budget,25)||!begin_budget(&work_budget,20))return FALSE;
    total_budget.tick=entry_tick;
    return TRUE;
}

static void pump(void)
{
    MSG message;
    unsigned count;
    for(count=0;count<64&&PeekMessageW(&message,NULL,0,0,PM_REMOVE);++count)
    {
        if(message.message==WM_QUIT){quit_seen=TRUE;continue;}
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

static BOOL same_identity(const struct live_host_fixture *a,const struct live_host_fixture *b)
{
    return a->root==b->root&&a->child==b->child&&a->root_pid==b->root_pid&&
           a->root_birth==b->root_birth&&a->epoch==b->epoch&&a->lease==b->lease&&
           a->ticket==b->ticket&&a->context_id==b->context_id&&
           a->producer_pid==b->producer_pid&&a->birth==b->birth;
}

static BOOL envelope(const struct live_host_fixture *f,unsigned operation)
{
    return f->size==sizeof(*f)&&f->version==LIVE_FIXTURE_VERSION&&f->operation==operation&&
           f->root==root_value&&f->child==child_value;
}

static BOOL producer_base(const struct live_host_fixture *f)
{
    return f->role==LIVE_ROLE_PRODUCER&&f->root_pid==root_pid&&root_pid&&
           f->root_birth==root_birth&&root_birth&&f->producer_pid==producer_pid&&producer_pid&&
           f->birth==producer_birth&&producer_birth&&root_pid!=producer_pid&&
           !f->window_owned&&!f->cocoa_valid&&!f->probe_bound&&!f->probe_sequence;
}

static BOOL host_observation(const struct live_host_fixture *f)
{
    return f->role==LIVE_ROLE_HOST&&identity_saved&&same_identity(f,&identity)&&f->probe_bound==1&&
           !f->context_owned&&!f->host_reader_owned&&!f->producer_reader_owned&&f->cocoa_valid==1;
}

static BOOL models(const struct live_host_fixture *f,unsigned count)
{
    return f->cocoa_valid==1&&f->attached==count&&f->ledger==count&&f->indexed==count;
}

static BOOL no_producer_owners(const struct live_host_fixture *f)
{
    return !f->context_owned&&!f->host_reader_owned&&!f->producer_reader_owned&&!f->window_owned;
}

/* Effects are latched BEFORE logging or post-clock acceptance. A late call may
 * have really installed/released/cleaned up; a dead process supplies no SAFE. */
static void observe(unsigned operation,const struct live_host_fixture *f,NTSTATUS nt)
{
    if(!envelope(f,operation))return;
    if(operation==LIVE_HOST_SETUP&&!nt&&f->role==LIVE_ROLE_HOST&&f->root_pid==root_pid)
        host_setup_acknowledged=TRUE; /* Reached Native setup, including its partial-error return. */
    if(f->role==LIVE_ROLE_PRODUCER)
    {
        if(f->context_owned)ever_context=TRUE;
        if(f->lease)ever_lease=TRUE;
        if(nt||!producer_base(f)||(identity_saved&&!same_identity(f,&identity)))return;
        if(f->host_read==MACDRV_STATUS_READ_SAFE_ONLY&&!f->host_values_valid)host_safe=TRUE;
        if(f->producer_read==MACDRV_STATUS_READ_SAFE_ONLY&&!f->producer_values_valid)producer_safe=TRUE;
        if(operation==LIVE_CLEANUP&&!f->status&&no_producer_owners(f)&&!f->close_status&&
           (!ever_context||(f->flags&LIVE_CONTEXT_RELEASED))&&(!ever_lease||producer_safe)&&
           (!(attempts&(1u<<(LIVE_INSTALL-1)))||host_safe||producer_safe))native_clean=TRUE;
    }
    else if(f->role==LIVE_ROLE_HOST&&!nt&&!f->status)
    {
        if(operation==LIVE_HOST_SETUP&&f->root_pid==root_pid&&f->root_birth==root_birth&&f->epoch&&
           f->window_owned==1&&!f->context_id&&!f->ticket&&!f->lease&&!f->producer_pid&&!f->birth&&
           !f->probe_bound&&!f->probe_sequence&&
           !f->context_owned&&!f->host_reader_owned&&!f->producer_reader_owned)host_setup_ok=TRUE;
        if(operation==LIVE_HOST_FINISH&&!host_spawned&&host_setup_acknowledged&&!host_bound&&!identity_saved&&
           f->root_pid==root_pid&&!f->window_owned&&
           !f->producer_pid&&!f->lease&&!f->ticket&&!f->context_id&&!f->context_owned&&
           !f->host_reader_owned&&!f->producer_reader_owned)
        {host_finished=TRUE;native_clean=TRUE;} /* No created producer/import exists; not a model-absence claim. */
        if((operation==LIVE_HOST_FINISH||(operation==LIVE_HOST_PROBE&&wire_sequence&&
            f->probe_sequence==wire_sequence))&&host_observation(f))
        {
            host_bound=TRUE;last_absent=models(f,0);
            if(operation==LIVE_HOST_FINISH&&last_absent&&!f->window_owned)
            {host_finished=TRUE;native_clean=TRUE;}
        }
    }
}

static BOOL native_operation(unsigned operation,struct live_host_fixture *f,const struct budget *b)
{
    UINT32 bit=1u<<(operation-1);
    NTSTATUS nt;
    last_nt_valid=FALSE;
    if(!fixture||!room(b)||(!cleaning&&(io_error||pipe_error||quit_seen)))return FALSE;
    if(operation!=LIVE_SNAPSHOT&&operation!=LIVE_CLEANUP&&operation!=LIVE_HOST_PROBE&&operation!=LIVE_HOST_FINISH&&(attempts&bit))return FALSE;
    attempts|=bit; /* Irreversible effects, particularly INSTALL, are never replayed. */
    if(operation==LIVE_SETUP||operation==LIVE_HOST_SETUP)native_clean=FALSE;
    f->size=sizeof(*f);f->version=LIVE_FIXTURE_VERSION;f->operation=operation;
    f->root=root_value;f->child=child_value;f->status=ERROR_INVALID_DATA;
    nt=fixture(f);
    ++native_calls;last_nt=nt;last_nt_valid=TRUE;last_frame=*f;
    observe(operation,f,nt);
    frame_record("frame",f,TRUE,nt,0);
    return !nt&&envelope(f,operation)&&room(b)&&(cleaning||(!io_error&&!pipe_error&&!quit_seen));
}

static BOOL producer_operation(unsigned operation,struct live_host_fixture *f,const struct budget *b)
{
    memset(f,0,sizeof(*f));
    return native_operation(operation,f,b);
}

static BOOL producer_once(unsigned operation,struct live_host_fixture *f)
{
    struct budget b;
    return begin_budget(&b,4)&&producer_operation(operation,f,&b)&&!f->status;
}

static BOOL load_fixture(HMODULE *module)
{
    *module=LoadLibraryW(L"winemac.drv");
    if(!check(*module!=NULL,"load-fixture-module"))return FALSE;
    fixture=(fixture_fn)(uintptr_t)GetProcAddress(*module,"cider_live_host_fixture");
    return check(fixture!=NULL,"native-fixture-export");
}

static BOOL close_owned(HANDLE *handle,const char *name)
{
    BOOL ok;
    if(!*handle)return TRUE;
    ok=CloseHandle(*handle);
    if(ok)*handle=NULL;
    return check(ok,name);
}

static BOOL process_birth(HANDLE process,UINT64 *birth)
{
    FILETIME created,exited,kernel,user;
    if(!GetProcessTimes(process,&created,&exited,&kernel,&user))return FALSE;
    *birth=((UINT64)created.dwHighDateTime<<32)|created.dwLowDateTime;
    return *birth!=0;
}

static int hex_digit(unsigned char c)
{
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}

static BOOL hex_value(const char *s,size_t maximum,UINT64 *value)
{
    size_t n=0;
    UINT64 v=0;
    int digit;
    if(!s||!*s)return FALSE;
    while(s[n])
    {
        if(n>=maximum||(digit=hex_digit((unsigned char)s[n]))<0||
           v>(UINT64_MAX-(unsigned)digit)/16u)return FALSE;
        v=v*16u+(unsigned)digit;++n;
    }
    *value=v;
    return TRUE;
}

static BOOL parse_nonce(const char *s)
{
    char half[17];
    if(strlen(s)!=32)return FALSE;
    memcpy(half,s,16);half[16]=0;
    if(!hex_value(half,16,&nonce[0]))return FALSE;
    memcpy(half,s+16,16);
    return hex_value(half,16,&nonce[1]);
}

static BOOL parse_window(const char *s,UINT32 *value)
{
    UINT64 v;
    if(!hex_value(s,8,&v)||!v||v>=UINT32_MAX)return FALSE;
    *value=(UINT32)v;
    return TRUE;
}

static BOOL window_value(HWND window,UINT32 *value)
{
    uintptr_t raw=(uintptr_t)window;
    UINT32 low=(UINT32)raw;
    if(!low||low==UINT32_MAX||(raw!=(uintptr_t)low&&raw!=(uintptr_t)(intptr_t)(int32_t)low))return FALSE;
    *value=low;
    return TRUE;
}

static BOOL pipe_argument(const char *s,HANDLE *handle)
{
    UINT64 value;
    HANDLE h;
    DWORD flags;
    if(!hex_value(s,2*sizeof(uintptr_t),&value)||!value)return FALSE;
    h=(HANDLE)(uintptr_t)value;
    if((UINT64)(uintptr_t)h!=value||h==INVALID_HANDLE_VALUE||GetFileType(h)!=FILE_TYPE_PIPE||
       !GetHandleInformation(h,&flags)||!(flags&HANDLE_FLAG_INHERIT))return FALSE;
    *handle=h; /* validated inherited ownership, before modifying its flags */
    return SetHandleInformation(h,HANDLE_FLAG_INHERIT,0);
}

static BOOL peer_path(const char *s,WCHAR *wide)
{
    static const char prefix[]="C:\\CiderTests\\";
    size_t i,n=strlen(s),segment=sizeof(prefix)-1;
    if(n<=segment||n>=MAX_PATH||strncmp(s,prefix,segment))return FALSE;
    for(i=0;i<n;++i)
    {
        unsigned char c=(unsigned char)s[i];
        if(c<32||c>126||c=='"'||c=='/'||c=='<'||c=='>'||c=='|'||c=='*'||c=='?'||
           (c==':'&&i!=1))return FALSE;
        if(i>=sizeof(prefix)-1&&c=='\\')
        {
            if(i==segment||s[i-1]=='.'||s[i-1]==' ')return FALSE;
            segment=i+1;
        }
        wide[i]=(WCHAR)c;
    }
    if(segment>=n||s[n-1]=='.'||s[n-1]==' '||n<4||s[n-4]!='.'||
       (s[n-3]!='e'&&s[n-3]!='E')||(s[n-2]!='x'&&s[n-2]!='X')||
       (s[n-1]!='e'&&s[n-1]!='E'))return FALSE;
    wide[n]=0;
    return TRUE;
}

/* Fresh 4096-byte anonymous pipes carry just four 200-byte records each, with
 * one request outstanding. Neither writer can fill its pipe if the peer stops
 * reading. Partial I/O advances an offset; every ReadFile is preceded by a
 * successful PeekNamedPipe byte count and reads no more than that count. */
static BOOL write_message(HANDLE pipe,const struct cross_message *m,const struct budget *b)
{
    DWORD used=0;
    while(used<(DWORD)sizeof(*m))
    {
        DWORD written=0;
        BOOL ok;
        pump();
        if(!room(b)||(!cleaning&&quit_seen))return FALSE;
        ok=WriteFile(pipe,(const unsigned char *)m+used,(DWORD)sizeof(*m)-used,&written,NULL);
        if(written<=(DWORD)sizeof(*m)-used)used+=written;else ok=FALSE;
        if(!ok||!written){pipe_error=TRUE;return FALSE;}
        if(!room(b))return FALSE;
    }
    return !io_error;
}

static BOOL read_message(HANDLE pipe,struct cross_message *m,HANDLE peer,const struct budget *b)
{
    DWORD used=0;
    memset(m,0,sizeof(*m));
    while(used<(DWORD)sizeof(*m))
    {
        DWORD available=0,got=0,want;
        BOOL ok;
        pump();
        if(!room(b)||(!cleaning&&(io_error||quit_seen)))return FALSE;
        if(!PeekNamedPipe(pipe,NULL,0,NULL,&available,NULL)){pipe_error=TRUE;return FALSE;}
        if(available)
        {
            want=(DWORD)sizeof(*m)-used;
            if(want>available)want=available;
            ok=ReadFile(pipe,(unsigned char *)m+used,want,&got,NULL);
            if(got<=want)used+=got;else ok=FALSE;
            if(!ok||!got){pipe_error=TRUE;return FALSE;}
            if(!room(b))return FALSE;
        }
        else
        {
            if(peer)
            {
                DWORD wait=WaitForSingleObject(peer,0);
                if(wait!=WAIT_TIMEOUT)
                {
                    record("peer-stop","\"wait_result\":%u,\"partial_bytes\":%u",(unsigned)wait,(unsigned)used);
                    return FALSE;
                }
            }
            Sleep(2);
        }
    }
    return TRUE;
}

static BOOL wire_header(const struct cross_message *m,unsigned operation,UINT64 sequence)
{
    return m->size==sizeof(*m)&&m->version==1&&m->operation==operation&&
           m->nonce[0]==nonce[0]&&m->nonce[1]==nonce[1]&&sequence&&m->sequence==sequence;
}

static void make_message(struct cross_message *m,unsigned operation,UINT64 sequence,
                         const struct live_host_fixture *f,NTSTATUS nt)
{
    memset(m,0,sizeof(*m));
    m->size=sizeof(*m);m->version=1;m->operation=operation;m->status=(UINT32)nt;
    m->nonce[0]=nonce[0];m->nonce[1]=nonce[1];m->sequence=sequence;m->frame=*f;
}

static BOOL exchange(HANDLE read_pipe,HANDLE write_pipe,unsigned operation,
                     const struct live_host_fixture *producer_frame,unsigned count)
{
    struct budget b;
    struct cross_message request,reply;
    if(!begin_budget(&b,4)||wire_sequence==UINT64_MAX)return FALSE;
    ++wire_sequence; /* Send + receive + validation share ONE four second budget. */
    make_message(&request,operation,wire_sequence,producer_frame,last_nt);
    message_record("pipe-request","send-attempt",&request);
    if(!write_message(write_pipe,&request,&b)||!read_message(read_pipe,&reply,NULL,&b))return FALSE;
    message_record("pipe-reply","receive",&reply);
    pair_record(producer_frame,&reply.frame);
    return wire_header(&reply,operation,wire_sequence)&&!reply.status&&
           envelope(&reply.frame,LIVE_HOST_PROBE)&&!reply.frame.status&&
           host_observation(&reply.frame)&&reply.frame.probe_sequence==wire_sequence&&
           reply.frame.window_owned==1&&models(&reply.frame,count)&&same_identity(producer_frame,&reply.frame)&&room(&b);
}

static BOOL poll_safe(struct live_host_fixture *f,BOOL want_producer)
{
    struct budget b;
    if(!begin_budget(&b,4))return FALSE;
    do
    {
        if(!producer_operation(LIVE_SNAPSHOT,f,&b)||(f->status&&f->status!=ERROR_IO_PENDING)||
           !producer_base(f)||!same_identity(f,&identity)||f->context_owned!=1||
           f->host_reader_owned!=1||f->producer_reader_owned!=1)return FALSE;
        if(want_producer?producer_safe:host_safe)return TRUE;
        Sleep(5);
    }while(room(&b));
    return FALSE;
}

static BOOL poll_cleanup(struct live_host_fixture *f)
{
    struct budget b;
    if(!begin_budget(&b,4))return FALSE;
    do
    {
        if(!producer_operation(LIVE_CLEANUP,f,&b)||(f->status&&f->status!=ERROR_IO_PENDING))return FALSE;
        if(native_clean)return !f->status&&no_producer_owners(f)&&!f->close_status;
        Sleep(5);
    }while(room(&b));
    return FALSE;
}

static void producer_failure_cleanup(void)
{
    struct budget b;
    struct live_host_fixture f;
    BOOL started=begin_budget(&b,4);
    if(!started&&!clock_error)return;
    record("cleanup-note","\"note\":\"release once; require actual producer RO SAFE before context cleanup; unknown permission retains context/module/pipes\"");
    if(last_frame.lease&&!(attempts&(1u<<(LIVE_RELEASE_PRODUCER-1))))
        (void)producer_operation(LIVE_RELEASE_PRODUCER,&f,&b);
    while(room(&b)&&!native_clean)
    {
        BOOL ok=producer_operation(LIVE_SNAPSHOT,&f,&b);
        if(!ok||(f.status&&f.status!=ERROR_IO_PENDING)||!producer_base(&f)||
           (identity_saved&&!same_identity(&f,&identity)))break;
        if(f.context_owned&&!producer_safe){Sleep(5);continue;}
        if(!producer_operation(LIVE_CLEANUP,&f,&b)||(f.status&&f.status!=ERROR_IO_PENDING))break;
        if(!native_clean)Sleep(5);
    }
}

static int producer_main(int argc,char **argv)
{
    HMODULE module=NULL;
    HANDLE read_pipe=NULL,write_pipe=NULL,parent=NULL;
    HWND root,child;
    DWORD owner=0,child_pid=0;
    UINT32 ancestor=0;
    struct live_host_fixture f;
    struct budget b;
    BOOL complete=FALSE,local_cleanup=TRUE,valid;
    UINT32 required=LIVE_SETUP_DONE|LIVE_PREPARE_SENT|LIVE_INSTALL_SENT|
                    LIVE_RELEASE_SENT|LIVE_CONTEXT_RELEASED;
    role_name="producer";producer_pid=GetCurrentProcessId();
    if(!check(argc==7,"producer-argument-count"))goto done;
    if(!check(initialize_clock(),"qpc-and-aggregate-watchdog"))goto done;
    if(!check(parse_window(argv[2],&root_value)&&parse_window(argv[3],&child_value)&&
              root_value!=child_value&&parse_nonce(argv[4]),"producer-window-values-and-nonce"))goto done;
    if(!check(pipe_argument(argv[5],&read_pipe)&&pipe_argument(argv[6],&write_pipe)&&
              read_pipe!=write_pipe,"owned-pipe-handle-width-type-and-inheritance"))goto done;
    root=(HWND)(uintptr_t)root_value;child=(HWND)(uintptr_t)child_value;
    valid=GetWindowThreadProcessId(root,&owner)!=0&&GetWindowThreadProcessId(child,&child_pid)!=0&&
          child_pid==owner&&owner!=producer_pid&&window_value(GetAncestor(child,GA_ROOT),&ancestor)&&ancestor==root_value;
    root_pid=(UINT32)owner;
    if(!check(valid,"actual-foreign-parent-owned-root-and-placeholder"))goto done;
    parent=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,(DWORD)root_pid);
    if(!check(parent!=NULL&&process_birth(parent,&root_birth)&&
              process_birth(GetCurrentProcess(),&producer_birth),"actual-parent-and-producer-births"))goto done;
    if(!close_owned(&parent,"close-owned-parent-query-handle"))goto done;
    if(!load_fixture(&module))goto done;
    if(!check(producer_once(LIVE_SETUP,&f),"actual-foreign-root-producer-setup"))goto done;
    if(!check(producer_base(&f)&&f.epoch&&f.lease&&f.context_id&&f.context_owned==1&&
              f.producer_reader_owned==1&&!f.host_reader_owned&&!f.ticket&&(f.flags&LIVE_SETUP_DONE)&&
              f.producer_read==MACDRV_STATUS_READ_SNAPSHOT&&f.producer_values_valid==1,
              "actual-context-producer-map-and-queried-root-identity"))goto done;
    identity=f;
    if(!check(producer_once(LIVE_PREPARE,&f),"prepare-known-ticket-once"))goto done;
    if(!check(producer_base(&f)&&f.root_birth==identity.root_birth&&f.epoch==identity.epoch&&
              f.lease==identity.lease&&f.context_id==identity.context_id&&f.birth==identity.birth&&
              f.ticket&&f.context_owned==1&&f.host_reader_owned==1&&f.producer_reader_owned==1&&
              f.host_read==MACDRV_STATUS_READ_SNAPSHOT&&f.host_values_valid==1&&
              (f.flags&LIVE_PREPARE_SENT)&&!(f.flags&LIVE_INSTALL_SENT),"actual-ticket-and-ro-before-install"))goto done;
    identity=f;identity_saved=TRUE;
    if(!check(begin_budget(&b,4)&&producer_operation(LIVE_INSTALL,&f,&b)&&
              (!f.status||f.status==ERROR_IO_PENDING),"install-once-preserve-actual-status"))goto done;
    if(!check(producer_base(&f)&&same_identity(&f,&identity)&&f.context_owned==1&&
              f.host_reader_owned==1&&f.producer_reader_owned==1&&(f.flags&LIVE_INSTALL_SENT),
              "producer-context-held-without-local-cocoa-evidence"))goto done;
    if(!check(exchange(read_pipe,write_pipe,CROSS_PROBE,&f,1),"actual-host-bound-installed-models-one"))goto done;
    if(!check(producer_once(LIVE_SNAPSHOT,&f),"fresh-live-snapshot-before-host-reparent"))goto done;
    if(!check(producer_base(&f)&&same_identity(&f,&identity)&&f.context_owned==1&&
              f.host_reader_owned==1&&f.producer_reader_owned==1&&
              f.flags==(LIVE_SETUP_DONE|LIVE_PREPARE_SENT|LIVE_INSTALL_SENT)&&
              f.host_read==MACDRV_STATUS_READ_SNAPSHOT&&f.host_values_valid==1&&
              f.producer_read==MACDRV_STATUS_READ_SNAPSHOT&&f.producer_values_valid==1,
              "active-host-ro-and-context-held-before-ordinary-reparent"))goto done;
    if(!check(exchange(read_pipe,write_pipe,CROSS_REPARENT,&f,0),
              "actual-host-models-zero-after-ordinary-reparent-without-cancel"))goto done;
    if(!check(poll_safe(&f,FALSE),"actual-host-ro-safe-values-invalid-context-held"))goto done;
    if(!check(f.host_read==MACDRV_STATUS_READ_SAFE_ONLY&&!f.host_values_valid&&f.context_owned==1&&
              f.host_reader_owned==1&&f.producer_reader_owned==1&&
              f.flags==(LIVE_SETUP_DONE|LIVE_PREPARE_SENT|LIVE_INSTALL_SENT),
              "retained-context-and-readers-after-automatic-host-revocation"))goto done;
    if(!check(exchange(read_pipe,write_pipe,CROSS_RETURN,&f,0),
              "actual-old-root-models-still-zero-after-child-return"))goto done;
    if(!check(begin_budget(&b,4)&&producer_operation(LIVE_RELEASE_PRODUCER,&f,&b)&&
              (!f.status||f.status==ERROR_IO_PENDING),"actual-producer-release-once"))goto done;
    if(!check(poll_safe(&f,TRUE)&&f.producer_read==MACDRV_STATUS_READ_SAFE_ONLY&&
              !f.producer_values_valid&&f.context_owned==1,"actual-producer-safe-before-local-release"))goto done;
    if(!check(poll_cleanup(&f),"actual-context-and-ro-cleanup"))goto done;
    if(!check(producer_base(&f)&&same_identity(&f,&identity)&&!f.status&&!f.close_status&&
              f.flags==required&&!(f.flags&LIVE_CANCEL_SENT)&&no_producer_owners(&f)&&
              f.host_read==MACDRV_STATUS_READ_INVALID&&f.producer_read==MACDRV_STATUS_READ_INVALID&&
              !f.host_values_valid&&!f.producer_values_valid,"actual-final-producer-frame"))goto done;
    if(!check(exchange(read_pipe,write_pipe,CROSS_FINISHED,&f,0),"parent-fresh-absence-ack-after-producer-cleanup"))goto done;
    complete=TRUE;
done:
    cleaning=TRUE;
    if(!native_clean&&fixture&&clock_ready)producer_failure_cleanup();
    if(parent&&!close_owned(&parent,"cleanup-parent-query-handle"))local_cleanup=FALSE;
    if(native_clean)
    {
        if(read_pipe&&read_pipe==write_pipe)write_pipe=NULL;
        if(!close_owned(&read_pipe,"close-owned-reply-pipe"))local_cleanup=FALSE;
        if(!close_owned(&write_pipe,"close-owned-request-pipe"))local_cleanup=FALSE;
        if(module)
        {
            BOOL ok=FreeLibrary(module);
            if(ok){module=NULL;fixture=NULL;}
            if(!check(ok,"release-producer-module"))local_cleanup=FALSE;
        }
    }
    else local_cleanup=FALSE;
    if(clock_ready&&!room(NULL))local_cleanup=FALSE;
    record("summary","\"complete\":%s,\"checks\":%u,\"failures\":%u,\"attempts\":%u,"
           "\"host_safe_observed\":%s,\"producer_safe_observed\":%s,\"native_cleanup\":%s,"
           "\"cleanup\":%s,\"retained_for_unknown_safety\":%s,\"io_error\":%s,\"pipe_error\":%s,"
           "\"clock_error\":%s,\"timeout\":%s,\"elapsed_ms\":%llu,"
           "\"note\":\"producer has no local Cocoa evidence; process exit is not internal resource SAFE\"",
           complete?"true":"false",checks,failures,(unsigned)attempts,host_safe?"true":"false",
           producer_safe?"true":"false",native_clean?"true":"false",local_cleanup&&native_clean?"true":"false",
           !native_clean?"true":"false",io_error?"true":"false",pipe_error?"true":"false",
           clock_error?"true":"false",timed_out?"true":"false",(unsigned long long)(GetTickCount64()-entry_tick));
    return !complete||failures||!native_clean||!local_cleanup||io_error||pipe_error||clock_error||timed_out||quit_seen?1:0;
}

static BOOL host_request(const struct cross_message *m,unsigned phase,
                         const struct live_host_fixture *setup,UINT64 previous_native_sequence)
{
    const struct live_host_fixture *f=&m->frame;
    unsigned operation=phase==1?CROSS_PROBE:(phase==2?CROSS_REPARENT:(phase==3?CROSS_RETURN:CROSS_FINISHED));
    unsigned native_op=phase==1?LIVE_INSTALL:(phase<4?LIVE_SNAPSHOT:LIVE_CLEANUP);
    UINT32 required=LIVE_SETUP_DONE|LIVE_PREPARE_SENT|LIVE_INSTALL_SENT;
    if(phase<1||phase>4||!wire_header(m,operation,(UINT64)phase)||m->status||
       !envelope(f,native_op)||!producer_base(f)||
       !f->lease||!f->ticket||!f->context_id||!f->sequence||f->epoch!=setup->epoch||
       f->root_pid!=setup->root_pid||f->root_birth!=setup->root_birth||
       (identity_saved&&!same_identity(f,&identity)))return FALSE;
    /* Pure snapshots send no control packet and cannot advance its sequence. */
    if((phase==2||phase==3)&&f->sequence!=previous_native_sequence)return FALSE;
    if(phase==4&&f->sequence<=previous_native_sequence)return FALSE;
    if(phase==4)required|=LIVE_RELEASE_SENT|LIVE_CONTEXT_RELEASED;
    if(f->flags!=required||(f->flags&LIVE_CANCEL_SENT))return FALSE;
    if(phase==1||phase==2)
        return (!f->status||f->status==ERROR_IO_PENDING)&&f->context_owned==1&&
               (phase==1||!f->status)&&
               f->host_reader_owned==1&&f->producer_reader_owned==1&&
               f->host_read==MACDRV_STATUS_READ_SNAPSHOT&&f->host_values_valid==1&&
               f->producer_read==MACDRV_STATUS_READ_SNAPSHOT&&f->producer_values_valid==1;
    if(phase==3)
        return !f->status&&f->context_owned==1&&f->host_reader_owned==1&&f->producer_reader_owned==1&&
               f->host_read==MACDRV_STATUS_READ_SAFE_ONLY&&!f->host_values_valid;
    return !f->status&&!f->close_status&&no_producer_owners(f)&&
           f->host_read==MACDRV_STATUS_READ_INVALID&&f->producer_read==MACDRV_STATUS_READ_INVALID&&
           !f->host_values_valid&&!f->producer_values_valid;
}

static BOOL change_tree(HWND root1,HWND root2,HWND child,HWND old_parent,HWND new_parent,
                        UINT32 root2_value,struct tree_change *change,const struct budget *b)
{
    BOOL within;
    if(change->attempted)return FALSE;
    change->before_parent=GetParent(child);change->before_ancestor=GetAncestor(child,GA_ROOT);
    if(root1==root2||old_parent==new_parent||change->before_parent!=old_parent||
       change->before_ancestor!=old_parent||!room(b)||io_error||pipe_error||quit_seen)return FALSE;
    /* Invalidate earlier absence before any tree effect. No failed/late call
     * may reuse a pre-change absence to release the bound Native observer. */
    last_absent=FALSE;change->attempted=TRUE;
    SetLastError(ERROR_SUCCESS);
    change->previous_parent=SetParent(child,new_parent);
    change->win32_error=GetLastError();change->returned=TRUE;
    change->actual_parent=GetParent(child);change->actual_ancestor=GetAncestor(child,GA_ROOT);
    change->root1_thread=GetWindowThreadProcessId(root1,&change->root1_pid);
    change->root2_thread=GetWindowThreadProcessId(root2,&change->root2_pid);
    change->child_thread=GetWindowThreadProcessId(child,&change->child_pid);
    change->effect_observed=change->actual_parent==new_parent&&change->actual_ancestor==new_parent;
    change->verified=change->previous_parent==old_parent&&change->effect_observed&&
                     change->root1_thread&&change->root2_thread&&change->child_thread&&
                     change->root1_pid==root_pid&&change->root2_pid==root_pid&&change->child_pid==root_pid&&
                     GetAncestor(root1,GA_ROOT)==root1&&GetAncestor(root2,GA_ROOT)==root2;
    /* The complete effect ledger is committed before post-clock acceptance or
     * I/O. Preserve the raw zero/nonzero Win32 return AND its captured error;
     * only the expected nonzero previous parent plus the real tree can pass. */
    within=room(b);
    record("tree-change","\"root1\":%u,\"root2\":%u,\"child\":%u,"
           "\"oldparent\":%llu,\"newparent\":%llu,\"before_parent\":%llu,\"before_ancestor\":%llu,"
           "\"actual_parent\":%llu,\"actualancestor\":%llu,\"win32_return\":%llu,\"win32_error\":%u,"
           "\"observed_pid\":%u,\"root1_pid\":%u,\"root2_pid\":%u,\"observed_thread\":%u,"
           "\"attempted\":%s,\"returned\":%s,\"effect_observed\":%s,\"verified\":%s,\"within_budget\":%s",
           (unsigned)root_value,(unsigned)root2_value,(unsigned)child_value,
           (unsigned long long)(uintptr_t)old_parent,(unsigned long long)(uintptr_t)new_parent,
           (unsigned long long)(uintptr_t)change->before_parent,(unsigned long long)(uintptr_t)change->before_ancestor,
           (unsigned long long)(uintptr_t)change->actual_parent,(unsigned long long)(uintptr_t)change->actual_ancestor,
           (unsigned long long)(uintptr_t)change->previous_parent,(unsigned)change->win32_error,
           (unsigned)change->child_pid,(unsigned)change->root1_pid,(unsigned)change->root2_pid,
           (unsigned)change->child_thread,change->attempted?"true":"false",change->returned?"true":"false",
           change->effect_observed?"true":"false",change->verified?"true":"false",within?"true":"false");
    return change->verified&&within&&!io_error&&!pipe_error&&!quit_seen;
}

static BOOL child_pipe_width(HANDLE handle)
{
    uintptr_t value=(uintptr_t)handle;
    /* Use exactly the same command in the x64 -> x86 and x86 -> x64 pairs. */
    return value&&value<UINT32_MAX;
}

static BOOL duplicate_output(DWORD which,HANDLE *copy)
{
    HANDLE source=GetStdHandle(which);
    if(!source||source==INVALID_HANDLE_VALUE)return FALSE;
    return DuplicateHandle(GetCurrentProcess(),source,GetCurrentProcess(),copy,0,TRUE,DUPLICATE_SAME_ACCESS);
}

/* Consume exit code only AFTER a signalled wait on the actual process object.
 * FINISHED is merely a cleanup report and is never an internal or kernel join. */
static BOOL join_child(HANDLE process,const struct budget *b,BOOL *joined,BOOL *exit_known,DWORD *exit_code)
{
    while(room(b))
    {
        DWORD wait;
        pump();wait=WaitForSingleObject(process,0);
        if(wait==WAIT_OBJECT_0)
        {
            *joined=TRUE;
            *exit_known=GetExitCodeProcess(process,exit_code);
            record("child-join","\"kernel_joined\":true,\"exit_known\":%s,\"child_exit\":%u",
                   *exit_known?"true":"false",(unsigned)*exit_code);
            return *exit_known&&room(b);
        }
        if(wait!=WAIT_TIMEOUT)return FALSE;
        Sleep(2);
    }
    return FALSE;
}

static int host_main(int argc,char **argv)
{
    static const WCHAR class_name[]=L"CiderCrossReparentHostV1";
    WNDCLASSW wc;
    HWND root=NULL,root2=NULL,child=NULL;
    HMODULE module=NULL;
    ATOM atom=0;
    HANDLE request_read=NULL,request_write=NULL,reply_read=NULL,reply_write=NULL;
    HANDLE stdout_child=NULL,stderr_child=NULL;
    SECURITY_ATTRIBUTES security={sizeof(security),NULL,TRUE};
    STARTUPINFOEXW startup;
    PROCESS_INFORMATION pi;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes=NULL;
    SIZE_T attribute_bytes=0;
    HANDLE inherited[4];
    WCHAR peer[MAX_PATH],command[1024];
    char command_ascii[1024];
    struct live_host_fixture setup,observed;
    struct cross_message request,reply;
    struct budget b;
    struct tree_change changes[2];
    UINT64 previous_native_sequence=0;
    UINT32 root2_value=0;
    unsigned phase,i;
    DWORD child_exit=STILL_ACTIVE;
    BOOL attribute_ready=FALSE,created=FALSE,create_attempted=FALSE,joined=FALSE,exit_known=FALSE;
    BOOL terminate_attempted=FALSE,terminate_returned=FALSE,finished_received=FALSE,producer_finished=FALSE;
    BOOL complete=FALSE,local_cleanup=TRUE,stop_ok=TRUE;
    int n;
    role_name="host";root_pid=GetCurrentProcessId();
    memset(&wc,0,sizeof(wc));memset(&startup,0,sizeof(startup));memset(&pi,0,sizeof(pi));
    memset(&setup,0,sizeof(setup));memset(&observed,0,sizeof(observed));
    memset(changes,0,sizeof(changes));
    if(!check(argc==4,"host-argument-count"))goto done;
    if(!check(initialize_clock(),"qpc-and-aggregate-watchdog"))goto done;
    if(!check(peer_path(argv[2],peer)&&parse_nonce(argv[3]),"absolute-owned-ascii-peer-and-nonce"))goto done;
    if(!check(process_birth(GetCurrentProcess(),&root_birth),"actual-host-birth"))goto done;
    wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(NULL);wc.lpszClassName=class_name;
    atom=RegisterClassW(&wc);
    if(!check(atom!=0,"register-host-class"))goto done;
    root=CreateWindowW(class_name,L"Cider reparent original root",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                       20,20,360,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root!=NULL&&window_value(root,&root_value),"actual-owned-root"))goto done;
    root2=CreateWindowW(class_name,L"Cider reparent second root",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                        400,20,360,240,NULL,NULL,wc.hInstance,NULL);
    if(!check(root2!=NULL&&root2!=root&&window_value(root2,&root2_value)&&root2_value!=root_value&&
              GetAncestor(root2,GA_ROOT)==root2,"actual-distinct-owned-root2-before-child-and-fixture"))goto done;
    child=CreateWindowW(class_name,L"host-owned placeholder",WS_CHILD|WS_VISIBLE|WS_BORDER,
                        10,10,180,100,root,NULL,wc.hInstance,NULL);
    if(!check(child!=NULL&&GetParent(child)==root&&GetAncestor(child,GA_ROOT)==root&&window_value(child,&child_value),
              "actual-host-owned-placeholder-no-foreign-child"))goto done;
    {
        DWORD owner=0,second_owner=0,child_owner=0;
        BOOL valid=GetWindowThreadProcessId(root,&owner)!=0&&GetWindowThreadProcessId(root2,&second_owner)!=0&&
                   GetWindowThreadProcessId(child,&child_owner)!=0&&
                   owner==root_pid&&second_owner==root_pid&&child_owner==root_pid;
        if(!check(valid,"both-root-and-child-window-pids-are-actual-host"))goto done;
    }
    if(!load_fixture(&module))goto done;
    if(!check(begin_budget(&b,4)&&native_operation(LIVE_HOST_SETUP,&setup,&b)&&!setup.status&&host_setup_ok,
              "actual-host-native-root-birth-and-epoch"))goto done;
    if(!check(CreatePipe(&request_read,&request_write,&security,4096)&&
              SetHandleInformation(request_read,HANDLE_FLAG_INHERIT,0),"request-pipe-only-child-end-inheritable"))goto done;
    if(!check(CreatePipe(&reply_read,&reply_write,&security,4096)&&
              SetHandleInformation(reply_write,HANDLE_FLAG_INHERIT,0),"reply-pipe-only-child-end-inheritable"))goto done;
    if(!check(child_pipe_width(reply_read)&&child_pipe_width(request_write),"cross-bitness-pipe-values-not-truncated"))goto done;
    if(!check(duplicate_output(STD_OUTPUT_HANDLE,&stdout_child)&&duplicate_output(STD_ERROR_HANDLE,&stderr_child),
              "owned-inheritable-stdout-stderr-copies"))goto done;
    (void)InitializeProcThreadAttributeList(NULL,1,0,&attribute_bytes);
    if(!check(attribute_bytes!=0,"explicit-inheritance-list-size"))goto done;
    attributes=HeapAlloc(GetProcessHeap(),0,attribute_bytes);
    if(!check(attributes!=NULL,"allocate-explicit-inheritance-list"))goto done;
    attribute_ready=InitializeProcThreadAttributeList(attributes,1,0,&attribute_bytes);
    if(!check(attribute_ready,"initialize-explicit-inheritance-list"))goto done;
    inherited[0]=reply_read;inherited[1]=request_write;inherited[2]=stdout_child;inherited[3]=stderr_child;
    if(!check(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),NULL,NULL),
              "restrict-child-to-pipe-ends-and-intended-output"))goto done;
    startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    /* stdin aliases the already listed reply end. Neither role uses stdio input. */
    startup.StartupInfo.hStdInput=reply_read;startup.StartupInfo.hStdOutput=stdout_child;
    startup.StartupInfo.hStdError=stderr_child;startup.lpAttributeList=attributes;
    n=snprintf(command_ascii,sizeof(command_ascii),"\"%s\" --producer %08x %08x %s %llx %llx",
               argv[2],(unsigned)root_value,(unsigned)child_value,argv[3],
               (unsigned long long)(uintptr_t)reply_read,(unsigned long long)(uintptr_t)request_write);
    if(!check(n>0&&(size_t)n<sizeof(command_ascii),"bounded-quoted-peer-command"))goto done;
    for(i=0;i<=(unsigned)n;++i)command[i]=(WCHAR)(unsigned char)command_ascii[i];
    if(!check(room(NULL),"watchdog-before-create-process"))goto done;
    create_attempted=TRUE;
    created=CreateProcessW(peer,command,NULL,NULL,TRUE,EXTENDED_STARTUPINFO_PRESENT,NULL,NULL,&startup.StartupInfo,&pi);
    if(!created)memset(&pi,0,sizeof(pi)); /* Failed CreateProcess outputs confer no ownership. */
    host_spawned=created;
    producer_pid=created?pi.dwProcessId:0; /* Own the real handle before any post-clock check. */
    if(!check(created,"create-owned-ordinary-guest-producer"))goto done;
    if(!check(pi.hProcess&&pi.hThread&&producer_pid&&producer_pid!=root_pid&&
              process_birth(pi.hProcess,&producer_birth)&&room(NULL),"actual-created-producer-pid-and-birth"))goto done;
    if(!close_owned(&pi.hThread,"close-owned-child-primary-thread"))goto done;
    if(!close_owned(&request_write,"close-host-copy-of-producer-request-end"))goto done;
    if(!close_owned(&reply_read,"close-host-copy-of-producer-reply-end"))goto done;
    if(!close_owned(&stdout_child,"close-host-child-stdout-copy"))goto done;
    if(!close_owned(&stderr_child,"close-host-child-stderr-copy"))goto done;
    for(phase=1;phase<=4;++phase)
    {
        /* Initial setup/PREPARE/INSTALL need ordinary USER round trips before
         * any pipe request exists. Pump throughout the aggregate work budget. */
        if(!check(read_message(request_read,&request,pi.hProcess,NULL),"bounded-pumped-complete-request"))goto done;
        wire_sequence=request.sequence;
        message_record("pipe-request","receive",&request);
        if(!check(host_request(&request,phase,&setup,previous_native_sequence),
                  "strict-wire-sequence-nonce-pid-birth-identity-and-phase"))goto done;
        previous_native_sequence=request.frame.sequence;
        if(!identity_saved){identity=request.frame;identity_saved=TRUE;}
        observed=request.frame;observed.probe_sequence=request.sequence;
        if(!check(begin_budget(&b,4),"host-probe-reply-budget"))goto done;
        if(phase==2&&!check(change_tree(root,root2,child,root,root2,root2_value,&changes[0],&b),
                            "ordinary-setparent-to-root2-return-and-actual-tree"))goto done;
        if(phase==3&&!check(change_tree(root,root2,child,root2,root,root2_value,&changes[1],&b),
                            "ordinary-setparent-back-to-root1-return-and-actual-tree"))goto done;
        {
            /* Probe the immutable original root/child after ordinary driver
             * retirement. One actual observation only: no forced refresh,
             * cancellation, root-change injection or invented absent model. */
            BOOL called=native_operation(LIVE_HOST_PROBE,&observed,&b);
            /* Send the exact actual result, including Native errors. A failed
             * Native observation can never stand in for model count zero. */
            if(last_nt_valid&&envelope(&observed,LIVE_HOST_PROBE))
            {
                make_message(&reply,(unsigned)request.operation,request.sequence,&observed,last_nt);
                message_record("pipe-reply","send-attempt",&reply);
                pair_record(&request.frame,&observed);
                if(!check(write_message(reply_write,&reply,&b),"actual-native-probe-reply-written"))goto done;
            }
            if(!check(called&&!observed.status&&host_observation(&observed)&&
                      observed.probe_sequence==request.sequence&&observed.window_owned==1&&models(&observed,phase==1?1u:0u),
                      "actual-host-native-bound-model-counts"))goto done;
        }
        if(phase==4)finished_received=TRUE; /* Only a report until actual kernel join. */
    }
    if(!check(join_child(pi.hProcess,NULL,&joined,&exit_known,&child_exit),"actual-producer-kernel-join"))goto done;
    if(!check(child_exit==0,"actual-producer-exit-zero"))goto done;
    producer_finished=finished_received&&joined&&exit_known&&!child_exit;
    if(!check(host_bound&&last_absent&&producer_finished,"bound-and-last-absent-before-host-finish"))goto done;
    if(!check(begin_budget(&b,4)&&native_operation(LIVE_HOST_FINISH,&observed,&b)&&!observed.status&&host_finished,
              "fresh-native-absence-and-host-observer-release"))goto done;
    complete=TRUE;
done:
    cleaning=TRUE;
    if(created&&!joined)
    {
        record("stop-note","\"note\":\"pumped grace only while two seconds remain, then owned-child TerminateProcess and actual kernel join within aggregate 25 seconds; exit is not SAFE\"");
        if(GetTickCount64()-entry_tick<22000u)
        {
            BOOL started=begin_budget(&b,1);
            if(started||clock_error)(void)join_child(pi.hProcess,&b,&joined,&exit_known,&child_exit);
        }
        if(!joined&&room(NULL))
        {
            terminate_attempted=TRUE;
            terminate_returned=TerminateProcess(pi.hProcess,1);
            record("child-terminate","\"attempted\":true,\"returned\":%s,\"win32_error\":%u",
                   terminate_returned?"true":"false",terminate_returned?0u:(unsigned)GetLastError());
            (void)join_child(pi.hProcess,NULL,&joined,&exit_known,&child_exit);
        }
        if(!joined)stop_ok=FALSE;
    }
    if(created&&joined&&!exit_known)exit_known=GetExitCodeProcess(pi.hProcess,&child_exit);
    producer_finished=finished_received&&joined&&exit_known&&!child_exit&&!terminate_attempted;
    if(!host_finished&&fixture&&room(NULL)&&((!created&&host_setup_acknowledged)||(host_bound&&last_absent&&joined)))
    {
        BOOL started=begin_budget(&b,1);
        observed=last_frame;
        if(started||clock_error)(void)native_operation(LIVE_HOST_FINISH,&observed,&b);
    }
    if(attribute_ready)DeleteProcThreadAttributeList(attributes);
    if(attributes&&!check(HeapFree(GetProcessHeap(),0,attributes),"free-owned-inheritance-list"))local_cleanup=FALSE;
    if(!close_owned(&pi.hThread,"cleanup-owned-primary-thread"))local_cleanup=FALSE;
    if(!close_owned(&request_write,"cleanup-owned-child-request-end"))local_cleanup=FALSE;
    if(!close_owned(&reply_read,"cleanup-owned-child-reply-end"))local_cleanup=FALSE;
    if(!close_owned(&stdout_child,"cleanup-owned-output-copy"))local_cleanup=FALSE;
    if(!close_owned(&stderr_child,"cleanup-owned-error-copy"))local_cleanup=FALSE;
    if(!close_owned(&request_read,"close-owned-host-request-end"))local_cleanup=FALSE;
    if(!close_owned(&reply_write,"close-owned-host-reply-end"))local_cleanup=FALSE;
    if((!created||joined)&&!close_owned(&pi.hProcess,"close-joined-owned-process-handle"))local_cleanup=FALSE;
    if(!native_clean)local_cleanup=FALSE;
    if(native_clean)
    {
        if(child)
        {
            BOOL ok=IsWindow(child)&&DestroyWindow(child);
            if(ok)child=NULL;
            if(!check(ok,"destroy-owned-placeholder"))local_cleanup=FALSE;
        }
        if(root&&!child)
        {
            BOOL ok=IsWindow(root)&&DestroyWindow(root);
            if(ok)root=NULL;
            if(!check(ok,"destroy-owned-root"))local_cleanup=FALSE;
        }
        if(root2&&!root&&!child&&(!created||joined))
        {
            BOOL ok=IsWindow(root2)&&DestroyWindow(root2);
            if(ok)root2=NULL;
            if(!check(ok,"destroy-owned-root2-after-consumer-and-child-windows"))local_cleanup=FALSE;
        }
        if(module&&!root&&!root2&&!child)
        {
            BOOL ok=FreeLibrary(module);
            if(ok){module=NULL;fixture=NULL;}
            if(!check(ok,"release-host-module"))local_cleanup=FALSE;
        }
        if(atom&&!root&&!root2&&!child)
        {
            BOOL ok=UnregisterClassW(class_name,wc.hInstance);
            if(ok)atom=0;
            if(!check(ok,"unregister-owned-host-class"))local_cleanup=FALSE;
        }
    }
    if(clock_ready&&!room(NULL))local_cleanup=FALSE;
    record("summary","\"complete\":%s,\"checks\":%u,\"failures\":%u,\"attempts\":%u,"
           "\"root1\":%u,\"root2\":%u,\"child\":%u,\"reparent_attempted\":%s,\"reparent_verified\":%s,"
           "\"return_attempted\":%s,\"return_verified\":%s,"
           "\"root_pid\":%u,\"child_pid\":%u,\"create_attempted\":%s,\"created\":%s,"
           "\"producer_cleanup_reported\":%s,\"kernel_joined\":%s,\"exit_known\":%s,\"child_exit\":%u,"
           "\"forced\":%s,\"terminate_returned\":%s,\"host_bound\":%s,\"last_absent\":%s,"
           "\"host_finished\":%s,\"native_cleanup\":%s,\"cleanup\":%s,\"io_error\":%s,\"pipe_error\":%s,"
           "\"clock_error\":%s,\"timeout\":%s,\"elapsed_ms\":%llu,"
           "\"note\":\"20 second work deadline reserves cleanup within 25 seconds; synchronous Native calls checked before/after, not forcibly interrupted; child exit is not internal resource SAFE; unknown observers/windows/module retained\"",
           complete?"true":"false",checks,failures,(unsigned)attempts,
           (unsigned)root_value,(unsigned)root2_value,(unsigned)child_value,
           changes[0].attempted?"true":"false",changes[0].verified?"true":"false",
           changes[1].attempted?"true":"false",changes[1].verified?"true":"false",
           (unsigned)root_pid,(unsigned)producer_pid,
           create_attempted?"true":"false",created?"true":"false",finished_received?"true":"false",
           joined?"true":"false",exit_known?"true":"false",(unsigned)child_exit,terminate_attempted?"true":"false",
           terminate_returned?"true":"false",host_bound?"true":"false",last_absent?"true":"false",
           host_finished?"true":"false",native_clean?"true":"false",
           local_cleanup&&stop_ok&&native_clean&&(!created||producer_finished)?"true":"false",
           io_error?"true":"false",pipe_error?"true":"false",clock_error?"true":"false",
           timed_out?"true":"false",(unsigned long long)(GetTickCount64()-entry_tick));
    return !complete||failures||!native_clean||!local_cleanup||!stop_ok||!joined||!exit_known||child_exit||
           !producer_finished||io_error||pipe_error||clock_error||timed_out||quit_seen?1:0;
}

int main(int argc,char **argv)
{
    entry_tick=GetTickCount64();
    if(setvbuf(stdout,NULL,_IONBF,0)!=0)io_error=TRUE;
    if(argc>1&&!strcmp(argv[1],"--host"))return host_main(argc,argv);
    if(argc>1&&!strcmp(argv[1],"--producer"))return producer_main(argc,argv);
    (void)check(FALSE,"expected-host-or-producer-role");
    record("summary","\"complete\":false,\"cleanup\":true,\"io_error\":%s,"
           "\"note\":\"host argc4 or producer argc7 required; no process or Native resource created\"",io_error?"true":"false");
    return 1;
}
