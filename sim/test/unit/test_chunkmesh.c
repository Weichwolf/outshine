/* Unit tests — command_center/chunkmesh.h
 *
 * These exist for ONE number: `err`, the measured geometric error of a chunk. The entire terrain
 * LOD is SSE = err * K / distance, so err decides, for every chunk on screen, whether to split.
 * It cannot be eyeballed: a wrong err does not crash and does not look broken -- it silently
 * refines the wrong ground. Until chunkmesh.h was split out of world3d.h, the maths was welded to
 * three glBufferData calls and the only available oracle was a human looking at pixels.
 *
 * The cases are chosen so that each one FAILS for a specific, plausible implementation:
 *   - tilted plane  -> a plane decimates exactly; err must be 0. Catches a sign or axis slip that
 *                      a FLAT plane hides (a flat plane is symmetric under most mistakes).
 *   - skipped spike -> err must equal the sag at a source point the decimation THREW AWAY. This
 *                      is the falsifier for "walk every SOURCE pixel": an implementation that
 *                      compares only at the kept nodes returns 0 here, because the drawn surface
 *                      passes exactly through those by construction.
 *   - curved field  -> the three vertices of one triangle must carry THREE DIFFERENT normals.
 *                      Face normals give three identical ones -- the "per-pixel is not smooth"
 *                      faceting regression, which per-pixel lighting cannot fix.
 */
#include "tassert.h"
#include <stdlib.h>
#include <math.h>
#include "../../command_center/chunkmesh.h"

/* Build a C x R source grid, row-major, rows running NORTH -> SOUTH, east increasing within a row
 * -- the layout osmmesh's terrain mesh delivers and that the grid detection keys on. h(e,n) gives
 * the elevation. Step is 100 m: the detector needs the east reset at a row start to exceed 0.5 m. */
#define STEP 100.0f
static void grid_make(osmmesh_mesh *m, int C, int R, float (*h)(float,float)){
    m->positions = (float*)malloc((size_t)C*R*3*sizeof(float));
    m->normals = 0; m->uvs = 0; m->indices = 0;
    m->n_vertices = (uint32_t)(C*R); m->n_triangles = 0;
    for(int r=0;r<R;r++) for(int c=0;c<C;c++){
        float e = c*STEP, n = -(float)r*STEP;      /* north DECREASES as r grows */
        float *P = m->positions + ((size_t)r*C + c)*3;
        P[0]=e; P[1]=n; P[2]=h(e,n);
    }
}
static void grid_free(osmmesh_mesh *m){ free(m->positions); m->positions=0; }

static float h_flat(float e, float n){ (void)e; (void)n; return 0.f; }
/* dz/de = 0.3, dz/dn = -0.2 */
static float h_tilt(float e, float n){ return 0.3f*e - 0.2f*n; }
/* dz/de = 0.02*e -- varies across the field, so neighbouring nodes cannot share a normal */
static float h_curve(float e, float n){ (void)n; return 0.01f*e*e; }

/* No cast: w3_chunk.verts IS w3_vtx*. It used to be a float* with the layout in a comment, and
 * the cast this helper needed was the tell -- a type that has to be re-asserted at the reader is
 * a type that was not doing its job. */
static const w3_vtx *vtx(const w3_chunk *c, int i){ return &c->verts[i]; }

void test_chunkmesh(void){
    tsection("chunkmesh: tilted plane decimates exactly (err == 0, analytic normal)");
    {
        /* A plane is reproduced exactly by the bilinear/two-triangle interpolation, at ANY
         * decimation. So err must be 0 -- not 'small'. Anything else means the drawn surface does
         * not pass through the data it was built from. */
        osmmesh_mesh m; grid_make(&m, 9, 9, h_tilt);
        w3_chunk c;
        ck(w3_chunk_build(&m, 4, &c) == 1, "tilted plane builds");
        ck_near(c.err, 0.0, 1e-3, "tilted plane: err is exactly 0 (a plane decimates exactly)");

        /* ENU normal (-dz/de, -dz/dn, 1) in render axes (E=+X, up=+Y, N=-Z) -> (-dz/de, 1, dz/dn).
         * With dz/de=0.3, dz/dn=-0.2: (-0.3, 1, -0.2)/|.| */
        float L = sqrtf(0.3f*0.3f + 1.f + 0.2f*0.2f);
        const w3_vtx *v = vtx(&c, 0);
        ck_near(v->norm[0], -0.3f/L, 1e-4, "plane normal x = -dz/de");
        ck_near(v->norm[1],  1.0f/L, 1e-4, "plane normal y = up");
        ck_near(v->norm[2], -0.2f/L, 1e-4, "plane normal z = +dz/dn (N=-Z)");
        ck_near(sqrtf(v->norm[0]*v->norm[0]+v->norm[1]*v->norm[1]+v->norm[2]*v->norm[2]),
                1.0, 1e-4, "normal is unit length");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: err is the sag at a SKIPPED source pixel");
    {
        /* 5x5 source, grid=2 -> the decimation keeps columns/rows 0,2,4 and throws away 1,3.
         * Everything is flat except one spike at source (r=1,c=1), which is NOT a kept node.
         * The drawn quad predicts (h00+h11)/2 = 0 there, so err must be exactly the spike.
         *
         * THIS is the test that matters: an implementation measuring err only at the kept nodes
         * -- a very natural thing to write -- reports 0 here and this fails. The comment in
         * chunkmesh.h claims 'walk every SOURCE pixel'; this is what makes that claim falsifiable. */
        osmmesh_mesh m; grid_make(&m, 5, 5, h_flat);
        const float SPIKE = 37.0f;
        m.positions[((size_t)1*5 + 1)*3 + 2] = SPIKE;
        w3_chunk c;
        ck(w3_chunk_build(&m, 2, &c) == 1, "spike grid builds");
        ck_near(c.err, SPIKE, 1e-3, "err == the sag at the skipped pixel (not 0: it is not a node)");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: err ignores a spike that IS a kept node");
    {
        /* Same spike, moved onto (2,2) -- a node the decimation keeps. The drawn surface passes
         * through it exactly, so it contributes NO error. This is the mirror of the case above:
         * together they pin that err measures what was discarded, not what was kept. */
        osmmesh_mesh m; grid_make(&m, 5, 5, h_flat);
        m.positions[((size_t)2*5 + 2)*3 + 2] = 37.0f;
        w3_chunk c;
        ck(w3_chunk_build(&m, 2, &c) == 1, "node-spike grid builds");
        /* Its neighbours are still flat, so the surface tents up to it and back down; the worst
         * miss is at the pixels around it, never at the node itself. What must NOT happen is
         * err == 37 (that would mean the node was treated as discarded). */
        ck(c.err < 37.0f - 1e-3f, "a kept node contributes no error of its own");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: normals are smooth (from the height field), not per-face");
    {
        /* Hard-won lesson: the lighting maths ran per-pixel, but each triangle got ONE face normal
         * copied to all three vertices, so vNorm was constant across it and the ground looked
         * faceted anyway. Per-pixel evaluation cannot fix a constant input. On a curved field the
         * three corners of a triangle sit at different slopes, so their normals MUST differ. */
        osmmesh_mesh m; grid_make(&m, 9, 9, h_curve);
        w3_chunk c;
        ck(w3_chunk_build(&m, 4, &c) == 1, "curved field builds");
        const w3_vtx *a = vtx(&c,0), *b = vtx(&c,1), *d = vtx(&c,2);
        int all_same = (fabsf(a->norm[0]-b->norm[0]) < 1e-6f) && (fabsf(a->norm[0]-d->norm[0]) < 1e-6f);
        ck(!all_same, "one triangle carries DIFFERENT normals per vertex (not a face normal)");
        ck(fabsf(a->norm[0]-b->norm[0]) > 1e-4f, "adjacent vertices differ measurably in slope");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: render axes and UV orientation");
    {
        /* Silent-mirror class: a flipped V or a wrong sign on north does not error, it renders a
         * mirrored world. The source NW corner (east=emin, north=nmax) must land at uv (0,0). */
        osmmesh_mesh m; grid_make(&m, 5, 5, h_tilt);
        w3_chunk c;
        ck(w3_chunk_build(&m, 4, &c) == 1, "axis grid builds");
        /* vertex 0 is quad (j=0,i=0) corner (0,0) = the NW source node */
        const w3_vtx *v = vtx(&c, 0);
        ck_near(v->pos[0], 0.0,  1e-3, "pos.x = east");
        ck_near(v->pos[1], h_tilt(0.f, 0.f), 1e-3, "pos.y = up (elevation)");
        ck_near(v->pos[2], 0.0,  1e-3, "pos.z = -north (north=0 at the NW corner here)");
        ck_near(v->uv[0], 0.0, 1e-4, "u = 0 at the WEST edge");
        ck_near(v->uv[1], 0.0, 1e-4, "v = 0 at the NORTH edge");
        /* The SE-most node: east=emax -> u=1, north=nmin -> v=1. It is the last quad's (1,1). */
        int found = 0;
        for(int i=0;i<c.nverts;i++){
            const w3_vtx *p = vtx(&c,i);
            if(p->uv[0] > 0.999f && p->uv[1] > 0.999f){
                ck_near(p->pos[0], 4*STEP, 1e-2, "u=1 lands at the east edge");
                ck_near(p->pos[2], 4*STEP, 1e-2, "v=1 lands at the south edge (pos.z=-north)");
                found = 1; break;
            }
        }
        ck(found, "the SE corner exists at uv (1,1)");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: skirt count and depth");
    {
        osmmesh_mesh m; grid_make(&m, 5, 5, h_flat);
        m.positions[((size_t)1*5 + 1)*3 + 2] = 10.0f;      /* err = 10 -> skirt = 2*err = 20 */
        w3_chunk c;
        ck(w3_chunk_build(&m, 2, &c) == 1, "skirt grid builds");
        ck_near(c.err, 10.0, 1e-3, "err = 10 for this field");
        /* gr = gc = 3 (grid=2 -> G+1=3 nodes per side) */
        int gr = 3, gc = 3;
        int nquad = (gr-1)*(gc-1), nedge = 2*((gr-1)+(gc-1));
        ck(c.nverts == (nquad + nedge)*6, "nverts = 6 * (quads + skirt edges)");
        /* The deepest skirt vertex hangs 2*err below the lowest ground vertex it hangs from.
         * The ground here is at 0 except the spike, so the skirt bottom must reach -20. */
        float lowest = 1e9f;
        for(int i=0;i<c.nverts;i++){ const w3_vtx *p = vtx(&c,i); if(p->pos[1] < lowest) lowest = p->pos[1]; }
        ck_near(lowest, -20.0, 1e-3, "skirt hangs 2*err (=20 m) below the edge, not a magic constant");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: skirt depth has a floor on flat ground");
    {
        /* err = 0 on a plane -> 2*err = 0 would give a zero-height curtain and a visible crack.
         * The floor is 5 m. */
        osmmesh_mesh m; grid_make(&m, 5, 5, h_flat);
        w3_chunk c;
        ck(w3_chunk_build(&m, 2, &c) == 1, "flat grid builds");
        ck_near(c.err, 0.0, 1e-6, "flat: err = 0");
        float lowest = 1e9f;
        for(int i=0;i<c.nverts;i++){ const w3_vtx *p = vtx(&c,i); if(p->pos[1] < lowest) lowest = p->pos[1]; }
        ck_near(lowest, -5.0, 1e-3, "skirt floors at 5 m when 2*err would be 0");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: irregular mesh -> soup fallback, err = 0");
    {
        /* No detectable grid (east never resets) -> full-resolution soup. Nothing is decimated, so
         * there IS no decimation error and the chunk must never want to split. Pinning err=0 here
         * pins that reasoning, which is otherwise invisible. */
        osmmesh_mesh m;
        static const float tri[9] = { 0,0,0,  100,0,10,  100,-100,0 };   /* e,n,u each */
        m.positions = (float*)malloc(sizeof tri); memcpy(m.positions, tri, sizeof tri);
        m.normals=0; m.uvs=0; m.indices=0; m.n_vertices=3; m.n_triangles=1;
        w3_chunk c;
        ck(w3_chunk_build(&m, 4, &c) == 1, "irregular mesh builds");
        ck(c.nverts == 3, "soup keeps full resolution (3 verts for 1 triangle)");
        ck_near(c.err, 0.0, 1e-6, "soup: err = 0 (nothing was decimated, so it never splits)");
        /* face normal, and it must point UP */
        ck(vtx(&c,0)->norm[1] > 0.f, "face normal is oriented upward");
        const w3_vtx *a = vtx(&c,0), *b = vtx(&c,1);
        ck(fabsf(a->norm[0]-b->norm[0]) < 1e-6f && fabsf(a->norm[1]-b->norm[1]) < 1e-6f,
           "soup is FLAT shaded: one face normal shared by the triangle's vertices");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: grid detection and clamping");
    {
        /* Non-square: 4 wide, 3 high. Detection keys on the east reset at a row start. */
        osmmesh_mesh m; grid_make(&m, 4, 3, h_tilt);
        w3_chunk c;
        ck(w3_chunk_build(&m, 8, &c) == 1, "4x3 grid builds");
        /* grid=8 asks for 9 nodes/side but the source has only 4x3 -> clamp to the source. */
        int gc = 4, gr = 3;
        int nquad = (gr-1)*(gc-1), nedge = 2*((gr-1)+(gc-1));
        ck(c.nverts == (nquad + nedge)*6, "a grid finer than the source clamps to the source");
        ck_near(c.err, 0.0, 1e-3, "clamped plane still decimates exactly");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh: degenerate input is refused, not crashed into");
    {
        w3_chunk c;
        osmmesh_mesh z = {0};
        ck(w3_chunk_build(&z, 4, &c) == 0, "empty mesh -> 0, not a crash");
        ck(c.verts == 0 && c.nverts == 0, "failure leaves the output zeroed");
        ck(w3_chunk_build(0, 4, &c) == 0, "NULL mesh -> 0");
        /* One row is not a height field. The old code checked only the column count and reached a
         * division by (gr-1) == 0 here. */
        osmmesh_mesh m; grid_make(&m, 5, 1, h_flat);
        ck(w3_chunk_build(&m, 4, &c) == 0 || c.nverts > 0, "single row does not divide by zero");
        w3_chunk_free(&c); grid_free(&m);
    }
}
