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
#include <GLES2/gl2.h>
#include "protocol.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- mat4 (column-major) ---- */
static void m_identity(float*m){ memset(m,0,64); m[0]=m[5]=m[10]=m[15]=1; }
static void m_mul(float*o,const float*a,const float*b){ float r[16];
  for(int c=0;c<4;c++)for(int rr=0;rr<4;rr++){ float s=0; for(int k=0;k<4;k++) s+=a[k*4+rr]*b[c*4+k]; r[c*4+rr]=s; }
  memcpy(o,r,64); }
static void m_persp(float*m,float fovy,float asp,float zn,float zf){ float f=1.f/tanf(fovy*0.5f);
  memset(m,0,64); m[0]=f/asp; m[5]=f; m[10]=(zf+zn)/(zn-zf); m[11]=-1; m[14]=(2*zf*zn)/(zn-zf); }
static void v_norm(float*v){ float l=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); if(l>1e-6f){v[0]/=l;v[1]/=l;v[2]/=l;} }
static void v_cross(float*o,const float*a,const float*b){ o[0]=a[1]*b[2]-a[2]*b[1]; o[1]=a[2]*b[0]-a[0]*b[2]; o[2]=a[0]*b[1]-a[1]*b[0]; }
static void m_lookat(float*m,const float*eye,const float*ctr,const float*up){
  float f[3]={ctr[0]-eye[0],ctr[1]-eye[1],ctr[2]-eye[2]}; v_norm(f);
  float s[3]; v_cross(s,f,up); v_norm(s); float u[3]; v_cross(u,s,f); m_identity(m);
  m[0]=s[0];m[4]=s[1];m[8]=s[2]; m[1]=u[0];m[5]=u[1];m[9]=u[2]; m[2]=-f[0];m[6]=-f[1];m[10]=-f[2];
  m[12]=-(s[0]*eye[0]+s[1]*eye[1]+s[2]*eye[2]); m[13]=-(u[0]*eye[0]+u[1]*eye[1]+u[2]*eye[2]); m[14]=(f[0]*eye[0]+f[1]*eye[1]+f[2]*eye[2]); }

/* ---- GL program helpers ---- */
static GLuint w3_shader(GLenum t,const char*src){ GLuint s=glCreateShader(t); glShaderSource(s,1,&src,0); glCompileShader(s);
  GLint ok; glGetShaderiv(s,GL_COMPILE_STATUS,&ok); if(!ok){ char b[512]; glGetShaderInfoLog(s,512,0,b); printf("shader err: %s\n",b);} return s; }
static GLuint w3_prog(const char*vs,const char*fs){ GLuint p=glCreateProgram();
  glAttachShader(p,w3_shader(GL_VERTEX_SHADER,vs)); glAttachShader(p,w3_shader(GL_FRAGMENT_SHADER,fs)); glLinkProgram(p); return p; }

static const char*W3_VSW=
 "attribute vec3 aPos; attribute vec3 aCol; uniform mat4 uMVP; varying vec3 vCol; varying float vFog;"
 "void main(){ vec4 p=uMVP*vec4(aPos,1.0); gl_Position=p; vCol=aCol; vFog=clamp(p.z/6000.0,0.0,1.0); }";
static const char*W3_FSW=
 "precision mediump float; varying vec3 vCol; varying float vFog;"
 "void main(){ vec3 sky=vec3(0.55,0.70,0.90); gl_FragColor=vec4(mix(vCol,sky,vFog*0.7),1.0); }";
/* textured terrain: OSM landcover/roads/buildings baked to a per-tile ortho texture */
static const char*W3_VSWT=
 "attribute vec3 aPos; attribute vec2 aUV; uniform mat4 uMVP; varying vec2 vUV; varying float vFog;"
 "void main(){ vec4 p=uMVP*vec4(aPos,1.0); gl_Position=p; vUV=aUV; vFog=clamp((p.w-3000.0)/28000.0,0.0,1.0); }";
static const char*W3_FSWT=
 "precision mediump float; varying vec2 vUV; varying float vFog; uniform sampler2D uTex;"
 "void main(){ vec3 c=texture2D(uTex,vUV).rgb; vec3 sky=vec3(0.55,0.70,0.90); gl_FragColor=vec4(mix(c,sky,vFog*0.6),1.0); }";
static const char*W3_VSH=
 "attribute vec2 aPos; attribute vec3 aCol; uniform vec2 uScale; varying vec3 vCol;"
 "void main(){ gl_Position=vec4(aPos.x*uScale.x-1.0, 1.0-aPos.y*uScale.y, 0.0,1.0); vCol=aCol; }";
static const char*W3_FSH="precision mediump float; varying vec3 vCol; void main(){ gl_FragColor=vec4(vCol,1.0); }";

static GLuint w3_pW,w3_pH,w3_pWT,w3_vTerr,w3_vBld,w3_hVBO; static int w3_nTerr,w3_nBld;
static GLint w3_wPos,w3_wCol,w3_wMVP,w3_hPos,w3_hCol,w3_hScale;
static GLint w3_wtPos,w3_wtUV,w3_wtMVP,w3_wtTex;

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
#define W3_FARTEX 384              /* far tier texture resolution (coarse) */
#endif

/* Old-flight-sim ground: OSM footprints/roads/rivers/rails + landcover are baked
 * into ONE orthographic texture per tile (no building geometry). The texture is
 * draped over the terrain heightfield. The vector features come from the Shortbread
 * PMTiles via osmmesh's MVT decoder; the terrain heightfield from osmmesh's terrain. */
static osmmesh_ctx *w3_osm=0;          /* terrain heightfield meshes */
static osmmesh_pmtiles *w3_vec=0;      /* raw vector tiles for texture baking */
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

static void w3_landcolor(const char*k,uint8_t*r,uint8_t*g,uint8_t*b){
  struct{const char*k;uint8_t r,g,bl;} T[]={
    {"wood",70,105,60},{"forest",70,105,60},{"scrub",125,150,85},{"heath",150,160,100},
    {"farmland",206,192,142},{"farmyard",192,178,132},{"allotments",182,186,122},{"vineyard",170,180,120},
    {"meadow",156,192,112},{"grass",162,196,116},{"grassland",162,196,116},{"park",142,192,122},
    {"garden",150,192,120},{"playground",150,190,120},{"cemetery",122,156,112},{"recreation_ground",150,190,120},
    {"residential",206,199,189},{"commercial",196,181,166},{"retail",202,182,162},{"industrial",176,166,176},
    {"quarry",180,170,160},{"sand",225,215,170},{"beach",235,225,180}};
  for(size_t i=0;i<sizeof(T)/sizeof(T[0]);i++) if(!strcmp(k,T[i].k)){*r=T[i].r;*g=T[i].g;*b=T[i].bl;return;}
  *r=150;*g=178;*b=118;
}
static float w3_roadstyle(const char*k,uint8_t*r,uint8_t*g,uint8_t*b,int*rail){
  *rail=0; float u=(float)W3_TEX/1024.0f;   /* widths tuned for a ~1.5 km tile */
  if(!strcmp(k,"rail")||!strcmp(k,"tram")){*r=95;*g=95;*b=105;*rail=1;return 2.0f*u;}
  if(!strcmp(k,"motorway")||!strcmp(k,"trunk")){*r=250;*g=205;*b=140;return 6*u;}
  if(!strcmp(k,"primary")){*r=250;*g=222;*b=165;return 5*u;}
  if(!strcmp(k,"secondary")){*r=250;*g=242;*b=205;return 4*u;}
  if(!strcmp(k,"tertiary")){*r=246;*g=242;*b=222;return 3.2f*u;}
  if(!strcmp(k,"residential")||!strcmp(k,"living_street")||!strcmp(k,"unclassified")||!strcmp(k,"service")){*r=236;*g=233;*b=225;return 2.4f*u;}
  *r=200;*g=175;*b=140;return 1.4f*u;    /* track/path/footway/steps */
}
static const osmmesh_mvt_layer* w3_layer(osmmesh_mvt_tile*t,const char*name){
  size_t nl=osmmesh_mvt_num_layers(t);
  for(size_t i=0;i<nl;i++){ const osmmesh_mvt_layer*l=osmmesh_mvt_layer_at(t,i); if(!strcmp(osmmesh_mvt_layer_name(l),name)) return l; }
  return 0; }
static const char* w3_kind(const osmmesh_mvt_layer*l,const osmmesh_mvt_feature*f){
  osmmesh_mvt_value v; if(osmmesh_mvt_feature_get_tag(l,f,"kind",&v)==0&&v.type==OSMMESH_MVT_VAL_STRING) return v.v.s; return ""; }

/* bake one tile's OSM vector data into an orthographic RGB texture -> GL texture id */
static GLuint w3_bake(uint32_t z,uint32_t x,uint32_t y,int TS){
  uint8_t*im=malloc((size_t)TS*TS*3);
  for(int i=0;i<TS*TS;i++){ im[i*3]=150;im[i*3+1]=178;im[i*3+2]=118; }   /* base ground */
  uint8_t*d=0; size_t n=0;
  if(w3_vec && osmmesh_pmtiles_fetch(w3_vec,z,x,y,&d,&n)==OSMMESH_PMTILES_OK){
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
          if(ft->geom_type!=OSMMESH_MVT_GEOM_LINESTRING) continue; float w=w3_roadstyle(w3_kind(L,ft),&r,&g,&b,&rail);
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
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  free(im); return tex;
}

/* Build a textured terrain VBO (interleaved x,y,z,u,v). The osmmesh heightfield is a
 * dense row-major grid (256x256); we DECIMATE it to a coarse W3_TERR x W3_TERR grid
 * (detail comes from the draped texture, not the geometry) and drape UV from the tile's
 * ENU bbox (north -> v=0 -> texture top). Coarse geometry = smooth framerate. */
static GLuint w3_push_soup(const float*v,size_t o,int*out_nv){
  GLuint vbo; glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
  glBufferData(GL_ARRAY_BUFFER,o*sizeof(float),v,GL_STATIC_DRAW); *out_nv=(int)(o/5); return vbo;
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
    float*v=malloc((size_t)(gr-1)*(gc-1)*6*5*sizeof(float)); size_t o=0;
    #define W3_MV(r,c) (m->positions + ((size_t)(r)*C + (size_t)(c))*3)
    for(int j=0;j<gr-1;j++)for(int i=0;i<gc-1;i++){
      int r0=(int)((long)j*(R-1)/(gr-1)), r1=(int)((long)(j+1)*(R-1)/(gr-1));
      int c0=(int)((long)i*(C-1)/(gc-1)), c1=(int)((long)(i+1)*(C-1)/(gc-1));
      const float*q[6]={W3_MV(r0,c0),W3_MV(r0,c1),W3_MV(r1,c1), W3_MV(r0,c0),W3_MV(r1,c1),W3_MV(r1,c0)};
      for(int k=0;k<6;k++){ const float*P=q[k]; v[o++]=P[0]; v[o++]=P[2]; v[o++]=-P[1]; v[o++]=(P[0]-emin)/de; v[o++]=(nmax-P[1])/dn; }
    }
    #undef W3_MV
    GLuint vbo=w3_push_soup(v,o,out_nv); free(v); return vbo;
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
    .origin_lat=origin_lat, .origin_lon=origin_lon,
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
/* stream one grid (zoom z, radius rad, texture size tex) around tile (cx,cy) into arr */
static int w3_stream_grid(uint32_t cx,uint32_t cy,int z,int rad,int tex,int maxt,w3_tileGL*arr){
  int n=0;
  for(int dy=-rad;dy<=rad;dy++)for(int dx=-rad;dx<=rad;dx++){
    osmmesh_tile t={0};
    if(osmmesh_fetch_tile(w3_osm,z,cx+dx,cy+dy,&t)!=OSMMESH_OK || !t.terrain){ osmmesh_free_tile(&t); continue; }
    if(!w3_yoff_set && dx==0 && dy==0 && z==W3_Z){
      float best=1e30f; for(uint32_t i=0;i<t.terrain->n_vertices;i++){ const float*P=t.terrain->positions+i*3;
        float dd=P[0]*P[0]+P[1]*P[1]; if(dd<best){best=dd; w3_yoff=P[2];} }
      w3_yoff_set=1;
    }
    if(n<maxt){ w3_tileGL*g=&arr[n++]; g->vbo=w3_terr_vbo(t.terrain,&g->nverts); g->tex=w3_bake(z,cx+dx,cy+dy,tex); }
    osmmesh_free_tile(&t);
  }
  return n;
}
/* stream near (z14 detailed) + far (low-zoom, wide) tiers; rebuild only on centre-tile change */
static void world3d_stream(double lat,double lon){
  if(!w3_osm) return;
  uint32_t tx,ty; if(osmmesh_geo_to_tile(lon,lat,W3_Z,&tx,&ty)!=0) return;
  if(w3_have_tile && tx==w3_tx && ty==w3_ty) return;
  w3_tx=tx; w3_ty=ty; w3_have_tile=1;
  for(int i=0;i<w3_nT;i++){  glDeleteBuffers(1,&w3_T[i].vbo);  glDeleteTextures(1,&w3_T[i].tex);  }
  for(int i=0;i<w3_nTF;i++){ glDeleteBuffers(1,&w3_TF[i].vbo); glDeleteTextures(1,&w3_TF[i].tex); }
  uint32_t fx,fy; osmmesh_geo_to_tile(lon,lat,W3_FARZ,&fx,&fy);
  w3_nTF = w3_stream_grid(fx,fy,W3_FARZ,W3_FARRAD,W3_FARTEX,W3_MAXTF,w3_TF);   /* distant coarse ring */
  w3_nT  = w3_stream_grid(tx,ty,W3_Z,   W3_RAD,   W3_TEX,   W3_MAXT, w3_T);    /* near detail */
  printf("[world3d] streamed: near %d (z%d) + far %d (z%d) tiles, yoff=%.0f\n",w3_nT,W3_Z,w3_nTF,W3_FARZ,w3_yoff);
}
#endif /* W3_USE_OSM */

/* ---- HUD (2D lines, pixel coords) ---- */
static float w3_hud[9000]; static int w3_hudN;
static void w3_line(float x0,float y0,float x1,float y1,float r,float g,float b){ if(w3_hudN>8980)return;
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
static void w3_build_hud(const telem_packet_t*t,int W,int H,int have){
  w3_hudN=0; float cx=W/2,cy=H/2;
  w3_line(cx-24,cy,cx-8,cy,0.4f,1,0.4f); w3_line(cx+8,cy,cx+24,cy,0.4f,1,0.4f); w3_line(cx,cy-8,cx,cy+8,0.4f,1,0.4f);
  if(!have){ w3_text(cx-60,30,3,1,0.8f,0.2f,"NO TELEMETRY"); return; }
  w3_printf(14,14,3,1,1,1,"ALT %4.0f",t->alt); w3_printf(14,34,3,1,1,1,"SPD %4.1f",t->gs);
  w3_printf(14,54,3,1,1,1,"HDG %4.0f",t->yaw<0?t->yaw+360:t->yaw);
  w3_printf(W-160,14,3,1,1,1,"HOME %4.0f",t->home_dist);
  int rth=(t->state==5||t->state==3); w3_printf(W-160,34,3,rth?1:0.4f,rth?0.8f:1,rth?0.2f:0.4f,"%s",W3_STN[t->state%6]);
  w3_printf(W-160,54,3,t->rssi>0?0.4f:1,t->rssi>0?1:0.8f,0.3f,"LNK %3d",t->rssi);
  float a=t->home_bearing*(float)M_PI/180.f,hx=cx,hy=110,len=34,tx=hx+sinf(a)*len,ty=hy-cosf(a)*len;
  w3_line(hx,hy,tx,ty,1,0.85f,0.2f); w3_line(tx,ty,tx+sinf(a+2.6f)*10,ty-cosf(a+2.6f)*10,1,0.85f,0.2f); w3_line(tx,ty,tx+sinf(a-2.6f)*10,ty-cosf(a-2.6f)*10,1,0.85f,0.2f);
  w3_text(cx-10,hy+16,2,1,0.85f,0.2f,"HOME");
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
  w3_pH=w3_prog(W3_VSH,W3_FSH); w3_hPos=glGetAttribLocation(w3_pH,"aPos"); w3_hCol=glGetAttribLocation(w3_pH,"aCol"); w3_hScale=glGetUniformLocation(w3_pH,"uScale");
  glGenBuffers(1,&w3_hVBO);
#ifdef W3_USE_OSM
  w3_pWT=w3_prog(W3_VSWT,W3_FSWT); w3_wtPos=glGetAttribLocation(w3_pWT,"aPos"); w3_wtUV=glGetAttribLocation(w3_pWT,"aUV");
  w3_wtMVP=glGetUniformLocation(w3_pWT,"uMVP"); w3_wtTex=glGetUniformLocation(w3_pWT,"uTex");
  if(w3_osm) return;              /* geometry (textured tiles) comes from world3d_stream() */
#endif
  w3_build_procedural();
}
static void world3d_render(const telem_packet_t*t,int W,int H,int have){
  float RAD=(float)M_PI/180.f;
  glViewport(0,0,W,H); glEnable(GL_DEPTH_TEST); glClearColor(0.55f,0.70f,0.90f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  float px=have?t->x:0, py=(have&&t->alt>2)?t->alt:120, pz=have?-t->y:0;
#ifdef W3_USE_OSM
  if(w3_nT>0) py=(have&&t->alt>1?t->alt:2)+w3_yoff;   /* AGL above the osmmesh ground */
#endif
  float yaw=have?t->yaw*RAD:0, pitch=have?t->pitch*RAD:0, roll=have?t->roll*RAD:0;
  float f[3]={cosf(pitch)*sinf(yaw),sinf(pitch),-cosf(pitch)*cosf(yaw)};
  float wup[3]={0,1,0},s[3]; v_cross(s,f,wup); v_norm(s); float u[3]; v_cross(u,s,f);
  float up[3]={u[0]*cosf(roll)-s[0]*sinf(roll),u[1]*cosf(roll)-s[1]*sinf(roll),u[2]*cosf(roll)-s[2]*sinf(roll)};
  float eye[3]={px,py,pz},ctr[3]={px+f[0],py+f[1],pz+f[2]};
  float view[16],proj[16],mvp[16]; m_lookat(view,eye,ctr,up); m_persp(proj,72*RAD,(float)W/H,2.0f,45000.f); m_mul(mvp,proj,view);
#ifdef W3_USE_OSM
  if(w3_nT>0||w3_nTF>0){   /* textured OSM terrain: far coarse ring + near detail, one draw per tile */
    glUseProgram(w3_pWT); glUniformMatrix4fv(w3_wtMVP,1,GL_FALSE,mvp);
    glActiveTexture(GL_TEXTURE0); glUniform1i(w3_wtTex,0);
    glEnableVertexAttribArray(w3_wtPos); glEnableVertexAttribArray(w3_wtUV);
    #define W3_DRAWARR(arr,n) for(int i=0;i<n;i++){ glBindTexture(GL_TEXTURE_2D,arr[i].tex); glBindBuffer(GL_ARRAY_BUFFER,arr[i].vbo); \
      glVertexAttribPointer(w3_wtPos,3,GL_FLOAT,GL_FALSE,20,0); glVertexAttribPointer(w3_wtUV,2,GL_FLOAT,GL_FALSE,20,(void*)12); \
      glDrawArrays(GL_TRIANGLES,0,arr[i].nverts); }
    glEnable(GL_POLYGON_OFFSET_FILL); glPolygonOffset(4.0f,64.0f);   /* push the far ring back so near detail wins on overlap */
    W3_DRAWARR(w3_TF,w3_nTF);
    glDisable(GL_POLYGON_OFFSET_FILL);
    W3_DRAWARR(w3_T,w3_nT);
    #undef W3_DRAWARR
    glDisableVertexAttribArray(w3_wtUV);
  } else
#endif
  { glUseProgram(w3_pW); glUniformMatrix4fv(w3_wMVP,1,GL_FALSE,mvp); glEnableVertexAttribArray(w3_wPos); glEnableVertexAttribArray(w3_wCol);
    glBindBuffer(GL_ARRAY_BUFFER,w3_vTerr); glVertexAttribPointer(w3_wPos,3,GL_FLOAT,GL_FALSE,24,0); glVertexAttribPointer(w3_wCol,3,GL_FLOAT,GL_FALSE,24,(void*)12); glDrawArrays(GL_TRIANGLES,0,w3_nTerr);
    glBindBuffer(GL_ARRAY_BUFFER,w3_vBld); glVertexAttribPointer(w3_wPos,3,GL_FLOAT,GL_FALSE,24,0); glVertexAttribPointer(w3_wCol,3,GL_FLOAT,GL_FALSE,24,(void*)12); glDrawArrays(GL_TRIANGLES,0,w3_nBld);
    glDisableVertexAttribArray(w3_wCol); }
  glDisable(GL_DEPTH_TEST); w3_build_hud(t,W,H,have);
  glBindBuffer(GL_ARRAY_BUFFER,w3_hVBO); glBufferData(GL_ARRAY_BUFFER,w3_hudN*4,w3_hud,GL_DYNAMIC_DRAW);
  glUseProgram(w3_pH); glUniform2f(w3_hScale,2.0f/W,2.0f/H); glEnableVertexAttribArray(w3_hPos); glEnableVertexAttribArray(w3_hCol);
  glVertexAttribPointer(w3_hPos,2,GL_FLOAT,GL_FALSE,20,0); glVertexAttribPointer(w3_hCol,3,GL_FLOAT,GL_FALSE,20,(void*)8); glDrawArrays(GL_LINES,0,w3_hudN/5);
}
#endif
