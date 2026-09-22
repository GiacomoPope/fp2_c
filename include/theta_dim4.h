#ifndef THETA_DIM4_H
#define THETA_DIM4_H

#include "fp.h"

/* Dimension-4 theta point: 16 coordinates in GF(p). */
typedef struct theta_dim4_t {
    fp_t coords[16];
} theta_dim4_t;

/* Hadamard transform: four butterfly stages at stride 1, 2, 4, 8, each
   built from fp_hadamard. Aliasing of out with in is allowed. */
void theta_dim4_hadamard(theta_dim4_t *out, const theta_dim4_t *in);

/* out->coords[i] = in->coords[i]^2 for all 16 coordinates. Aliasing of out
   with in is allowed. */
void theta_dim4_square(theta_dim4_t *out, const theta_dim4_t *in);

#endif /* THETA_DIM4_H */
