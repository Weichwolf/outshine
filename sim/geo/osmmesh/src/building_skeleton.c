/* libosmmesh/src/building_skeleton.c
 *
 * Straight-skeleton implementation ported from
 * streetwalk/tools/building/builders/roofs/_polyskel.py (Felkel &
 * Obdrzalek 1998, Botffy's Python reference) + roofs/_skeleton.py (the
 * planar-graph face reconstruction).
 *
 * Structure mirrors the Python sources so future upstream fixes map
 * straight across:
 *
 *   - Geometry primitives: Vec2, Ray2 / Line2 / Segment2 (2-D only).
 *   - LAVertex doubly-linked on prev/next, belongs to a _LAV.
 *   - _SLAV = list of _LAVs + immutable _OriginalEdge array.
 *   - Event queue = binary heap sorted by distance (edge + split events).
 *   - Main loop: pop event, validate, handle_edge or handle_split,
 *     push new events.
 *   - After: build planar graph (polygon edges + skeleton arcs), walk
 *     faces by leftmost-turn rule — one face per original polygon edge.
 *
 * Memory: vertices are heap-allocated with vset tracking; LAV nodes are
 * reference-counted by _valid flag but never freed mid-run — we reap
 * everything in one sweep at the end.
 *
 * Robustness notes vs the Python port:
 *   - We use a tolerance EPS_POS = 1e-5 for "approximately equal points"
 *     (Felkel's EPSILON was dimensionless 1e-5 in Python on unit input;
 *     our input is meters, so 1e-5 m = 10 um. This matches the Python's
 *     observed behavior on unit-scale polygons.)
 *   - Coincident skeleton points inside face extraction get merged at
 *     1e-3 m (Python _EPS) — too loose for robust Felkel math but the
 *     right grain for tessellation-level topology.
 *   - Pre-centering at centroid (Python _skeleton.compute): skeletons
 *     become unstable at large world-offsets because denominators in
 *     the bisector math cancel badly. We mirror the fix here.
 */

#include "building_skeleton.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 * Geometry primitives (lightweight 2-D, double precision)
 * ========================================================================== */

typedef struct { double x, y; } vec2;

static inline vec2 vec2_mk(double x, double y) { vec2 v = {x, y}; return v; }
static inline vec2 vec2_add(vec2 a, vec2 b) { return vec2_mk(a.x + b.x, a.y + b.y); }
static inline vec2 vec2_sub(vec2 a, vec2 b) { return vec2_mk(a.x - b.x, a.y - b.y); }
static inline vec2 vec2_scl(vec2 a, double s) { return vec2_mk(a.x * s, a.y * s); }
static inline vec2 vec2_neg(vec2 a) { return vec2_mk(-a.x, -a.y); }
static inline double vec2_dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
static inline double vec2_len(vec2 a) { return sqrt(a.x * a.x + a.y * a.y); }
static inline double vec2_cross(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

static inline vec2 vec2_norm(vec2 a)
{
    double L = vec2_len(a);
    if (L < 1e-15) return vec2_mk(0.0, 0.0);
    return vec2_mk(a.x / L, a.y / L);
}

static inline double vec2_dist(vec2 a, vec2 b) { return vec2_len(vec2_sub(a, b)); }

/* Point-to-line distance. Line = (p, v); formula d = |v x (point - p)| / |v|. */
static double line_dist(vec2 p, vec2 v, vec2 point)
{
    double L = vec2_len(v);
    if (L < 1e-15) return vec2_dist(p, point);
    vec2 d = vec2_sub(point, p);
    return fabs(vec2_cross(v, d)) / L;
}

/* Infinite-line / infinite-line intersection (Cramer). Returns 0 on
 * parallel/coincident; 1 on single-point intersection (written to *out). */
static int line_line_intersect(vec2 ap, vec2 av, vec2 bp, vec2 bv, vec2 *out)
{
    double d = bv.y * av.x - bv.x * av.y;
    if (fabs(d) < 1e-15) return 0;
    double dy = ap.y - bp.y;
    double dx = ap.x - bp.x;
    double ua = (bv.x * dy - bv.y * dx) / d;
    out->x = ap.x + ua * av.x;
    out->y = ap.y + ua * av.y;
    return 1;
}

/* Ray / ray intersection. Same math as line/line but both u parameters
 * must be >= 0 for validity. */
static int ray_ray_intersect(vec2 ap, vec2 av, vec2 bp, vec2 bv, vec2 *out)
{
    double d = bv.y * av.x - bv.x * av.y;
    if (fabs(d) < 1e-15) return 0;
    double dy = ap.y - bp.y;
    double dx = ap.x - bp.x;
    double ua = (bv.x * dy - bv.y * dx) / d;
    if (ua < 0.0) return 0;
    double ub = (av.x * dy - av.y * dx) / d;
    if (ub < 0.0) return 0;
    out->x = ap.x + ua * av.x;
    out->y = ap.y + ua * av.y;
    return 1;
}

/* Felkel EPSILON — dimensionless / pure-number comparator tolerance. */
#define SK_EPSILON 1e-5

static int approx_eq(double a, double b)
{
    if (a == b) return 1;
    double aa = fabs(a), ab = fabs(b);
    double m = aa > ab ? aa : ab;
    return fabs(a - b) <= m * 0.001;
}

static int approx_same(vec2 a, vec2 b)
{
    return approx_eq(a.x, b.x) && approx_eq(a.y, b.y);
}

/* ==========================================================================
 * SLAV / LAV structures
 * ========================================================================== */

/* A segment p -> p+v kept as (p, v). The Python code stores
 * LineSegment2(prev_pt, pt); here we keep (prev_pt, pt-prev_pt). The
 * original endpoint lives in .p, direction in .v. */
typedef struct {
    vec2 p;
    vec2 v;
} sk_seg;

struct sk_lavertex;
struct sk_lav;
struct sk_slav;

typedef struct sk_lavertex {
    vec2    point;
    sk_seg  edge_left;
    sk_seg  edge_right;
    vec2    bisector_p;      /* bisector Ray2 origin (== point). */
    vec2    bisector_v;      /* bisector Ray2 direction. */
    int     is_reflex;
    int     valid;
    struct sk_lavertex *prev;
    struct sk_lavertex *next;
    struct sk_lav      *lav;
} sk_lavertex;

typedef struct sk_lav {
    sk_lavertex *head;
    uint32_t     len;
    struct sk_slav *slav;
    /* Track all vertices ever allocated through this lav so we can free. */
} sk_lav;

/* Original polygon edge, frozen at init time; carries the two bisectors
 * that bordered it — used by the split-event validation predicate. */
typedef struct {
    sk_seg edge;
    vec2   bisector_left_p, bisector_left_v;
    vec2   bisector_right_p, bisector_right_v;
} sk_orig_edge;

typedef struct sk_slav {
    sk_lav       **lavs;
    uint32_t       n_lavs;
    uint32_t       cap_lavs;
    sk_orig_edge  *original_edges;
    uint32_t       n_original_edges;
    /* Bookkeeping: every sk_lavertex ever allocated, so we can free all. */
    sk_lavertex  **all_verts;
    uint32_t       n_all_verts;
    uint32_t       cap_all_verts;
    /* Same for lavs. */
    sk_lav       **all_lavs;
    uint32_t       n_all_lavs;
    uint32_t       cap_all_lavs;
} sk_slav;

/* ==========================================================================
 * Subtree output + event types
 * ========================================================================== */

typedef struct {
    vec2   source;
    double height;
    vec2  *sinks;
    uint32_t n_sinks;
    uint32_t cap_sinks;
} sk_subtree;

typedef enum { SK_EVT_EDGE = 0, SK_EVT_SPLIT = 1 } sk_evt_kind;

typedef struct {
    sk_evt_kind kind;
    double      distance;
    vec2        intersection;
    /* For EDGE events: vertex_a / vertex_b are the two participating LAV
     * vertices. For SPLIT events: vertex is the reflex vertex and
     * opposite_edge is a sk_seg snapshot of the opposite edge. */
    sk_lavertex *va;    /* EDGE: vertex_a; SPLIT: the reflex vertex. */
    sk_lavertex *vb;    /* EDGE: vertex_b; unused for SPLIT. */
    sk_seg       opposite;  /* SPLIT only. */
} sk_event;

/* Min-heap keyed by event.distance. */
typedef struct {
    sk_event *data;
    uint32_t  n;
    uint32_t  cap;
} sk_pq;

/* ==========================================================================
 * Dynamic-array helpers
 * ========================================================================== */

static int dyn_grow(void **base, uint32_t *cap, uint32_t want,
                    size_t elem_size)
{
    if (want <= *cap) return 0;
    uint32_t newcap = *cap ? *cap * 2 : 16;
    while (newcap < want) newcap *= 2;
    void *n = realloc(*base, (size_t)newcap * elem_size);
    if (!n) return -1;
    *base = n;
    *cap  = newcap;
    return 0;
}

/* ==========================================================================
 * Priority queue (min-heap)
 * ========================================================================== */

static int pq_push(sk_pq *q, sk_event e)
{
    if (dyn_grow((void **)&q->data, &q->cap, q->n + 1, sizeof(sk_event)) != 0)
        return -1;
    uint32_t i = q->n++;
    q->data[i] = e;
    while (i > 0) {
        uint32_t p = (i - 1) / 2;
        if (q->data[p].distance <= q->data[i].distance) break;
        sk_event tmp = q->data[p];
        q->data[p] = q->data[i];
        q->data[i] = tmp;
        i = p;
    }
    return 0;
}

static sk_event pq_pop(sk_pq *q)
{
    sk_event top = q->data[0];
    --q->n;
    if (q->n > 0) {
        q->data[0] = q->data[q->n];
        uint32_t i = 0;
        for (;;) {
            uint32_t l = 2 * i + 1, r = 2 * i + 2, m = i;
            if (l < q->n && q->data[l].distance < q->data[m].distance) m = l;
            if (r < q->n && q->data[r].distance < q->data[m].distance) m = r;
            if (m == i) break;
            sk_event tmp = q->data[m];
            q->data[m] = q->data[i];
            q->data[i] = tmp;
            i = m;
        }
    }
    return top;
}

/* ==========================================================================
 * Polygon normalisation (drop collinear triples)
 *
 * Mirrors polyskel._normalize_contour: remove a vertex if it equals its
 * successor or if (prev->v == v->next) (collinear + same direction).
 * ========================================================================== */

static vec2 *normalize_contour(const vec2 *pts, uint32_t n, uint32_t *out_n)
{
    *out_n = 0;
    if (n < 3) return NULL;
    vec2 *out = (vec2 *)malloc(sizeof(vec2) * n);
    if (!out) return NULL;
    uint32_t m = 0;
    for (uint32_t i = 0; i < n; ++i) {
        vec2 prev = pts[(i + n - 1) % n];
        vec2 cur  = pts[i];
        vec2 nxt  = pts[(i + 1) % n];
        /* Drop if cur == nxt. */
        if (approx_same(cur, nxt)) continue;
        /* Drop if (cur - prev).norm() == (nxt - cur).norm(). */
        vec2 dprev = vec2_norm(vec2_sub(cur, prev));
        vec2 dnext = vec2_norm(vec2_sub(nxt, cur));
        if (approx_eq(dprev.x, dnext.x) && approx_eq(dprev.y, dnext.y)) continue;
        out[m++] = cur;
    }
    if (m < 3) { free(out); return NULL; }
    *out_n = m;
    return out;
}

/* ==========================================================================
 * SLAV construction
 * ========================================================================== */

static sk_lavertex *sk_vert_new(sk_slav *slav, vec2 point, sk_seg el, sk_seg er,
                                  const vec2 *dir_left, const vec2 *dir_right)
{
    sk_lavertex *v = (sk_lavertex *)calloc(1, sizeof(sk_lavertex));
    if (!v) return NULL;
    if (dyn_grow((void **)&slav->all_verts, &slav->cap_all_verts,
                  slav->n_all_verts + 1, sizeof(sk_lavertex *)) != 0) {
        free(v);
        return NULL;
    }
    slav->all_verts[slav->n_all_verts++] = v;

    v->point = point;
    v->edge_left = el;
    v->edge_right = er;
    v->valid = 1;

    /* creator vectors: (-edge_left.v.normalized(), edge_right.v.normalized()) */
    vec2 cl = vec2_neg(vec2_norm(el.v));
    vec2 cr =            vec2_norm(er.v);
    vec2 dl = dir_left  ? *dir_left  : cl;
    vec2 dr = dir_right ? *dir_right : cr;

    double crossd = vec2_cross(dl, dr);
    v->is_reflex = (crossd < 0.0);

    vec2 sumv = vec2_add(cl, cr);
    double sign = v->is_reflex ? -1.0 : 1.0;
    v->bisector_p = point;
    v->bisector_v = vec2_scl(sumv, sign);
    return v;
}

static sk_lav *sk_lav_new(sk_slav *slav)
{
    sk_lav *lav = (sk_lav *)calloc(1, sizeof(sk_lav));
    if (!lav) return NULL;
    if (dyn_grow((void **)&slav->all_lavs, &slav->cap_all_lavs,
                  slav->n_all_lavs + 1, sizeof(sk_lav *)) != 0) {
        free(lav);
        return NULL;
    }
    slav->all_lavs[slav->n_all_lavs++] = lav;
    lav->slav = slav;
    return lav;
}

static int sk_slav_push_lav(sk_slav *slav, sk_lav *lav)
{
    if (dyn_grow((void **)&slav->lavs, &slav->cap_lavs,
                  slav->n_lavs + 1, sizeof(sk_lav *)) != 0)
        return -1;
    slav->lavs[slav->n_lavs++] = lav;
    return 0;
}

static void sk_slav_remove_lav(sk_slav *slav, sk_lav *lav)
{
    for (uint32_t i = 0; i < slav->n_lavs; ++i) {
        if (slav->lavs[i] == lav) {
            for (uint32_t j = i + 1; j < slav->n_lavs; ++j)
                slav->lavs[j - 1] = slav->lavs[j];
            --slav->n_lavs;
            return;
        }
    }
}

/* Build an initial LAV from a normalized contour. */
static sk_lav *sk_lav_from_polygon(sk_slav *slav, const vec2 *pts, uint32_t n)
{
    sk_lav *lav = sk_lav_new(slav);
    if (!lav) return NULL;
    for (uint32_t i = 0; i < n; ++i) {
        vec2 prev = pts[(i + n - 1) % n];
        vec2 cur  = pts[i];
        vec2 nxt  = pts[(i + 1) % n];
        sk_seg el = { prev, vec2_sub(cur, prev) };
        sk_seg er = { cur,  vec2_sub(nxt, cur)  };
        sk_lavertex *v = sk_vert_new(slav, cur, el, er, NULL, NULL);
        if (!v) return NULL;
        v->lav = lav;
        ++lav->len;
        if (!lav->head) {
            lav->head = v;
            v->prev = v->next = v;
        } else {
            v->next = lav->head;
            v->prev = lav->head->prev;
            v->prev->next = v;
            lav->head->prev = v;
        }
    }
    return lav;
}

static sk_lav *sk_lav_from_chain(sk_slav *slav, sk_lavertex *head)
{
    sk_lav *lav = sk_lav_new(slav);
    if (!lav) return NULL;
    lav->head = head;
    sk_lavertex *cur = head;
    do {
        cur->lav = lav;
        ++lav->len;
        cur = cur->next;
    } while (cur != head);
    return lav;
}

static void sk_lav_invalidate(sk_lav *lav, sk_lavertex *v)
{
    v->valid = 0;
    if (lav->head == v) lav->head = lav->head->next;
    v->lav = NULL;
}

static void sk_vert_invalidate(sk_lavertex *v)
{
    if (v->lav) sk_lav_invalidate(v->lav, v);
    else v->valid = 0;
}

/* Unify vertex_a and vertex_b into a new replacement vertex at `point`.
 * Returns the replacement (added to slav->all_verts). */
static sk_lavertex *sk_lav_unify(sk_lav *lav, sk_lavertex *a, sk_lavertex *b, vec2 point)
{
    vec2 dl = vec2_norm(b->bisector_v);
    vec2 dr = vec2_norm(a->bisector_v);
    sk_lavertex *r = sk_vert_new(lav->slav, point, a->edge_left, b->edge_right, &dl, &dr);
    if (!r) return NULL;
    r->lav = lav;

    if (lav->head == a || lav->head == b) lav->head = r;

    a->prev->next = r;
    b->next->prev = r;
    r->prev = a->prev;
    r->next = b->next;

    sk_vert_invalidate(a);
    sk_vert_invalidate(b);

    --lav->len;  /* net effect: -2 then +1. Python does --self._len after
                  * invalidates; we collapsed the bookkeeping: a+b -> r. */
    return r;
}

/* ==========================================================================
 * Event computation — a vertex's next_event
 * ==========================================================================
 *
 * A (non-reflex) vertex can generate an edge event (with each of its two
 * neighbours). A reflex vertex additionally generates up to |E| split-event
 * candidates, one per original edge other than its own two. We pick the
 * minimum-distance event from the candidate set.
 *
 * Distance is MEASURED BY POINT-DISTANCE, NOT BY EVENT.distance. Polyskel
 * returns `min(events, key=lambda e: self.point.distance(e.intersection))`
 * — an easily-confused nuance. We mirror it.
 */

typedef struct {
    int       has;
    sk_event  event;
    double    pdist;        /* distance from self.point to intersection. */
} sk_cand;

static void cand_update(sk_cand *best, sk_event e, vec2 self_point)
{
    double d = vec2_dist(self_point, e.intersection);
    if (!best->has || d < best->pdist) {
        best->has = 1;
        best->event = e;
        best->pdist = d;
    }
}

static int sk_vertex_next_event(sk_slav *slav, sk_lavertex *self,
                                 sk_event *out_event)
{
    sk_cand best = {0};
    vec2 self_pt = self->point;

    if (self->is_reflex) {
        for (uint32_t ei = 0; ei < slav->n_original_edges; ++ei) {
            sk_orig_edge *oe = &slav->original_edges[ei];
            /* Skip the reflex vertex's own edges (identified by matching
             * start point AND direction — same as Python's `==`
             * comparison on LineSegment2 objects). */
            if (approx_same(oe->edge.p, self->edge_left.p) &&
                approx_eq(oe->edge.v.x, self->edge_left.v.x) &&
                approx_eq(oe->edge.v.y, self->edge_left.v.y))
                continue;
            if (approx_same(oe->edge.p, self->edge_right.p) &&
                approx_eq(oe->edge.v.x, self->edge_right.v.x) &&
                approx_eq(oe->edge.v.y, self->edge_right.v.y))
                continue;

            vec2 el_n = vec2_norm(self->edge_left.v);
            vec2 er_n = vec2_norm(self->edge_right.v);
            vec2 oe_n = vec2_norm(oe->edge.v);
            double leftdot  = fabs(vec2_dot(el_n, oe_n));
            double rightdot = fabs(vec2_dot(er_n, oe_n));
            sk_seg selfedge = (leftdot < rightdot) ? self->edge_left : self->edge_right;

            vec2 ipt;
            if (!line_line_intersect(selfedge.p, selfedge.v, oe->edge.p, oe->edge.v, &ipt))
                continue;
            if (approx_same(ipt, self->point)) continue;

            vec2 linvec = vec2_norm(vec2_sub(self->point, ipt));
            vec2 edvec  = vec2_norm(oe->edge.v);
            if (vec2_dot(linvec, edvec) < 0.0) edvec = vec2_neg(edvec);

            vec2 bisecvec = vec2_add(edvec, linvec);
            if (vec2_len(bisecvec) < 1e-15) continue;

            /* Line(ipt, bisecvec) vs self.bisector (Ray, treated as line). */
            vec2 b;
            if (!line_line_intersect(ipt, bisecvec, self->bisector_p, self->bisector_v, &b))
                continue;

            /* Validity predicates: b must lie inside the "wedge" spanned
             * by the opposite edge's two bisectors AND on the correct side
             * of the opposite edge. */
            vec2 bl_n = vec2_norm(oe->bisector_left_v);
            vec2 br_n = vec2_norm(oe->bisector_right_v);
            vec2 oe_n2 = vec2_norm(oe->edge.v);

            vec2 to_b_l = vec2_norm(vec2_sub(b, oe->bisector_left_p));
            vec2 to_b_r = vec2_norm(vec2_sub(b, oe->bisector_right_p));
            vec2 to_b_e = vec2_norm(vec2_sub(b, oe->edge.p));

            double xleft  = vec2_cross(bl_n, to_b_l);
            double xright = vec2_cross(br_n, to_b_r);
            double xedge  = vec2_cross(oe_n2, to_b_e);

            if (!(xleft > -SK_EPSILON && xright < SK_EPSILON && xedge < SK_EPSILON))
                continue;

            double dist = line_dist(oe->edge.p, oe->edge.v, b);
            sk_event ev = {0};
            ev.kind = SK_EVT_SPLIT;
            ev.distance = dist;
            ev.intersection = b;
            ev.va = self;
            ev.vb = NULL;
            ev.opposite = oe->edge;
            cand_update(&best, ev, self_pt);
        }
    }

    /* Edge-event candidates: bisector vs prev/next bisector. */
    vec2 i_prev, i_next;
    int have_prev = ray_ray_intersect(self->bisector_p, self->bisector_v,
                                       self->prev->bisector_p, self->prev->bisector_v,
                                       &i_prev);
    int have_next = ray_ray_intersect(self->bisector_p, self->bisector_v,
                                       self->next->bisector_p, self->next->bisector_v,
                                       &i_next);
    if (have_prev) {
        sk_event ev = {0};
        ev.kind = SK_EVT_EDGE;
        ev.distance = line_dist(self->edge_left.p, self->edge_left.v, i_prev);
        ev.intersection = i_prev;
        ev.va = self->prev;
        ev.vb = self;
        cand_update(&best, ev, self_pt);
    }
    if (have_next) {
        sk_event ev = {0};
        ev.kind = SK_EVT_EDGE;
        ev.distance = line_dist(self->edge_right.p, self->edge_right.v, i_next);
        ev.intersection = i_next;
        ev.va = self;
        ev.vb = self->next;
        cand_update(&best, ev, self_pt);
    }

    if (!best.has) return 0;
    *out_event = best.event;
    return 1;
}

/* ==========================================================================
 * Subtree output buffer
 * ========================================================================== */

typedef struct {
    sk_subtree *data;
    uint32_t    n;
    uint32_t    cap;
} sk_subtree_arr;

static int subtree_push(sk_subtree_arr *arr, sk_subtree st)
{
    if (dyn_grow((void **)&arr->data, &arr->cap, arr->n + 1, sizeof(sk_subtree)) != 0)
        return -1;
    arr->data[arr->n++] = st;
    return 0;
}

static int subtree_add_sink(sk_subtree *st, vec2 p)
{
    if (dyn_grow((void **)&st->sinks, &st->cap_sinks, st->n_sinks + 1, sizeof(vec2)) != 0)
        return -1;
    /* Dedup: the Python code doesn't dedup sinks in handle_* but does in
     * _merge_sources when merging sources. Here we inline a cheap dedup
     * so the face walker doesn't see repeated sinks. */
    for (uint32_t i = 0; i < st->n_sinks; ++i) {
        if (approx_same(st->sinks[i], p)) return 0;
    }
    st->sinks[st->n_sinks++] = p;
    return 0;
}

/* ==========================================================================
 * handle_edge_event / handle_split_event
 *
 * Each returns 0 on success (appending an optional subtree + generating
 * follow-up events), -1 on OOM. If no subtree is produced we leave
 * st.source = {0,0} and caller checks st.n_sinks==0 to skip.
 * ========================================================================== */

static int handle_edge_event(sk_slav *slav, const sk_event *ev,
                              sk_subtree_arr *out, sk_pq *pq)
{
    sk_lav *lav = ev->va->lav;
    sk_subtree st;
    st.source = ev->intersection;
    st.height = ev->distance;
    st.sinks = NULL; st.n_sinks = 0; st.cap_sinks = 0;

    if (ev->va->prev == ev->vb->next) {
        /* Triangle collapse: entire LAV disappears. */
        sk_slav_remove_lav(slav, lav);
        sk_lavertex *cur = lav->head;
        sk_lavertex *start = lav->head;
        if (cur) {
            do {
                if (subtree_add_sink(&st, cur->point) != 0) { free(st.sinks); return -1; }
                sk_lavertex *nxt = cur->next;
                sk_vert_invalidate(cur);
                cur = nxt;
            } while (cur != start && cur != NULL);
        }
    } else {
        sk_lavertex *nv = sk_lav_unify(lav, ev->va, ev->vb, ev->intersection);
        if (!nv) { free(st.sinks); return -1; }
        if (subtree_add_sink(&st, ev->va->point) != 0) { free(st.sinks); return -1; }
        if (subtree_add_sink(&st, ev->vb->point) != 0) { free(st.sinks); return -1; }
        sk_event next_ev;
        if (sk_vertex_next_event(slav, nv, &next_ev)) {
            if (pq_push(pq, next_ev) != 0) { free(st.sinks); return -1; }
        }
    }

    return subtree_push(out, st);
}

static int handle_split_event(sk_slav *slav, const sk_event *ev,
                               sk_subtree_arr *out, sk_pq *pq)
{
    sk_lav *orig_lav = ev->va->lav;
    sk_subtree st;
    st.source = ev->intersection;
    st.height = ev->distance;
    st.sinks = NULL; st.n_sinks = 0; st.cap_sinks = 0;
    if (subtree_add_sink(&st, ev->va->point) != 0) { free(st.sinks); return -1; }

    /* Find the vertices x, y that own the opposite edge in some current
     * LAV — identified by matching edge direction + starting point on
     * either the vertex's edge_left or edge_right. */
    sk_lavertex *x = NULL, *y = NULL;
    vec2 opp_n = vec2_norm(ev->opposite.v);

    for (uint32_t li = 0; li < slav->n_lavs && !x; ++li) {
        sk_lav *lv = slav->lavs[li];
        if (!lv->head) continue;
        sk_lavertex *cur = lv->head;
        sk_lavertex *start = lv->head;
        do {
            vec2 vl_n = vec2_norm(cur->edge_left.v);
            vec2 vr_n = vec2_norm(cur->edge_right.v);
            sk_lavertex *xc = NULL, *yc = NULL;

            if (approx_eq(opp_n.x, vl_n.x) && approx_eq(opp_n.y, vl_n.y) &&
                approx_same(ev->opposite.p, cur->edge_left.p)) {
                xc = cur; yc = cur->prev;
            } else if (approx_eq(opp_n.x, vr_n.x) && approx_eq(opp_n.y, vr_n.y) &&
                        approx_same(ev->opposite.p, cur->edge_right.p)) {
                yc = cur; xc = cur->next;
            }

            if (xc) {
                vec2 by_n = vec2_norm(yc->bisector_v);
                vec2 bx_n = vec2_norm(xc->bisector_v);
                vec2 to_i_y = vec2_norm(vec2_sub(ev->intersection, yc->point));
                vec2 to_i_x = vec2_norm(vec2_sub(ev->intersection, xc->point));
                double xleft  = vec2_cross(by_n, to_i_y);
                double xright = vec2_cross(bx_n, to_i_x);
                if (xleft >= -SK_EPSILON && xright <= SK_EPSILON) {
                    x = xc; y = yc;
                    break;
                }
            }
            cur = cur->next;
        } while (cur != start);
    }

    if (!x) {
        /* No matching edge: drop this event. Python: return (None, []). */
        free(st.sinks);
        return 0;
    }

    /* Build two new replacement vertices v1, v2 at the intersection. */
    sk_lavertex *va = ev->va;
    sk_seg opp_edge = ev->opposite;
    sk_lavertex *v1 = sk_vert_new(slav, ev->intersection, va->edge_left, opp_edge, NULL, NULL);
    sk_lavertex *v2 = sk_vert_new(slav, ev->intersection, opp_edge, va->edge_right, NULL, NULL);
    if (!v1 || !v2) { free(st.sinks); return -1; }

    v1->prev = va->prev;
    v1->next = x;
    va->prev->next = v1;
    x->prev = v1;

    v2->prev = y;
    v2->next = va->next;
    va->next->prev = v2;
    y->next = v2;

    /* Split into new LAV(s). */
    sk_slav_remove_lav(slav, orig_lav);
    sk_lav *new_lavs[2] = { NULL, NULL };
    uint32_t n_new = 0;
    if (orig_lav != x->lav) {
        sk_slav_remove_lav(slav, x->lav);
        new_lavs[n_new++] = sk_lav_from_chain(slav, v1);
    } else {
        new_lavs[n_new++] = sk_lav_from_chain(slav, v1);
        new_lavs[n_new++] = sk_lav_from_chain(slav, v2);
    }
    if (!new_lavs[0]) { free(st.sinks); return -1; }

    sk_lavertex *vertices_to_event[2] = { NULL, NULL };
    uint32_t n_vte = 0;

    for (uint32_t i = 0; i < n_new; ++i) {
        sk_lav *nl = new_lavs[i];
        if (!nl) continue;
        if (nl->len > 2) {
            if (sk_slav_push_lav(slav, nl) != 0) { free(st.sinks); return -1; }
            vertices_to_event[n_vte++] = nl->head;
        } else {
            /* 2-vertex LAV: the head's next is its only neighbor. */
            if (subtree_add_sink(&st, nl->head->next->point) != 0) { free(st.sinks); return -1; }
            sk_lavertex *cur = nl->head;
            sk_lavertex *start = cur;
            if (cur) {
                do {
                    sk_lavertex *nxt = cur->next;
                    sk_vert_invalidate(cur);
                    cur = nxt;
                } while (cur != start);
            }
        }
    }

    for (uint32_t i = 0; i < n_vte; ++i) {
        sk_event ne;
        if (sk_vertex_next_event(slav, vertices_to_event[i], &ne)) {
            if (pq_push(pq, ne) != 0) { free(st.sinks); return -1; }
        }
    }

    sk_vert_invalidate(va);

    return subtree_push(out, st);
}

/* ==========================================================================
 * Merge coincident subtree sources (polyskel._merge_sources)
 * ========================================================================== */

static void merge_sources(sk_subtree_arr *out)
{
    for (uint32_t i = 0; i < out->n; ++i) {
        for (uint32_t j = i + 1; j < out->n;) {
            if (approx_same(out->data[i].source, out->data[j].source)) {
                /* Merge j's sinks into i. */
                for (uint32_t k = 0; k < out->data[j].n_sinks; ++k) {
                    int present = 0;
                    vec2 p = out->data[j].sinks[k];
                    for (uint32_t m = 0; m < out->data[i].n_sinks; ++m) {
                        if (approx_same(out->data[i].sinks[m], p)) { present = 1; break; }
                    }
                    if (!present) {
                        if (dyn_grow((void **)&out->data[i].sinks,
                                       &out->data[i].cap_sinks,
                                       out->data[i].n_sinks + 1, sizeof(vec2)) == 0) {
                            out->data[i].sinks[out->data[i].n_sinks++] = p;
                        }
                    }
                }
                free(out->data[j].sinks);
                for (uint32_t m = j + 1; m < out->n; ++m) out->data[m - 1] = out->data[m];
                --out->n;
            } else {
                ++j;
            }
        }
    }
}

/* ==========================================================================
 * Free helpers
 * ========================================================================== */

static void sk_slav_free(sk_slav *slav)
{
    if (!slav) return;
    if (slav->all_verts) {
        for (uint32_t i = 0; i < slav->n_all_verts; ++i) free(slav->all_verts[i]);
        free(slav->all_verts);
    }
    if (slav->all_lavs) {
        for (uint32_t i = 0; i < slav->n_all_lavs; ++i) free(slav->all_lavs[i]);
        free(slav->all_lavs);
    }
    free(slav->lavs);
    free(slav->original_edges);
    free(slav);
}

static void subtree_arr_free(sk_subtree_arr *arr)
{
    if (!arr) return;
    for (uint32_t i = 0; i < arr->n; ++i) free(arr->data[i].sinks);
    free(arr->data);
}

/* ==========================================================================
 * skeletonize
 * ========================================================================== */

static int skeletonize(const vec2 *polygon, uint32_t n_polygon,
                        sk_subtree_arr *out)
{
    uint32_t nn;
    vec2 *contour = normalize_contour(polygon, n_polygon, &nn);
    if (!contour) return -1;

    sk_slav *slav = (sk_slav *)calloc(1, sizeof(sk_slav));
    if (!slav) { free(contour); return -1; }

    sk_lav *lav0 = sk_lav_from_polygon(slav, contour, nn);
    free(contour);
    if (!lav0) { sk_slav_free(slav); return -1; }
    if (sk_slav_push_lav(slav, lav0) != 0) { sk_slav_free(slav); return -1; }

    /* Freeze original edges. */
    slav->original_edges = (sk_orig_edge *)calloc(nn, sizeof(sk_orig_edge));
    if (!slav->original_edges) { sk_slav_free(slav); return -1; }
    slav->n_original_edges = nn;
    {
        sk_lavertex *cur = lav0->head;
        for (uint32_t i = 0; i < nn; ++i) {
            sk_orig_edge *oe = &slav->original_edges[i];
            sk_seg e = { cur->prev->point, vec2_sub(cur->point, cur->prev->point) };
            oe->edge = e;
            oe->bisector_left_p  = cur->prev->bisector_p;
            oe->bisector_left_v  = cur->prev->bisector_v;
            oe->bisector_right_p = cur->bisector_p;
            oe->bisector_right_v = cur->bisector_v;
            cur = cur->next;
        }
    }

    /* Seed event queue. */
    sk_pq pq = {0};
    {
        sk_lavertex *cur = lav0->head;
        for (uint32_t i = 0; i < nn; ++i) {
            sk_event e;
            if (sk_vertex_next_event(slav, cur, &e)) {
                if (pq_push(&pq, e) != 0) { free(pq.data); sk_slav_free(slav); return -1; }
            }
            cur = cur->next;
        }
    }

    /* Safety cap: prevent infinite loops on pathological input. */
    uint32_t max_iters = 32 + nn * 64;

    while (pq.n > 0 && slav->n_lavs > 0 && max_iters--) {
        sk_event ev = pq_pop(&pq);
        if (ev.kind == SK_EVT_EDGE) {
            if (!ev.va->valid || !ev.vb->valid) continue;
            if (handle_edge_event(slav, &ev, out, &pq) != 0) {
                free(pq.data); sk_slav_free(slav); return -1;
            }
        } else {
            if (!ev.va->valid) continue;
            if (handle_split_event(slav, &ev, out, &pq) != 0) {
                free(pq.data); sk_slav_free(slav); return -1;
            }
        }
    }

    free(pq.data);
    sk_slav_free(slav);
    merge_sources(out);
    return 0;
}

/* ==========================================================================
 * Face reconstruction
 *
 * Mirror of _skeleton._build_faces. Steps:
 *   1. Collect nodes: polygon vertices (t=0) + subtree sources (t=height).
 *      Dedup by a quantised key at FACE_EPS = 1e-3 m. If two nodes collide,
 *      keep the larger t.
 *   2. Build an undirected arc set: ring edges + subtree source<->sink arcs.
 *      For sinks not yet in the node set, add them at t=0.
 *   3. For each node, compute the angular-sorted adjacency list (CCW).
 *   4. For each directed polygon edge (CCW polygon => interior on the
 *      LEFT), walk the face: at each arrival node take the PREVIOUS entry
 *      in the adjacency (= one step CW from the reverse of incoming =
 *      leftmost turn, interior stays on left). Close when we return to
 *      the start edge.
 *
 * Returns 0 on success, -1 on OOM or a face that failed to close.
 * ========================================================================== */

#define FACE_EPS 1e-3

typedef struct {
    int64_t kx, ky;
} face_key;

static face_key face_key_make(vec2 p)
{
    face_key k;
    k.kx = (int64_t)floor(p.x / FACE_EPS + 0.5);
    k.ky = (int64_t)floor(p.y / FACE_EPS + 0.5);
    return k;
}

/* Linear-probed hash map from face_key -> vertex index. */
typedef struct {
    face_key *keys;
    int32_t  *vals;    /* -1 = empty. */
    uint32_t  cap;
    uint32_t  n;
} face_map;

static int fm_init(face_map *m, uint32_t cap)
{
    m->cap = cap;
    m->n = 0;
    m->keys = (face_key *)malloc(sizeof(face_key) * cap);
    m->vals = (int32_t *)malloc(sizeof(int32_t) * cap);
    if (!m->keys || !m->vals) return -1;
    for (uint32_t i = 0; i < cap; ++i) m->vals[i] = -1;
    return 0;
}

static void fm_free(face_map *m)
{
    free(m->keys); free(m->vals);
    m->keys = NULL; m->vals = NULL; m->cap = m->n = 0;
}

static uint32_t fm_hash(face_key k)
{
    uint64_t h = ((uint64_t)(k.kx) * 0x9E3779B97F4A7C15ull) ^
                   ((uint64_t)(k.ky) * 0xBF58476D1CE4E5B9ull);
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 27;
    return (uint32_t)h;
}

static int fm_get(const face_map *m, face_key k)
{
    uint32_t mask = m->cap - 1;
    uint32_t idx = fm_hash(k) & mask;
    for (uint32_t i = 0; i < m->cap; ++i) {
        uint32_t p = (idx + i) & mask;
        if (m->vals[p] < 0) return -1;
        if (m->keys[p].kx == k.kx && m->keys[p].ky == k.ky) return m->vals[p];
    }
    return -1;
}

static void fm_put(face_map *m, face_key k, int32_t v)
{
    uint32_t mask = m->cap - 1;
    uint32_t idx = fm_hash(k) & mask;
    for (uint32_t i = 0; i < m->cap; ++i) {
        uint32_t p = (idx + i) & mask;
        if (m->vals[p] < 0) {
            m->keys[p] = k;
            m->vals[p] = v;
            ++m->n;
            return;
        }
        if (m->keys[p].kx == k.kx && m->keys[p].ky == k.ky) {
            m->vals[p] = v;
            return;
        }
    }
}

/* Adjacency list entry. */
typedef struct {
    uint32_t nbr;
    double   ang;
} adj_entry;

typedef struct {
    adj_entry *entries;
    uint32_t   n;
    uint32_t   cap;
} adj_list;

/* Next pow2 >= x (min 16). */
static uint32_t next_pow2(uint32_t x)
{
    uint32_t r = 16;
    while (r < x) r <<= 1;
    return r;
}

static int cmp_adj(const void *a, const void *b)
{
    const adj_entry *pa = (const adj_entry *)a;
    const adj_entry *pb = (const adj_entry *)b;
    if (pa->ang < pb->ang) return -1;
    if (pa->ang > pb->ang) return  1;
    return 0;
}

typedef struct {
    uint32_t a, b;   /* a < b. */
} arc_pair;

static int build_faces_from_subtrees(
    const vec2 *footprint, uint32_t n_fp,
    const sk_subtree_arr *subs,
    osmmesh_skeleton *out)
{
    /* ---- Step 1: collect nodes. ---- */
    /* Worst case size: n_fp + n_sub_sources + n_sub_sinks. */
    uint32_t worst = n_fp;
    for (uint32_t i = 0; i < subs->n; ++i) worst += 1 + subs->data[i].n_sinks;

    uint32_t cap = next_pow2(worst * 2 + 4);
    face_map fm;
    if (fm_init(&fm, cap) != 0) return -1;

    osmmesh_skeleton_vertex *verts =
        (osmmesh_skeleton_vertex *)malloc(sizeof(osmmesh_skeleton_vertex) * worst);
    if (!verts) { fm_free(&fm); return -1; }
    uint32_t nv = 0;

    /* Helper: add node, return index. If existing, keep larger t. */
    #define ADD_NODE(PX, PY, T) do { \
        vec2 _pp = {(PX),(PY)}; \
        face_key _kk = face_key_make(_pp); \
        int _ex = fm_get(&fm, _kk); \
        if (_ex < 0) { \
            verts[nv].xy[0] = (float)(PX); \
            verts[nv].xy[1] = (float)(PY); \
            verts[nv].t = (float)(T); \
            fm_put(&fm, _kk, (int32_t)nv); \
            ++nv; \
        } else { \
            if ((float)(T) > verts[_ex].t) verts[_ex].t = (float)(T); \
        } \
    } while (0)

    for (uint32_t i = 0; i < n_fp; ++i) {
        ADD_NODE(footprint[i].x, footprint[i].y, 0.0);
    }
    for (uint32_t i = 0; i < subs->n; ++i) {
        const sk_subtree *st = &subs->data[i];
        ADD_NODE(st->source.x, st->source.y, st->height);
    }

    /* Polygon vertex indices (in footprint order). */
    uint32_t *poly_idx = (uint32_t *)malloc(sizeof(uint32_t) * n_fp);
    if (!poly_idx) { free(verts); fm_free(&fm); return -1; }
    for (uint32_t i = 0; i < n_fp; ++i) {
        int pi = fm_get(&fm, face_key_make(footprint[i]));
        if (pi < 0) { free(poly_idx); free(verts); fm_free(&fm); return -1; }
        poly_idx[i] = (uint32_t)pi;
    }

    /* ---- Step 2: collect arcs (undirected, unique). ---- */
    uint32_t arc_cap = 16;
    uint32_t arc_n = 0;
    arc_pair *arcs = (arc_pair *)malloc(sizeof(arc_pair) * arc_cap);
    if (!arcs) { free(poly_idx); free(verts); fm_free(&fm); return -1; }

    #define ADD_ARC(IA, IB) do { \
        if ((IA) != (IB)) { \
            uint32_t _a = (IA) < (IB) ? (IA) : (IB); \
            uint32_t _b = (IA) < (IB) ? (IB) : (IA); \
            int _dup = 0; \
            for (uint32_t _k = 0; _k < arc_n; ++_k) { \
                if (arcs[_k].a == _a && arcs[_k].b == _b) { _dup = 1; break; } \
            } \
            if (!_dup) { \
                if (dyn_grow((void**)&arcs, &arc_cap, arc_n + 1, sizeof(arc_pair)) != 0) { \
                    free(arcs); free(poly_idx); free(verts); fm_free(&fm); return -1; \
                } \
                arcs[arc_n].a = _a; arcs[arc_n].b = _b; ++arc_n; \
            } \
        } \
    } while (0)

    for (uint32_t i = 0; i < n_fp; ++i) {
        ADD_ARC(poly_idx[i], poly_idx[(i + 1) % n_fp]);
    }
    for (uint32_t i = 0; i < subs->n; ++i) {
        const sk_subtree *st = &subs->data[i];
        int si = fm_get(&fm, face_key_make(st->source));
        if (si < 0) continue;
        for (uint32_t k = 0; k < st->n_sinks; ++k) {
            int ki = fm_get(&fm, face_key_make(st->sinks[k]));
            if (ki < 0) {
                /* Unknown sink — add as a fresh node with t=0 (same as
                 * Python). We must grow verts[] if we're already at cap. */
                if (nv >= worst) {
                    /* Grow verts + leave map intact. */
                    uint32_t new_cap = worst * 2;
                    osmmesh_skeleton_vertex *nv2 =
                        (osmmesh_skeleton_vertex *)realloc(verts,
                            sizeof(osmmesh_skeleton_vertex) * new_cap);
                    if (!nv2) {
                        free(arcs); free(poly_idx); free(verts); fm_free(&fm);
                        return -1;
                    }
                    verts = nv2;
                    worst = new_cap;
                }
                verts[nv].xy[0] = (float)st->sinks[k].x;
                verts[nv].xy[1] = (float)st->sinks[k].y;
                verts[nv].t = 0.0f;
                fm_put(&fm, face_key_make(st->sinks[k]), (int32_t)nv);
                ki = (int32_t)nv;
                ++nv;
            }
            ADD_ARC((uint32_t)si, (uint32_t)ki);
        }
    }

    /* ---- Step 3: adjacency with sorted angular order. ---- */
    adj_list *adj = (adj_list *)calloc(nv, sizeof(adj_list));
    if (!adj) {
        free(arcs); free(poly_idx); free(verts); fm_free(&fm); return -1;
    }

    for (uint32_t i = 0; i < arc_n; ++i) {
        uint32_t a = arcs[i].a, b = arcs[i].b;
        double dx = verts[b].xy[0] - verts[a].xy[0];
        double dy = verts[b].xy[1] - verts[a].xy[1];
        double ang_ab = atan2(dy, dx);
        double ang_ba = atan2(-dy, -dx);
        if (dyn_grow((void **)&adj[a].entries, &adj[a].cap, adj[a].n + 1, sizeof(adj_entry)) != 0) goto oom_adj;
        adj[a].entries[adj[a].n].nbr = b;
        adj[a].entries[adj[a].n].ang = ang_ab;
        ++adj[a].n;
        if (dyn_grow((void **)&adj[b].entries, &adj[b].cap, adj[b].n + 1, sizeof(adj_entry)) != 0) goto oom_adj;
        adj[b].entries[adj[b].n].nbr = a;
        adj[b].entries[adj[b].n].ang = ang_ba;
        ++adj[b].n;
    }
    for (uint32_t i = 0; i < nv; ++i) {
        qsort(adj[i].entries, adj[i].n, sizeof(adj_entry), cmp_adj);
    }

    /* ---- Step 4: face walk. One face per directed polygon edge. ---- */
    osmmesh_skeleton_face *faces =
        (osmmesh_skeleton_face *)calloc(n_fp, sizeof(osmmesh_skeleton_face));
    if (!faces) goto oom_adj;

    uint32_t n_faces = 0;
    for (uint32_t ei = 0; ei < n_fp; ++ei) {
        uint32_t v0 = poly_idx[ei];
        uint32_t v1 = poly_idx[(ei + 1) % n_fp];
        uint32_t face_cap = 8;
        uint32_t *face_buf = (uint32_t *)malloc(sizeof(uint32_t) * face_cap);
        if (!face_buf) goto oom_faces;
        uint32_t face_n = 0;
        face_buf[face_n++] = v0;
        face_buf[face_n++] = v1;

        uint32_t prev = v0, cur = v1;
        int closed = 0;
        uint32_t hard_cap = nv * 2 + 8;
        for (uint32_t step = 0; step < hard_cap; ++step) {
            /* Find prev in adj[cur], pick entry (idx-1) modulo len. */
            const adj_list *al = &adj[cur];
            int found = -1;
            for (uint32_t k = 0; k < al->n; ++k) {
                if (al->entries[k].nbr == prev) { found = (int)k; break; }
            }
            if (found < 0) break;
            uint32_t nxt = al->entries[(found + (int)al->n - 1) % (int)al->n].nbr;
            if (nxt == v0 && cur == face_buf[face_n - 1]) {
                closed = 1;
                break;
            }
            if (dyn_grow((void **)&face_buf, &face_cap, face_n + 1, sizeof(uint32_t)) != 0) {
                free(face_buf); goto oom_faces;
            }
            face_buf[face_n++] = nxt;
            prev = cur;
            cur = nxt;
        }
        if (!closed) {
            free(face_buf);
            /* A failed face breaks the whole skeleton — polyskel output
             * is inconsistent. Clean up and bail. */
            for (uint32_t k = 0; k < n_faces; ++k) free(faces[k].indices);
            free(faces);
            goto oom_adj;
        }
        faces[n_faces].indices = face_buf;
        faces[n_faces].n_indices = face_n;
        ++n_faces;
    }

    /* ---- Finish: compact verts, fill out. ---- */
    float tmax = 0.0f;
    for (uint32_t i = 0; i < nv; ++i) if (verts[i].t > tmax) tmax = verts[i].t;

    osmmesh_skeleton_vertex *shrunk =
        (osmmesh_skeleton_vertex *)realloc(verts,
            sizeof(osmmesh_skeleton_vertex) * (nv ? nv : 1));
    out->vertices = shrunk ? shrunk : verts;
    out->n_vertices = nv;
    out->faces = faces;
    out->n_faces = n_faces;
    out->t_max = tmax;

    for (uint32_t i = 0; i < nv; ++i) free(adj[i].entries);
    free(adj);
    free(arcs);
    free(poly_idx);
    fm_free(&fm);
    return 0;

oom_faces:
    for (uint32_t k = 0; k < n_faces; ++k) free(faces[k].indices);
    free(faces);
oom_adj:
    for (uint32_t i = 0; i < nv; ++i) free(adj[i].entries);
    free(adj);
    free(arcs);
    free(poly_idx);
    free(verts);
    fm_free(&fm);
    return -1;

    #undef ADD_NODE
    #undef ADD_ARC
}

/* ==========================================================================
 * Public API
 * ==========================================================================
 *
 * Pre-center input at centroid for numerical stability (mirror of
 * _skeleton.compute). polyskel assumes y-axis DOWN; we flip y on the way
 * in and back on the way out.
 */

osmmesh_skeleton *osmmesh_skeleton_compute(const float *xy, uint32_t n)
{
    if (!xy || n < 3) return NULL;

    /* Centroid. */
    double cx = 0.0, cy = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        cx += xy[i * 2 + 0];
        cy += xy[i * 2 + 1];
    }
    cx /= n; cy /= n;

    /* Centered + y-flipped input for polyskel. */
    vec2 *poly_in = (vec2 *)malloc(sizeof(vec2) * n);
    if (!poly_in) return NULL;
    for (uint32_t i = 0; i < n; ++i) {
        poly_in[i].x =  xy[i * 2 + 0] - cx;
        poly_in[i].y = -(xy[i * 2 + 1] - cy);
    }

    sk_subtree_arr subs = {0};
    int rc = skeletonize(poly_in, n, &subs);
    free(poly_in);
    if (rc != 0 || subs.n == 0) {
        subtree_arr_free(&subs);
        return NULL;
    }

    /* Unflip subtree y for face reconstruction. */
    for (uint32_t i = 0; i < subs.n; ++i) {
        subs.data[i].source.y = -subs.data[i].source.y;
        for (uint32_t k = 0; k < subs.data[i].n_sinks; ++k) {
            subs.data[i].sinks[k].y = -subs.data[i].sinks[k].y;
        }
    }

    /* Centered footprint for face reconstruction. */
    vec2 *fp_centered = (vec2 *)malloc(sizeof(vec2) * n);
    if (!fp_centered) { subtree_arr_free(&subs); return NULL; }
    for (uint32_t i = 0; i < n; ++i) {
        fp_centered[i].x = xy[i * 2 + 0] - cx;
        fp_centered[i].y = xy[i * 2 + 1] - cy;
    }

    osmmesh_skeleton *sk = (osmmesh_skeleton *)calloc(1, sizeof(osmmesh_skeleton));
    if (!sk) { free(fp_centered); subtree_arr_free(&subs); return NULL; }

    if (build_faces_from_subtrees(fp_centered, n, &subs, sk) != 0) {
        free(fp_centered);
        subtree_arr_free(&subs);
        free(sk);
        return NULL;
    }

    free(fp_centered);
    subtree_arr_free(&subs);

    /* Translate back to world frame. */
    for (uint32_t i = 0; i < sk->n_vertices; ++i) {
        sk->vertices[i].xy[0] += (float)cx;
        sk->vertices[i].xy[1] += (float)cy;
    }

    /* Post-merge near-duplicate vertices. Felkel's event loop sometimes
     * produces two ridge endpoints for a near-square hipped roof that
     * should collapse to a single apex; they land ~1 mm apart because
     * of floating-point drift and then defeat the face-graph EPS merge.
     * Downstream tessellation (and test_087's quantisation) treats them
     * as separate vertices and produces 4-way edges where the two
     * near-duplicates' shared-face boundaries overlap.
     *
     * Merge anything within 5 mm: every vertex gets remapped to the
     * lowest-index representative, face index lists are rewritten, then
     * a linear sweep compacts the vertex array. */
    {
        const float MERGE_TOL_SQ = 2.5e-5f;   /* (5 mm)^2 */
        uint32_t nv = sk->n_vertices;
        int32_t *remap = (int32_t *)malloc(sizeof(int32_t) * nv);
        if (!remap) return sk;
        for (uint32_t i = 0; i < nv; ++i) remap[i] = (int32_t)i;

        /* O(n^2) pairwise merge — nv is typically small (< 64). */
        for (uint32_t i = 0; i < nv; ++i) {
            if (remap[i] != (int32_t)i) continue;  /* already merged into earlier */
            for (uint32_t j = i + 1; j < nv; ++j) {
                if (remap[j] != (int32_t)j) continue;
                float dx = sk->vertices[i].xy[0] - sk->vertices[j].xy[0];
                float dy = sk->vertices[i].xy[1] - sk->vertices[j].xy[1];
                if (dx * dx + dy * dy < MERGE_TOL_SQ) {
                    remap[j] = (int32_t)i;
                    if (sk->vertices[j].t > sk->vertices[i].t) {
                        sk->vertices[i].t = sk->vertices[j].t;
                    }
                }
            }
        }

        /* Compact: build new_idx mapping and copy surviving verts. */
        int32_t *new_idx = (int32_t *)malloc(sizeof(int32_t) * nv);
        if (!new_idx) { free(remap); return sk; }
        uint32_t nv_compact = 0;
        for (uint32_t i = 0; i < nv; ++i) {
            if (remap[i] == (int32_t)i) {
                new_idx[i] = (int32_t)nv_compact;
                if (nv_compact != i) sk->vertices[nv_compact] = sk->vertices[i];
                ++nv_compact;
            } else {
                new_idx[i] = -1;  /* filled below */
            }
        }
        for (uint32_t i = 0; i < nv; ++i) {
            if (new_idx[i] < 0) new_idx[i] = new_idx[remap[i]];
        }
        /* Rewrite face indices, drop consecutive duplicates. */
        for (uint32_t fi = 0; fi < sk->n_faces; ++fi) {
            osmmesh_skeleton_face *f = &sk->faces[fi];
            uint32_t out_n = 0;
            for (uint32_t k = 0; k < f->n_indices; ++k) {
                uint32_t new_v = (uint32_t)new_idx[f->indices[k]];
                if (out_n > 0 && f->indices[out_n - 1] == new_v) continue;
                f->indices[out_n++] = new_v;
            }
            /* Also drop final-equals-first (closed-ring duplicate). */
            if (out_n >= 2 && f->indices[out_n - 1] == f->indices[0]) --out_n;
            f->n_indices = out_n;
        }
        sk->n_vertices = nv_compact;
        free(remap);
        free(new_idx);

        /* Recompute t_max. */
        float tmax = 0.0f;
        for (uint32_t i = 0; i < sk->n_vertices; ++i)
            if (sk->vertices[i].t > tmax) tmax = sk->vertices[i].t;
        sk->t_max = tmax;
    }

    return sk;
}

void osmmesh_skeleton_free(osmmesh_skeleton *sk)
{
    if (!sk) return;
    if (sk->faces) {
        for (uint32_t i = 0; i < sk->n_faces; ++i) free(sk->faces[i].indices);
        free(sk->faces);
    }
    free(sk->vertices);
    free(sk);
}
