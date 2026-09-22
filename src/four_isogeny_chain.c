#include "four_isogeny_chain.h"

#include <assert.h>

static void
xdbl(fp2_t *X, fp2_t *Z, const fp2_t *A24, const fp2_t *C24)
{
    fp2_t t0, t1, t2;

    fp2_add(&t0, X, Z);
    fp2_sqr(&t0, &t0);

    fp2_sub(&t1, X, Z);
    fp2_sqr(&t1, &t1);

    fp2_sub(&t2, &t0, &t1);

    fp2_mul(&t1, &t1, C24);
    fp2_mul(X, &t0, &t1);

    fp2_mul(&t0, &t2, A24);
    fp2_add(&t0, &t0, &t1);
    fp2_mul(Z, &t0, &t2);
}


static void
xdbl_iter(fp2_t *X, fp2_t *Z, const fp2_t *A24, const fp2_t *C24, unsigned n)
{
    while (n--)
        xdbl(X, Z, A24, C24);
}


static void
four_isogeny_codomain(fp2_t *A24, fp2_t *C24, fp2_t *c0, fp2_t *c1, fp2_t *c2, const fp2_t *X, const fp2_t *Z)
{
    fp2_t t;

    fp2_sub(c1, X, Z);
    fp2_add(c2, X, Z);

    fp2_sqr(&t, Z);
    fp2_double(&t, &t);

    fp2_sqr(C24, &t);
    fp2_double(c0, &t);

    fp2_sqr(&t, X);
    fp2_double(&t, &t);
    fp2_sqr(A24, &t);
}


static void
four_isogeny_eval(fp2_t *X, fp2_t *Z,
                  const fp2_t *c0,
                  const fp2_t *c1,
                  const fp2_t *c2)
{
    fp2_t t0, t1, t2;

    fp2_add(&t0, X, Z);
    fp2_sub(&t1, X, Z);

    fp2_mul(X, &t0, c1);
    fp2_mul(Z, &t1, c2);

    fp2_mul(&t2, &t0, &t1);
    fp2_mul(&t2, &t2, c0);

    fp2_add(&t0, X, Z);
    fp2_sqr(&t0, &t0);

    fp2_sub(&t1, X, Z);
    fp2_sqr(&t1, &t1);

    fp2_add(X, &t2, &t0);
    fp2_mul(X, X, &t0);

    fp2_sub(Z, &t1, &t2);
    fp2_mul(Z, &t1, Z);
}

void
four_isogeny_chain(fp2_t *out_X, fp2_t *out_Z,
                   const fp2_t *A,
                   const fp2_t *kernel_X,
                   const fp2_t *kernel_Z,
                   unsigned e)
{
    assert((e & 1) == 0);

    size_t space = 1;
    for (size_t i = 1; i < e; i <<= 1)
        ++space;

    fp2_t A24, C24;
    fp2_t c0, c1, c2;
    fp2_t sx[space];
    fp2_t sz[space];
    unsigned orders[space];

    fp2_t two;
    unsigned sp = 0;

    /* A24 = (A + 2) / 4 */
    fp2_set_small(&two, 2);
    fp2_add(&A24, A, &two);
    fp2_half(&A24, &A24);
    fp2_half(&A24, &A24);

    fp2_set_one(&C24);


    sx[0] = *kernel_X;
    sz[0] = *kernel_Z;
    orders[0] = e + 2;

    for (unsigned i = 0; i < e / 2; i++) {

        assert(sp < space);
        assert(orders[sp] >= 2);


        while (orders[sp] != 2) {
            unsigned order = orders[sp];
            unsigned m = 2 * (order / 4) + (order & 1);

            assert(order > 2);
            assert(m > 0);
            assert(m < order);
            assert(order - m >= 2);
            assert(sp + 1 < space);

            sx[sp + 1] = sx[sp];
            sz[sp + 1] = sz[sp];

            xdbl_iter(&sx[sp + 1], &sz[sp + 1],
                      &A24, &C24, m);

            orders[sp + 1] = order - m;
            ++sp;
            assert(sp < space);
            assert(orders[sp] >= 2);
        }

        assert(sp > 0);
        assert(orders[sp] == 2);

        fp2_t kernel_X4 = sx[sp];
        fp2_t kernel_Z4 = sz[sp];
        --sp;

        assert(sp < space);
        four_isogeny_codomain(&A24, &C24,
                              &c0, &c1, &c2,
                              &kernel_X4, &kernel_Z4);

        for (unsigned j = 0; j <= sp; j++) {
            assert(orders[j] >= 4);

            four_isogeny_eval(&sx[j], &sz[j], &c0, &c1, &c2);
            orders[j] -= 2;
            assert(orders[j] >= 2);
        }
    }

    assert(sp == 0);
    assert(orders[0] == 2);

    *out_X = sx[0];
    *out_Z = sz[0];
}
