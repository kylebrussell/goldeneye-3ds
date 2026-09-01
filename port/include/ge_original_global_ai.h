#ifndef GE_ORIGINAL_GLOBAL_AI_H
#define GE_ORIGINAL_GLOBAL_AI_H

#include <stdint.h>

struct AIRecord;

/* Exact bytecode extracted from the matching decomp chraidata object. */
struct AIRecord *ge_original_global_ai_find(int32_t ai_list_id);
uint32_t ge_original_global_ai_count(void);

#endif
