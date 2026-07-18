/* Unit tests — command_center/chunkmesh_ecef.h  (the worker's ECEF terrain mesh)
 *
 * The ECEF builder is what the global camera-relative renderer draws. Its output cannot be
 * eyeballed any more than `err` could: a wrong offset floats the tile, a wrong origin puts the
 * world in the wrong place, an inward normal lights the ground from underneath -- none crash, none
 * look obviously broken. So the contract is pinned here by construction:
 *
 *   - origin sits on the ellipsoid (geocentric radius), offsets stay SMALL (the precision point);
 *   - origin + offset reconstructs the vertex's true ECEF to sub-cm (the floating-origin guarantee);
 *   - normals are unit and point OUTWARD (radial dot > 0) -- the winding/lighting orientation;
 *   - err is IDENTICAL to the ENU builder on the same height field (a projection-independent
 *     property -- the falsifier for "the ECEF projection changed the LOD").
 */
#include "tassert.h"
#include <stdlib.h>
#include <math.h>
#include "../../command_center/chunkmesh_ecef.h"
#include "osmmesh/geo.h"

/* Same source grid as test_chunkmesh: row-major, north->south, east increasing within a row (the
 * layout osmmesh delivers and the detector keys on). Only P[0] (C-detection) and P[2] (height) are
 * read by the ECEF builder; P[1] is filler. */
#define STEP 100.0f
static void grid_make(osmmesh_mesh *m, int C, int R, float (*h)(float,float)){
    m->positions=(float*)malloc((size_t)C*R*3*sizeof(float));
    m->normals=0; m->uvs=0; m->indices=0;
    m->n_vertices=(uint32_t)(C*R); m->n_triangles=0;
    for(int r=0;r<R;r++)for(int c=0;c<C;c++){
        float e=c*STEP, n=-(float)r*STEP;
        float*P=m->positions+((size_t)r*C+c)*3; P[0]=e; P[1]=n; P[2]=h(e,n);
    }
}
static void grid_free(osmmesh_mesh *m){ free(m->positions); m->positions=0; }
static float h_flat(float e,float n){ (void)e;(void)n; return 0.f; }
static float h_tilt(float e,float n){ return 0.3f*e - 0.2f*n; }
static float h_bump(float e,float n){ (void)n; return 40.f*sinf(e*0.002f); }   /* varies across the field */

/* A Hameln-ish z14 tile (the osmmesh header's own example), lat ~52 N. */
enum { TZ=14, TX=8619, TY=5408 };

void test_chunkmesh_ecef(void){
    tsection("chunkmesh_ecef: origin on the ellipsoid, offsets small, reconstruction sub-cm");
    {
        osmmesh_mesh m; grid_make(&m,17,17,h_tilt);
        w3_chunk c; double origin[3];
        ck(w3_chunk_build_ecef(&m,TZ,TX,TY,8,&c,origin)==1,"tilted field builds");

        double olen=sqrt(origin[0]*origin[0]+origin[1]*origin[1]+origin[2]*origin[2]);
        ck(olen>6.30e6 && olen<6.40e6,"origin is on the ellipsoid (geocentric radius ~6.37e6 m)");

        /* Every offset stays well under a tile (the whole reason float holds them). */
        float maxoff=0.f;
        for(int i=0;i<c.nverts;i++){ const w3_vtx*v=&c.verts[i];
            float d=sqrtf(v->pos[0]*v->pos[0]+v->pos[1]*v->pos[1]+v->pos[2]*v->pos[2]);
            if(d>maxoff)maxoff=d; }
        ck(maxoff<3000.f,"all vertex offsets < 3 km (float keeps them to sub-cm)");

        /* Vertex 0 is the NW corner at frac (0,0): origin + offset must reconstruct its true ECEF. */
        osmmesh_geo g0=osmmesh_tile_frac_to_geo(TZ,TX,TY,0.0,0.0); g0.alt=h_tilt(0,0);
        osmmesh_ecef e0=osmmesh_geo_to_ecef(g0);
        const w3_vtx*v0=&c.verts[0];
        double rx=origin[0]+v0->pos[0], ry=origin[1]+v0->pos[1], rz=origin[2]+v0->pos[2];
        double err=sqrt((rx-e0.x)*(rx-e0.x)+(ry-e0.y)*(ry-e0.y)+(rz-e0.z)*(rz-e0.z));
        ck(err<0.02,"origin + offset reconstructs the vertex ECEF to < 2 cm (floating origin)");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh_ecef: normals are unit and point OUTWARD (radial dot > 0)");
    {
        osmmesh_mesh m; grid_make(&m,17,17,h_bump);
        w3_chunk c; double origin[3];
        ck(w3_chunk_build_ecef(&m,TZ,TX,TY,8,&c,origin)==1,"bumped field builds");
        double olen=sqrt(origin[0]*origin[0]+origin[1]*origin[1]+origin[2]*origin[2]);
        float rad[3]={(float)(origin[0]/olen),(float)(origin[1]/olen),(float)(origin[2]/olen)};
        int all_unit=1, all_out=1;
        for(int i=0;i<c.nverts;i++){ const w3_vtx*v=&c.verts[i];
            float L=sqrtf(v->norm[0]*v->norm[0]+v->norm[1]*v->norm[1]+v->norm[2]*v->norm[2]);
            if(fabsf(L-1.f)>1e-3f) all_unit=0;
            if(v->norm[0]*rad[0]+v->norm[1]*rad[1]+v->norm[2]*rad[2] <= 0.f) all_out=0; }
        ck(all_unit,"every normal is unit length");
        ck(all_out,"every normal points outward (dot with radial > 0) -- the winding is right");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh_ecef: a varying field gives DIFFERENT normals across one triangle (smooth)");
    {
        /* The faceting regression: face normals give all three vertices of a triangle the SAME
         * normal. Smooth normals from the height field must differ. */
        osmmesh_mesh m; grid_make(&m,17,17,h_bump);
        w3_chunk c; double origin[3];
        w3_chunk_build_ecef(&m,TZ,TX,TY,8,&c,origin);
        const w3_vtx*a=&c.verts[0], *b=&c.verts[1], *d=&c.verts[2];
        int differ = (a->norm[0]!=b->norm[0])||(a->norm[1]!=b->norm[1])||(a->norm[2]!=b->norm[2])
                   ||(a->norm[0]!=d->norm[0])||(a->norm[1]!=d->norm[1])||(a->norm[2]!=d->norm[2]);
        ck(differ,"a curved field: the triangle's three normals are not identical (not per-face)");
        w3_chunk_free(&c); grid_free(&m);
    }

    tsection("chunkmesh_ecef: err is the height-field decimation miss (projection-independent)");
    {
        /* err = max |decimated surface - source height|, a purely VERTICAL quantity: the ECEF
         * rotation cannot change it, so the LOD behaves the same globally as it did flat. A flat
         * field decimates exactly (err == 0); a bumpy one does not (err > 0). Same invariant the
         * ENU builder is pinned on in test_chunkmesh, checked here on the ECEF path. */
        osmmesh_mesh mf; grid_make(&mf,33,33,h_flat);
        w3_chunk cf; double of[3];
        ck(w3_chunk_build_ecef(&mf,TZ,TX,TY,8,&cf,of)==1,"flat builds");
        ck_near(cf.err,0.f,1e-3,"flat field: err is exactly 0 (decimates exactly)");
        w3_chunk_free(&cf); grid_free(&mf);

        osmmesh_mesh mb; grid_make(&mb,33,33,h_bump);
        w3_chunk cb; double ob[3];
        ck(w3_chunk_build_ecef(&mb,TZ,TX,TY,8,&cb,ob)==1,"bump builds");
        ck(cb.err>0.f,"a bumpy field has a nonzero decimation error");
        w3_chunk_free(&cb); grid_free(&mb);

        /* THE falsifier for "walk every SOURCE pixel": a 5x5 grid, grid=2 keeps rows/cols 0,2,4 and
         * throws away 1,3. A lone spike at the SKIPPED (1,1) must show up as err exactly equal to it
         * -- an implementation that measured only at kept nodes would report 0 here. Same decimation
         * (W3_RI/W3_CI) as the ENU builder, so the same case pins it. */
        osmmesh_mesh ms; grid_make(&ms,5,5,h_flat);
        const float SPIKE=37.0f; ms.positions[((size_t)1*5+1)*3+2]=SPIKE;
        w3_chunk cs; double os[3];
        ck(w3_chunk_build_ecef(&ms,TZ,TX,TY,2,&cs,os)==1,"spike grid builds");
        ck_near(cs.err,SPIKE,1e-3,"err == the sag at the SKIPPED pixel (not 0: it is not a kept node)");
        w3_chunk_free(&cs); grid_free(&ms);

        /* The mirror: the spike moved onto a KEPT node (2,2). The drawn surface passes through it, so
         * it contributes no error of its own -- err must NOT be 37 (that would mean a kept node was
         * scored as discarded). */
        osmmesh_mesh mn; grid_make(&mn,5,5,h_flat);
        mn.positions[((size_t)2*5+2)*3+2]=37.0f;
        w3_chunk cn; double on[3];
        ck(w3_chunk_build_ecef(&mn,TZ,TX,TY,2,&cn,on)==1,"node-spike grid builds");
        ck(cn.err<37.0f-1e-3f,"a kept node contributes no error of its own");
        w3_chunk_free(&cn); grid_free(&mn);
    }

    tsection("chunkmesh_ecef: uv orientation — u=0 west, v=0 north; refuses non-grid soup");
    {
        osmmesh_mesh m; grid_make(&m,17,17,h_flat);
        w3_chunk c; double origin[3];
        w3_chunk_build_ecef(&m,TZ,TX,TY,8,&c,origin);
        /* Vertex 0 = NW corner (frac 0,0) -> uv (0,0). */
        ck_near(c.verts[0].uv[0],0.f,1e-4,"NW corner u = 0 (west)");
        ck_near(c.verts[0].uv[1],0.f,1e-4,"NW corner v = 0 (north)");
        w3_chunk_free(&c); grid_free(&m);

        /* An irregular soup (no grid) is refused, not crashed into -- terrain is always a grid. */
        osmmesh_mesh soup; soup.positions=(float*)malloc(9*sizeof(float));
        soup.n_vertices=3; soup.n_triangles=1; soup.normals=0; soup.uvs=0; soup.indices=0;
        for(int i=0;i<9;i++) soup.positions[i]=(float)i;       /* east never resets -> C stays 0 */
        w3_chunk sc; double so[3];
        ck(w3_chunk_build_ecef(&soup,TZ,TX,TY,8,&sc,so)==0,"non-grid input is refused (returns 0)");
        free(soup.positions);
    }
}
