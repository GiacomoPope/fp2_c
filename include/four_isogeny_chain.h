#ifndef FOUR_ISOGENY_CHAIN_H
#define FOUR_ISOGENY_CHAIN_H

#include "fp2.h"

void
four_isogeny_chain(fp2_t *out_X, fp2_t *out_Z,
                   const fp2_t *A,
                   const fp2_t *kernel_X,
                   const fp2_t *kernel_Z,
                   unsigned e);

#endif
