#include "status_wire.h"
#include <stdio.h>
int main(void)
{
    struct macdrv_status_wire wire={0};
    struct macdrv_status_identity a={1,101,202,303,MACDRV_STATUS_HOST,0x100000001ULL,0x200000002ULL,0x300000003ULL,0x400000004ULL,0x500000005ULL,0x600000006ULL},b=a;
    struct macdrv_status_values values={MACDRV_STATUS_ACTIVE,0,0,0,0,0x700000007ULL,0x700000006ULL};
    struct macdrv_status_snapshot output;
    enum macdrv_status_read_result normal,safe; unsigned failures=0;
    b.generation++;
    if(!macdrv_status_initialize(&wire,&b,&values))return 2;
    output.identity=a;normal=macdrv_status_read_wire(&wire,&output.identity,&output);
    failures+=normal!=MACDRV_STATUS_READ_MISMATCH;
    values.state=MACDRV_STATUS_RETIRED;values.quiescent_sequence=values.applied_sequence;
    if(macdrv_status_publish_wire(&wire,&values,1)!=MACDRV_STATUS_PUBLISHED)return 2;
    output.identity=a;safe=macdrv_status_read_wire(&wire,&output.identity,&output);
    failures+=safe!=MACDRV_STATUS_READ_MISMATCH;
    printf("{\"type\":\"status-cell-alias\",\"bits\":%u,\"checks\":2,\"failures\":%u,\"normal\":%u,\"safe\":%u}\n",(unsigned)(8*sizeof(void*)),failures,normal,safe);
    fflush(stdout);return failures?1:0;
}
