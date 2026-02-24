#include <stdint.h>
#include <stdatomic.h>

typedef union
{
    uint64_t raw;
    struct
    {
        int32_t eval;
        uint16_t depth;
        uint16_t flag;
    } fields;
} tt_payload_t;

typedef struct
{
    _Atomic(uint64_t) hash_entry;
    _Atomic(uint64_t) data;
} tt_entry_t;

typedef struct 
{
    tt_entry_t depth_preferred;
    tt_entry_t scratch;
} tt_bucket_t;

tt_bucket_t transposition_table[TABLE_SIZE];
