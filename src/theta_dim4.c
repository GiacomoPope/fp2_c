#include "theta_dim4.h"

#include <stddef.h>

void
theta_dim4_hadamard(theta_dim4_t *out, const theta_dim4_t *in)
{
    /* Stage 0 (stride 1) is the only stage that reads `in`; it writes
       straight into `out` (fp_hadamard allows r1/r2 to alias a/b, so this
       is safe even when out == in). Stages 1-3 then run in place on
       `out`, since every coordinate was written by stage 0 -- no
       temporary array needed. */
    for (size_t i = 0; i < 8; i++) {
        fp_hadamard(&out->coords[2 * i], &out->coords[2 * i + 1],
                    &in->coords[2 * i], &in->coords[2 * i + 1]);
    }

    static const size_t strides[3] = { 2, 4, 8 };
    for (size_t s = 0; s < 3; s++) {
        size_t stride = strides[s];
        for (size_t base = 0; base < 16; base += stride * 2) {
            for (size_t k = 0; k < stride; k++) {
                size_t i = base + k;
                size_t j = i + stride;
                fp_hadamard(&out->coords[i], &out->coords[j], &out->coords[i], &out->coords[j]);
            }
        }
    }
}

void
theta_dim4_square(theta_dim4_t *out, const theta_dim4_t *in)
{
    for (size_t i = 0; i < 16; i++) {
        fp_sqr(&out->coords[i], &in->coords[i]);
    }
}
