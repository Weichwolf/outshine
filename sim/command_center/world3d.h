/* FlightBox — shared 3D world + HUD renderer (GLES2/WebGL).
 * Included by both the WASM command center (cc.c) and the native offscreen
 * renderer (render_native.c) so the browser and the headless screenshot draw
 * exactly the same thing. Caller provides the GL context. */
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
/* OSM cartography (kind -> colour/width): GL-free, unit-tested in test/unit/test_style.c */
#include "gfx/style.h"

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
#ifndef W3_Z
#define W3_Z 14                    /* streaming zoom level */
#endif
#ifndef W3_RAD
#define W3_RAD 2                   /* tile radius around the aircraft (grid = 2R+1) */
#endif
#ifndef W3_TEX
#define W3_TEX 1024                /* per-tile ortho texture resolution (detail comes from here) */
#endif
#ifndef W3_TERR
#define W3_TERR 24                 /* terrain geometry: coarse GxG grid per tile (heightfield
                                    * shape only — the texture carries the detail). Small = fast. */
#endif
/* Distant terrain LOD: a wide ring of low-zoom tiles gives the 30-40 km horizon
 * without streaming thousands of z14 tiles. Coarse terrain + landcover texture only. */
#ifndef W3_FARZ
#define W3_FARZ 11                 /* far tier zoom (each tile ~20 km at z11) */
#endif
#ifndef W3_FARRAD
#define W3_FARRAD 2                /* far tier radius (2 -> 5x5 tiles ~ 100 km across) */
#endif
#ifndef W3_FARTEX
#define W3_FARTEX 256              /* far tier texture resolution (coarse; power-of-two for mipmaps) */
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

#define W3_MAXT ((2*W3_RAD+1)*(2*W3_RAD+1))
#define W3_MAXTF ((2*W3_FARRAD+1)*(2*W3_FARRAD+1))
typedef struct { GLuint vbo, tex; int nverts; } w3_tileGL;
static w3_tileGL w3_T[W3_MAXT];  static int w3_nT=0;    /* near tier (z14, detailed) */
static w3_tileGL w3_TF[W3_MAXTF]; static int w3_nTF=0;  /* far tier (low zoom, coarse) */

/* --- software raster into an RGB image (tile-local coords * sc) --- */
static void w3_px(uint8_t*im,int W,int H,int x,int y,uint8_t r,uint8_t g,uint8_t b){
  if((unsigned)x<(unsigned)W&&(unsigned)y<(unsigned)H){ uint8_t*p=im+((size_t)y*W+x)*3; p[0]=r;p[1]=g;p[2]=b; } }
static void w3_disk(uint8_t*im,int W,int H,float cx,float cy,float rad,uint8_t r,uint8_t g,uint8_t b){
  int x0=(int)(cx-rad),x1=(int)(cx+rad),y0=(int)(cy-rad),y1=(int)(cy+rad); float rr=rad*rad;
  for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){ float dx=x-cx,dy=y-cy; if(dx*dx+dy*dy<=rr) w3_px(im,W,H,x,y,r,g,b); } }
static void w3_thick(uint8_t*im,int W,int H,float x0,float y0,float x1,float y1,float w,uint8_t r,uint8_t g,uint8_t b){
  float dx=x1-x0,dy=y1-y0,len=sqrtf(dx*dx+dy*dy); int n=(int)len+1; float rad=w*0.5f; if(rad<0.6f)rad=0.6f;
  for(int i=0;i<=n;i++){ float t=(float)i/n; w3_disk(im,W,H,x0+dx*t,y0+dy*t,rad,r,g,b); } }
/* scanline fill of an MVT polygon (all rings, even-odd -> holes carved out) */
static void w3_fill(uint8_t*im,int W,int H,const osmmesh_mvt_feature*ft,float sc,uint8_t r,uint8_t g,uint8_t b){
  const osmmesh_mvt_coord*co=ft->coords; size_t nco=ft->n_coords; if(nco<3) return;
  float ymin=1e9f,ymax=-1e9f; for(size_t i=0;i<nco;i++){ float y=co[i].y*sc; if(y<ymin)ymin=y; if(y>ymax)ymax=y; }
  int iy0=(int)floorf(ymin); if(iy0<0)iy0=0; int iy1=(int)ceilf(ymax); if(iy1>=H)iy1=H-1;
  size_t nr=ft->n_rings?ft->n_rings:1; float xs[512];
  for(int y=iy0;y<=iy1;y++){ float yc=y+0.5f; int nx=0;
    for(size_t ring=0;ring<nr;ring++){
      size_t a=ft->n_rings?ft->ring_offsets[ring]:0, bb=ft->n_rings?ft->ring_offsets[ring+1]:nco;
      for(size_t k=a;k<bb;k++){ size_t k2=(k+1<bb)?k+1:a;
        float ya=co[k].y*sc, yb=co[k2].y*sc, xa=co[k].x*sc, xb=co[k2].x*sc;
        if((ya<=yc&&yb>yc)||(yb<=yc&&ya>yc)){ float xi=xa+(yc-ya)/(yb-ya)*(xb-xa); if(nx<512)xs[nx++]=xi; } } }
    for(int i=1;i<nx;i++){ float v=xs[i]; int j=i-1; while(j>=0&&xs[j]>v){xs[j+1]=xs[j];j--;} xs[j+1]=v; }
    for(int i=0;i+1<nx;i+=2){ int xa=(int)ceilf(xs[i]-0.5f); if(xa<0)xa=0; int xb=(int)floorf(xs[i+1]-0.5f); if(xb>=W)xb=W-1;
      for(int x=xa;x<=xb;x++) w3_px(im,W,H,x,y,r,g,b); } } }
static void w3_drawline(uint8_t*im,int W,int H,const osmmesh_mvt_feature*ft,float sc,float w,uint8_t r,uint8_t g,uint8_t b){
  const osmmesh_mvt_coord*co=ft->coords; size_t nr=ft->n_rings?ft->n_rings:1;
  for(size_t ring=0;ring<nr;ring++){ size_t a=ft->n_rings?ft->ring_offsets[ring]:0, bb=ft->n_rings?ft->ring_offsets[ring+1]:ft->n_coords;
    for(size_t k=a+1;k<bb;k++) w3_thick(im,W,H,co[k-1].x*sc,co[k-1].y*sc,co[k].x*sc,co[k].y*sc,w,r,g,b); } }

static const osmmesh_mvt_layer* w3_layer(osmmesh_mvt_tile*t,const char*name){
  size_t nl=osmmesh_mvt_num_layers(t);
  for(size_t i=0;i<nl;i++){ const osmmesh_mvt_layer*l=osmmesh_mvt_layer_at(t,i); if(!strcmp(osmmesh_mvt_layer_name(l),name)) return l; }
  return 0; }
static const char* w3_kind(const osmmesh_mvt_layer*l,const osmmesh_mvt_feature*f){
  osmmesh_mvt_value v; if(osmmesh_mvt_feature_get_tag(l,f,"kind",&v)==0&&v.type==OSMMESH_MVT_VAL_STRING) return v.v.s; return ""; }

/* bake one tile's OSM vector data into an orthographic RGB texture -> GL texture id */
/* Raw vector bytes for texture baking: from fb-tiles via the provider when we stream, else
 * from the legacy preloaded archive. Returns 1 and hands over a malloc'd buffer. */
static int w3_vec_bytes(uint32_t z,uint32_t x,uint32_t y,uint8_t**d,size_t*n){
  if(w3_stream_tiles) return w3_tile_provider(0,OSMMESH_TILE_VECTOR,z,x,y,d,n);
  return (w3_vec && osmmesh_pmtiles_fetch(w3_vec,z,x,y,d,n)==OSMMESH_PMTILES_OK);
}
/* ---- ground albedo source: OSM cartography or aerial photo (TAB) --------------------------
 * The renderer keeps BOTH because they are not competing textures: the photo is what a camera
 * would see, the OSM render is what we can draw when a camera cannot -- lost signal, dead sensor,
 * blinded by the sun, or simply night. Synthetic vision as a fallback, which is why the vector
 * path must stay first-class rather than become a legacy branch. */
enum { W3_GROUND_OSM = 0, W3_GROUND_PHOTO = 1 };
static int w3_ground_mode = W3_GROUND_OSM;

/* Fill a tile texture from aerial photos. Returns 0 if any child is still in flight, so the
 * caller can retry rather than cache a half-empty tile forever.
 *
 * The zoom arithmetic is exact, not a resample: a 256 px imagery tile is one child, so a TS-px
 * texture wants (TS/256)^2 children at zoom z + log2(TS/256). For the near tier that is z14 ->
 * 4x4 z16 tiles = exactly 1024 px = W3_TEX; for the far tier, z11 at 256 px = one tile. No
 * scaling, a straight blit. */
static int w3_fill_imagery(uint8_t*im,int TS,uint32_t z,uint32_t x,uint32_t y){
  int f=TS/256; if(f<1) f=1;
  uint32_t zi=z; for(int t=f;t>1;t>>=1) zi++;
  for(int j=0;j<f;j++) for(int i=0;i<f;i++){
    uint32_t cx=x*(uint32_t)f+(uint32_t)i, cy=y*(uint32_t)f+(uint32_t)j;
    int n=w3_tiles_size(W3_TILE_IMAGERY,(int)zi,(int)cx,(int)cy);
    if(n<0) return 0;                       /* still fetching -> tell the caller to retry */
    if(n==0) continue;                      /* a genuine hole: leave the base colour */
    uint8_t*b=(uint8_t*)malloc((size_t)n); if(!b) return 0;
    w3_tiles_copy(W3_TILE_IMAGERY,(int)zi,(int)cx,(int)cy,b);
    int w=0,h=0,comp=0; uint8_t*px=stbi_load_from_memory(b,n,&w,&h,&comp,3);
    free(b);
    if(!px) continue;                       /* undecodable tile: hole, not a crash */
    for(int yy=0;yy<h;yy++){ int dy=j*256+yy; if(dy>=TS) break;
      for(int xx=0;xx<w;xx++){ int dx=i*256+xx; if(dx>=TS) break;
        uint8_t*d=im+((size_t)dy*TS+dx)*3, *sp=px+((size_t)yy*w+xx)*3;
        d[0]=sp[0]; d[1]=sp[1]; d[2]=sp[2]; } }
    stbi_image_free(px);
  }
  return 1;
}

/* Returns 0 when the tile is not bakeable yet (imagery still streaming). */
static GLuint w3_bake(uint32_t z,uint32_t x,uint32_t y,int TS){
  uint8_t*im=malloc((size_t)TS*TS*3);
  for(int i=0;i<TS*TS;i++){ im[i*3]=150;im[i*3+1]=178;im[i*3+2]=118; }   /* base ground */
  if(w3_ground_mode==W3_GROUND_PHOTO && !w3_fill_imagery(im,TS,z,x,y)){
    free(im); return 0;                    /* children still in flight; retry next frame */
  }
  uint8_t*d=0; size_t n=0;
  if(w3_ground_mode==W3_GROUND_OSM && w3_vec_bytes(z,x,y,&d,&n)){
    osmmesh_mvt_tile*t=0;
    if(osmmesh_mvt_decode(d,n,&t)==OSMMESH_MVT_OK){
      const osmmesh_mvt_layer*L; uint8_t r,g,b; int rail;
      /* landcover polygons -> land-use colours */
      if((L=w3_layer(t,"land"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type==OSMMESH_MVT_GEOM_POLYGON){ w3_landcolor(w3_kind(L,ft),&r,&g,&b); w3_fill(im,TS,TS,ft,sc,r,g,b); } } }
      /* water */
      if((L=w3_layer(t,"water_polygons"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type==OSMMESH_MVT_GEOM_POLYGON) w3_fill(im,TS,TS,ft,sc,92,140,190); } }
      /* parking / sites */
      if((L=w3_layer(t,"sites"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type==OSMMESH_MVT_GEOM_POLYGON) w3_fill(im,TS,TS,ft,sc,175,175,180); } }
      /* pedestrian paved areas */
      if((L=w3_layer(t,"street_polygons"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type==OSMMESH_MVT_GEOM_POLYGON) w3_fill(im,TS,TS,ft,sc,208,203,193); } }
      /* building footprints */
      if((L=w3_layer(t,"buildings"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type==OSMMESH_MVT_GEOM_POLYGON) w3_fill(im,TS,TS,ft,sc,128,114,102); } }
      /* roads then rails (2nd pass keeps rails visible) then rivers */
      if((L=w3_layer(t,"streets"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(int pass=0;pass<2;pass++) for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type!=OSMMESH_MVT_GEOM_LINESTRING) continue; float w=w3_roadstyle(w3_kind(L,ft),TS,&r,&g,&b,&rail);
          if(rail!=pass) continue; w3_drawline(im,TS,TS,ft,sc,w,r,g,b); } }
      if((L=w3_layer(t,"water_lines"))){ float sc=(float)TS/osmmesh_mvt_layer_extent(L); size_t nf=osmmesh_mvt_num_features(L);
        for(size_t f=0;f<nf;f++){ const osmmesh_mvt_feature*ft=osmmesh_mvt_feature_at(L,f);
          if(ft->geom_type==OSMMESH_MVT_GEOM_LINESTRING) w3_drawline(im,TS,TS,ft,sc,3.0f*TS/1024.0f,92,140,190); } }
      osmmesh_mvt_free(t);
    }
    osmmesh_pmtiles_free_tile(d);
  }
  GLuint tex; glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,TS,TS,0,GL_RGB,GL_UNSIGNED_BYTE,im);
  /* trilinear (mipmaps) kills shimmer on distant tiles; anisotropy keeps the ground
   * sharp at grazing angles (the terrain is seen almost edge-on). */
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  #ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
  #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
  #endif
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_ANISOTROPY_EXT,8.0f);   /* ignored if unsupported */
  free(im); return tex;
}

/* Build a textured terrain VBO (interleaved x,y,z,u,v). The osmmesh heightfield is a
 * dense row-major grid (256x256); we DECIMATE it to a coarse W3_TERR x W3_TERR grid
 * (detail comes from the draped texture, not the geometry) and drape UV from the tile's
 * ENU bbox (north -> v=0 -> texture top). Coarse geometry = smooth framerate. */
/* The terrain vertex layout, in one place.
 *
 * The writer (w3_push8/w3_terr_vbo) and the reader (glVertexAttribPointer) used to agree only by
 * hand: literal stride 32 and offsets 0/12/20 spelled out at the draw call. When normals were
 * added the stride went 20 -> 32 and every one of those numbers had to change together; getting
 * one wrong does not error, it renders garbage. Derive them from the struct instead, and pin the
 * result so a layout change breaks the build rather than the picture. */
typedef struct { float pos[3]; float uv[2]; float norm[3]; } w3_vtx;
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(w3_vtx) == 8*sizeof(float), "terrain vertex must be tightly packed (no padding)");
_Static_assert(offsetof(w3_vtx, pos)  == 0,  "aPos offset");
_Static_assert(offsetof(w3_vtx, uv)   == 12, "aUV offset");
_Static_assert(offsetof(w3_vtx, norm) == 20, "aNorm offset");
#endif
#define W3_VTX_STRIDE ((GLsizei)sizeof(w3_vtx))
#define W3_VTX_OFF(f)  ((void*)offsetof(w3_vtx, f))

/* Upload a ready 8-float/vertex soup (pos.xyz, uv, normal.xyz) as-is. */
static GLuint w3_push8(const float*v,size_t o,int*out_nv){
  GLuint vbo; glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,o*sizeof(float),v,GL_STATIC_DRAW); *out_nv=(int)(o/(sizeof(w3_vtx)/sizeof(float))); return vbo;
}
/* Take a 5-float/vertex triangle soup (pos.xyz, uv) and emit an 8-float/vertex VBO with a
 * per-triangle FACE NORMAL appended (flat shading). Used only for the irregular-mesh
 * fallback, where there is no height grid to take smooth normals from. */
static GLuint w3_push_soup(const float*v,size_t o,int*out_nv){
  int nv=(int)(o/5); float*w=malloc((size_t)nv*8*sizeof(float));
  for(int t=0;t+2<nv;t+=3){
    const float*a=v+(size_t)t*5,*b=v+(size_t)(t+1)*5,*c=v+(size_t)(t+2)*5;
    float e1[3]={b[0]-a[0],b[1]-a[1],b[2]-a[2]}, e2[3]={c[0]-a[0],c[1]-a[1],c[2]-a[2]};
    float nx=e1[1]*e2[2]-e1[2]*e2[1], ny=e1[2]*e2[0]-e1[0]*e2[2], nz=e1[0]*e2[1]-e1[1]*e2[0];
    float L=sqrtf(nx*nx+ny*ny+nz*nz); if(L<1e-6f)L=1; nx/=L;ny/=L;nz/=L;
    if(ny<0){nx=-nx;ny=-ny;nz=-nz;}                       /* orient upward */
    for(int k=0;k<3;k++){ const float*s=v+(size_t)(t+k)*5; float*d=w+(size_t)(t+k)*8;
      d[0]=s[0];d[1]=s[1];d[2]=s[2];d[3]=s[3];d[4]=s[4];d[5]=nx;d[6]=ny;d[7]=nz; }
  }
  GLuint vbo; glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,(size_t)nv*8*sizeof(float),w,GL_STATIC_DRAW); free(w);
  *out_nv=nv; return vbo;
}
static GLuint w3_terr_vbo(const osmmesh_mesh*m,int*out_nv){
  float emin=1e18f,emax=-1e18f,nmin=1e18f,nmax=-1e18f;
  for(uint32_t i=0;i<m->n_vertices;i++){ const float*P=m->positions+i*3;
    if(P[0]<emin)emin=P[0]; if(P[0]>emax)emax=P[0]; if(P[1]<nmin)nmin=P[1]; if(P[1]>nmax)nmax=P[1]; }
  float de=emax-emin?emax-emin:1, dn=nmax-nmin?nmax-nmin:1;
  /* detect grid width C (row-major, north->south rows; x resets west at each row start) */
  uint32_t C=0;
  for(uint32_t i=1;i<m->n_vertices;i++) if(m->positions[i*3] < m->positions[(i-1)*3]-0.5f){ C=i; break; }
  #define W3_UV(P) (P[0]-emin)/de, (nmax-P[1])/dn
  if(C>=2 && m->n_vertices%C==0){                 /* regular grid -> decimate to W3_TERR x W3_TERR */
    uint32_t R=m->n_vertices/C; int G=W3_TERR<2?2:W3_TERR;
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
    float*v=malloc((size_t)(gr-1)*(gc-1)*6*8*sizeof(float)); size_t o=0;
    for(int j=0;j<gr-1;j++)for(int i=0;i<gc-1;i++){
      int q[6]={ j*gc+i, j*gc+(i+1), (j+1)*gc+(i+1), j*gc+i, (j+1)*gc+(i+1), (j+1)*gc+i };
      for(int k=0;k<6;k++){
        const float*P=np+(size_t)q[k]*3, *N=nv+(size_t)q[k]*3;
        v[o++]=P[0]; v[o++]=P[2]; v[o++]=-P[1];                 /* pos: east, up, -north */
        v[o++]=(P[0]-emin)/de; v[o++]=(nmax-P[1])/dn;           /* uv */
        v[o++]=N[0]; v[o++]=N[1]; v[o++]=N[2];                  /* smooth normal */
      }
    }
    free(np); free(nv);
    #undef W3_MV
    #undef W3_RI
    #undef W3_CI
    GLuint vbo=w3_push8(v,o,out_nv); free(v); return vbo;
  }
  /* fallback: full-resolution soup (irregular mesh) */
  uint32_t nt=m->n_triangles; float*v=malloc((size_t)nt*3*5*sizeof(float)); size_t o=0;
  for(uint32_t i=0;i<nt*3;i++){ uint32_t idx=m->indices?m->indices[i]:i; const float*P=m->positions+idx*3;
    v[o++]=P[0]; v[o++]=P[2]; v[o++]=-P[1]; v[o++]=(P[0]-emin)/de; v[o++]=(nmax-P[1])/dn; }
  #undef W3_UV
  GLuint vbo=w3_push_soup(v,o,out_nv); free(v); return vbo;
}

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
  printf("[world3d] streaming tiles from %s (origin %.4f/%.4f)\n",base,lat,lon);
  return 1;
}
/* stream one grid (zoom z, radius rad, texture size tex) around tile (cx,cy) into arr */
/* ---- baked-tile cache (LRU, keyed by z/x/y) ----
 * Crossing a tile boundary used to DELETE and re-bake all 34 tiles: a multi-hundred-ms
 * freeze every ~44 s in a 1000 m orbit (the video froze then jumped = the "kick"). But an
 * orbit flies over the SAME tiles again and again, so we keep baked VBO+texture around and
 * only bake genuinely new ones. Sized to hold a full orbit -> after the first lap nothing
 * is re-baked at all. Eviction is least-recently-used. */
#define W3_CACHE 64
typedef struct { int z; uint32_t x,y; GLuint vbo,tex; int nverts; unsigned touch; int valid; } w3_cent;
static w3_cent w3_cache[W3_CACHE];
static unsigned w3_touch=0;
static int w3_cache_hits=0, w3_cache_bakes=0;

/* Return a cache slot holding the baked tile, baking it only if not already resident. */
static int w3_cache_get(int z,uint32_t x,uint32_t y,int tex,int is_centre){
  for(int i=0;i<W3_CACHE;i++)
    if(w3_cache[i].valid && w3_cache[i].z==z && w3_cache[i].x==x && w3_cache[i].y==y){
      w3_cache[i].touch=++w3_touch; w3_cache_hits++; return i; }
  osmmesh_tile t={0};
  if(osmmesh_fetch_tile(w3_osm,z,x,y,&t)!=OSMMESH_OK || !t.terrain){ osmmesh_free_tile(&t); return -1; }
  if(!w3_yoff_set && is_centre && z==W3_Z){
    float best=1e30f; for(uint32_t i=0;i<t.terrain->n_vertices;i++){ const float*P=t.terrain->positions+i*3;
      float dd=P[0]*P[0]+P[1]*P[1]; if(dd<best){best=dd; w3_yoff=P[2];} }
    w3_yoff_set=1;
  }
  /* Bake BEFORE evicting: in photo mode the bake can legitimately fail (children still in
   * flight), and throwing away a good resident tile for one that isn't ready would make the
   * cache churn exactly when the network is slowest. */
  GLuint newtex=w3_bake(z,x,y,tex);
  if(!newtex){ osmmesh_free_tile(&t); return -1; }        /* retry next frame */

  int slot=-1; unsigned oldest=~0u;
  for(int i=0;i<W3_CACHE;i++){
    if(!w3_cache[i].valid){ slot=i; break; }
    if(w3_cache[i].touch<oldest){ oldest=w3_cache[i].touch; slot=i; }
  }
  if(w3_cache[slot].valid){ glDeleteBuffers(1,&w3_cache[slot].vbo); glDeleteTextures(1,&w3_cache[slot].tex); }
  w3_cent*c=&w3_cache[slot];
  c->vbo=w3_terr_vbo(t.terrain,&c->nverts); c->tex=newtex;
  c->z=z; c->x=x; c->y=y; c->valid=1; c->touch=++w3_touch; w3_cache_bakes++;
  osmmesh_free_tile(&t);
  return slot;
}
/* Build a draw list for the grid around (cx,cy); entries just reference cached tiles.
 *
 * Walks the grid in RINGS outward from the centre: the tile under the aircraft first, then its
 * 8 neighbours, then the rest. This matters because tiles now arrive over the network with a
 * concurrency cap — iterating the grid corner-to-corner (dy=-rad..rad, dx=-rad..rad) let distant
 * corner tiles occupy the whole request budget while the ground directly under the aircraft was
 * still queued. Nearest-first means what you can actually see loads first. */
static int w3_stream_grid(uint32_t cx,uint32_t cy,int z,int rad,int tex,int maxt,w3_tileGL*arr){
  int n=0;
  for(int ring=0;ring<=rad;ring++){
    for(int dy=-ring;dy<=ring;dy++)for(int dx=-ring;dx<=ring;dx++){
      if(dx>-ring&&dx<ring&&dy>-ring&&dy<ring) continue;   /* interior: done on an earlier ring */
      int ci=w3_cache_get(z,cx+dx,cy+dy,tex,(dx==0&&dy==0));
      if(ci<0 || n>=maxt) continue;
      arr[n].vbo=w3_cache[ci].vbo; arr[n].tex=w3_cache[ci].tex; arr[n].nverts=w3_cache[ci].nverts; n++;
    }
  }
  return n;
}
/* Stream the near (z14 detailed) + far (low-zoom, wide) tiers.
 *
 * Called every frame. Two reasons to do work:
 *   1. the aircraft crossed into a new centre tile -> the wanted grid moved
 *   2. the last pass was INCOMPLETE -> tiles were still in flight from fb-tiles
 * (2) is what makes async sourcing work: a tile that hasn't arrived is simply absent from the
 * draw list and picked up on a later frame, so the world fills in progressively and the frame
 * loop never blocks. Cached tiles cost nothing to re-visit, so retrying is cheap. */
static int w3_stream_done=0;
static void world3d_stream(double lat,double lon){
  if(!w3_osm) return;
  uint32_t tx,ty; if(osmmesh_geo_to_tile(lon,lat,W3_Z,&tx,&ty)!=0) return;
  int moved = !w3_have_tile || tx!=w3_tx || ty!=w3_ty;
  if(!moved && w3_stream_done) return;
  w3_tx=tx; w3_ty=ty; w3_have_tile=1;
  int b0=w3_cache_bakes, h0=w3_cache_hits;
  /* NEAR tier first: the ground under and around the aircraft is what you actually see, and it
   * must win the network budget over the distant ring. */
  w3_nT  = w3_stream_grid(tx,ty,W3_Z,   W3_RAD,   W3_TEX,   W3_MAXT, w3_T);    /* near detail */
  uint32_t fx,fy; osmmesh_geo_to_tile(lon,lat,W3_FARZ,&fx,&fy);
  w3_nTF = w3_stream_grid(fx,fy,W3_FARZ,W3_FARRAD,W3_FARTEX,W3_MAXTF,w3_TF);   /* distant coarse ring */
  int want = (2*W3_RAD+1)*(2*W3_RAD+1) + (2*W3_FARRAD+1)*(2*W3_FARRAD+1);
  int was_done = w3_stream_done;
  w3_stream_done = (w3_nT + w3_nTF) >= want;
  /* Only report on a real transition, or this would print every frame while tiles land. */
  if(moved || (w3_stream_done && !was_done))
    printf("[world3d] tiles near %d + far %d of %d | baked %d, cached %d%s\n",
           w3_nT,w3_nTF,want,w3_cache_bakes-b0,w3_cache_hits-h0,
           w3_stream_done?"":" (waiting on fb-tiles)");
}
#endif /* W3_USE_OSM */

/* Switch the ground albedo source. Drops every baked tile: the cache is keyed by z/x/y only,
 * so without this you would keep seeing the old texture until each tile happened to be evicted.
 *
 * This DOES cost one re-bake of everything visible -- the very freeze the tile cache exists to
 * avoid. Accepted here because it is a deliberate keypress, not something that happens on its own
 * every time you cross a tile boundary. */
static void w3_ground_toggle(void){
  w3_ground_mode = (w3_ground_mode==W3_GROUND_OSM) ? W3_GROUND_PHOTO : W3_GROUND_OSM;
  for(int i=0;i<W3_CACHE;i++) if(w3_cache[i].valid){
    glDeleteBuffers(1,&w3_cache[i].vbo); glDeleteTextures(1,&w3_cache[i].tex);
    w3_cache[i].valid=0;
  }
  w3_nT=0; w3_nTF=0;
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
  if(w3_nT>0) py=(have&&t->alt>1?t->alt:2)+w3_yoff;   /* AGL above the osmmesh ground */
#endif
  float yaw=have?t->yaw*RAD:0, pitch=have?t->pitch*RAD:0, roll=have?t->roll*RAD:0;
  float f[3]={cosf(pitch)*sinf(yaw),sinf(pitch),-cosf(pitch)*cosf(yaw)};
  float wup[3]={0,1,0},s[3]; v_cross(s,f,wup); v_norm(s); float u[3]; v_cross(u,s,f);
  /* roll the camera-up around the forward axis. +roll (right bank, right wing down) must
   * tilt the camera up toward the RIGHT (+s), so the world appears to roll left in view.
   * (The previous -s inverted it: a right bank looked like a left bank.) */
  float up[3]={u[0]*cosf(roll)+s[0]*sinf(roll),u[1]*cosf(roll)+s[1]*sinf(roll),u[2]*cosf(roll)+s[2]*sinf(roll)};
  float sr[3]={s[0]*cosf(roll)-u[0]*sinf(roll),s[1]*cosf(roll)-u[1]*sinf(roll),s[2]*cosf(roll)-u[2]*sinf(roll)}; /* rolled screen-right */
  float eye[3]={px,py,pz},ctr[3]={px+f[0],py+f[1],pz+f[2]};
  float view[16],proj[16],mvp[16]; m_lookat(view,eye,ctr,up); m_persp(proj,W3_FOV*RAD,(float)W/H,2.0f,45000.f); m_mul(mvp,proj,view);

  /* ---- environment: real sun/moon direction (ENU: E=+X, up=+Y, N=-Z), sky, light ---- */
  float sun_el=have?t->sun_el:35.f, sun_az=have?t->sun_az:150.f;
  float moon_el=have?t->moon_el:25.f, moon_az=have?t->moon_az:300.f, moon_ph=have?t->moon_phase:0.5f;
  float cloud=have?t->cloud:0.3f; if(cloud<0)cloud=0; if(cloud>1)cloud=1;
  float ce=cosf(sun_el*RAD), sun[3]={ce*sinf(sun_az*RAD),sinf(sun_el*RAD),-ce*cosf(sun_az*RAD)};
  float me=cosf(moon_el*RAD), moon[3]={me*sinf(moon_az*RAD),sinf(moon_el*RAD),-me*cosf(moon_az*RAD)};
  float day=fmaxf(0.f,fminf(1.f,(sun_el+6.f)/12.f));           /* 0 night (<-6°) .. 1 day (>6°) */
  float haze[3]={0.05f+0.67f*day,0.06f+0.76f*day,0.13f+0.79f*day};
  for(int i=0;i<3;i++){ float tgt=0.80f*day+0.30f*(1.f-day); haze[i]+=(tgt-haze[i])*cloud*0.45f; }
  float light=0.20f+0.80f*day;                                 /* dim scene at night, full by day */
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
    time_t tt=time(NULL); double jd=tt/86400.0+2440587.5, dd=jd-2451545.0;
    double gmst=fmod(280.46061837+360.98564736629*dd,360.0);
    double lst=fmod(gmst+w3_olon,360.0), slat=sin(w3_olat*RAD), clat=cos(w3_olat*RAD);
    static float sv[W3_NSTARS*4]; int ns=0;
    for(int i=0;i<W3_NSTARS;i++){
      double Hh=(lst-W3_STARS[i].ra)*RAD, dec=W3_STARS[i].dec*RAD;
      double sinAlt=slat*sin(dec)+clat*cos(dec)*cos(Hh);
      if(sinAlt<=0.03) continue;                              /* below/at horizon */
      double az=atan2(-cos(dec)*sin(Hh), sin(dec)*clat-cos(dec)*slat*cos(Hh));
      double ca=sqrt(fmax(0.0,1.0-sinAlt*sinAlt));
      float dE=(float)(ca*sin(az)), dU=(float)sinAlt, dN=(float)(ca*cos(az));
      sv[ns*4]=eye[0]+dE*40000.f; sv[ns*4+1]=eye[1]+dU*40000.f; sv[ns*4+2]=eye[2]-dN*40000.f;
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
  if(w3_nT>0||w3_nTF>0){   /* textured OSM terrain: far coarse ring + near detail, one draw per tile */
    glUseProgram(w3_pWT); glUniformMatrix4fv(w3_wtMVP,1,GL_FALSE,mvp);
    glUniform3fv(w3_wtHaze,1,haze); glUniform1f(w3_wtLight,light); glUniform3fv(w3_wtSun,1,sun);
    glActiveTexture(GL_TEXTURE0); glUniform1i(w3_wtTex,0);
    glEnableVertexAttribArray(w3_wtPos); glEnableVertexAttribArray(w3_wtUV); glEnableVertexAttribArray(w3_wtNorm);
    #define W3_DRAWARR(arr,n) for(int i=0;i<n;i++){ glBindTexture(GL_TEXTURE_2D,arr[i].tex); glBindBuffer(GL_ARRAY_BUFFER,arr[i].vbo); \
      glVertexAttribPointer(w3_wtPos,3,GL_FLOAT,GL_FALSE,W3_VTX_STRIDE,W3_VTX_OFF(pos)); \
      glVertexAttribPointer(w3_wtUV,2,GL_FLOAT,GL_FALSE,W3_VTX_STRIDE,W3_VTX_OFF(uv)); \
      glVertexAttribPointer(w3_wtNorm,3,GL_FLOAT,GL_FALSE,W3_VTX_STRIDE,W3_VTX_OFF(norm)); \
      glDrawArrays(GL_TRIANGLES,0,arr[i].nverts); }
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(4.0f,64.0f);   /* push the far ring back so near detail wins on overlap */
    W3_DRAWARR(w3_TF,w3_nTF);
    glDisable(GL_POLYGON_OFFSET_FILL);
    W3_DRAWARR(w3_T,w3_nT);
    #undef W3_DRAWARR
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
/* convenience: scene + HUD directly (used by the native offscreen renderer) */
static void world3d_render(const telem_packet_t*t,int W,int H,int have){
  world3d_render_scene(t,W,H,have); world3d_render_hud(t,W,H,have);
}
#endif
