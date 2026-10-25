#ifndef COORDINATOR_FIXTURE_H
#define COORDINATOR_FIXTURE_H
#include <stdint.h>
#define COORDINATOR_FIXTURE_MAGIC 0x434f4f52u
struct coordinator_fixture
{
    uint32_t magic, version, root, child, checks, failures;
    uint64_t epoch, tickets[4];
    uint32_t snapshots, allocation_faults, exhaustion_faults, cleanup;
};
struct coordinator_snapshot
{
    uint64_t generation;
    double frame[4], source[4];
    uint32_t found, count, hidden, flipped;
};
#endif
