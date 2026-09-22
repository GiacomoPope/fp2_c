#ifndef TWO_TWO_ISOGENY_CHAIN_H
#define TWO_TWO_ISOGENY_CHAIN_H

#include <fp.h>

typedef struct theta_t {
    fp_t x;
    fp_t y;
    fp_t z;
    fp_t t;
} theta_t;

void
two_two_isogeny_chain(theta_t *codomain,
                      const theta_t *domain,
                      const theta_t *k1,
                      const theta_t *k2,
                      unsigned e);

#endif
