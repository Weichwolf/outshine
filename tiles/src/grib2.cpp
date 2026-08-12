#include "grib2.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Upper bound on a single field, checked before any allocation is sized from file content: the
 * 0.25 deg global grid is 1440*721 = 1038240 points, so this leaves room for a finer product
 * without ever letting a malformed length drive a multiply. */
#define FB_G2_MAX_POINTS 8000000u

static thread_local const char *g_err;

const char *fb_grib2_last_error(void) { return g_err ? g_err : "none"; }

/* The one place a GRIB2 failure gets its reason: `return g2_fail("...")` records and returns in the
 * same line at the call site. */
static int g2_fail(const char *why) { g_err = why; return -1; }

static uint16_t be16(const uint8_t *p) { return (uint16_t)(((uint32_t)p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static uint64_t be64(const uint8_t *p) { return ((uint64_t)be32(p) << 32) | be32(p + 4); }

/* GRIB2 signs are sign-bit + magnitude, not two's complement. */
static int32_t g2_sign(uint32_t v, unsigned bits) {
    uint32_t sb = 1u << (bits - 1);
    return (v & sb) ? -(int32_t)(v & (sb - 1)) : (int32_t)v;
}
static float g2_f32(const uint8_t *p) {
    uint32_t u = be32(p); float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

/* Days since 1970-01-01 from a proleptic Gregorian date (Hinnant). No timegm/TZ dependency: GRIB
 * reference times are UTC by definition and must decode identically wherever this runs. */
static int64_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

typedef struct {
    const uint8_t *b;
    size_t   n, bp;
    uint64_t acc;
    unsigned have;
    int      over;
} fb_bitr;

static void br_init(fb_bitr *r, const uint8_t *b, size_t n) {
    r->b = b; r->n = n; r->bp = 0; r->acc = 0; r->have = 0; r->over = 0;
}

static uint32_t br_get(fb_bitr *r, unsigned nb) {
    if (nb == 0) return 0;
    while (r->have < nb) {
        uint8_t byte = 0;
        if (r->bp < r->n) byte = r->b[r->bp];
        else r->over = 1;
        r->bp++;
        r->acc = (r->acc << 8) | byte;
        r->have += 8;
    }
    r->have -= nb;
    uint64_t mask = (nb >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << nb) - 1);
    return (uint32_t)((r->acc >> r->have) & mask);
}

/* The three descriptor arrays of a complex-packed section 7 each start on an octet boundary. */
static void br_align(fb_bitr *r) { r->have -= r->have % 8; }

typedef struct {
    int32_t *iv;
    float   *fv;
    size_t   cap;
} fb_g2_scratch;

static int scratch_reserve(fb_g2_scratch *s, uint32_t npoints) {
    if (npoints <= s->cap) return 1;
    int32_t * iv = (int32_t *)realloc(s->iv, (size_t)npoints * sizeof *iv);
    if (!iv) return 0;
    s->iv = iv;
    float * fv = (float *)realloc(s->fv, (size_t)npoints * sizeof *fv);
    if (!fv) return 0;
    s->fv = fv;
    s->cap = npoints;
    return 1;
}

static int32_t clamp32(int64_t v) {
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return (int32_t)v;
}

/* Section 5 template 5.3 (and 5.2, which is 5.3 with differencing order 0). Layout, section-
 * relative 1-based octets: 12-15 R, 16-17 E, 18-19 D, 20 nbits, 22 group split method,
 * 23 missing-value management, 32-35 NG, 36 group-width reference, 37 group-width bits,
 * 38-41 group-length reference, 42 length increment, 43-46 true length of last group,
 * 47 group-length bits, 48 differencing order, 49 extra-descriptor octets. */
static int drt_complex(int tmpl, const uint8_t *s5, size_t n5, const uint8_t *s7, size_t n7,
                       uint32_t ndpts, int32_t *iv, float *out) {
    /* 5.2 is 5.3 minus the two trailing octets (differencing order and extra-descriptor width),
     * i.e. the same layout with order 0. */
    if (n5 < (tmpl == 3 ? 49u : 47u)) return g2_fail("drt5.3 section too short");
    float    R  = g2_f32(s5 + 11);
    int32_t  E  = g2_sign(be16(s5 + 15), 16);
    int32_t  D  = g2_sign(be16(s5 + 17), 16);
    unsigned nb = s5[19];
    if (s5[22] != 0) return g2_fail("drt5.3 missing-value management unsupported");
    uint32_t NG   = be32(s5 + 31);
    uint32_t rgw  = s5[35];
    unsigned nbgw = s5[36];
    uint32_t rgl  = be32(s5 + 37);
    uint32_t linc = s5[41];
    uint32_t lastgl = be32(s5 + 42);
    unsigned nbgl = s5[46];
    unsigned order = (tmpl == 3) ? s5[47] : 0u;
    unsigned nex   = (tmpl == 3) ? s5[48] : 0u;
    if (nb > 32 || nbgw > 32 || nbgl > 32) return g2_fail("drt5.3 implausible bit width");
    if (NG == 0 || NG > ndpts) return g2_fail("drt5.3 group count out of range");
    if (order > 2) return g2_fail("drt5.3 differencing order unsupported");
    if (nex > 4) return g2_fail("drt5.3 extra-descriptor width unsupported");
    if (order > 0 && nex == 0) return g2_fail("drt5.3 differencing without descriptors");

    fb_bitr r;
    br_init(&r, s7 + 5, n7 - 5);

    int64_t ival1 = 0, ival2 = 0, minsd = 0;
    if (order > 0) {
        unsigned nbitsd = nex * 8;
        ival1 = br_get(&r, nbitsd);
        if (order == 2) ival2 = br_get(&r, nbitsd);
        unsigned sign = br_get(&r, 1);
        minsd = br_get(&r, nbitsd - 1);
        if (sign) minsd = -minsd;
    }

    /* Group descriptors: references, then widths, then lengths -- each array starts octet-aligned.
     * NG is bounded by ndpts above, so this multiply cannot wrap. */
    uint32_t * grefs = (uint32_t *)malloc((size_t)NG * 3 * sizeof(uint32_t));
    if (!grefs) return g2_fail("drt5.3 out of memory");
    uint32_t *gw = grefs + NG, *gl = grefs + 2 * (size_t)NG;
    for (uint32_t i = 0; i < NG; i++) grefs[i] = br_get(&r, nb);
    br_align(&r);
    for (uint32_t i = 0; i < NG; i++) gw[i] = br_get(&r, nbgw) + rgw;
    br_align(&r);
    for (uint32_t i = 0; i < NG; i++) gl[i] = br_get(&r, nbgl) * linc + rgl;
    gl[NG - 1] = lastgl;
    br_align(&r);

    uint64_t total = 0;
    for (uint32_t i = 0; i < NG; i++) {
        if (gw[i] > 32) { free(grefs); return g2_fail("drt5.3 group width out of range"); }
        total += gl[i];
    }
    if (total != ndpts) { free(grefs); return g2_fail("drt5.3 group lengths do not cover the grid"); }

    uint32_t k = 0;
    for (uint32_t g = 0; g < NG; g++) {
        uint32_t w = gw[g], L = gl[g], ref = grefs[g];
        if (w == 0) { for (uint32_t i = 0; i < L; i++) iv[k++] = (int32_t)ref; continue; }
        for (uint32_t i = 0; i < L; i++) iv[k++] = clamp32((int64_t)ref + br_get(&r, w));
    }
    free(grefs);
    if (r.over) return g2_fail("drt5.3 data section truncated");

    if (order == 1) {
        iv[0] = clamp32(ival1);
        for (uint32_t i = 1; i < ndpts; i++)
            iv[i] = clamp32((int64_t)iv[i] + minsd + iv[i - 1]);
    } else if (order == 2) {
        if (ndpts < 2) return g2_fail("drt5.3 second-order differencing on a 1-point grid");
        iv[0] = clamp32(ival1);
        iv[1] = clamp32(ival2);
        for (uint32_t i = 2; i < ndpts; i++)
            iv[i] = clamp32((int64_t)iv[i] + minsd + 2 * (int64_t)iv[i - 1] - iv[i - 2]);
    }

    double bscale = ldexp(1.0, E), dscale = pow(10.0, -D);
    for (uint32_t i = 0; i < ndpts; i++) out[i] = (float)(((double)iv[i] * bscale + R) * dscale);
    return 0;
}

/* Section 5 template 5.0: 12-15 R, 16-17 E, 18-19 D, 20 nbits. */
static int drt_simple(const uint8_t *s5, size_t n5, const uint8_t *s7, size_t n7,
                      uint32_t ndpts, float *out) {
    if (n5 < 21) return g2_fail("drt5.0 section too short");
    float    R  = g2_f32(s5 + 11);
    int32_t  E  = g2_sign(be16(s5 + 15), 16);
    int32_t  D  = g2_sign(be16(s5 + 17), 16);
    unsigned nb = s5[19];
    if (nb > 32) return g2_fail("drt5.0 implausible bit width");
    double bscale = ldexp(1.0, E), dscale = pow(10.0, -D);
    if (nb == 0) {
        float v = (float)((double)R * dscale);
        for (uint32_t i = 0; i < ndpts; i++) out[i] = v;
        return 0;
    }
    fb_bitr r;
    br_init(&r, s7 + 5, n7 - 5);
    for (uint32_t i = 0; i < ndpts; i++)
        out[i] = (float)(((double)br_get(&r, nb) * bscale + R) * dscale);
    if (r.over) return g2_fail("drt5.0 data section truncated");
    return 0;
}

static int parse_grid(const uint8_t *s3, size_t n3, fb_grib2_field *f) {
    if (n3 < 72) return g2_fail("section 3 too short");
    if (be16(s3 + 12) != 0) return g2_fail("grid template != 3.0");
    uint32_t basic = be32(s3 + 38);
    if (basic != 0 && basic != 0xFFFFFFFFu) return g2_fail("non-degree grid angle unit");
    uint8_t scan = s3[71];
    if (scan & 0xE0) return g2_fail("grid scanning mode != 0");
    f->npoints = be32(s3 + 6);
    f->nx = be32(s3 + 30);
    f->ny = be32(s3 + 34);
    if (f->nx == 0 || f->ny == 0 || f->npoints == 0 || f->npoints > FB_G2_MAX_POINTS)
        return g2_fail("grid dimensions out of range");
    if ((uint64_t)f->nx * f->ny != f->npoints) return g2_fail("grid dimensions disagree with point count");
    f->lat0 = g2_sign(be32(s3 + 46), 32) * 1e-6;
    f->lon0 = g2_sign(be32(s3 + 50), 32) * 1e-6;
    f->dlon = g2_sign(be32(s3 + 63), 32) * 1e-6;
    f->dlat = g2_sign(be32(s3 + 67), 32) * 1e-6;
    /* scanning mode 0: +i is east and +j is SOUTH, so the stored dlat is a magnitude. */
    f->dlat = -f->dlat;
    return 0;
}

static int parse_product(const uint8_t *s4, size_t n4, fb_grib2_field *f) {
    if (n4 < 29) return g2_fail("section 4 too short");
    uint16_t pdt = be16(s4 + 7);
    if (pdt != 0 && pdt != 8) return g2_fail("product template unsupported");
    f->category = s4[9];
    f->number   = s4[10];
    f->level_type = s4[22];
    int32_t  sf = g2_sign(s4[23], 8);
    uint32_t sv = be32(s4 + 24);
    f->level_value = (s4[23] == 0xFF || sv == 0xFFFFFFFFu) ? 0.0 : (double)sv * pow(10.0, -sf);

    static const int32_t unit_s[14] = { 60, 3600, 86400, 0, 0, 0, 0, 0, 0, 0, 10800, 21600, 43200, 1 };
    int32_t us = (s4[17] < 14) ? unit_s[s4[17]] : 0;
    f->valid_epoch = f->ref_epoch + (int64_t)be32(s4 + 18) * us;
    return 0;
}

static int parse_ident(const uint8_t *s1, size_t n1, fb_grib2_field *f) {
    if (n1 < 21) return g2_fail("section 1 too short");
    int y = be16(s1 + 12);
    unsigned mo = s1[14], d = s1[15], h = s1[16], mi = s1[17], se = s1[18];
    if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || se > 60)
        return g2_fail("implausible reference time");
    f->ref_epoch = days_from_civil(y, mo, d) * 86400 + (int64_t)h * 3600 + mi * 60 + se;
    return 0;
}

int fb_grib2_walk(const uint8_t *buf, size_t n, fb_grib2_sink sink, void *ud) {
    g_err = 0;
    fb_g2_scratch sc = { 0, 0, 0 };
    int emitted = 0, rc = 0;
    size_t p = 0;

    while (p + 16 <= n) {
        if (memcmp(buf + p, "GRIB", 4) != 0) { rc = g2_fail("no GRIB magic"); goto done; }
        if (buf[p + 7] != 2)                 { rc = g2_fail("not GRIB edition 2"); goto done; }
        uint64_t mlen = be64(buf + p + 8);
        if (mlen < 24 || mlen > n - p)       { rc = g2_fail("message length out of range"); goto done; }

        fb_grib2_field f = {};
        f.discipline = buf[p + 6];
        const uint8_t *s3 = 0, *s5 = 0, *s7 = 0;
        size_t n3 = 0, n5 = 0, n7 = 0;
        int have_grid = 0, have_product = 0, have_time = 0, bitmap_present = 0;

        size_t q = p + 16, mend = p + (size_t)mlen;
        while (q + 4 <= mend) {
            if (memcmp(buf + q, "7777", 4) == 0) break;
            if (q + 5 > mend) { rc = g2_fail("truncated section header"); goto done; }
            uint32_t slen = be32(buf + q);
            if (slen < 5 || slen > mend - q) { rc = g2_fail("section length out of range"); goto done; }
            const uint8_t *s = buf + q;
            switch (s[4]) {
            case 1: if (parse_ident(s, slen, &f) < 0)   { rc = -1; goto done; } have_time = 1;    break;
            case 3: s3 = s; n3 = slen; have_grid = 0;                                            break;
            case 4: if (parse_product(s, slen, &f) < 0) { rc = -1; goto done; } have_product = 1; break;
            case 5: s5 = s; n5 = slen;                                                           break;
            case 6: bitmap_present = (slen >= 6 && s[5] != 255);                                 break;
            case 7: s7 = s; n7 = slen;                                                           break;
            default: break;
            }

            if (s[4] == 7) {
                if (!s3 || !s5 || !have_product || !have_time) { rc = g2_fail("record missing a section"); goto done; }
                if (bitmap_present) { rc = g2_fail("section 6 bitmap unsupported"); goto done; }
                if (!have_grid) {
                    if (parse_grid(s3, n3, &f) < 0) { rc = -1; goto done; }
                    have_grid = 1;
                }
                if (n5 < 11) { rc = g2_fail("section 5 too short"); goto done; }
                uint32_t ndpts = be32(s5 + 5);
                if (ndpts != f.npoints) { rc = g2_fail("packed point count disagrees with the grid"); goto done; }
                if (!scratch_reserve(&sc, ndpts)) { rc = g2_fail("out of memory"); goto done; }

                uint16_t drt = be16(s5 + 9);
                int dr;
                if (drt == 0)                   dr = drt_simple(s5, n5, s7, n7, ndpts, sc.fv);
                else if (drt == 2 || drt == 3)  dr = drt_complex(drt, s5, n5, s7, n7, ndpts, sc.iv, sc.fv);
                else                            dr = g2_fail("data representation template unsupported");
                if (dr < 0) { rc = -1; goto done; }

                emitted++;
                int stop = sink(&f, sc.fv, ud);
                if (stop) { rc = stop; goto done; }
            }
            q += slen;
        }
        p += (size_t)mlen;
    }
    rc = emitted;

done:
    free(sc.iv);
    free(sc.fv);
    return rc;
}
