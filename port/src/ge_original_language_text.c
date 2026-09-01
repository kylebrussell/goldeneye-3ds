#include "ge_original_language_text.h"

#include <stddef.h>
#include <ultra64.h>

#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>

extern char *LarchE[];
extern char *LarkE[];
extern char *LaztE[];
extern char *LcaveE[];
extern char *LarecE[];
extern char *LcradE[];
extern char *LcrypE[];
extern char *LdamE[];
extern char *LdepoE[];
extern char *LdestE[];
extern char *LjunE[];
extern char *LlenE[];
extern char *LpeteE[];
extern char *LrunE[];
extern char *LsevE[];
extern char *LsevbE[];
extern char *LsevxE[];
extern char *LsevxbE[];
extern char *LsiloE[];
extern char *LstatE[];
extern char *LtraE[];
extern char *LgunE[];
extern char *LmiscE[];
extern char *LoptionsE[];
extern char *LpropobjE[];
extern char *LtitleE[];

const char *ge_original_language_text_by_bank(
    uint16_t bank, uint16_t slot)
{
    char **strings = NULL;

    switch (bank) {
    case LARCH: strings = LarchE; break;
    case LARK: strings = LarkE; break;
    case LAZT: strings = LaztE; break;
    case LCAVE: strings = LcaveE; break;
    case LAREC: strings = LarecE; break;
    case LCRAD: strings = LcradE; break;
    case LCRYP: strings = LcrypE; break;
    case LDAM: strings = LdamE; break;
    case LDEPO: strings = LdepoE; break;
    case LDEST: strings = LdestE; break;
    case LJUN: strings = LjunE; break;
    case LLEN: strings = LlenE; break;
    case LPETE: strings = LpeteE; break;
    case LRUN: strings = LrunE; break;
    case LSEV: strings = LsevE; break;
    case LSEVB: strings = LsevbE; break;
    case LSEVX: strings = LsevxE; break;
    case LSEVXB: strings = LsevxbE; break;
    case LSILO: strings = LsiloE; break;
    case LSTAT: strings = LstatE; break;
    case LTRA: strings = LtraE; break;
    case LGUN: strings = LgunE; break;
    case LMISC: strings = LmiscE; break;
    case LOPTIONS: strings = LoptionsE; break;
    case LPROPOBJ: strings = LpropobjE; break;
    case LTITLE: strings = LtitleE; break;
    default: return NULL;
    }
    /* The slot is authored by the same setup/briefing source as the selected
     * bank. It is already limited to ten bits by getStringID/langGet. */
    return strings[slot & UINT16_C(0x03ff)];
}

const char *ge_original_language_text(uint16_t text_id)
{
    return ge_original_language_text_by_bank(
        (uint16_t)(text_id >> 10U),
        (uint16_t)(text_id & UINT16_C(0x03ff)));
}
