/* Private same-prefix opposite-width consumer; no fixture publication. */
struct producer_cell_control
{
    uint32_t magic,version,bits,pid,birth_low,birth_high,checks,failures;
    uint32_t initial_read,primary_read,terminal_read,protection;
    uint32_t root_destroyed,close_failure,reader_closed,owner_finished,reserved[8];
    struct macdrv_status_wire identity;
};
_Static_assert(sizeof(struct producer_cell_control)==256,"producer cell control256");
static BOOL poll_terminal_cell(struct macdrv_status_cell *reader,const struct macdrv_status_identity *identity,
    struct macdrv_status_snapshot *snapshot,uint32_t *last_kind)
{
    ULONGLONG end=GetTickCount64()+4000;
    for(;;)
    {
        if(GetTickCount64()>=end)return FALSE;
        *last_kind=macdrv_status_cell_read(reader,snapshot);
        if(GetTickCount64()>=end)return FALSE;
        if(*last_kind==MACDRV_STATUS_READ_SAFE_ONLY)
            return snapshot->safe==1&&!snapshot->values_valid&&macdrv_status_identity_equal(&snapshot->identity,identity);
        if(*last_kind==MACDRV_STATUS_READ_SNAPSHOT)
        {
            if(!snapshot->values_valid||snapshot->safe||!macdrv_status_identity_equal(&snapshot->identity,identity))return FALSE;
        }
        else if(*last_kind==MACDRV_STATUS_READ_EXHAUSTED)
        {
            if(snapshot->safe||snapshot->values_valid||!macdrv_status_identity_equal(&snapshot->identity,identity))return FALSE;
        }
        else if(*last_kind!=MACDRV_STATUS_READ_RETRY)return FALSE;
        Sleep(10); /* read-only observer; no producer cleanup entry */
    }
}
static int producer_cell_reader(int argc,char **argv)
{
    HANDLE mapping=NULL,events[7]={0};struct producer_cell_control *control=NULL;
    struct macdrv_status_cell *reader=NULL;struct macdrv_status_identity identity;
    struct macdrv_status_snapshot snapshot;MEMORY_BASIC_INFORMATION info;uint64_t birth=0;
    unsigned i;BOOL clean=TRUE;
    uint32_t initial_kind=UINT32_MAX,primary_kind=UINT32_MAX,terminal_kind=UINT32_MAX,protection=0;
    if(!CHECK(argc==10))goto done;
    mapping=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,argv[2]);if(!CHECK(mapping!=NULL))goto done;
    for(i=0;i<7;++i)events[i]=OpenEventA(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,argv[3+i]);
    if(!CHECK(events[0]&&events[1]&&events[2]&&events[3]&&events[4]&&events[5]&&events[6]))goto done;
    control=MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*control));if(!CHECK(control!=NULL))goto done;
    if(!CHECK(birth_of(GetCurrentProcess(),&birth)))goto done;
    control->bits=8*sizeof(void*);control->pid=GetCurrentProcessId();control->birth_low=(uint32_t)birth;control->birth_high=(uint32_t)(birth>>32);
    if(!CHECK(SetEvent(events[0])))goto done;
    if(!CHECK(wait_before_deadline(events[1],10000)))goto done;
    if(!CHECK(control->magic==0x50435343&&control->version==1))goto done;
    macdrv_status_read_identity(&control->identity,&identity);
    if(!CHECK(macdrv_status_identity_valid(&identity)&&identity.kind==MACDRV_STATUS_PRODUCER))goto done;
    if(!CHECK(macdrv_status_cell_open(&identity,&reader)==0))goto done;
    control->initial_read=macdrv_status_cell_read(reader,&snapshot);
    if(!CHECK(control->initial_read==MACDRV_STATUS_READ_SNAPSHOT&&snapshot.values_valid&&snapshot.values.state==MACDRV_STATUS_ACTIVE&&!snapshot.safe&&snapshot.values.applied_sequence==0&&snapshot.values.quiescent_sequence==0))goto done;
    if(!CHECK(VirtualQuery(macdrv_status_cell_view(reader,NULL),&info,sizeof(info))==sizeof(info)&&info.Protect==PAGE_READONLY&&info.State==MEM_COMMIT))goto done;
    control->protection=info.Protect;
    if(!CHECK(macdrv_status_cell_close_handle(reader)==0&&macdrv_status_cell_handle(reader)==NULL))goto done;
    if(!CHECK(SetEvent(events[2])))goto done;
    if(!CHECK(wait_before_deadline(events[3],10000)))goto done;
    control->primary_read=macdrv_status_cell_read(reader,&snapshot);
    /* Exact negative oracle: a real held request must prevent terminal safety. */
    CHECK(control->primary_read==MACDRV_STATUS_READ_SNAPSHOT&&snapshot.values_valid&&snapshot.values.state==MACDRV_STATUS_ACTIVE&&!snapshot.safe);
    if(!CHECK(SetEvent(events[4])))goto done;
    if(!CHECK(wait_before_deadline(events[5],10000)))goto done;
    CHECK(control->root_destroyed==1&&control->owner_finished==1);
    CHECK(poll_terminal_cell(reader,&identity,&snapshot,&control->terminal_read));
    CHECK(macdrv_status_cell_close(&reader)==0&&reader==NULL);
    control->reader_closed=reader==NULL;
    CHECK(SetEvent(events[6]));
done:
    clean=macdrv_status_cell_close(&reader)==0&&clean;
    if(control){control->checks=checks;control->failures=failures;}
    if(control){initial_kind=control->initial_read;primary_kind=control->primary_read;terminal_kind=control->terminal_read;protection=control->protection;}
    clean=close_controls(events,mapping,control)&&clean;
    printf("{\"type\":\"producer-cell-reader\",\"bits\":%u,\"checks\":%u,\"failures\":%u,\"cleanup\":%u,\"initial\":%u,\"primary\":%u,\"terminal\":%u,\"protection\":%u}\n",(unsigned)(8*sizeof(void*)),checks,failures,(unsigned)clean,initial_kind,primary_kind,terminal_kind,protection);fflush(stdout);
    return failures||!clean?1:0;
}
