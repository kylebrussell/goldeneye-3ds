#include "ge_original_language_text.h"

#include <assert.h>
#include <stddef.h>

#include <bondconstants.h>

#define SOLO_BANKS(X) \
    X(LARCH, LarchE) \
    X(LARK, LarkE) \
    X(LAZT, LaztE) \
    X(LCAVE, LcaveE) \
    X(LAREC, LarecE) \
    X(LCRAD, LcradE) \
    X(LCRYP, LcrypE) \
    X(LDAM, LdamE) \
    X(LDEPO, LdepoE) \
    X(LDEST, LdestE) \
    X(LJUN, LjunE) \
    X(LLEN, LlenE) \
    X(LPETE, LpeteE) \
    X(LRUN, LrunE) \
    X(LSEV, LsevE) \
    X(LSEVB, LsevbE) \
    X(LSEVX, LsevxE) \
    X(LSEVXB, LsevxbE) \
    X(LSILO, LsiloE) \
    X(LSTAT, LstatE) \
    X(LTRA, LtraE)

#define DECLARE_BANK(bank, symbol) extern char *symbol[];
SOLO_BANKS(DECLARE_BANK)
extern char *LgunE[];
extern char *LmiscE[];
extern char *LoptionsE[];
extern char *LpropobjE[];
extern char *LtitleE[];
#undef DECLARE_BANK

int main(void)
{
#define CHECK_BANK(bank, symbol) \
    do { \
        uint16_t slot; \
        for (slot = 0U; slot < 5U; ++slot) { \
            assert(ge_original_language_text_by_bank( \
                (bank), slot) == (symbol)[slot]); \
            assert(ge_original_language_text( \
                (uint16_t)(((bank) << 10U) | slot)) == (symbol)[slot]); \
        } \
    } while (0);
    SOLO_BANKS(CHECK_BANK)
    CHECK_BANK(LGUN, LgunE)
    CHECK_BANK(LMISC, LmiscE)
    CHECK_BANK(LOPTIONS, LoptionsE)
    CHECK_BANK(LPROPOBJ, LpropobjE)
    CHECK_BANK(LTITLE, LtitleE)
#undef CHECK_BANK
    assert(ge_original_language_text_by_bank(LNULL, 0U) == NULL);
    return 0;
}
