#include "two_two_isogeny_chain.h"

#include <assert.h>
#include <stddef.h>


static inline void
hadamard(theta_t *out, const theta_t *in)
{
    fp_t t1, t2, t3, t4;

    fp_add(&t1, &in->x, &in->y);
    fp_sub(&t2, &in->x, &in->y);
    fp_add(&t3, &in->z, &in->t);
    fp_sub(&t4, &in->z, &in->t);

    fp_add(&out->x, &t1, &t3);
    fp_add(&out->y, &t2, &t4);
    fp_sub(&out->z, &t1, &t3);
    fp_sub(&out->t, &t2, &t4);
}


static inline void
square(theta_t *out, const theta_t *in)
{
    fp_sqr(&out->x, &in->x);
    fp_sqr(&out->y, &in->y);
    fp_sqr(&out->z, &in->z);
    fp_sqr(&out->t, &in->t);
}


static inline void
square_hadamard(theta_t *out, const theta_t *in)
{
    square(out, in);
    hadamard(out, out);
}


static inline void
coordinate_mul(theta_t *out, const theta_t *p, const theta_t *q)
{
    fp_mul(&out->x, &p->x, &q->x);
    fp_mul(&out->y, &p->y, &q->y);
    fp_mul(&out->z, &p->z, &q->z);
    fp_mul(&out->t, &p->t, &q->t);
}


static inline void
theta_dbl(theta_t *out,
          const theta_t *in,
          const theta_t *c1,
          const theta_t *c2)
{
    square(out, in);
    hadamard(out, out);
    square(out, out);
    coordinate_mul(out, out, c2);
    hadamard(out, out);
    coordinate_mul(out, out, c1);
}


static inline void
theta_dbl_iter(theta_t *out,
               const theta_t *in,
               const theta_t *c1,
               const theta_t *c2,
               unsigned n)
{
    *out = *in;

    while (n--)
        theta_dbl(out, out, c1, c2);
}


static inline void
arithmetic_precomputation(theta_t *c1,
                          theta_t *c2,
                          const theta_t *domain)
{
    fp_t t1, t2;

    fp_mul(&t1, &domain->x, &domain->y);
    fp_mul(&t2, &domain->z, &domain->t);

    fp_mul(&c1->x, &t2, &domain->y);
    fp_mul(&c1->y, &t2, &domain->x);
    fp_mul(&c1->z, &t1, &domain->t);
    fp_mul(&c1->t, &t1, &domain->z);

    theta_t p;
    square_hadamard(&p, domain);

    fp_mul(&t1, &p.x, &p.y);
    fp_mul(&t2, &p.z, &p.t);

    fp_mul(&c2->x, &t2, &p.y);
    fp_mul(&c2->y, &t2, &p.x);
    fp_mul(&c2->z, &t1, &p.t);
    fp_mul(&c2->t, &t1, &p.z);
}


static inline void
two_two_codomain(theta_t *codomain,
                 theta_t *inv,
                 const theta_t *k1,
                 const theta_t *k2)
{
    theta_t p1, p2;

    hadamard(&p1, k1);
    hadamard(&p2, k2);
    square_hadamard(&p1, &p1);
    square_hadamard(&p2, &p2);


    fp_t xa_tb;
    fp_t za_xb;
    fp_t zc_td;
    fp_mul(&xa_tb, &p1.x, &p2.t);
    fp_mul(&za_xb, &p2.x, &p1.y);
    fp_mul(&zc_td, &p2.z, &p2.t);

    fp_mul(&codomain->x, &p2.x, &xa_tb);
    fp_mul(&codomain->y, &p2.t, &za_xb);
    fp_mul(&codomain->z, &p2.z, &xa_tb);
    fp_mul(&codomain->t, &p2.t, &za_xb);

    fp_mul(&inv->x, &p1.y, &zc_td);
    fp_mul(&inv->y, &p1.x, &zc_td);
    inv->z = codomain->t;
    inv->t = codomain->z;
}


static inline void
two_two_eval(theta_t *out,
             const theta_t *in,
             const theta_t *inv)
{
    theta_t p;

    hadamard(&p, in);
    square_hadamard(&p, &p);
    coordinate_mul(out, &p, inv);
}


void
two_two_isogeny_chain(theta_t *codomain,
                      const theta_t *domain_in,
                      const theta_t *k1,
                      const theta_t *k2,
                      unsigned e)
{
    assert(e > 0);

    size_t space = 1;
    for (size_t i = 1; i < e; i <<= 1)
        ++space;

    theta_t kernel_pts[2 * space];
    unsigned orders[space];

    kernel_pts[0] = *k1;
    kernel_pts[1] = *k2;
    orders[0] = e;

    theta_t oa = *domain_in;
    theta_t c1, c2, inv;

    size_t k = 0;
    for (unsigned i = 0; i < e; i++) {
        arithmetic_precomputation(&c1, &c2, &oa);
        
        while (orders[k] != 1) {
            assert(k + 1 < space);
            unsigned m = orders[k] >> 1;
            theta_dbl_iter(&kernel_pts[2 * (k + 1)],
                           &kernel_pts[2 * k],
                           &c1, &c2,
                           m);

            theta_dbl_iter(&kernel_pts[2 * (k + 1) + 1],
                           &kernel_pts[2 * k + 1],
                           &c1, &c2,
                           m);
            ++k;
            orders[k] = orders[k - 1] - m;
        }

        assert(orders[k] == 1);

        theta_t k1_i = kernel_pts[2 * k];
        theta_t k2_i = kernel_pts[2 * k + 1];
        two_two_codomain(&oa, &inv, &k1_i, &k2_i);

        for (size_t j = 0; j < 2 * k; j++) {
            two_two_eval(&kernel_pts[j], &kernel_pts[j], &inv);
        }

        for (size_t j = 0; j < k; j++) {
            assert(orders[j] >= 1);
            --orders[j];
        }

        if (k != 0) {
            --k;
        }
    }

    *codomain = oa;
    assert(k == 0);
}
