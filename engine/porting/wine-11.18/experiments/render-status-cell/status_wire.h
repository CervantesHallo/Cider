#ifndef CIDER_RENDER_STATUS_WIRE_H
#define CIDER_RENDER_STATUS_WIRE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define MACDRV_STATUS_MAGIC UINT32_C(0x43525350)
#define MACDRV_STATUS_VERSION 1u
#define MACDRV_STATUS_WORDS 40u
struct macdrv_status_wire { uint32_t word[MACDRV_STATUS_WORDS]; };
_Static_assert(sizeof(struct macdrv_status_wire)==160, "fixed status wire160");
enum macdrv_status_kind { MACDRV_STATUS_PRODUCER=1, MACDRV_STATUS_HOST=2 };
enum macdrv_status_state { MACDRV_STATUS_ACTIVE=1, MACDRV_STATUS_REVOKING=2, MACDRV_STATUS_RETIRED=3 };
enum macdrv_status_read_result
{
    MACDRV_STATUS_READ_SNAPSHOT=0, MACDRV_STATUS_READ_RETRY, MACDRV_STATUS_READ_INVALID,
    MACDRV_STATUS_READ_MISMATCH, MACDRV_STATUS_READ_EXHAUSTED, MACDRV_STATUS_READ_SAFE_ONLY
};
enum macdrv_status_publish_result
{
    MACDRV_STATUS_PUBLISHED=0, MACDRV_STATUS_PUBLISH_INVALID,
    MACDRV_STATUS_PUBLISH_BUSY, MACDRV_STATUS_PUBLISH_EXHAUSTED
};
/* These are local structures, never copied across the process boundary. */
struct macdrv_status_identity
{
    uint32_t session_id, root_pid, producer_pid, context_id, kind;
    uint64_t root_birth, root_epoch, producer_birth, generation, lease, ticket;
};
struct macdrv_status_values
{
    uint32_t state, error, pending_requests, pending_models, cleanup_pending;
    uint64_t applied_sequence, quiescent_sequence;
};
struct macdrv_status_snapshot
{
    struct macdrv_status_identity identity;
    struct macdrv_status_values values;
    uint32_t sequence, safe, exhausted, values_valid;
};
static inline uint32_t macdrv_status_load(const struct macdrv_status_wire *w, unsigned i)
{
    return __atomic_load_n(&w->word[i],__ATOMIC_ACQUIRE); /* pure load, RO-view safe */
}
static inline void macdrv_status_store(struct macdrv_status_wire *w,unsigned i,uint32_t value)
{
    __atomic_store_n(&w->word[i],value,__ATOMIC_RELEASE);
}
static inline uint64_t macdrv_status_load64(const struct macdrv_status_wire *w,unsigned i)
{
    uint32_t low=macdrv_status_load(w,i),high=macdrv_status_load(w,i+1);
    return (uint64_t)low|((uint64_t)high<<32);
}
static inline void macdrv_status_store64(struct macdrv_status_wire *w,unsigned i,uint64_t value)
{
    macdrv_status_store(w,i,(uint32_t)value);macdrv_status_store(w,i+1,(uint32_t)(value>>32));
}
static inline int macdrv_status_identity_valid(const struct macdrv_status_identity *id)
{
    return id && id->root_pid && id->root_birth && id->root_epoch && id->producer_pid &&
           id->producer_birth && id->generation && id->lease &&
           ((id->kind==MACDRV_STATUS_PRODUCER && !id->ticket && !id->context_id)||
            (id->kind==MACDRV_STATUS_HOST && id->ticket && id->context_id));
}
static inline int macdrv_status_identity_equal(const struct macdrv_status_identity *a,
                                              const struct macdrv_status_identity *b)
{
    return a->session_id==b->session_id && a->root_pid==b->root_pid && a->producer_pid==b->producer_pid &&
           a->context_id==b->context_id && a->kind==b->kind && a->root_birth==b->root_birth &&
           a->root_epoch==b->root_epoch && a->producer_birth==b->producer_birth &&
           a->generation==b->generation && a->lease==b->lease && a->ticket==b->ticket;
}
static inline void macdrv_status_write_identity(struct macdrv_status_wire *w,const struct macdrv_status_identity *id)
{
    macdrv_status_store(w,7,id->kind);macdrv_status_store(w,8,id->session_id);macdrv_status_store(w,9,id->root_pid);
    macdrv_status_store64(w,10,id->root_birth);macdrv_status_store64(w,12,id->root_epoch);
    macdrv_status_store(w,14,id->producer_pid);macdrv_status_store64(w,15,id->producer_birth);
    macdrv_status_store64(w,17,id->generation);macdrv_status_store64(w,19,id->lease);
    macdrv_status_store64(w,21,id->ticket);macdrv_status_store(w,23,id->context_id);
}
static inline void macdrv_status_read_identity(const struct macdrv_status_wire *w,struct macdrv_status_identity *id)
{
    id->kind=macdrv_status_load(w,7);id->session_id=macdrv_status_load(w,8);id->root_pid=macdrv_status_load(w,9);
    id->root_birth=macdrv_status_load64(w,10);id->root_epoch=macdrv_status_load64(w,12);
    id->producer_pid=macdrv_status_load(w,14);id->producer_birth=macdrv_status_load64(w,15);
    id->generation=macdrv_status_load64(w,17);id->lease=macdrv_status_load64(w,19);
    id->ticket=macdrv_status_load64(w,21);id->context_id=macdrv_status_load(w,23);
}
static inline int macdrv_status_values_valid(const struct macdrv_status_values *v)
{
    return v && v->state>=MACDRV_STATUS_ACTIVE && v->state<=MACDRV_STATUS_RETIRED &&
           v->quiescent_sequence<=v->applied_sequence &&
           (v->state!=MACDRV_STATUS_RETIRED || (!v->pending_requests&&!v->pending_models));
}
static inline void macdrv_status_write_values(struct macdrv_status_wire *w,const struct macdrv_status_values *v)
{
    macdrv_status_store(w,24,v->state);macdrv_status_store(w,25,v->error);
    macdrv_status_store64(w,26,v->applied_sequence);macdrv_status_store64(w,28,v->quiescent_sequence);
    macdrv_status_store(w,30,v->pending_requests);macdrv_status_store(w,31,v->pending_models);
    macdrv_status_store(w,32,v->cleanup_pending);
}
/* Exactly one externally serialized writer. No OS call or borrowed buffer. */
static inline int macdrv_status_initialize(struct macdrv_status_wire *w,const struct macdrv_status_identity *id,
                                          const struct macdrv_status_values *v)
{
    unsigned i;
    if (!w||!macdrv_status_identity_valid(id)||!macdrv_status_values_valid(v)||
        macdrv_status_load(w,4)||macdrv_status_load(w,0)||macdrv_status_load(w,3)) return 0;
    for(i=0;i<MACDRV_STATUS_WORDS;++i)macdrv_status_store(w,i,0);
    macdrv_status_store(w,3,1);macdrv_status_store(w,0,MACDRV_STATUS_MAGIC);
    macdrv_status_store(w,1,MACDRV_STATUS_VERSION);macdrv_status_store(w,2,sizeof(*w));
    macdrv_status_write_identity(w,id);macdrv_status_write_values(w,v);
    macdrv_status_store(w,3,2);macdrv_status_store(w,4,1);return 1;
}
static inline enum macdrv_status_publish_result macdrv_status_publish_wire(struct macdrv_status_wire *w,
    const struct macdrv_status_values *v,int mark_safe)
{
    uint32_t sequence;
    if(!w||!macdrv_status_values_valid(v)||macdrv_status_load(w,4)!=1||
       macdrv_status_load(w,0)!=MACDRV_STATUS_MAGIC||macdrv_status_load(w,6)||
       (mark_safe&&(v->state!=MACDRV_STATUS_RETIRED||v->pending_requests||v->pending_models)))
        return MACDRV_STATUS_PUBLISH_INVALID;
    if(v->state<macdrv_status_load(w,24)||v->applied_sequence<macdrv_status_load64(w,26)||
       v->quiescent_sequence<macdrv_status_load64(w,28)) return MACDRV_STATUS_PUBLISH_INVALID;
    sequence=macdrv_status_load(w,3);
    if(sequence<2)return MACDRV_STATUS_PUBLISH_INVALID;
    if(sequence&1)return MACDRV_STATUS_PUBLISH_BUSY;
    if(macdrv_status_load(w,5)||sequence>UINT32_MAX-2)
    {
        macdrv_status_store(w,5,1); /* latch; never wrap to an earlier sequence */
        if(mark_safe)macdrv_status_store(w,6,1); /* explicit safe bit; dynamic values remain invalid */
        return MACDRV_STATUS_PUBLISH_EXHAUSTED;
    }
    macdrv_status_store(w,3,sequence+1);__atomic_thread_fence(__ATOMIC_SEQ_CST);
    macdrv_status_write_values(w,v);macdrv_status_store(w,3,sequence+2);
    if(mark_safe)macdrv_status_store(w,6,1);
    return MACDRV_STATUS_PUBLISHED;
}
/* One attempt, no blocking. All shared reads are atomic32 loads, including
 * split64 values; no compare-exchange/RMW writes are made to a read-only view. */
static inline enum macdrv_status_read_result macdrv_status_read_wire(const struct macdrv_status_wire *w,
    const struct macdrv_status_identity *expected,struct macdrv_status_snapshot *out)
{
    uint32_t before,after;unsigned i;
    struct macdrv_status_identity expected_copy;
    if(!w||!out||!macdrv_status_identity_valid(expected))return MACDRV_STATUS_READ_INVALID;
    expected_copy=*expected; /* expected may alias out->identity */
    memset(out,0,sizeof(*out));
    if(!macdrv_status_load(w,4))return MACDRV_STATUS_READ_RETRY;
    if(macdrv_status_load(w,4)!=1)return MACDRV_STATUS_READ_INVALID;
    if(macdrv_status_load(w,0)!=MACDRV_STATUS_MAGIC||macdrv_status_load(w,1)!=MACDRV_STATUS_VERSION||
       macdrv_status_load(w,2)!=sizeof(*w))return MACDRV_STATUS_READ_INVALID;
    for(i=33;i<MACDRV_STATUS_WORDS;++i)if(macdrv_status_load(w,i))return MACDRV_STATUS_READ_INVALID;
    macdrv_status_read_identity(w,&out->identity);
    if(!macdrv_status_identity_equal(&out->identity,&expected_copy))return MACDRV_STATUS_READ_MISMATCH;
    out->exhausted=macdrv_status_load(w,5);out->safe=macdrv_status_load(w,6);
    if(out->safe>1||out->exhausted>1)return MACDRV_STATUS_READ_INVALID;
    if(out->safe==1)return MACDRV_STATUS_READ_SAFE_ONLY;
    if(out->exhausted)return MACDRV_STATUS_READ_EXHAUSTED;
    before=macdrv_status_load(w,3);if(before<2)return MACDRV_STATUS_READ_INVALID;
    if(before&1)return MACDRV_STATUS_READ_RETRY;
    out->values.state=macdrv_status_load(w,24);out->values.error=macdrv_status_load(w,25);
    out->values.applied_sequence=macdrv_status_load64(w,26);out->values.quiescent_sequence=macdrv_status_load64(w,28);
    out->values.pending_requests=macdrv_status_load(w,30);out->values.pending_models=macdrv_status_load(w,31);
    out->values.cleanup_pending=macdrv_status_load(w,32);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);after=macdrv_status_load(w,3);
    out->exhausted=macdrv_status_load(w,5);out->safe=macdrv_status_load(w,6);
    if(out->safe>1||out->exhausted>1)return MACDRV_STATUS_READ_INVALID;
    if(out->safe==1)return MACDRV_STATUS_READ_SAFE_ONLY;
    if(out->exhausted)return MACDRV_STATUS_READ_EXHAUSTED;
    if(before!=after||(after&1))return MACDRV_STATUS_READ_RETRY;
    if(!macdrv_status_values_valid(&out->values))return MACDRV_STATUS_READ_INVALID;
    out->sequence=after;out->values_valid=1;return MACDRV_STATUS_READ_SNAPSHOT;
}
#endif
