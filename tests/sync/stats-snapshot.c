/* Read the actual server-published pump counters; no simulated occupancy. */
#define _DARWIN_C_SOURCE
#include <stddef.h>
#include <inttypes.h>
#include <libproc.h>
#include <sys/resource.h>
#include <string.h>
#include "wine/msync_coord.h"
int main(int argc,char **argv)
{
    int fd,stable=0;
    struct stat st;
    unsigned int attempt;
    uint32_t first=0,last=0;
    struct msync_coord_header *header;
    struct msync_coord_statistics *stats,snapshot;
    struct rusage_info_v4 usage;
    struct proc_fdinfo *fds = NULL;
    int usage_ok, fd_bytes, fd_count = -1;
    char hint_fields[512] = "";
    if(argc!=2)return 64;
    fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0 || fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_uid!=getuid() ||
       (st.st_mode&0777)!=0600 || st.st_size!=MSYNC_COORD_SIZE)return 65;
    header=mmap(NULL,MSYNC_COORD_HEADER_SIZE,PROT_READ,MAP_SHARED,fd,0);
    if(header==MAP_FAILED || header->magic!=MSYNC_COORD_MAGIC)return 66;
    stats=(struct msync_coord_statistics *)((char *)header+MSYNC_STATS_OFFSET);
    if(__atomic_load_n(&stats->enabled,__ATOMIC_ACQUIRE)!=1 || stats->magic!=MSYNC_STATS_MAGIC)return 67;
    for(attempt=0;attempt<100000;++attempt)
    {
        first=__atomic_load_n(&stats->sequence,__ATOMIC_SEQ_CST);
        if(first&1)continue;
        snapshot.magic=stats->magic;
#define READ_FIELD(field) snapshot.field=__atomic_load_n(&stats->field,__ATOMIC_RELAXED)
        READ_FIELD(enabled);READ_FIELD(allocations);READ_FIELD(frees);READ_FIELD(current);READ_FIELD(peak);
        READ_FIELD(fallback_total);READ_FIELD(fallback_current);READ_FIELD(pool_free);READ_FIELD(capacity);READ_FIELD(producer_pid);
#undef READ_FIELD
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        last=__atomic_load_n(&stats->sequence,__ATOMIC_SEQ_CST);
        if(first==last && !(last&1)){stable=1;break;}
    }
    if(!stable)return 68;
    memset(&usage,0,sizeof(usage));
    usage_ok=proc_pid_rusage(snapshot.producer_pid,RUSAGE_INFO_V4,(rusage_info_t *)&usage)==0;
    fd_bytes=proc_pidinfo(snapshot.producer_pid,PROC_PIDLISTFDS,0,NULL,0);
    if(fd_bytes>0 && fd_bytes<8*1024*1024)
    {
        int capacity=fd_bytes+32*(int)sizeof(*fds),received;
        fds=malloc(capacity);
        if(fds)
        {
            received=proc_pidinfo(snapshot.producer_pid,PROC_PIDLISTFDS,0,fds,capacity);
            if(received>0 && received<capacity && received%(int)sizeof(*fds)==0)
                fd_count=received/(int)sizeof(*fds);
            free(fds);
        }
    }
#ifdef MSYNC_COORD_HINT_BUCKETS
    {
        uint32_t *hints = mmap(NULL, MSYNC_COORD_HINT_BUCKETS * sizeof(*hints), PROT_READ,
                              MAP_SHARED, fd, MSYNC_COORD_HINT_OFFSET);
        if (hints == MAP_FAILED)
            strcpy(hint_fields, ",\"eventHintsAvailable\":false");
        else
        {
            uint64_t sum = 0;
            uint32_t nonzero = 0, sticky = 0, i, value;
            int length;
            for (i = 0; i < MSYNC_COORD_HINT_BUCKETS; ++i)
            {
                value = __atomic_load_n(&hints[i], __ATOMIC_ACQUIRE);
                nonzero += value != 0;
                if (value == UINT32_MAX) ++sticky;
                else sum += value;
            }
            if (munmap(hints, MSYNC_COORD_HINT_BUCKETS * sizeof(*hints))) return 70;
            /* Concurrent buckets are not a coherent global waiter snapshot;
             * collisions and process death can leave conservative overcounts. */
            length = snprintf(hint_fields, sizeof(hint_fields),
                ",\"eventHintsAvailable\":true,\"eventHintSnapshotCoherent\":false,"
                "\"eventHintNonstickySum\":%" PRIu64 ",\"eventHintNonzeroBuckets\":%u,"
                "\"eventHintStickyBuckets\":%u", sum, nonzero, sticky);
            if (length < 0 || (size_t)length >= sizeof(hint_fields)) return 71;
        }
    }
#endif
    printf("{\"type\":\"msync-pump-statistics\",\"pid\":%u,\"sequence\":%u,\"allocations\":%" PRIu64 ",\"frees\":%" PRIu64 ",\"current\":%" PRIu64 ",\"peak\":%" PRIu64 ",\"fallbackTotal\":%" PRIu64 ",\"fallbackCurrent\":%" PRIu64 ",\"poolFree\":%" PRIu64 ",\"capacity\":%u,\"producerResourcesAvailable\":%s,\"producerRSSBytes\":%" PRIu64 ",\"producerPhysicalFootprintBytes\":%" PRIu64 ",\"producerUserTimeNs\":%" PRIu64 ",\"producerSystemTimeNs\":%" PRIu64 ",\"producerStartAbstime\":%" PRIu64 ",\"producerFDCount\":%d%s}\n",snapshot.producer_pid,last,snapshot.allocations,snapshot.frees,snapshot.current,snapshot.peak,snapshot.fallback_total,snapshot.fallback_current,snapshot.pool_free,snapshot.capacity,usage_ok?"true":"false",(uint64_t)usage.ri_resident_size,(uint64_t)usage.ri_phys_footprint,(uint64_t)usage.ri_user_time,(uint64_t)usage.ri_system_time,(uint64_t)usage.ri_proc_start_abstime,fd_count,hint_fields);
    if(munmap(header,MSYNC_COORD_HEADER_SIZE) || close(fd))return 69;
    return fflush(stdout)==0 && !ferror(stdout) ? 0:1;
}
