/* Exact server function bodies are included below; the handle/map allocator
 * dependencies are bounded models, not live Windows objects or access APIs. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uintptr_t client_ptr_t, obj_handle_t;
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000u
#define STATUS_INVALID_PARAMETER 0xc000000du
#define STATUS_OBJECT_TYPE_MISMATCH 0xc0000024u
#define STATUS_NO_TOKEN 0xc000007cu
#define STATUS_ACCESS_DENIED 0xc0000022u
#define STATUS_INVALID_HANDLE 0xc0000008u
#define STATUS_OBJECT_NAME_COLLISION 0xc0000035u
#define STATUS_NO_MEMORY 0xc0000017u
struct object_ops { int type; };
struct object { const struct object_ops *ops; int refs, destroyed; };
struct token { struct object obj; int marker; };
struct process { struct object obj; struct token *token; };
struct kernel_object { struct object *object; client_ptr_t user_ptr; int owned, active; };
struct device_manager { struct object obj; struct process *owner; struct kernel_object maps[8]; };
struct request { obj_handle_t manager, process; client_ptr_t process_ptr, candidate_ptr, user_ptr; };
struct reply { client_ptr_t user_ptr; int new_owned; };
static const struct object_ops process_ops={1}, token_ops={2}, device_manager_ops={3}, wrong_ops={4};
static struct process host, target, other;
static struct token host_token, token_a, token_b, other_token;
static struct device_manager manager;
static struct { struct process *process; } thread;
#define current (&thread)
static unsigned int status, allocation_failure, checks;
static void set_error(unsigned int value) { status=value; }
static void *grab_object(void *p) { struct object *o=p; assert(o && o->refs>0 && !o->destroyed); ++o->refs; return p; }
static void release_object(void *p) {
    struct object *o=p; unsigned int i;
    assert(o && o->refs>0); --o->refs;
    if (!o->refs) {
        o->destroyed=1;
        for(i=0;i<8;++i) if(manager.maps[i].active && manager.maps[i].object==o) {
            assert(!manager.maps[i].owned); manager.maps[i].active=0;
        }
    }
}
static void *get_handle_obj(struct process *owner,obj_handle_t handle,unsigned int access,const struct object_ops *ops) {
    (void)owner; assert(!access && ops==&device_manager_ops);
    if(handle!=1) { set_error(STATUS_INVALID_HANDLE); return NULL; }
    return grab_object(&manager);
}
static struct process *get_process_from_handle(obj_handle_t handle,unsigned int access) {
    assert(access==PROCESS_QUERY_LIMITED_INFORMATION);
    if(handle==2) return grab_object(&target);
    if(handle==3) return grab_object(&other);
    set_error(STATUS_INVALID_HANDLE); return NULL;
}
static struct kernel_object *kernel_object_from_ptr(struct device_manager *m,client_ptr_t ptr) {
    unsigned int i; assert(m==&manager);
    for(i=0;i<8;++i) if(m->maps[i].active && m->maps[i].user_ptr==ptr) return &m->maps[i];
    return NULL;
}
static struct kernel_object *kernel_object_from_obj(struct device_manager *m,struct object *object) {
    unsigned int i; assert(m==&manager);
    for(i=0;i<8;++i) if(m->maps[i].active && m->maps[i].object==object) return &m->maps[i];
    return NULL;
}
static struct kernel_object *set_kernel_object(struct device_manager *m,struct object *object,client_ptr_t ptr) {
    unsigned int i; assert(m==&manager);
    if(allocation_failure) return NULL;
    for(i=0;i<8;++i) if(!m->maps[i].active) {
        m->maps[i]=(struct kernel_object){object,ptr,0,1}; return &m->maps[i];
    }
    return NULL;
}
#define DECL_HANDLER(name) static void server_##name(const struct request *req,struct reply *reply)
#include "server-extracted.h"
#define CHECK(value) do { ++checks; assert(value); } while(0)
static struct reply invoke(struct request req,unsigned int expected) {
    struct reply reply={0}; status=0;
    server_reference_process_primary_token(&req,&reply);
    CHECK(status==expected); CHECK(manager.obj.refs==1 && target.obj.refs==1 && other.obj.refs==1);
    return reply;
}
static void reset(void) {
    memset(&manager,0,sizeof(manager));
    host_token=(struct token){{&token_ops,1,0},99};
    token_a=(struct token){{&token_ops,1,0},11}; token_b=(struct token){{&token_ops,1,0},22};
    other_token=(struct token){{&token_ops,1,0},77};
    host=(struct process){{&process_ops,1,0},&host_token};
    target=(struct process){{&process_ops,1,0},&token_a}; other=(struct process){{&process_ops,1,0},&other_token};
    manager.obj=(struct object){&device_manager_ops,1,0}; manager.owner=&host;
    manager.maps[0]=(struct kernel_object){&target.obj,0x100,0,1};
    manager.maps[1]=(struct kernel_object){&other.obj,0x200,0,1};
    current->process=&host; allocation_failure=0; status=0;
}
int main(void) {
    struct request req={1,2,0x100,0x300,0};
    struct reply first,second;
    struct object wrong={&wrong_ops,1,0};
    unsigned int i;
    reset(); first=invoke(req,0);
    CHECK(first.user_ptr==0x300 && first.new_owned==1 && token_a.obj.refs==2);
    CHECK(host_token.obj.refs==1 && !host_token.obj.destroyed);
    req.candidate_ptr=0x400;
    for(i=0;i<12;++i) { second=invoke(req,0); CHECK(second.user_ptr==first.user_ptr && !second.new_owned && token_a.obj.refs==2); }
    /* Transfer the fixture's retained new-token reference into the process slot. */
    target.token=&token_b; release_object(&token_a);
    second=invoke(req,0);
    CHECK(second.user_ptr==0x400 && second.new_owned && token_a.obj.refs==1 && token_b.obj.refs==2);
    req.user_ptr=first.user_ptr; status=0;
    server_release_kernel_object(&req,&second);
    CHECK(!status && token_a.obj.destroyed && !token_b.obj.destroyed);
    req.user_ptr=0x400; server_release_kernel_object(&req,&second);
    CHECK(token_b.obj.refs==1 && !token_b.obj.destroyed);

    reset(); current->process=&other; req=(struct request){1,2,0x100,0x300,0};
    second=invoke(req,STATUS_ACCESS_DENIED); CHECK(!second.user_ptr && token_a.obj.refs==1);
    reset(); first=invoke(req,0); current->process=&other; req.user_ptr=first.user_ptr; status=0;
    server_release_kernel_object(&req,&second);
    CHECK(status==STATUS_ACCESS_DENIED && token_a.obj.refs==2 && manager.maps[2].owned);

    reset(); req=(struct request){1,3,0x100,0x300,0};
    second=invoke(req,STATUS_INVALID_HANDLE); CHECK(!second.user_ptr && token_a.obj.refs==1);
    reset(); req=(struct request){1,2,0x100,0x100,0}; invoke(req,STATUS_OBJECT_NAME_COLLISION);
    reset(); req=(struct request){1,2,0,0x300,0}; invoke(req,STATUS_INVALID_PARAMETER);
    reset(); req=(struct request){1,2,0x100,0,0}; invoke(req,STATUS_INVALID_PARAMETER);
    reset(); req=(struct request){1,2,0x100,0x300,0}; target.token=NULL; invoke(req,STATUS_NO_TOKEN);
    reset(); allocation_failure=1; invoke(req,STATUS_NO_MEMORY); CHECK(token_a.obj.refs==1 && !manager.maps[2].active);
    reset(); manager.maps[2]=(struct kernel_object){&token_a.obj,0,0,1}; invoke(req,STATUS_INVALID_HANDLE);
    CHECK(token_a.obj.refs==1 && !manager.maps[2].owned);
    reset(); CHECK(get_process_primary_token_reference(&wrong)==NULL && status==STATUS_OBJECT_TYPE_MISMATCH);
    CHECK(get_process_primary_token_reference(NULL)==NULL && status==STATUS_INVALID_PARAMETER);
    printf("PASS: %u ownership/alias/slot/owner/binding/failure checks; modeled dependencies only\n",checks);
    return 0;
}
