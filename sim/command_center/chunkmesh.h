/* FlightBox renderer — the geometry of ONE terrain chunk, as pure maths. No GL.
 *
 * Split out of world3d.h for the same reason as gfx/mat4.h: this is the part of the terrain
 * pipeline that needs no GL context, so it can be asserted directly instead of being judged by
 * looking at pixels. That matters more here than anywhere else in the renderer, because
 * w3_chunk_build produces `err` -- the measured geometric error that the ENTIRE level-of-detail
 * runs on (SSE = err * K / distance). A wrong err does not crash and does not look obviously
 * broken: it silently refines the wrong ground, everywhere, forever. test/unit/run.sh used to
 * carry the honest excuse for that gap -- "command_center/world3d.h needs a GL context" -- which
 * was true only because the maths was glued to three glBufferData lines.
 *
 * The caller owns the result and uploads it: build here, glBufferData there.
 */
#ifndef W3_CHUNKMESH_H
#define W3_CHUNKMESH_H
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <osmmesh/mesh.h>

/* The terrain vertex layout, owned by the code that WRITES it.
 *
 * The writer and the reader (glVertexAttribPointer) used to agree only by hand: literal stride 32
 * and offsets 0/12/20 spelled out at the draw call. When normals were added the stride went
 * 20 -> 32 and every one of those numbers had to change together; getting one wrong does not
 * error, it renders garbage. Derive them from the struct instead, and pin the result so a layout
 * change breaks the build rather than the picture. */
typedef struct { float pos[3]; float uv[2]; float norm[3]; } w3_vtx;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(w3_vtx) == 8*sizeof(float), "terrain vertex must be tightly packed (no padding)");
_Static_assert(offsetof(w3_vtx, pos)  == 0,  "aPos offset");
_Static_assert(offsetof(w3_vtx, uv)   == 12, "aUV offset");
_Static_assert(offsetof(w3_vtx, norm) == 20, "aNorm offset");
#endif
#define W3_VTX_FLOATS ((int)(sizeof(w3_vtx)/sizeof(float)))

/* One built chunk. `verts` is nverts * w3_vtx, malloc'd -- release with w3_chunk_free. */
typedef struct {
  float *verts;
  int    nverts;
  float  err;      /* max |drawn surface - source height| in METRES; drives the LOD.
                    * 0 for the irregular-mesh fallback: nothing was decimated there, so there is
                    * no decimation error, so that chunk never wants to split. */
} w3_chunk;

static void w3_chunk_free(w3_chunk *c){ if(!c) return; free(c->verts); c->verts=0; c->nverts=0; c->err=0.f; }

/* Build one chunk's vertices and MEASURE its geometric error while doing it.
 *
 * `grid` = quads per side to decimate to (the renderer passes W3_TERR). It is a parameter, not a
 * macro, so this file knows nothing about the renderer's configuration -- and so a test can build
 * at grid=2, the resolution at which the error is analytically checkable by hand.
 *
 * err = max |decimated surface - source height| over every SOURCE pixel, in metres. It costs
 * nothing extra: the decimation already walks the whole height field, so the comparison rides
 * along.
 *
 * It has to be measured per chunk, not tabulated per zoom. An error ladder measured over Hameln
 * says z8 = 234 m -- but Hameln has 96 m of total relief, so the ladder saturates and encodes this
 * one flat valley as a global constant. The same measurement at the Matterhorn gives z8 = 1396 m
 * and at Everest 2093 m. fb-tiles serves any origin on earth; a constant from one valley would be
 * wrong everywhere else. From the data, flat terrain saturates by itself and mountains subdivide
 * by themselves -- same line of code, both right.
 *
 * Returns 1 on success, 0 on failure (out is zeroed). Source positions are ENU metres
 * (east, north, up); the output is render space (east, up, -north). */
static int w3_chunk_build(const osmmesh_mesh *m, int grid, w3_chunk *out){
  if(!out) return 0;
  out->verts=0; out->nverts=0; out->err=0.f;
  if(!m || !m->positions || m->n_vertices==0) return 0;

  float emin=1e18f,emax=-1e18f,nmin=1e18f,nmax=-1e18f;
  for(uint32_t i=0;i<m->n_vertices;i++){ const float*P=m->positions+i*3;
    if(P[0]<emin)emin=P[0]; if(P[0]>emax)emax=P[0]; if(P[1]<nmin)nmin=P[1]; if(P[1]>nmax)nmax=P[1]; }
  float de=emax-emin?emax-emin:1, dn=nmax-nmin?nmax-nmin:1;

  /* detect grid width C (row-major, north->south rows; x resets west at each row start) */
  uint32_t C=0;
  for(uint32_t i=1;i<m->n_vertices;i++) if(m->positions[i*3] < m->positions[(i-1)*3]-0.5f){ C=i; break; }
  uint32_t R = C ? m->n_vertices/C : 0;
  /* R>=2 as well as C>=2: a single row is not a height field, and W3_RI would divide by gr-1==0.
   * The old code only checked C and reached that division; no real tile is one row high, but
   * "cannot happen" is not a bound, and the soup path below handles it correctly anyway. */
  if(C>=2 && R>=2 && m->n_vertices%C==0){                 /* regular grid -> decimate to grid x grid */
    int G=grid<2?2:grid;
    int gc=(int)C<G+1?(int)C:G+1, gr=(int)R<G+1?(int)R:G+1;
    #define W3_MV(r,c) (m->positions + ((size_t)(r)*C + (size_t)(c))*3)
    #define W3_RI(j)   ((int)((long)(j)*(R-1)/(gr-1)))
    #define W3_CI(i)   ((int)((long)(i)*(C-1)/(gc-1)))
    /* SMOOTH per-vertex normals from the HEIGHT FIELD (central differences over the
     * decimated neighbours, so they match the geometry we actually draw). Face normals
     * would be constant across each triangle -> the fragment shader's per-pixel lighting
     * would still come out flat-shaded/faceted. Interpolating these makes vNorm vary
     * across the triangle -> true smooth per-pixel shading on the slopes. */
    int NN=gr*gc;
    float*np=malloc((size_t)NN*3*sizeof(float));   /* node position (east,north,elev) */
    float*nv=malloc((size_t)NN*3*sizeof(float));   /* node normal (render space) */
    if(!np||!nv){ free(np); free(nv); return 0; }
    for(int j=0;j<gr;j++)for(int i=0;i<gc;i++){
      const float*P=W3_MV(W3_RI(j),W3_CI(i)); float*d=np+((size_t)j*gc+i)*3;
      d[0]=P[0]; d[1]=P[1]; d[2]=P[2];
    }
    for(int j=0;j<gr;j++)for(int i=0;i<gc;i++){
      int i0=i>0?i-1:i, i1=i<gc-1?i+1:i, j0=j>0?j-1:j, j1=j<gr-1?j+1:j;
      const float*W=np+((size_t)j*gc+i0)*3, *E=np+((size_t)j*gc+i1)*3;
      const float*Nn=np+((size_t)j0*gc+i)*3,*Sn=np+((size_t)j1*gc+i)*3;
      float dE=E[0]-W[0], dN=Sn[1]-Nn[1];
      float dzde=(fabsf(dE)>1e-6f)?(E[2]-W[2])/dE:0.f;   /* d(elev)/d(east)  */
      float dzdn=(fabsf(dN)>1e-6f)?(Sn[2]-Nn[2])/dN:0.f; /* d(elev)/d(north) */
      /* ENU normal = (-dz/de, -dz/dn, 1); render axes are E=+X, up=+Y, N=-Z */
      float nx=-dzde, ny=1.f, nz=dzdn;
      float L=sqrtf(nx*nx+ny*ny+nz*nz); if(L<1e-6f)L=1;
      float*d=nv+((size_t)j*gc+i)*3; d[0]=nx/L; d[1]=ny/L; d[2]=nz/L;
    }
    /* --- the chunk's own geometric error, from the data ---------------------------------
     * Walk every SOURCE pixel, evaluate the surface we are about to draw at that spot (same two
     * triangles per quad as below), and keep the worst vertical miss. 256x256 lerps per chunk --
     * nothing next to the PNG decode that produced the heights.
     * Every source pixel, not just the kept nodes: at a kept node the drawn surface passes exactly
     * through the data by construction, so an error measured there is always 0 and would be a
     * number that agrees with itself. The error lives precisely in what the decimation THREW AWAY. */
    float err=0.f;
    for(int j=0;j<gr-1;j++)for(int i=0;i<gc-1;i++){
      int r0=W3_RI(j), r1=W3_RI(j+1), c0=W3_CI(i), c1=W3_CI(i+1);
      float h00=np[((size_t)j*gc+i)*3+2],     h10=np[((size_t)j*gc+i+1)*3+2];
      float h11=np[((size_t)(j+1)*gc+i+1)*3+2], h01=np[((size_t)(j+1)*gc+i)*3+2];
      if(r1<=r0||c1<=c0) continue;
      for(int r=r0;r<=r1;r++)for(int c=c0;c<=c1;c++){
        float sv=(float)(r-r0)/(float)(r1-r0), su=(float)(c-c0)/(float)(c1-c0);
        /* same diagonal as the quad below: (0,0)-(1,1) */
        float h = (su>=sv) ? h00+(h10-h00)*su+(h11-h10)*sv
                           : h00+(h11-h01)*su+(h01-h00)*sv;
        float d=fabsf(h-W3_MV(r,c)[2]); if(d>err) err=d;
      }
    }
    out->err=err;
    /* --- skirt height, also from the data ------------------------------------------------
     * Neighbouring chunks may differ by one level, and their edges do NOT weld: each level
     * decimates its own DEM, so the shared edge is two different polylines. Measured over the
     * Weser valley the gap reaches 1.4 m between z13/z14 but 47.6 m between z11/z12 -- a constant
     * skirt would be waste in one place and a visible crack in the other.
     * The crack at a shared edge is bounded by the two chunks' errors, and the finer chunk's is
     * the smaller, so 2*err is a sound bound that needs no magic number and travels to any
     * terrain on earth. The skirt hangs straight down and is hidden by the neighbour. */
    float skirt=2.f*err; if(skirt<5.f) skirt=5.f;
    int nquad=(gr-1)*(gc-1), nedge=2*((gr-1)+(gc-1));
    float*v=malloc(((size_t)nquad+nedge)*6*(size_t)W3_VTX_FLOATS*sizeof(float)); size_t o=0;
    if(!v){ free(np); free(nv); return 0; }
    for(int j=0;j<gr-1;j++)for(int i=0;i<gc-1;i++){
      int q[6]={ j*gc+i, j*gc+(i+1), (j+1)*gc+(i+1), j*gc+i, (j+1)*gc+(i+1), (j+1)*gc+i };
      for(int k=0;k<6;k++){
        const float*P=np+(size_t)q[k]*3, *N=nv+(size_t)q[k]*3;
        v[o++]=P[0]; v[o++]=P[2]; v[o++]=-P[1];                 /* pos: east, up, -north */
        v[o++]=(P[0]-emin)/de; v[o++]=(nmax-P[1])/dn;           /* uv: u=0 west, v=0 NORTH */
        v[o++]=N[0]; v[o++]=N[1]; v[o++]=N[2];                  /* smooth normal */
      }
    }
    /* skirt: a curtain hanging from each boundary edge, textured with the edge's own texels so
     * it reads as a continuation of the ground rather than a stripe. */
    #define W3_SKIRT_EDGE(a,b) do{ \
      const float*A=np+(size_t)(a)*3, *B=np+(size_t)(b)*3; \
      float au=(A[0]-emin)/de, av=(nmax-A[1])/dn, bu=(B[0]-emin)/de, bv=(nmax-B[1])/dn; \
      const float NA[3]={0.f,1.f,0.f}; \
      float P6[6][3]={{A[0],A[2],-A[1]},{B[0],B[2],-B[1]},{B[0],B[2]-skirt,-B[1]}, \
                      {A[0],A[2],-A[1]},{B[0],B[2]-skirt,-B[1]},{A[0],A[2]-skirt,-A[1]}}; \
      float U6[6][2]={{au,av},{bu,bv},{bu,bv},{au,av},{bu,bv},{au,av}}; \
      for(int k=0;k<6;k++){ v[o++]=P6[k][0]; v[o++]=P6[k][1]; v[o++]=P6[k][2]; \
        v[o++]=U6[k][0]; v[o++]=U6[k][1]; v[o++]=NA[0]; v[o++]=NA[1]; v[o++]=NA[2]; } \
    }while(0)
    for(int i=0;i<gc-1;i++){ W3_SKIRT_EDGE(i+1, i);                                  }  /* north */
    for(int i=0;i<gc-1;i++){ W3_SKIRT_EDGE((gr-1)*gc+i, (gr-1)*gc+i+1);              }  /* south */
    for(int j=0;j<gr-1;j++){ W3_SKIRT_EDGE(j*gc, (j+1)*gc);                          }  /* west  */
    for(int j=0;j<gr-1;j++){ W3_SKIRT_EDGE((j+1)*gc+gc-1, j*gc+gc-1);                }  /* east  */
    #undef W3_SKIRT_EDGE
    free(np); free(nv);
    #undef W3_MV
    #undef W3_RI
    #undef W3_CI
    out->verts=v; out->nverts=(int)(o/(size_t)W3_VTX_FLOATS);
    return 1;
  }

  /* fallback: full-resolution soup (irregular mesh), with a per-triangle FACE normal.
   * Flat shading is the honest answer here: there is no height grid to take smooth normals from. */
  uint32_t nt=m->n_triangles;
  if(nt==0) return 0;
  int nv3=(int)(nt*3);
  float*w=malloc((size_t)nv3*(size_t)W3_VTX_FLOATS*sizeof(float));
  if(!w) return 0;
  for(int t=0;t+2<nv3;t+=3){
    const float*P[3];
    for(int k=0;k<3;k++){ uint32_t idx=m->indices?m->indices[t+k]:(uint32_t)(t+k); P[k]=m->positions+(size_t)idx*3; }
    /* render space (east, up, -north) BEFORE the cross product, so the normal comes out in the
     * same space as the positions it belongs to. */
    float a[3]={P[0][0],P[0][2],-P[0][1]}, b[3]={P[1][0],P[1][2],-P[1][1]}, c[3]={P[2][0],P[2][2],-P[2][1]};
    float e1[3]={b[0]-a[0],b[1]-a[1],b[2]-a[2]}, e2[3]={c[0]-a[0],c[1]-a[1],c[2]-a[2]};
    float nx=e1[1]*e2[2]-e1[2]*e2[1], ny=e1[2]*e2[0]-e1[0]*e2[2], nz=e1[0]*e2[1]-e1[1]*e2[0];
    float L=sqrtf(nx*nx+ny*ny+nz*nz); if(L<1e-6f)L=1; nx/=L;ny/=L;nz/=L;
    if(ny<0){nx=-nx;ny=-ny;nz=-nz;}                       /* orient upward */
    for(int k=0;k<3;k++){
      const float*S=P[k]; float*d=w+(size_t)(t+k)*(size_t)W3_VTX_FLOATS;
      d[0]=S[0]; d[1]=S[2]; d[2]=-S[1];
      d[3]=(S[0]-emin)/de; d[4]=(nmax-S[1])/dn;
      d[5]=nx; d[6]=ny; d[7]=nz;
    }
  }
  out->verts=w; out->nverts=nv3; out->err=0.f;
  return 1;
}

#endif /* W3_CHUNKMESH_H */
