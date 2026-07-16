/* FlightBox — shared 3D world + HUD renderer (GLES2/WebGL).
 * Included by cc.c, the WASM command center — the only consumer. There used to be a second,
 * native renderer (render_native.c) that included this too "so both draw the same thing"; the
 * two drifted anyway (it compiled W3_TERR=24/W3_FARTEX=256 while the browser shipped 22/512).
 * The regression check is now a headless browser screenshot of the real app (test/shot.sh), so
 * there is one renderer and nothing to keep in sync. Caller provides the GL context. */
#ifndef WORLD3D_H
#define WORLD3D_H
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <GLES2/gl2.h>
#include "protocol.h"
#ifdef W3_USE_OSM
#include "camera.h"      /* attitude -> basis + MVP, pure */
#include "stars.h"       /* catalogue coords + time + place -> direction, pure */
#include "atmo.h"        /* the frame's sun/sky/light, pure — shared by sky, terrain and buildings */
#include "tilesrc_js.h"   /* tile bytes from fb-tiles (async cache + osmmesh provider) */
#endif
#ifndef W3_FOV
#define W3_FOV 80.0f   /* vertical FOV (deg). Wide FPV cam (Caddx Ratel 2 ~164° diag ->
                        * ~80° vertical / ~112° horizontal at 16:9). */
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ~45 brightest stars: right ascension (deg), declination (deg), visual magnitude.
 * Rendered at their TRUE alt/az for the origin + wall-clock time, so the real
 * constellations (Orion, Big Dipper, Cassiopeia, ...) appear where they actually are. */
typedef struct { float ra, dec, mag; } w3_star;
static const w3_star W3_STARS[] = {
 {101.29f,-16.72f,-1.46f},{95.99f,-52.70f,-0.74f},{219.90f,-60.83f,-0.27f},{213.92f,19.18f,-0.05f},
 {279.23f,38.78f,0.03f},{79.17f,46.00f,0.08f},{78.63f,-8.20f,0.13f},{114.83f,5.22f,0.34f},
 {88.79f,7.41f,0.50f},{24.43f,-57.24f,0.46f},{210.96f,-60.37f,0.61f},{297.70f,8.87f,0.77f},
 {186.65f,-63.10f,0.77f},{68.98f,16.51f,0.85f},{201.30f,-11.16f,0.98f},{247.35f,-26.43f,1.09f},
 {116.33f,28.03f,1.14f},{344.41f,-29.62f,1.16f},{310.36f,45.28f,1.25f},{191.93f,-59.69f,1.25f},
 {152.09f,11.97f,1.35f},{104.66f,-28.97f,1.50f},{113.65f,31.89f,1.58f},{263.40f,-37.10f,1.62f},
 {81.28f,6.35f,1.64f},{81.57f,28.61f,1.65f},{84.05f,-1.20f,1.69f},{85.19f,-1.94f,1.77f},
 {165.93f,61.75f,1.79f},{51.08f,49.86f,1.79f},{107.10f,-26.39f,1.83f},{276.04f,-34.38f,1.85f},
 {206.89f,49.31f,1.86f},{89.88f,44.95f,1.90f},{99.43f,16.40f,1.93f},{37.95f,89.26f,1.98f},
 {141.90f,-8.66f,1.98f},{154.99f,19.84f,2.08f},{31.79f,23.46f,2.00f},{17.43f,35.62f,2.07f},
 {2.10f,29.09f,2.06f},{283.82f,-26.30f,2.05f},{211.67f,-36.37f,2.06f},{306.41f,-56.74f,1.94f}
};
#define W3_NSTARS ((int)(sizeof(W3_STARS)/sizeof(W3_STARS[0])))
static double w3_olat=52.045, w3_olon=9.385;   /* origin, set on osmmesh open; drives star alt/az */

/* mat4 / vec3 maths: GL-free, so it lives in gfx/mat4.h and is unit-tested directly. */
/* stb_image: declarations only -- osmmesh/src/terrain.c owns the one implementation we link. */
#include "../geo/osmmesh/src/3rdparty/stb_image.h"
#include "gfx/mat4.h"
/* The main thread needs only the vertex LAYOUT (to set glVertexAttribPointer and size the VBO) --
 * the mesh BUILD (w3_chunk_build) runs in the tile worker now, so including chunkmesh.h here would
 * be a -Wunused-function error. The worker includes chunkmesh.h; both share chunkvtx.h. */
#include "chunkvtx.h"

/* ---- GL program helpers ---- */
static GLuint w3_shader(GLenum t,const char*src){ GLuint s=glCreateShader(t); glShaderSource(s,1,&src,0); glCompileShader(s);
  GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok); if(!ok){ char b[512]; glGetShaderInfoLog(s,512,0,b); printf("shader err: %s\n",b);} return s; }
static GLuint w3_prog(const char*vs,const char*fs){ GLuint p=glCreateProgram();
  glAttachShader(p,w3_shader(GL_VERTEX_SHADER,vs)); glAttachShader(p,w3_shader(GL_FRAGMENT_SHADER,fs)); glLinkProgram(p); return p; }

/* uHaze = horizon/fog colour for the current time of day (set from sun elevation +
 * cloud on the CPU) so distant geometry fades into the real sky, day or night. */
static const char*W3_VSW=
 "attribute vec3 aPos; attribute vec3 aCol; uniform mat4 uMVP; varying vec3 vCol; varying float vFog;"
 "void main(){ vec4 p=uMVP*vec4(aPos,1.0); gl_Position=p; vCol=aCol; vFog=clamp(p.z/6000.0,0.0,1.0); }";
static const char*W3_FSW=
 "precision mediump float; varying vec3 vCol; varying float vFog; uniform vec3 uHaze; uniform float uLight;"
 "void main(){ gl_FragColor=vec4(mix(vCol*uLight,uHaze,vFog*0.7),1.0); }";
/* textured terrain: OSM landcover/roads/buildings baked to a per-tile ortho texture */
static const char*W3_VSWT=
 "attribute vec3 aPos; attribute vec2 aUV; attribute vec3 aNorm; uniform mat4 uMVP;"
 "varying vec2 vUV; varying float vFog; varying vec3 vNorm;"
 "void main(){ vec4 p=uMVP*vec4(aPos,1.0); gl_Position=p; vUV=aUV; vNorm=aNorm; vFog=clamp((p.w-3000.0)/28000.0,0.0,1.0); }";
/* Self-rendered lighting: the tile texture is treated as ALBEDO (base colour) and lit by
 * OUR sun (uSun, direction-to-sun in render space E=+X,up=+Y,N=-Z) via the terrain slope
 * normal, plus sky ambient — so relief/shadows track our real, dynamic sun instead of any
 * lighting baked into the texture. uLight carries the day->night level, uHaze the horizon. */
static const char*W3_FSWT=
 "precision mediump float; varying vec2 vUV; varying float vFog; varying vec3 vNorm;"
 "uniform sampler2D uTex; uniform vec3 uHaze; uniform float uLight; uniform vec3 uSun;"
 "void main(){ vec3 N=normalize(vNorm); float sup=smoothstep(-0.05,0.12,uSun.y);"
 "  float diff=max(0.0,dot(N,uSun))*sup;"
 "  float shade=0.55+0.65*diff;"                       /* 0.55 sky ambient .. +direct sun on lit slopes */
 "  vec3 c=texture2D(uTex,vUV).rgb*shade*uLight;"
 "  gl_FragColor=vec4(mix(c,uHaze,vFog*0.6),1.0); }";
static const char*W3_VSH=
 "attribute vec2 aPos; attribute vec3 aCol; uniform vec2 uScale; varying vec3 vCol;"
 "void main(){ gl_Position=vec4(aPos.x*uScale.x-1.0, 1.0-aPos.y*uScale.y, 0.0,1.0); vCol=aCol; }";
static const char*W3_FSH="precision mediump float; varying vec3 vCol; void main(){ gl_FragColor=vec4(vCol,1.0); }";

/* ---- sky dome: fullscreen pass, per-pixel ray from the camera basis ----
 * Colours the whole sky by the real sun position (day / dusk / night gradient),
 * draws sun + moon discs and a night star field, and blends procedural clouds.
 * Drawn first each frame with depth writes off; terrain paints over it. */
static const char*W3_VSKY=
 "attribute vec2 aPos; uniform vec3 uF,uS,uU; uniform float uTan,uAsp; varying vec3 vRay;"
 "void main(){ vRay=uF + uS*(aPos.x*uTan*uAsp) + uU*(aPos.y*uTan); gl_Position=vec4(aPos,0.999,1.0); }";
static const char*W3_FSKY=
 "precision highp float; varying vec3 vRay;"
 "uniform vec3 uSun,uMoon; uniform float uMoonPh,uCloud;"
 "float h21(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }"
 "float vnoise(vec2 p){ vec2 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f);"
 "  float a=h21(i),b=h21(i+vec2(1,0)),c=h21(i+vec2(0,1)),d=h21(i+vec2(1,1));"
 "  return mix(mix(a,b,f.x),mix(c,d,f.x),f.y); }"
 "void main(){ vec3 r=normalize(vRay); float hgt=clamp(r.y,-0.15,1.0);"
 "  float sEl=uSun.y; float day=smoothstep(-0.12,0.10,sEl);"
 "  float t=pow(clamp(hgt,0.0,1.0),0.55);"
 "  vec3 dayC=mix(vec3(0.72,0.82,0.92),vec3(0.22,0.45,0.82),t);"
 "  vec3 ngtC=mix(vec3(0.05,0.06,0.13),vec3(0.01,0.02,0.06),t);"
 "  vec3 sky=mix(ngtC,dayC,day);"
 /* warm dusk band toward the sun, low on the horizon */
 "  float dusk=exp(-(sEl*sEl)/(0.18*0.18));"          /* pow(neg,2.0) is undefined in GLES2 -> multiply */
 "  float low=exp(-(max(hgt,0.0)*max(hgt,0.0))/(0.20*0.20));"
 "  float tow=max(0.0,dot(normalize(vec3(r.x,0.0,r.z)),normalize(vec3(uSun.x,0.001,uSun.z))));"
 "  sky=mix(sky,vec3(0.98,0.46,0.20),dusk*low*(0.30+0.55*tow));"
 /* clouds: value-noise sheet projected on the dome, lit by day, silver by night */
 "  if(uCloud>0.01 && hgt>0.04){ vec2 cuv=r.xz/(hgt+0.25)*1.6;"
 "     float n=0.55*vnoise(cuv)+0.35*vnoise(cuv*2.3)+0.15*vnoise(cuv*4.7);"
 "     float cl=smoothstep(1.0-uCloud,1.0-uCloud*0.35,n)*smoothstep(0.04,0.22,hgt);"
 "     vec3 cc=mix(vec3(0.34,0.36,0.45),vec3(0.95,0.96,1.0),day);"
 "     sky=mix(sky,cc,cl*0.85); }"
 /* (real stars are drawn as a separate GL_POINTS pass at their true alt/az) */
 /* moon disc + phase (visible when up, mostly at night) */
 "  float ma=length(cross(r,uMoon)); float md=smoothstep(0.012,0.006,ma);"
 "  float mb=(0.25+0.75*uMoonPh)*step(-0.03,uMoon.y)*(1.0-0.7*day);"
 "  sky=mix(sky,vec3(0.92,0.92,0.86),md*mb);"
 /* sun disc + glow (visible when up) */
 "  float sa=length(cross(r,uSun)); float sd=smoothstep(0.016,0.004,sa);"
 "  float glow=exp(-sa*7.0)*0.35 + exp(-sa*1.5)*0.12*day;"
 "  float sup=smoothstep(-0.06,0.0,uSun.y);"
 "  sky+=(sd*vec3(1.0,0.96,0.86)*2.2 + glow*vec3(1.0,0.80,0.55))*sup;"
 "  gl_FragColor=vec4(sky,1.0); }";

/* ---- real stars: a GL_POINTS pass, each star placed at its true celestial direction ---- */
static const char*W3_VSTAR=
 "attribute vec3 aPos; attribute float aMag; uniform mat4 uMVP; varying float vB;"
 "void main(){ gl_Position=uMVP*vec4(aPos,1.0);"
 "  float b=clamp(1.45-0.42*aMag,0.15,1.4);"          /* brighter (lower mag) -> bigger, brighter */
 "  gl_PointSize=1.0+2.4*b; vB=b; }";
static const char*W3_FSTAR=
 "precision mediump float; varying float vB; uniform float uDay;"
 "void main(){ vec2 pc=gl_PointCoord-0.5; float d=dot(pc,pc);"
 "  float a=smoothstep(0.25,0.0,d)*vB*(1.0-uDay);"    /* soft round point, fades out toward day */
 "  gl_FragColor=vec4(vec3(0.85,0.89,1.0)*a, a); }";

static GLuint w3_pW,w3_pH,w3_pWT,w3_pSky,w3_pStar,w3_vTerr,w3_vBld,w3_hVBO,w3_skyVBO,w3_starVBO; static int w3_nTerr,w3_nBld;
static GLint w3_stPos,w3_stMag,w3_stMVP,w3_stDay;
static GLint w3_wPos,w3_wCol,w3_wMVP,w3_wHaze,w3_wLight,w3_hPos,w3_hCol,w3_hScale;
static GLint w3_wtPos,w3_wtUV,w3_wtMVP,w3_wtTex,w3_wtHaze,w3_wtLight,w3_wtNorm,w3_wtSun;
static GLint w3_skPos,w3_skF,w3_skS,w3_skU,w3_skTan,w3_skAsp,w3_skSun,w3_skMoon,w3_skMoonPh,w3_skCloud;

/* world uses OSM/terrain meshes if loaded via w3_load_mesh(), else procedural. */
#define W3_GRID 48
#define W3_CELL 120.0f
static float w3_hgt(float e,float n){ return 20.0f*sinf(e/900.f)*cosf(n/1100.f)+8.0f*sinf(e/300.f+n/250.f); }

static void w3_upload_terrain(const float*v,int nverts){ glGenBuffers(1,&w3_vTerr); glBindBuffer(GL_ARRAY_BUFFER,w3_vTerr); glBufferData(GL_ARRAY_BUFFER,nverts*6*4,v,GL_STATIC_DRAW); w3_nTerr=nverts; }
static void w3_upload_buildings(const float*v,int nverts){ glGenBuffers(1,&w3_vBld); glBindBuffer(GL_ARRAY_BUFFER,w3_vBld); glBufferData(GL_ARRAY_BUFFER,nverts*6*4,v,GL_STATIC_DRAW); w3_nBld=nverts; }

static void w3_build_procedural(void){
  float*v=malloc(W3_GRID*W3_GRID*6*6*sizeof(float)); int o=0;
  for(int j=0;j<W3_GRID;j++)for(int i=0;i<W3_GRID;i++){
    float e0=(i-W3_GRID/2)*W3_CELL,n0=(j-W3_GRID/2)*W3_CELL,e1=e0+W3_CELL,n1=n0+W3_CELL;
    float p[4][3]={{e0,w3_hgt(e0,n0),-n0},{e1,w3_hgt(e1,n0),-n0},{e1,w3_hgt(e1,n1),-n1},{e0,w3_hgt(e0,n1),-n1}};
    int idx[6]={0,1,2,0,2,3};
    for(int k=0;k<6;k++){ float*P=p[idx[k]]; float g=0.35f+0.0045f*P[1]; if(g<0.25f)g=0.25f; if(g>0.6f)g=0.6f;
      v[o++]=P[0];v[o++]=P[1];v[o++]=P[2]; v[o++]=0.20f+g*0.2f;v[o++]=g;v[o++]=0.18f; } }
  w3_upload_terrain(v,o/6); free(v);
  float*b=malloc(600*36*6*sizeof(float)); int bo=0; unsigned s=12345;
  for(int k=0;k<600;k++){ s=s*1103515245+12345; float e=((int)((s>>9)%4000))-2000;
    s=s*1103515245+12345; float n=((int)((s>>9)%4000))-2000; if(fabsf(e)<150&&fabsf(n)<150) continue;
    s=s*1103515245+12345; float w=8+((s>>9)%20),d=8+((s>>10)%20); s=s*1103515245+12345; float ht=10+((s>>9)%40); float base=w3_hgt(e,n);
    float x0=e-w/2,x1=e+w/2,z0=-(n-d/2),z1=-(n+d/2),y0=base,y1=base+ht; float col=0.45f+0.15f*((k%5)/5.0f);
    float C[8][3]={{x0,y0,z0},{x1,y0,z0},{x1,y1,z0},{x0,y1,z0},{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}};
    int F[6][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}};
    for(int f=0;f<6;f++){ int*fi=F[f],tri[6]={0,1,2,0,2,3}; float sh=col*(0.7f+0.05f*f);
      for(int t=0;t<6;t++){ float*P=C[fi[tri[t]]]; b[bo++]=P[0];b[bo++]=P[1];b[bo++]=P[2]; b[bo++]=sh;b[bo++]=sh;b[bo++]=sh*1.05f; } } }
  w3_upload_buildings(b,bo/6); free(b);
}

/* ---- live OSM streaming (osmmesh) -------------------------------------------
 * Location-agnostic: the world is streamed on demand from OSM Shortbread +
 * Copernicus terrain PMTiles around the aircraft's current GPS position. No
 * static geometry — as the aircraft crosses a tile boundary the grid is re-fetched
 * and the terrain/building VBOs are rebuilt. Origin (lat/lon) is configurable. */
#ifdef W3_USE_OSM
#include "osmmesh/osmmesh.h"
#include "osmmesh/pmtiles.h"
#include "osmmesh/mvt.h"
/* ---- terrain LOD: a chunked quadtree (Ulrich 2002), not a stack of rings -------------------
 *
 * What was here before: three fixed 5x5 grids at z14/z11/z8, drawn coarse-to-fine with a polygon
 * offset so the fine one "wins". They overlapped COMPLETELY -- the ground under the aircraft was
 * rasterised three times -- and that was not an oversight to be tuned away, it was unfixable by
 * construction:
 *
 *   - 5x5 z14 spans 7.5 km; ONE z11 tile spans 12.0 km. No z11 tile is ever fully covered, so not
 *     one of them could be skipped. Same for z11 vs z8. 0 of the 50 coarse tiles were droppable.
 *   - MEASURED at the origin: the z8 surface sits ABOVE the z14 surface at 72.8 % of near-field
 *     points, by up to +85 m; z11 by up to +30 m at 55.8 %. The coarse mesh does not merely
 *     disagree, it pokes through.
 *   - glPolygonOffset cannot fix that. It biases DEPTH, not position, and the bias it buys is a
 *     function of the triangle's depth slope: ~9 m of world depth for z11 at 1 km, ~37 m at 3 km,
 *     ~584 m at 30 km. Against a 30 m intrusion that is a coin flip that lands differently per
 *     triangle and per viewing angle -- which is exactly what "es ueberschneidet sich an den
 *     raendern" looks like. Too weak up close, absurdly strong far away.
 *
 * A quadtree has a HOLE where the finer level sits: a node draws ITSELF or its four children,
 * never both. Every square metre of ground is covered by exactly one chunk. No overlap, no
 * offset, no fighting -- the problem is removed rather than biased.
 *
 * Refinement is by screen-space error, from each chunk's OWN measured geometric error:
 *     SSE = node.error * W3_SSE_K / distance      (pixels)
 *     split while SSE > W3_EPS
 * See w3_terr_vbo for why the error must be measured per chunk and not tabulated per zoom. */
#ifndef W3_ROOTZ
#define W3_ROOTZ 8                 /* coarsest level: one chunk ~96 km at 52 N */
#endif
#ifndef W3_MAXZ
#define W3_MAXZ 14                 /* finest level. The albedo bake stops here: fb-tiles' vector
                                    * source (VersaTiles Shortbread) has no z15. */
#endif
/* --- texture LOD ramp -----------------------------------------------------------------------
 * The albedo used to be ONE fixed size (512) at every level -- "what makes it a chunk". But a
 * chunk right under the camera fills far more screen than one at the horizon, so a single size is
 * wrong at both ends: 512 stretched over a near chunk is blurry, 512 baked for a far one is wasted
 * VRAM and a wasted server bake. The size is now chosen per draw from the chunk's on-screen size
 * (w3_chunk_px -> w3_lod_for_px): the smallest ramp step that still covers it.
 *
 * The two callers -- w3_walk (the chunk's own distance) and w3_children_ready (the parent asking on
 * the children's behalf) -- can pick different steps for the same tile. That disagreement is the
 * OLD 1-FPS thrash: back then `tex` was a size passed BESIDE an unkeyed single texture, so a hit
 * returned the wrong size and the "fix" was a re-bake, every frame, both ways. The cure is to make
 * the step part of what a texture IS: tex[mode][lod] holds every size the tree has asked for, side
 * by side. A disagreement then costs one extra bake, ONCE, and is a cache hit forever after --
 * monotone state cannot flutter. Release stays at exactly one place, w3_cache_trim.
 *
 * The sizes are the server's bake sizes: the JS bake cache is already keyed by tex (tilesrc_js.h),
 * and the server bakes per-tex, so a step maps straight to a bake request. 2048 is safe by the
 * WebGL2 spec (MAX_TEXTURE_SIZE >= 2048); w3_lod_cap trims the table to what the context reports,
 * so a spec-minimum context simply never uses a step it cannot hold. */
#ifndef W3_NLOD
#define W3_NLOD 4
#endif
static const int w3_lod_px[W3_NLOD] = { 256, 512, 1024, 2048 };
static int w3_lod_cap = W3_NLOD;   /* trimmed at gfx init: min(hardware cap, policy ceiling below) */

/* The ramp uses its full range (256..2048); the only cap is the hardware MAX_TEXTURE_SIZE. Load
 * time is second to quality -- a big bake is paid ONCE and cached, and progressive loading (below)
 * means it never blocks the first, visible image (a fresh 2048 OSM bake is 7-20 s against the live
 * server; the 256 shows at once and the tile sharpens up the ladder behind it). 2048 is justified
 * for BOTH albedos, each for its own reason, both MEASURED: PHOTO carries real z16 sub-metre imagery
 * detail; OSM is VECTOR, and the MVT source extent is 4096 -- finer than a 2048 texture -- so
 * rasterising at 2048 keeps street and footprint edges HARD (the max luma gradient is identical at
 * 1024 and 2048; an upscale of 1024 would soften it). Sharper edges, less aliasing -- not upscaled.
 * (An earlier note here claimed 2048 OSM was detail-free upscaling because "the source ends at z14".
 * That was a measurement error: z14 is the TILE zoom, not the vector precision -- frequency/area
 * averages miss edge sharpness, which is exactly where vector rasterisation wins. Corrected against
 * the extent and the gradient.) The only real ceiling above 2048 is the extent (4096), which is
 * VRAM-bound, not a source limit. W3_LOD_MAXSTEPS stays a knob only for a spec-minimum context that
 * cannot hold the full ramp; normal contexts (>=2048 by the WebGL2 spec) use every step. */
#ifndef W3_LOD_MAXSTEPS
#define W3_LOD_MAXSTEPS W3_NLOD
#endif

/* Smallest ramp step whose texels meet the chunk's on-screen pixels (>= px), clamped to the caps.
 * Under-supply (a small texture stretched over a big screen area) is the visible defect; over-
 * supply is absorbed by the mipmap chain, so round UP. */
static int w3_lod_for_px(double px){
  int lod=0;
  while(lod < w3_lod_cap-1 && (double)w3_lod_px[lod] < px) lod++;
  return lod;
}

/* Proof scaffolding, both compiled out of production (default 0):
 *   W3_LOD_INJECT -- force w3_children_ready to request one step away from w3_walk, so the
 *                    side-by-side slots are actually exercised by a disagreement.
 *   W3_LOD_NOKEY  -- collapse the ramp back to a single stored texture per mode that re-bakes on
 *                    a size change: this reproduces the OLD thrash, and is the control the middle
 *                    line of the proof is measured against. See w3_bake / the mipmap counter. */
#ifndef W3_LOD_INJECT
#define W3_LOD_INJECT 0
#endif
#ifndef W3_LOD_NOKEY
#define W3_LOD_NOKEY 0
#endif
#ifndef W3_LOD_SPIN
#define W3_LOD_SPIN 0             /* proof only: walk every frame (see world3d_stream_at) */
#endif
#if W3_LOD_NOKEY
#define W3_LODIDX(lod) 0
#else
#define W3_LODIDX(lod) (lod)
#endif
#ifndef W3_TERR
#define W3_TERR 22                 /* chunk geometry: 22x22 quads. The texture carries the detail;
                                    * the height field only has to carry the shape. */
#endif
#ifndef W3_EPS
#define W3_EPS 2.0f                /* the ONLY quality knob: allowed screen-space error, pixels. */
#endif
#ifndef W3_REACH
#define W3_REACH 240000.0f         /* how far the ground is drawn. Just a parameter -- the tree
                                    * covers whatever it is given; the horizon at 4000 m is 226 km. */
#endif
#ifndef W3_VIEWH
#define W3_VIEWH 480               /* the height SSE is measured in: the camera FBO (NTSC 480). */
#endif
/* SSE = error * K / distance, with K = viewport_height / (2 tan(fov/2)). This is the standard
 * chunked-LOD projection of a vertical error onto the screen. */
#define W3_SSE_K ((float)(W3_VIEWH) / (2.0f * 0.8390996f))   /* tan(40 deg) for W3_FOV=80 */
/* A budget is needed because the error comes from the DATA: over Hameln (96 m of relief) the z8
 * chunks stop splitting immediately, but the same threshold at Everest keeps splitting even at the
 * horizon (2093 m error -> 2.65 px at 226 km). Flat terrain costs nothing, mountains would run
 * away. So: cap the resident set and spend it on the worst error first. */
#ifndef W3_BUDGET
#define W3_BUDGET 128              /* max chunks drawn/resident. 128 * 512^2 RGB * (mips) * 2
                                    * albedos ~ 268 MB -- about what the three rings cost. */
#endif
/* Camera near/far. near was 2 m: at that ratio a 16-bit depth buffer cannot resolve 700 m at
 * 10 km. Nothing is ever within 20 m of the camera -- it sits in the aircraft -- and moving the
 * near plane out is the single cheapest precision win there is (10x, for one number). */
#ifndef W3_NEAR
#define W3_NEAR 20.0f
#endif
#ifndef W3_FARPLANE
#define W3_FARPLANE 240000.0f      /* past the z8 ring; the horizon at 4000 m is 226 km */
#endif

/* Old-flight-sim ground: OSM footprints/roads/rivers/rails + landcover are baked
 * into ONE orthographic texture per tile (no building geometry). The texture is
 * draped over the terrain heightfield. The vector features come from the Shortbread
 * PMTiles via osmmesh's MVT decoder; the terrain heightfield from osmmesh's terrain. */
static osmmesh_ctx *w3_osm=0;          /* terrain heightfield meshes */
static osmmesh_pmtiles *w3_vec=0;      /* raw vector tiles for texture baking (legacy archive path) */
static int w3_stream_tiles=0;          /* 1 = tiles come from fb-tiles on demand, no preloaded region */
static int w3_have_tile=0; static uint32_t w3_tx,w3_ty;
static float w3_yoff=0; static int w3_yoff_set=0;   /* origin ground elevation (camera lift) */

/* ONE draw list, built by the tree walk each stream pass. There used to be three of these
 * (w3_T/w3_TF/w3_TF2 with w3_nT/w3_nTF/w3_nTF2) -- the same code three times with suffixes,
 * which is how the overlap got written down as a fact and then lived on as one. */
typedef struct { GLuint vbo, tex[2]; int nverts; } w3_tileGL;
static w3_tileGL w3_D[W3_BUDGET]; static int w3_nD=0;

/* --- software raster into an RGB image (tile-local coords * sc) --- */
/* ---- ground albedo: downloaded ready-made from fb-tiles ------------------------------------
 * There used to be ~150 lines of software rasteriser here: scanline polygon fill, thick lines,
 * MVT layer walking, the aerial mosaic blit. It ran in the browser on EVERY page load, re-drawing
 * cartography that had not changed since the last one, from a vector tile plus its 4 seam
 * neighbours. Now fb-tiles does it once and keeps the result (tiles/raster.c, tiles/bake.c), so
 * this is a download and an upload.
 *
 * The albedo is view-independent -- what the ground IS, not how it is lit -- which is exactly why
 * it may be precomputed at all. Lighting stays here, per-pixel, from our own sun and the DEM
 * normals. Baking an albedo is not baking light.
 */
enum { W3_GROUND_OSM = 0, W3_GROUND_PHOTO = 1 };
static int w3_ground_mode = W3_GROUND_OSM;
/* Set while any resident tile still lacks the albedo we want. Keeps world3d_stream from
 * declaring victory and going back to sleep until the next tile boundary. */
static int w3_ground_dirty = 0;
/* Chunks the tree asked for and did not get. The streamer may only sleep when this is 0:
 * "nothing is waiting" and "nothing was asked for" are different states, and conflating
 * them let the tree declare victory over an EMPTY world and never look again. */
static int w3_pending=0;
/* Chunks that ARE drawable (floor 256 present) but still below their SSE target -- the ones
 * climbing the ramp. Kept apart from w3_pending on purpose: progressive loading means "shown" and
 * "fully sharp" are different states, and only the FIRST gates convergence. w3_sharpen keeps the
 * streamer awake so it keeps climbing, but never blocks "0 pending" -- otherwise a moving camera,
 * always mid-climb on some near tile (a fresh 2048 is 7-20 s), would never let the gate latch. */
static int w3_sharpen=0;
static unsigned w3_pass_mark=0;   /* touch stamp at the start of THIS stream pass; want_lod_max is
                                   * reset once per pass off it (first touch), then max'd. */

/* Every glGenerateMipmap the tile path does. The tile cache is the ONLY caller (EVS uploads a raw
 * video frame with no mipmap), so this is a clean per-frame thrash counter: it is ~0 once the cache
 * is warm and explodes the instant a tile re-bakes every frame. That is the whole proof of the
 * side-by-side LOD slots -- read it out of cc_mipmaps(). */
static long w3_mipmaps=0;

/* Upload one baked albedo as a GL texture. Returns 0 if it has not arrived yet (retry later) or
 * the server has nothing for this tile. */
static GLuint w3_bake(uint32_t z,uint32_t x,uint32_t y,int TS,int mode){
  int photo = (mode==W3_GROUND_PHOTO);
  int n = w3_bake_size(photo,(int)z,(int)x,(int)y,TS);   /* also STARTS the fetch on a miss */
  if(n<=0) return 0;                                     /* pending (-1) or a genuine hole (0) */
  uint8_t*enc=(uint8_t*)malloc((size_t)n); if(!enc) return 0;
  w3_bake_copy(photo,(int)z,(int)x,(int)y,TS,enc);
  int w=0,h=0,comp=0;
  uint8_t*px=stbi_load_from_memory(enc,n,&w,&h,&comp,3);
  free(enc);
  if(!px) return 0;                                      /* undecodable: a hole, not a crash */

  GLuint tex; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,px);
  /* trilinear (mipmaps) kills shimmer on distant tiles; anisotropy keeps the ground sharp at
   * grazing angles (the terrain is seen almost edge-on). */
  glGenerateMipmap(GL_TEXTURE_2D); w3_mipmaps++;
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  #ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
  #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
  #endif
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,8.0f);   /* ignored if unsupported */
  stbi_image_free(px);
  return tex;
}

/* The terrain vertex layout is defined by its WRITER, in chunkmesh.h. These two macros are the
 * READER's half of that contract -- they need GL types, so they stay here, but they derive from
 * the same struct, so a layout change breaks the build rather than the picture. */
#define W3_VTX_STRIDE ((GLsizei)sizeof(w3_vtx))
#define W3_VTX_OFF(f)  ((void*)offsetof(w3_vtx, f))

/* w3_chunk_build (chunkmesh.h) -- decimation, smooth normals, the measured error, the skirt -- now
 * runs in the WORKER (tileworker.c), which hands back a finished w3_vtx[] array. All that is left
 * on the main thread is glBufferData, inline in w3_cache_get's MESH->READY transition. The GL
 * upload wrapper that used to live here (w3_terr_vbo) is gone with the synchronous path. */

static int world3d_osm_open_mem(const char*vec_path,const uint8_t*vec_data,size_t vec_len,
                                const char*terr_path,const uint8_t*terr_data,size_t terr_len,
                                double origin_lat,double origin_lon){
  osmmesh_config cfg={ .vector_url=vec_path, .vector_data=vec_data, .vector_len=vec_len,
    .terrain_url=terr_path, .terrain_data=terr_data, .terrain_len=terr_len,
    .origin_lat=(w3_olat=origin_lat), .origin_lon=(w3_olon=origin_lon),
    .enable_terrain=1, .enable_buildings=0, .enable_linears=0 };
  if(osmmesh_create(&cfg,&w3_osm)!=OSMMESH_OK){ printf("[world3d] osmmesh_create failed\n"); w3_osm=0; return 0; }
  int vrc = vec_data ? osmmesh_pmtiles_open_memory(&w3_vec,vec_data,vec_len)
                     : osmmesh_pmtiles_open_file(&w3_vec,vec_path);
  if(vrc!=OSMMESH_PMTILES_OK){ printf("[world3d] vector pmtiles open failed: %d\n",vrc); w3_vec=0; }
  w3_have_tile=0; return 1;
}
static int world3d_osm_open(const char*vec_path,const char*terr_path,double lat,double lon){
  return world3d_osm_open_mem(vec_path,0,0,terr_path,0,0,lat,lon);
}
/* Stream tiles on demand from the fb-tiles service instead of a preloaded region archive.
 * This is what makes any origin on earth work: nothing is bundled, everything is fetched. */
static int world3d_tiles_open(const char*base,double lat,double lon){
  w3_tiles_init(base);
  osmmesh_config cfg={ .origin_lat=(w3_olat=lat), .origin_lon=(w3_olon=lon),
    .tile_provider=w3_tile_provider, .tile_provider_user=0,
    .provider_terrain_max_zoom=15,   /* Tilezen terrarium; no archive header to read */
    .enable_terrain=1, .enable_buildings=0, .enable_linears=0 };
  if(osmmesh_create(&cfg,&w3_osm)!=OSMMESH_OK){ printf("[world3d] osmmesh_create (streaming) failed\n"); w3_osm=0; return 0; }
  w3_vec=0; w3_stream_tiles=1; w3_have_tile=0;
  /* Spawn the tile worker with the SAME origin: the mesh it builds is ENU-relative to it. The main
   * thread's osmmesh ctx above is now only the "world is open" gate and osmmesh_geo_to_tile -- the
   * fetch/decode/mesh it used to do runs in the worker. */
  w3_worker_init(base,lat,lon);
  printf("[world3d] streaming tiles from %s (origin %.4f/%.4f), worker spawned\n",base,lat,lon);
  return 1;
}
/* stream one grid (zoom z, radius rad, texture size tex) around tile (cx,cy) into arr */
/* ---- baked-tile cache (LRU, keyed by z/x/y) ----
 * Crossing a tile boundary used to DELETE and re-bake all 34 tiles: a multi-hundred-ms
 * freeze every ~44 s in a 1000 m orbit (the video froze then jumped = the "kick"). But an
 * orbit flies over the SAME tiles again and again, so we keep baked VBO+texture around and
 * only bake genuinely new ones. Sized to hold a full orbit -> after the first lap nothing
 * is re-baked at all. Eviction is least-recently-used. */
/* The cache holds every chunk the tree may draw, plus room for the ones it is refining INTO.
 * A chunk that is still being replaced by its children must stay resident -- that is what lets the
 * parent keep drawing until all four children have landed, instead of a hole appearing.
 * Eviction is by "the tree did not ask for it this pass", which is strictly better than either LRU
 * or a radius: the tree already knows exactly what it wants. Flying straight for an hour, the
 * chunks behind you are the NEWEST in the cache and the most useless; an LRU would keep them. */
#define W3_CACHE (2*W3_BUDGET)

/* tex[mode][lod] holds BOTH albedos at EVERY size the tree has asked for. [mode] is the ground
 * source: [W3_GROUND_OSM] = the OSM render, [W3_GROUND_PHOTO] = the aerial photo -- both baked
 * while streaming, so switching the ground is an index change, not a re-bake (the moment you need
 * the camera fallback is the moment you cannot afford to build it). [lod] is the texture size step
 * (w3_lod_px): a moving camera asks the same tile for different sizes as it nears, and each is kept
 * side by side rather than re-baked over the last -- that is what turns the old size-disagreement
 * thrash into one extra bake. The VBO (the expensive part) is shared across all of it -- the
 * geometry is LOD-independent and does not care what size is painted on it.
 * Any tex[mode][lod] is 0 until baked; the draw picks the nearest resident size (w3_pick_lod), and
 * PHOTO falls back to OSM. */
/* A slot's LIFECYCLE lives IN the slot, not in a side channel. The mesh is built OFF the main
 * thread now (tileworker.c), so a tile is no longer either absent or done -- it passes through
 * "asked the worker" and "worker delivered, not yet on the GPU". Putting `state` beside the entry
 * it describes is the same lesson as `tex` belonging IN the cache key: a per-entry fact kept next
 * to the entry instead of inside it is the bug this project has already paid for twice. It also
 * keeps the coming tex[mode][lod] change orthogonal -- that adds a dimension to `tex`, it never
 * touches `state`. */
enum { W3_SLOT_FREE=0,   /* unused */
       W3_SLOT_WAIT,     /* posted to the worker; no mesh back yet */
       W3_SLOT_MESH,     /* worker delivered verts (in mverts, main heap); not yet uploaded */
       W3_SLOT_READY };  /* vbo uploaded + OSM albedo present -> drawable (the old `valid`) */
typedef struct { int z; uint32_t x,y; GLuint vbo,tex[2][W3_NLOD]; int nverts; unsigned touch; int state;
                 int photo_none;      /* the server has no photo here, ever (size-independent) */
                 int want_lod_max;    /* highest ramp step this chunk justifies THIS pass (its SSE
                                       * target). trim frees resident steps above it -> a receding
                                       * chunk gives its fine steps back. Per-pass max, distance-only. */
                 float err;           /* measured geometric error, metres -- drives the LOD */
                 float *mverts; int mnverts; float merr;  /* worker's mesh, awaiting GL upload */
#if W3_LOD_NOKEY
                 int texpx[2];        /* proof control: the size stored in tex[mode][0], to detect
                                       * a size change and force the old re-bake */
#endif
                 int want_yoff; } w3_cent;                /* this WAIT was the origin-elevation probe */
static w3_cent w3_cache[W3_CACHE];
static unsigned w3_touch=0;
static int w3_cache_hits=0, w3_cache_bakes=0, w3_cache_evict=0;

/* The ONE place a resident texture or VBO is freed, in both directions:
 *  - a chunk the tree did NOT touch this pass is unreachable -> free it whole. `touch` is stamped by
 *    w3_cache_get, so "not asked for" and "not reachable" are the same question, answered by the
 *    tree. This is DISTANCE-based (the walk covers the 360 deg ring in range), NOT view-based: a
 *    chunk turned out of frame but still near keeps its textures, so turning back is instant.
 *  - a chunk the tree DID touch, but now at a lower SSE target than before (it is receding), frees
 *    its ramp steps ABOVE want_lod_max -> VRAM tracks the resolution actually needed, symmetric with
 *    w3_climb building it up. The floor (l=0) is never above any target, so a drawable tile never
 *    loses its last texture here.
 *
 * Each state frees exactly what it owns. A WAIT slot owns nothing yet -- its worker request may
 * still be in flight; when the reply lands, w3_worker_mesh finds no matching WAIT slot (this freed
 * it) and frees the verts there. A MESH slot owns heap verts the worker delivered and the GPU
 * never received. A READY slot owns GL objects. Miss one and it leaks silently. */
static void w3_cache_trim(unsigned mark){
  for(int i=0;i<W3_CACHE;i++){
    w3_cent*c=&w3_cache[i];
    if(c->state==W3_SLOT_FREE) continue;
    if(c->touch>=mark){
      /* touched: keep the chunk, but release ramp steps finer than it now justifies */
      if(c->state==W3_SLOT_READY)
        for(int m=0;m<2;m++) for(int l=c->want_lod_max+1;l<W3_NLOD;l++)
          if(c->tex[m][l]){ glDeleteTextures(1,&c->tex[m][l]); c->tex[m][l]=0; }
      continue;
    }
    if(c->state==W3_SLOT_READY){
      glDeleteBuffers(1,&c->vbo);
      for(int m=0;m<2;m++) for(int l=0;l<W3_NLOD;l++)
        if(c->tex[m][l]) glDeleteTextures(1,&c->tex[m][l]);
    } else if(c->state==W3_SLOT_MESH){
      free(c->mverts); c->mverts=0;
    }
    c->state=W3_SLOT_FREE; w3_cache_evict++;
  }
}

/* Real resident texture VRAM in bytes -- summed from what is actually on the GPU, not estimated.
 * Each texture is size^2 RGB (3 B) plus its mipmap chain (+1/3), both albedos, every held step. This
 * is the number that decides whether the top of the ramp fits the 2 GB budget, not a guess. */
static long w3_texvram(void){
  long b=0;
  for(int i=0;i<W3_CACHE;i++){
    if(w3_cache[i].state!=W3_SLOT_READY) continue;
    for(int m=0;m<2;m++) for(int l=0;l<W3_NLOD;l++)
      if(w3_cache[i].tex[m][l]){ long s=w3_lod_px[l]; b += s*s*3*4/3; }
  }
  return b;
}

/* The worker delivered a finished mesh for (z,x,y). Called from JS (the worker's onmessage copied
 * the transferred vertex buffer into `verts`, a main-heap malloc we now OWN). Move the matching
 * WAIT slot to MESH; if the tree stopped asking and trim already freed the slot, there is nobody
 * to hand it to -- free it rather than leak. */
EMSCRIPTEN_KEEPALIVE
void w3_worker_mesh(int z,uint32_t x,uint32_t y,float*verts,int nverts,float err,float yoff){
  for(int i=0;i<W3_CACHE;i++){
    w3_cent*c=&w3_cache[i];
    if(c->state==W3_SLOT_WAIT && c->z==z && c->x==x && c->y==y){
      c->mverts=verts; c->mnverts=nverts; c->merr=err; c->state=W3_SLOT_MESH;
      if(c->want_yoff && !w3_yoff_set){ w3_yoff=yoff; w3_yoff_set=1; }
      return;
    }
  }
  free(verts);   /* no taker: the tile was trimmed while its build was in flight */
}
/* The worker could not build (a real 204 hole, or it gave up after retries). Free the WAIT slot so
 * the tree either re-asks next frame or leaves it absent -- the same "retry, never silently hole"
 * the sync path had. */
EMSCRIPTEN_KEEPALIVE
void w3_worker_fail(int z,uint32_t x,uint32_t y){
  for(int i=0;i<W3_CACHE;i++){
    w3_cent*c=&w3_cache[i];
    if(c->state==W3_SLOT_WAIT && c->z==z && c->x==x && c->y==y){ c->state=W3_SLOT_FREE; return; }
  }
}

/* Bake tex[mode][lod] for (z,x,y) at size TS if it is not already resident. Returns:
 *   1  present (already there, or baked just now)
 *   0  the server has nothing here -- a genuine hole (only meaningful for PHOTO -> photo_none)
 *  -1  in flight (or momentarily undecodable): ask again next frame
 * This is the ONE place a tile texture is created, mirroring w3_cache_trim as the one place it is
 * freed. The size is TS; the STORAGE slot is W3_LODIDX(lod) -- the two differ only under the
 * W3_LOD_NOKEY control, where every size collapses onto slot 0 and a size change evicts first,
 * reproducing the old re-bake-every-frame thrash on purpose. */
static int w3_ensure_tex(w3_cent*c,int mode,int z,uint32_t x,uint32_t y,int TS,int lod){
  int idx=W3_LODIDX(lod);
#if W3_LOD_NOKEY
  if(c->tex[mode][0] && c->texpx[mode]!=TS){ glDeleteTextures(1,&c->tex[mode][0]); c->tex[mode][0]=0; }
#endif
  if(c->tex[mode][idx]) return 1;
  int n=w3_bake_size(mode==W3_GROUND_PHOTO,(int)z,(int)x,(int)y,TS);
  if(n<0) return -1;
  if(n==0) return 0;
  GLuint t=w3_bake(z,x,y,TS,mode);
  if(!t) return -1;                              /* undecodable: treat as in flight, keep asking */
  c->tex[mode][idx]=t;
#if W3_LOD_NOKEY
  c->texpx[mode]=TS;
#endif
  w3_cache_bakes++;
  return 1;
}

/* The GL texture to DRAW for this tile at the wanted step: the wanted size if resident, else the
 * nearest resident FINER size (never blurrier than asked), else the nearest coarser, else 0. A
 * tile can be READY with only one size baked -- the others land on later frames -- so the draw must
 * always have something to bind rather than a hole. */
static GLuint w3_pick_lod(const w3_cent*c,int mode,int lod){
  for(int l=lod;l<W3_NLOD;l++) if(c->tex[mode][l]) return c->tex[mode][l];
  for(int l=lod-1;l>=0;l--)    if(c->tex[mode][l]) return c->tex[mode][l];
  return 0;
}

/* Progressive refinement: climb the ramp one step at a time toward `target`. The floor (256) is
 * already resident when this runs (the MESH gate ensures it), so the tile is drawable the whole
 * time -- each call requests only the NEXT step above the highest one present. That lets the cheap
 * sizes show first and the expensive ones (a fresh 2048 is 7-20 s) land in the background without
 * ever blocking the image. Sequential, not all-at-once: a 2048 bake is never kicked off before the
 * 512 it supersedes is even shown, and the in-flight set stays small (the JS side caps at 256).
 * Returns 1 when OSM has reached `target`, 0 while still climbing -- the caller counts a 0 as
 * w3_sharpen (drawable, not yet sharp), NEVER as w3_pending, so climbing does not hold the gate. */
static int w3_climb(w3_cent*c,int z,uint32_t x,uint32_t y,int target){
  int hi=-1; for(int l=0;l<=target;l++) if(c->tex[W3_GROUND_OSM][l]) hi=l;
  if(hi>=target){
    /* OSM at target: let PHOTO catch up to the same step (opportunistic, never blocks the gate). */
    if(!c->photo_none && !c->tex[W3_GROUND_PHOTO][target]){
      int p=w3_ensure_tex(c,W3_GROUND_PHOTO,z,x,y,w3_lod_px[target],target);
      if(p==0) c->photo_none=1; else if(p<0) return 0;   /* top photo step still fetching */
    }
    return 1;
  }
  int next=hi+1, TS=w3_lod_px[next];
  w3_ensure_tex(c,W3_GROUND_OSM,z,x,y,TS,next);           /* -1 pending is the normal case here */
  if(!c->photo_none){
    int p=w3_ensure_tex(c,W3_GROUND_PHOTO,z,x,y,TS,next);
    if(p==0) c->photo_none=1;
  }
  return 0;
}

/* Return a READY (drawable) slot for (z,x,y) at texture step `lod`, or -1 if not ready yet. NEVER
 * blocks.
 *
 * The expensive work -- fetch DEM+MVT, decode, build the mesh (measured: 76 % of the per-tile
 * cost, ~23 ms) -- runs OFF the main thread now (tileworker.c). So this no longer DOES that work;
 * it drives the per-slot state machine and posts to the worker:
 *   not resident -> claim a slot, post to the worker, go WAIT, return -1
 *   WAIT         -> worker still building; return -1 WITHOUT re-posting (WAIT is the dedup)
 *   MESH         -> verts delivered; when the FLOOR albedo (256) is here too, upload the VBO, go
 *                   READY at the floor and start climbing toward `lod` -- return the slot
 *   READY        -> drawable; climb one ramp step toward `lod` (progressive), return the slot
 *
 * Progressive by design: a tile becomes drawable at the cheap 256 floor and SHARPENS up the ramp to
 * its SSE target `lod` over later frames (w3_climb). So the readiness gate is the FLOOR, not the
 * target -- a fresh 2048 (7-20 s) never blocks the first image. `lod` is part of what identifies a
 * texture (tex[mode][lod]), never a size passed beside an unkeyed one, so the several steps a chunk
 * climbs through sit side by side, not re-baked over each other. want_lod_max records the target so
 * trim can release steps above it once the chunk recedes. */
static int w3_cache_get(int z,uint32_t x,uint32_t y,int lod,int is_centre){
  int slot=-1;
  for(int i=0;i<W3_CACHE;i++)
    if(w3_cache[i].state!=W3_SLOT_FREE && w3_cache[i].z==z && w3_cache[i].x==x && w3_cache[i].y==y){ slot=i; break; }

  if(slot<0){
    /* Not resident: claim a free slot and ask the worker. A free slot always exists -- W3_CACHE is
     * twice the budget and trim drops everything unreached -- but if a pass ever fills it, treat it
     * as pending rather than thrash. */
    for(int i=0;i<W3_CACHE;i++) if(w3_cache[i].state==W3_SLOT_FREE){ slot=i; break; }
    if(slot<0){ w3_pending++; return -1; }
    w3_cent*c=&w3_cache[slot];
    c->z=z; c->x=x; c->y=y; c->state=W3_SLOT_WAIT; c->touch=++w3_touch; c->want_lod_max=lod;
    c->photo_none=0; memset(c->tex,0,sizeof c->tex); c->mverts=0; c->vbo=0;
#if W3_LOD_NOKEY
    c->texpx[0]=c->texpx[1]=0;
#endif
    /* Preserve the original yoff condition EXACTLY (is_centre && z==W3_MAXZ && !set): the worker
     * computes the origin ground elevation only when asked, and w3_worker_mesh applies it. */
    c->want_yoff = (is_centre && z==W3_MAXZ && !w3_yoff_set);
    w3_worker_post(z,(int)x,(int)y,W3_TERR,c->want_yoff);
    w3_pending++;
    return -1;
  }

  w3_cent*c=&w3_cache[slot];
  int fresh=(c->touch < w3_pass_mark);                    /* first touch this pass? */
  c->touch=++w3_touch;
  if(fresh || lod>c->want_lod_max) c->want_lod_max=lod;   /* per-pass MAX of the requested target */

  if(c->state==W3_SLOT_WAIT){ w3_pending++; return -1; }   /* worker still building */

  if(c->state==W3_SLOT_MESH){
    /* Mesh in hand. Gate drawability on the FLOOR albedo (256) -- cheap and fast, so the tile
     * shows almost at once and sharpens later. Upload the VBO only once that texture is in hand, so
     * a slot never holds a VBO with no texture (a placeholder square over Grohnde is the bug). */
    int r=w3_ensure_tex(c,W3_GROUND_OSM,z,x,y,w3_lod_px[0],0);
    if(r<=0){ w3_pending++; return -1; }                   /* floor in flight: keep asking */
    GLuint vbo; glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)((size_t)c->mnverts*sizeof(w3_vtx)),c->mverts,GL_STATIC_DRAW);
    free(c->mverts); c->mverts=0;
    c->vbo=vbo; c->nverts=c->mnverts; c->err=c->merr;
    { int p=w3_ensure_tex(c,W3_GROUND_PHOTO,z,x,y,w3_lod_px[0],0);  /* photo floor; OSM covers if -1 */
      if(p==0) c->photo_none=1; }
    c->state=W3_SLOT_READY;
    if(!w3_climb(c,z,x,y,lod)) w3_sharpen++;                /* start the climb toward the target */
    return slot;
  }

  /* READY: drawable. Climb one step toward the target if not there yet -- side by side with the
   * steps already resident, never re-baked over them. A chunk below its target is w3_sharpen, not
   * w3_pending: it is on screen, just not yet at full resolution. */
  w3_cache_hits++;
  if(!w3_climb(c,z,x,y,lod)) w3_sharpen++;
  return slot;
}

/* Fractional tile coords -- the camera sits between tiles, and the tree needs to know where.
 * osmmesh_geo_to_tile only answers in whole tiles. */
static void w3_geo_to_tile_f(double lat,double lon,int z,double*tx,double*ty){
  double n=ldexp(1.0,z), la=lat*M_PI/180.0;
  *tx=(lon+180.0)/360.0*n;
  *ty=(1.0-asinh(tan(la))/M_PI)/2.0*n;
}
/* Metres per tile at this zoom and latitude. */
static double w3_tile_span(int z,double lat){
  return 40075016.686*cos(lat*M_PI/180.0)/ldexp(1.0,z);
}
/* On-screen size of a chunk in FBO pixels: span * K / distance, the same projection the SSE uses
 * (K = viewport_height / (2 tan(fov/2))). This is what the texture step is chosen to match, so a
 * chunk's texel density tracks the pixels it actually covers -- and, being the identical arithmetic
 * for a given (z,x,y,camera), it makes w3_walk and w3_children_ready agree by construction rather
 * than by convention. (tx,ty) is the camera's fractional tile coord AT LEVEL z. */
static double w3_chunk_px(int z,long x,long y,double lat,double alt,double tx,double ty){
  double span=w3_tile_span(z,lat);
  double dx=fabs(tx-((double)x+0.5))-0.5, dy=fabs(ty-((double)y+0.5))-0.5;
  if(dx<0)dx=0; if(dy<0)dy=0;
  double horiz=sqrt(dx*dx+dy*dy)*span;
  double dist=sqrt(horiz*horiz+alt*alt); if(dist<1.0) dist=1.0;
  return span*(double)W3_SSE_K/dist;
}
/* --- the tree walk -------------------------------------------------------------------------
 *
 * One rule, and everything else falls out of it:
 *
 *     a chunk draws ITSELF, or its four children -- never both.
 *
 * That is the whole difference from the ring stack. There is no overlap to bias away, so no
 * polygon offset, no draw order, no z-fighting: each square metre belongs to exactly one chunk.
 *
 * Refinement is async and never blocks or leaves a hole. If a chunk wants to split but its
 * children have not arrived, the PARENT keeps drawing while the children load; the swap happens
 * only when all four are resident. "LOD ist doch nur ein VBO pointer" -- exactly: the chunks are
 * already baked and cached, so switching level is picking a different vbo, and a chunk that is not
 * there yet simply is not picked yet.
 *
 * Loading is nearest-first because the walk is depth-first from the camera outward, and fb-tiles
 * has a concurrency cap: whatever asks first gets the network. */
static int w3_split_want=0, w3_split_wait=0;   /* chunks that wanted to split; couldn't (yet) */
static int w3_over=0;                          /* splits refused by the budget -> coarser ground */
static int w3_lvl[8];                          /* chunks drawn per level, W3_ROOTZ..W3_MAXZ */
#ifndef W3_TRACE
#define W3_TRACE 0                             /* 1 = log every stream pass */
#endif

/* Can this chunk be replaced by its four children right now? Asks for them either way -- that ask
 * is what starts the fetch. Each child is requested at the SAME step w3_walk will pick when it
 * recurses into it (w3_chunk_px on the child's own coords), so in production the two callers agree
 * and no size is baked twice. W3_LOD_INJECT bumps the step by one to force the disagreement the
 * side-by-side slots exist to absorb -- the proof control, compiled out otherwise. */
static int w3_children_ready(int z,uint32_t x,uint32_t y,int ci[4],
                             double lat,double alt,double ctx,double cty){
  int ok=1;
  for(int q=0;q<4;q++){
    long cx=x*2+(q&1), cy=y*2+(q>>1);
    int lod=w3_lod_for_px(w3_chunk_px(z+1,cx,cy,lat,alt,ctx,cty));
    if(W3_LOD_INJECT) lod = (lod+1<w3_lod_cap) ? lod+1 : lod-1;
    if(lod<0) lod=0;
    ci[q]=w3_cache_get(z+1,(uint32_t)cx,(uint32_t)cy,lod,0);
    if(ci[q]<0) ok=0;                 /* still in flight: keep drawing the parent */
  }
  return ok;
}
static void w3_emit(int ci,int lod){
  if(ci<0) return;
  /* Dropping here is a HOLE in the ground, not a coarser chunk -- the split guard is supposed to
   * have coarsened long before. Count it loudly: a silent drop here would look exactly like the
   * tree working, which is the one thing this must never do. */
  if(w3_nD>=W3_BUDGET){ w3_over++; return; }
  { int L=w3_cache[ci].z-W3_ROOTZ; if(L>=0&&L<8) w3_lvl[L]++; }
  w3_D[w3_nD].vbo=w3_cache[ci].vbo; w3_D[w3_nD].nverts=w3_cache[ci].nverts;
  w3_D[w3_nD].tex[0]=w3_pick_lod(&w3_cache[ci],W3_GROUND_OSM,lod);
  w3_D[w3_nD].tex[1]=w3_pick_lod(&w3_cache[ci],W3_GROUND_PHOTO,lod); w3_nD++;
}
/* Recurse. (lat,lon,alt) = camera; (tx,ty) = camera's fractional tile coord at THIS level. */
static void w3_walk(int z,long x,long y,double lat,double alt,double tx,double ty){
  long n=1L<<z; if(x<0||y<0||x>=n||y>=n) return;        /* off the map: a real hole, not a gap */
  double span=w3_tile_span(z,lat);
  /* horizontal distance from the camera to the nearest point of this chunk */
  double dx=fabs(tx-((double)x+0.5))-0.5, dy=fabs(ty-((double)y+0.5))-0.5;
  if(dx<0)dx=0; if(dy<0)dy=0;
  double horiz=sqrt(dx*dx+dy*dy)*span;
  if(horiz-span*0.71>W3_REACH) return;                  /* past the drawn world */
  double dist=sqrt(horiz*horiz+alt*alt); if(dist<1.0) dist=1.0;

  /* Texture step for THIS chunk from its own on-screen size -- the same arithmetic
   * w3_children_ready used to request it, so no size is baked twice for one tile. */
  int lod=w3_lod_for_px(span*(double)W3_SSE_K/dist);
  int ci=w3_cache_get(z,(uint32_t)x,(uint32_t)y,lod,(z==W3_ROOTZ));
  if(ci<0) return;                                      /* not here yet; a later frame gets it */

  /* SSE from THIS chunk's own measured error. Flat terrain saturates by itself, mountains
   * subdivide by themselves -- no per-zoom table, no assumption about where we are flying. */
  float sse = w3_cache[ci].err * W3_SSE_K / (float)dist;
  if(z>=W3_MAXZ || sse<=W3_EPS){ w3_emit(ci,lod); return; }

  /* BUDGET: coarsen, never hole. Refusing to split draws this chunk instead -- blurrier, but the
   * ground is still there. Dropping it from the draw list would punch a hole in the world, which
   * is the one thing the tree exists to prevent. */
  if(w3_nD+4>W3_BUDGET){ w3_over++; w3_emit(ci,lod); return; }

  w3_split_want++;
  int cc[4];
  double ctx=tx*2.0, cty=ty*2.0;
  if(!w3_children_ready(z,(uint32_t)x,(uint32_t)y,cc,lat,alt,ctx,cty)){
    w3_split_wait++; w3_ground_dirty=1;                 /* keep the streamer awake for them */
    w3_emit(ci,lod);                                    /* parent covers the ground meanwhile */
    return;
  }
  for(int q=0;q<4;q++) w3_walk(z+1,x*2+(q&1),y*2+(q>>1),lat,alt,ctx,cty);
}

/* Called every frame. Two reasons to do work:
 *   1. the camera moved into a new chunk -> the tree wants a different set
 *   2. the last pass was INCOMPLETE -> chunks were still in flight from fb-tiles
 * (2) is what makes async sourcing work: a chunk that has not arrived is covered by its parent and
 * picked up on a later frame, so the world refines progressively and the frame loop never blocks.
 * Cached chunks cost nothing to re-visit, so retrying is cheap. */
static int w3_stream_done=0;
static void world3d_stream_at(double lat,double lon,double alt_agl){
  if(!w3_osm) return;
  uint32_t tx,ty; if(osmmesh_geo_to_tile(lon,lat,W3_MAXZ,&tx,&ty)!=0) return;
  int moved = !w3_have_tile || tx!=w3_tx || ty!=w3_ty;
  /* W3_LOD_SPIN (proof only, 0 in production): defeat the "converged -> sleep" early-out so the
   * tree walk runs EVERY frame. That is the adversarial condition the LOD-slot proof needs -- the
   * old size-disagreement thrash was 256 mipmaps/frame precisely because the walk ran every frame;
   * once it sleeps, no design re-bakes. Spinning here lets the mipmap counter separate a design
   * that re-bakes under a persistent disagreement (NOKEY) from one that does not (the slots). */
  if(!moved && w3_stream_done && w3_sharpen==0 && !W3_LOD_SPIN) return;   /* climb keeps us awake */
  w3_tx=tx; w3_ty=ty; w3_have_tile=1;
  int b0=w3_cache_bakes, h0=w3_cache_hits;
  w3_ground_dirty=0;                     /* recomputed by the w3_cache_get calls below */
  unsigned mark=w3_touch+1;              /* everything the walk touches gets touch >= mark */
  w3_pass_mark=mark;                     /* want_lod_max resets off this on a chunk's first touch */

  w3_nD=0; w3_split_want=0; w3_split_wait=0; w3_over=0; w3_pending=0; w3_sharpen=0;
  for(int i=0;i<8;i++) w3_lvl[i]=0;
  double rtx,rty; w3_geo_to_tile_f(lat,lon,W3_ROOTZ,&rtx,&rty);
  /* Roots: enough W3_ROOTZ chunks to cover W3_REACH. The tree prunes the rest immediately. */
  int rad=(int)ceil(W3_REACH/w3_tile_span(W3_ROOTZ,lat))+1;
  for(int dy=-rad;dy<=rad;dy++)for(int dx=-rad;dx<=rad;dx++)
    w3_walk(W3_ROOTZ,(long)rtx+dx,(long)rty+dy,lat,alt_agl,rtx,rty);

  w3_cache_trim(mark);                   /* whatever the walk did not ask for is unreachable */

  int was_done = w3_stream_done;
  /* "Done" means the tree got everything it asked for AND every chunk shows the albedo we want.
   * Without the second half a TAB only took effect at the next chunk boundary, because this
   * function returns early once done and w3_cache_get, which does the upgrading, never ran again.
   * The tiles were on disk the whole time; we had simply stopped asking. */
  /* w3_nD>0 is not pedantry: "nothing is waiting" is also true of a tree that asked for nothing.
   * Without it the first pass over a cold region -- where every chunk is still in flight -- sets
   * done=1, this function returns early forever, and the world stays empty with no error anywhere.
   * That is exactly what happened: "0 chunks drawn, 0 waiting", then silence. */
  /* "Done" is the FLOOR criterion: every chunk drawable (256 present), nothing waiting. Sharpening
   * up the ramp (w3_sharpen) is deliberately NOT here -- it keeps the streamer awake (the early-out
   * above) but must not hold the gate, or a moving camera forever mid-climb on a fresh 2048 would
   * never let "0 pending" latch. */
  w3_stream_done = w3_nD>0 && w3_pending==0 && w3_split_wait==0 && !w3_ground_dirty;
  /* Report on a transition, or when the sharpening count CHANGES (a step landed) -- not every frame
   * while it merely stays nonzero, which under motion would be every frame. */
  static int w3_last_sharpen=-1;
  if(moved || (w3_stream_done && !was_done) || w3_sharpen!=w3_last_sharpen || W3_TRACE){
    w3_last_sharpen=w3_sharpen;
    printf("[world3d] quadtree: %d chunks drawn (budget %d), %d wanted split, %d waiting,"
           " %d pending, %d sharpening, %d over budget | levels %d/%d/%d/%d/%d/%d/%d | baked %d, cached %d, evicted %d, vram %ldMB%s\n",
           w3_nD,W3_BUDGET,w3_split_want,w3_split_wait,w3_pending,w3_sharpen,w3_over,
           w3_lvl[0],w3_lvl[1],w3_lvl[2],w3_lvl[3],w3_lvl[4],w3_lvl[5],w3_lvl[6],
           w3_cache_bakes-b0,w3_cache_hits-h0,w3_cache_evict,w3_texvram()/(1024*1024),
           w3_stream_done?"":" (waiting on fb-tiles)");
  }
}
#endif /* W3_USE_OSM */

/* Switch the ground albedo source. Deliberately throws NOTHING away: each cached tile carries the
 * mode it was baked from, and w3_cache_get re-bakes it in place once the new albedo is on hand.
 * The geometry (VBO) is identical either way, so only the texture is redone.
 *
 * The first version deleted every baked tile here. That emptied the world until everything had
 * re-baked -- which reads as "the switch takes forever" even though the tiles were already on the
 * server's disk in under a millisecond. A fallback view you reach for when the camera dies must
 * not begin by removing the view. */
static void w3_ground_toggle(void){
  /* Both textures are already resident, so this really is just an index flip -- no re-bake, no
   * flush, no network. Earlier versions re-baked here and the switch took about a minute, because
   * world3d_stream returns early once everything is loaded and only wakes when the aircraft
   * crosses a tile boundary (~1 min in a 1000 m orbit). The pictures were on disk the whole time. */
  w3_ground_mode = (w3_ground_mode==W3_GROUND_OSM) ? W3_GROUND_PHOTO : W3_GROUND_OSM;
  printf("[world3d] ground = %s\n", w3_ground_mode==W3_GROUND_PHOTO?"aerial photo":"OSM render");
}

/* ---- HUD (2D lines, pixel coords) ---- */
/* Regenerated every frame (glBufferData DYNAMIC). Sized for the full OSD: the bitmap
 * font draws ~2 line segments per lit pixel, so all the text + arrows + ladders add up
 * to a few thousand segments. Too small a buffer silently drops the LAST-drawn elements
 * (home arrow, glideslope). 65536 floats = ~6500 segments, comfortably above the OSD. */
static float w3_hud[65536]; static int w3_hudN;
static void w3_line(float x0,float y0,float x1,float y1,float r,float g,float b){ if(w3_hudN>65516)return;
  w3_hud[w3_hudN++]=x0;w3_hud[w3_hudN++]=y0;w3_hud[w3_hudN++]=r;w3_hud[w3_hudN++]=g;w3_hud[w3_hudN++]=b;
  w3_hud[w3_hudN++]=x1;w3_hud[w3_hudN++]=y1;w3_hud[w3_hudN++]=r;w3_hud[w3_hudN++]=g;w3_hud[w3_hudN++]=b; }
static const char*W3_CS=" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/";
static const unsigned char W3_FONT[41][5]={
 {0,0,0,0,0},{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
 {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,7,4,7},{7,4,7,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},{5,5,6,5,5},{4,4,4,4,7},
 {5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},{7,5,7,4,4},{7,5,5,7,1},{6,5,6,5,5},{7,4,7,1,7},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7},
 {0,0,7,0,0},{0,0,0,0,2},{0,2,0,2,0},{1,1,2,4,4}};
static void w3_gpx(float x,float y,float s,float r,float g,float b){ w3_line(x,y,x+s,y,r,g,b); w3_line(x,y+s*0.5f,x+s,y+s*0.5f,r,g,b); }
static void w3_text(float x,float y,float s,float r,float g,float b,const char*t){
  for(;*t;t++){ char u=*t; if(u>='a'&&u<='z')u-=32; const char*p=strchr(W3_CS,u); int ix=p?(int)(p-W3_CS):0;
    for(int row=0;row<5;row++){ unsigned char m=W3_FONT[ix][row]; for(int c=0;c<3;c++) if(m&(4>>c)) w3_gpx(x+c*s,y+row*s,s,r,g,b);} x+=4*s; } }
static void w3_printf(float x,float y,float s,float r,float g,float b,const char*fmt,...){ char bb[96]; va_list a; va_start(a,fmt); vsnprintf(bb,96,fmt,a); va_end(a); w3_text(x,y,s,r,g,b,bb); }
static const char*W3_STN[]={"DISARM","ARMED","CLIMB","LOITER","MANUAL","RTH"};
/* Full OSD: every telemetry field, computed/derived correctly. The bitmap font only
 * has [ 0-9 A-Z - . : / ], so no '%'/'+': percent is implied by the label, sign shown
 * via '-' plus colour (green=climb/good, amber=caution, red=warning). */
static void w3_build_hud(const telem_packet_t*t,int W,int H,int have){
  w3_hudN=0; float cx=W/2,cy=H/2;
  w3_line(cx-24,cy,cx-8,cy,0.4f,1,0.4f); w3_line(cx+8,cy,cx+24,cy,0.4f,1,0.4f); w3_line(cx,cy-8,cx,cy+8,0.4f,1,0.4f);
  if(!have){ w3_text(cx-60,30,3,1,0.8f,0.2f,"NO TELEMETRY"); return; }
  float hdg=t->yaw<0?t->yaw+360:t->yaw;
  /* left column: flight state */
  w3_printf(14, 14,3,1,1,1,      "ALT %5.0f",t->alt);
  w3_printf(14, 34,3,0.7f,0.9f,1,"AS  %5.1f",t->airspeed);
  w3_printf(14, 54,3,1,1,1,      "GS  %5.1f",t->gs);
  { float v=t->vs, r=1,g=1,b=1; if(v>0.4f){r=0.4f;g=1;b=0.4f;} else if(v<-2.0f){r=1;g=0.7f;b=0.2f;}
    w3_printf(14,74,3,r,g,b,     "VS  %5.1f",v); }
  w3_printf(14, 94,3,1,1,1,      "HDG %5.0f",hdg);
  /* right column: navigation / power / link / mode */
  w3_printf(W-176,14,3,1,1,1,    "HOME %5.0f",t->home_dist);
  { float v=t->batt, r=0.4f,g=1,b=0.3f; if(v<10.0f){r=1;g=0.3f;b=0.2f;} else if(v<11.0f){r=1;g=0.85f;b=0.2f;}
    w3_printf(W-176,34,3,r,g,b,  "BAT %4.1fV",v); }
  { int q=t->rssi; float r=0.4f,g=1,b=0.3f; if(q<25){r=1;g=0.3f;b=0.2f;} else if(q<50){r=1;g=0.85f;b=0.2f;}
    w3_printf(W-176,54,3,r,g,b,  "LNK %4d",q); }
  { int rth=(t->state==5||t->state==3);
    w3_printf(W-176,74,3,rth?1:0.4f,rth?0.85f:1,rth?0.2f:0.4f,"%s",W3_STN[t->state%6]); }
  /* Vision source, in the avionics idiom:
   *   EVS = Enhanced Vision System  -- a real sensor image (today the aerial photo; the real
   *                                    camera feed is the point of the switch)
   *   SVS = Synthetic Vision System -- terrain drawn from a database (our OSM render)
   * Which one you are on matters because the synthetic view is what you fall back to when the
   * sensor cannot deliver: signal lost, sensor dead, too dark, blinded. Not colour-coded as a
   * warning: right now it is a deliberate choice (TAB), not a failure. */
  { int evs=(w3_ground_mode==W3_GROUND_PHOTO);
    w3_printf(W-176,94,3, evs?0.4f:0.5f, evs?1.0f:0.85f, evs?0.4f:1.0f, "VIS %s", evs?"EVS":"SVS"); }
  /* attitude + environment (bottom) */
  w3_printf(14,H-44,2,0.8f,0.8f,0.9f,"ROLL %4.0f   PITCH %4.0f",t->roll,t->pitch);
  w3_printf(14,H-24,2,0.7f,0.85f,0.7f,"CLD %3.0f  VIS %4.1fKM  SUN %3.0f  MOON %3.0f",
            t->cloud*100.f,t->vis/1000.f,t->sun_el,t->moon_phase*100.f);
  /* home-direction arrow (top center) */
  float a=t->home_bearing*(float)M_PI/180.f,hx=cx,hy=110,len=34,tx=hx+sinf(a)*len,ty=hy-cosf(a)*len;
  w3_line(hx,hy,tx,ty,1,0.85f,0.2f); w3_line(tx,ty,tx+sinf(a+2.6f)*10,ty-cosf(a+2.6f)*10,1,0.85f,0.2f); w3_line(tx,ty,tx+sinf(a-2.6f)*10,ty-cosf(a-2.6f)*10,1,0.85f,0.2f);
  w3_text(cx-10,hy+16,2,1,0.85f,0.2f,"HOME");
  /* glideslope ladder (right of center) */
  float gx=W-52,gy=cy; for(int i=-2;i<=2;i++) w3_line(gx-8,gy+i*26,gx+8,gy+i*26,1,1,1);
  float dy=-t->glideslope_err*5; if(dy<-52)dy=-52; if(dy>52)dy=52;
  w3_line(gx-6,gy+dy-5,gx+6,gy+dy-5,1,0.85f,0.2f); w3_line(gx+6,gy+dy-5,gx+6,gy+dy+5,1,0.85f,0.2f);
  w3_line(gx+6,gy+dy+5,gx-6,gy+dy+5,1,0.85f,0.2f); w3_line(gx-6,gy+dy+5,gx-6,gy+dy-5,1,0.85f,0.2f);
}

/* ---- init + render ----
 * Compiles shaders + HUD buffer. Geometry is provided by world3d_stream()
 * (live osmmesh) when an archive has been opened; otherwise a procedural world
 * is built as a standalone fallback. */
static void world3d_init(void){
  /* Trim the texture-LOD ramp to what this context can actually hold. WebGL2 guarantees >= 2048,
   * so normally all four steps survive; a spec-minimum context simply never asks for a size it
   * cannot bind, rather than failing the glTexImage2D silently at draw time. */
  { GLint mx=2048; glGetIntegerv(GL_MAX_TEXTURE_SIZE,&mx);
    int cap=0; while(cap<W3_NLOD && w3_lod_px[cap]<=(int)mx) cap++;
    if(cap>W3_LOD_MAXSTEPS) cap=W3_LOD_MAXSTEPS;    /* policy ceiling: 2048 is gated (see table) */
    if(cap<1) cap=1; w3_lod_cap=cap;
    printf("[world3d] LOD ramp: %d steps, top %d px (MAX_TEXTURE_SIZE %d)\n",cap,w3_lod_px[cap-1],(int)mx); }
  w3_pW=w3_prog(W3_VSW,W3_FSW); w3_wPos=glGetAttribLocation(w3_pW,"aPos"); w3_wCol=glGetAttribLocation(w3_pW,"aCol"); w3_wMVP=glGetUniformLocation(w3_pW,"uMVP");
  w3_wHaze=glGetUniformLocation(w3_pW,"uHaze"); w3_wLight=glGetUniformLocation(w3_pW,"uLight");
  w3_pH=w3_prog(W3_VSH,W3_FSH); w3_hPos=glGetAttribLocation(w3_pH,"aPos"); w3_hCol=glGetAttribLocation(w3_pH,"aCol"); w3_hScale=glGetUniformLocation(w3_pH,"uScale");
  glGenBuffers(1,&w3_hVBO);
  /* sky dome program + a fullscreen quad (two triangles in NDC) */
  w3_pSky=w3_prog(W3_VSKY,W3_FSKY);
  w3_skPos=glGetAttribLocation(w3_pSky,"aPos");
  w3_skF=glGetUniformLocation(w3_pSky,"uF"); w3_skS=glGetUniformLocation(w3_pSky,"uS"); w3_skU=glGetUniformLocation(w3_pSky,"uU");
  w3_skTan=glGetUniformLocation(w3_pSky,"uTan"); w3_skAsp=glGetUniformLocation(w3_pSky,"uAsp");
  w3_skSun=glGetUniformLocation(w3_pSky,"uSun"); w3_skMoon=glGetUniformLocation(w3_pSky,"uMoon");
  w3_skMoonPh=glGetUniformLocation(w3_pSky,"uMoonPh"); w3_skCloud=glGetUniformLocation(w3_pSky,"uCloud");
  { float q[12]={-1,-1, 1,-1, -1,1,  -1,1, 1,-1, 1,1}; glGenBuffers(1,&w3_skyVBO);
    glBindBuffer(GL_ARRAY_BUFFER,w3_skyVBO); glBufferData(GL_ARRAY_BUFFER,sizeof q,q,GL_STATIC_DRAW); }
  w3_pStar=w3_prog(W3_VSTAR,W3_FSTAR);
  w3_stPos=glGetAttribLocation(w3_pStar,"aPos"); w3_stMag=glGetAttribLocation(w3_pStar,"aMag");
  w3_stMVP=glGetUniformLocation(w3_pStar,"uMVP"); w3_stDay=glGetUniformLocation(w3_pStar,"uDay");
  glGenBuffers(1,&w3_starVBO);
#ifdef W3_USE_OSM
  w3_pWT=w3_prog(W3_VSWT,W3_FSWT); w3_wtPos=glGetAttribLocation(w3_pWT,"aPos"); w3_wtUV=glGetAttribLocation(w3_pWT,"aUV");
  w3_wtMVP=glGetUniformLocation(w3_pWT,"uMVP"); w3_wtTex=glGetUniformLocation(w3_pWT,"uTex");
  w3_wtHaze=glGetUniformLocation(w3_pWT,"uHaze"); w3_wtLight=glGetUniformLocation(w3_pWT,"uLight");
  w3_wtNorm=glGetAttribLocation(w3_pWT,"aNorm"); w3_wtSun=glGetUniformLocation(w3_pWT,"uSun");
  if(w3_osm) return;              /* geometry (textured tiles) comes from world3d_stream() */
#endif
  w3_build_procedural();
}
/* Render just the 3D world (the aircraft "camera image") into the bound framebuffer.
 * The HUD is drawn separately (world3d_render_hud) so it can be overlaid on top of the
 * decoded video, not encoded into it. */
static void world3d_render_scene(const telem_packet_t*t,int W,int H,int have){
  float RAD=(float)M_PI/180.f;
  glViewport(0,0,W,H); glEnable(GL_DEPTH_TEST); glClearColor(0.55f,0.70f,0.90f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  float px=have?t->x:0, py=(have&&t->alt>2)?t->alt:120, pz=have?-t->y:0;
#ifdef W3_USE_OSM
  if(w3_nD>0) py=(have&&t->alt>1?t->alt:2)+w3_yoff;   /* AGL above the osmmesh ground */
#endif
  /* Basis and MVP from the attitude — pure maths, so it lives in camera.h and is testable there.
   * The roll sign is the one that once made a right bank look like a left bank; a screenshot
   * cannot catch that, a test can. The eye position stays HERE because it needs w3_yoff, which
   * belongs to the tile side. The aliases keep every consumer below unchanged. */
  const float eye[3]={px,py,pz};
  const w3_cam C = w3_cam_from(have?t->yaw:0, have?t->pitch:0, have?t->roll:0,
                               eye, W3_FOV, (float)W/H, W3_NEAR, W3_FARPLANE);
  const float *f=C.f, *sr=C.sr, *up=C.up, *mvp=C.mvp;

  /* ---- environment: sun/moon direction, sky colour, light level ----
   * The arithmetic lives in atmo.h because it is pure and therefore testable, and because these
   * values are NOT the sky's: `haze`/`light`/`sun` are read by the terrain pass below and `haze`
   * again by the buildings. They were locals that three passes happened to share -- per-frame
   * state with no name and no owner. The names below are aliases so that every consumer stays
   * exactly as it was; threading `A` through them is a separate step with its own proof. */
  const w3_atmo A = w3_atmo_from(t, have);
  const float *sun=A.sun, *moon=A.moon, *haze=A.haze;
  const float day=A.day, light=A.light, cloud=A.cloud, moon_ph=A.moon_ph;
  /* draw the sky first, depth writes off, so terrain paints over it */
  glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST);
  glUseProgram(w3_pSky);
  glUniform3fv(w3_skF,1,f); glUniform3fv(w3_skS,1,sr); glUniform3fv(w3_skU,1,up);
  glUniform1f(w3_skTan,tanf(W3_FOV*RAD*0.5f)); glUniform1f(w3_skAsp,(float)W/H);
  glUniform3fv(w3_skSun,1,sun); glUniform3fv(w3_skMoon,1,moon);
  glUniform1f(w3_skMoonPh,moon_ph); glUniform1f(w3_skCloud,cloud);
  glBindBuffer(GL_ARRAY_BUFFER,w3_skyVBO); glEnableVertexAttribArray(w3_skPos);
  glVertexAttribPointer(w3_skPos,2,GL_FLOAT,GL_FALSE,0,0); glDrawArrays(GL_TRIANGLES,0,6);
  glDisableVertexAttribArray(w3_skPos);
  /* real stars: place each above-horizon catalogue star at its true alt/az (from wall-clock
   * sidereal time + origin), far along that direction, additively blended, fading toward day. */
  if(day<0.6f){
    /* The celestial maths is in stars.h because it is pure and therefore checkable — Polaris must
     * stand at the observer's latitude, due north, and nothing in a screenshot says whether it
     * does. The clock is passed IN rather than read there: an input, not a dependency. */
    double lst=w3_lst_deg(w3_gmst_deg((double)time(NULL)), w3_olon);
    static float sv[W3_NSTARS*4]; int ns=0;
    for(int i=0;i<W3_NSTARS;i++){
      w3_stardir d=w3_star_dir(lst,w3_olat,W3_STARS[i].ra,W3_STARS[i].dec);
      if(!d.above) continue;
      sv[ns*4]=eye[0]+d.e*40000.f; sv[ns*4+1]=eye[1]+d.u*40000.f; sv[ns*4+2]=eye[2]-d.n*40000.f;
      sv[ns*4+3]=W3_STARS[i].mag; ns++;
    }
    if(ns>0){
      glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
      glUseProgram(w3_pStar); glUniformMatrix4fv(w3_stMVP,1,GL_FALSE,mvp); glUniform1f(w3_stDay,day);
      glBindBuffer(GL_ARRAY_BUFFER,w3_starVBO); glBufferData(GL_ARRAY_BUFFER,(size_t)ns*16,sv,GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(w3_stPos); glEnableVertexAttribArray(w3_stMag);
      glVertexAttribPointer(w3_stPos,3,GL_FLOAT,GL_FALSE,16,0);
      glVertexAttribPointer(w3_stMag,1,GL_FLOAT,GL_FALSE,16,(void*)12);
      glDrawArrays(GL_POINTS,0,ns);
      glDisableVertexAttribArray(w3_stPos); glDisableVertexAttribArray(w3_stMag);
      glDisable(GL_BLEND);
    }
  }
  glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
#ifdef W3_USE_OSM
  if(w3_nD>0){                       /* textured terrain: one quadtree cut, one draw per chunk */
    glUseProgram(w3_pWT); glUniformMatrix4fv(w3_wtMVP,1,GL_FALSE,mvp);
    glUniform3fv(w3_wtHaze,1,haze); glUniform1f(w3_wtLight,light); glUniform3fv(w3_wtSun,1,sun);
    glActiveTexture(GL_TEXTURE0); glUniform1i(w3_wtTex,0);
    glEnableVertexAttribArray(w3_wtPos); glEnableVertexAttribArray(w3_wtUV); glEnableVertexAttribArray(w3_wtNorm);
    /* No polygon offset, no draw order, no coarse-to-fine: the chunks in w3_D are a CUT through
     * the tree, so they tile the ground without overlapping. There is nothing to bias apart. */
    for(int i=0;i<w3_nD;i++){
      /* THE ground switch, in full: an index. Both albedos are already on the GPU. */
      GLuint _t=w3_D[i].tex[w3_ground_mode]; if(!_t)_t=w3_D[i].tex[W3_GROUND_OSM];
      glBindTexture(GL_TEXTURE_2D,_t); glBindBuffer(GL_ARRAY_BUFFER,w3_D[i].vbo);
      glVertexAttribPointer(w3_wtPos,3,GL_FLOAT,GL_FALSE,W3_VTX_STRIDE,W3_VTX_OFF(pos));
      glVertexAttribPointer(w3_wtUV,2,GL_FLOAT,GL_FALSE,W3_VTX_STRIDE,W3_VTX_OFF(uv));
      glVertexAttribPointer(w3_wtNorm,3,GL_FLOAT,GL_FALSE,W3_VTX_STRIDE,W3_VTX_OFF(norm));
      glDrawArrays(GL_TRIANGLES,0,w3_D[i].nverts);
    }
    glDisableVertexAttribArray(w3_wtUV); glDisableVertexAttribArray(w3_wtNorm);
  } else
#endif
  { glUseProgram(w3_pW); glUniformMatrix4fv(w3_wMVP,1,GL_FALSE,mvp); glUniform3fv(w3_wHaze,1,haze); glUniform1f(w3_wLight,light); glEnableVertexAttribArray(w3_wPos); glEnableVertexAttribArray(w3_wCol);
    glBindBuffer(GL_ARRAY_BUFFER,w3_vTerr); glVertexAttribPointer(w3_wPos,3,GL_FLOAT,GL_FALSE,24,0); glVertexAttribPointer(w3_wCol,3,GL_FLOAT,GL_FALSE,24,(void*)12); glDrawArrays(GL_TRIANGLES,0,w3_nTerr);
    glBindBuffer(GL_ARRAY_BUFFER,w3_vBld); glVertexAttribPointer(w3_wPos,3,GL_FLOAT,GL_FALSE,24,0); glVertexAttribPointer(w3_wCol,3,GL_FLOAT,GL_FALSE,24,(void*)12); glDrawArrays(GL_TRIANGLES,0,w3_nBld);
    glDisableVertexAttribArray(w3_wCol); }
}
/* Draw the 2D HUD (line overlay) into the bound framebuffer at W×H pixel coords. */
static void world3d_render_hud(const telem_packet_t*t,int W,int H,int have){
  glViewport(0,0,W,H); glDisable(GL_DEPTH_TEST); w3_build_hud(t,W,H,have);
  glBindBuffer(GL_ARRAY_BUFFER,w3_hVBO); glBufferData(GL_ARRAY_BUFFER,w3_hudN*4,w3_hud,GL_DYNAMIC_DRAW);
  glUseProgram(w3_pH); glUniform2f(w3_hScale,2.0f/W,2.0f/H); glEnableVertexAttribArray(w3_hPos); glEnableVertexAttribArray(w3_hCol);
  glVertexAttribPointer(w3_hPos,2,GL_FLOAT,GL_FALSE,20,0); glVertexAttribPointer(w3_hCol,3,GL_FLOAT,GL_FALSE,20,(void*)8); glDrawArrays(GL_LINES,0,w3_hudN/5);
}
/* There was a world3d_render() here that called scene+HUD back to back, "for the native offscreen
 * renderer". That renderer is gone (f36f147) and it was the last caller, so the function outlived
 * its only purpose. It is deliberately NOT replaced by a convenience wrapper: scene and HUD must
 * stay separate, because the video codec runs BETWEEN them (cc.c) -- the HUD is overlaid on the
 * decoded frame, never encoded into it. A wrapper would suggest a coupling whose absence is the
 * whole point of the video pipeline. */
#endif
