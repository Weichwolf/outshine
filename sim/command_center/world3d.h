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
static const char*W3_VSH=
 "attribute vec2 aPos; attribute vec3 aCol; uniform vec2 uScale; varying vec3 vCol;"
 "void main(){ gl_Position=vec4(aPos.x*uScale.x-1.0, 1.0-aPos.y*uScale.y, 0.0,1.0); vCol=aCol; }";
static const char*W3_FSH="precision mediump float; varying vec3 vCol; void main(){ gl_FragColor=vec4(vCol,1.0); }";

static GLuint w3_pW,w3_pH,w3_vTerr,w3_vBld,w3_hVBO; static int w3_nTerr,w3_nBld;
static GLint w3_wPos,w3_wCol,w3_wMVP,w3_hPos,w3_hCol,w3_hScale;

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
#ifndef W3_Z
#define W3_Z 14                    /* streaming zoom level (lower = coarser LOD, larger tiles) */
#endif
#ifndef W3_RAD
#define W3_RAD 2                   /* tile radius around the aircraft (grid = 2R+1) */
#endif
#ifndef W3_TERR_STRIDE
#define W3_TERR_STRIDE 1           /* terrain LOD stride. NOTE: osmmesh crops edge terrain
                                    * grids to non-power-of-two sizes at z14, so stride>1 fails
                                    * the divisibility check and drops whole tiles (incl. their
                                    * buildings). Keep 1; use W3_RAD to bound weight instead. */
#endif
static osmmesh_ctx *w3_osm=0; static int w3_have_tile=0; static uint32_t w3_tx,w3_ty;
static const float W3_SUN[3]={0.40f,0.85f,0.35f};

typedef struct { float*v; size_t n,cap; } w3_vbuf;
static void w3_vpush(w3_vbuf*b,float x,float y,float z,float r,float g,float bl){
  if(b->n+6>b->cap){ b->cap=b->cap?b->cap*2:(1<<18); b->v=realloc(b->v,b->cap*sizeof(float)); }
  float*p=b->v+b->n; p[0]=x;p[1]=y;p[2]=z;p[3]=r;p[4]=g;p[5]=bl; b->n+=6;
}
/* expand an indexed osmmesh into shaded triangle soup, ENU(e,n,u)->render(x=e,y=u,z=-n) */
static void w3_emit(w3_vbuf*out,const osmmesh_mesh*m,const float base[3],float lo,float hi){
  if(!m||!m->n_triangles) return;
  for(uint32_t i=0;i<m->n_triangles*3;i++){
    uint32_t idx=m->indices?m->indices[i]:i; const float*P=m->positions+idx*3;
    float nx=0,ny=1,nz=0; if(m->normals){ const float*N=m->normals+idx*3; nx=N[0];ny=N[1];nz=N[2]; }
    float d=nx*W3_SUN[0]+ny*W3_SUN[1]+nz*W3_SUN[2]; if(d<0)d=-d;
    float sh=lo+(hi-lo)*(0.35f+0.65f*d);
    w3_vpush(out,P[0],P[2],-P[1],base[0]*sh,base[1]*sh,base[2]*sh);
  }
}
/* open the PMTiles archives at a configurable origin. Accepts file paths OR
 * in-memory buffers (WASM hands fetch()'d ArrayBuffers). Returns 1 on success. */
static int world3d_osm_open_mem(const char*vec_path,const uint8_t*vec_data,size_t vec_len,
                                const char*terr_path,const uint8_t*terr_data,size_t terr_len,
                                double origin_lat,double origin_lon){
  static const osmmesh_terrain_build_opts topts={ .stride=W3_TERR_STRIDE, .compute_normals=1 };
  osmmesh_config cfg={ .vector_url=vec_path, .vector_data=vec_data, .vector_len=vec_len,
    .terrain_url=terr_path, .terrain_data=terr_data, .terrain_len=terr_len,
    .origin_lat=origin_lat, .origin_lon=origin_lon, .terrain_opts=&topts,
    .enable_terrain=1, .enable_buildings=1, .enable_linears=0 };
  int rc=osmmesh_create(&cfg,&w3_osm);
  if(rc!=OSMMESH_OK){ printf("[world3d] osmmesh_create failed: %d\n",rc); w3_osm=0; return 0; }
  w3_have_tile=0; return 1;
}
static int world3d_osm_open(const char*vec_path,const char*terr_path,double lat,double lon){
  return world3d_osm_open_mem(vec_path,0,0,terr_path,0,0,lat,lon);
}
/* stream tiles around (lat,lon); rebuilds VBOs only when the centre tile changes. */
static void world3d_stream(double lat,double lon){
  if(!w3_osm) return;
  uint32_t tx,ty; if(osmmesh_geo_to_tile(lon,lat,W3_Z,&tx,&ty)!=0) return;
  if(w3_have_tile && tx==w3_tx && ty==w3_ty) return;      /* same tile -> keep VBOs */
  w3_tx=tx; w3_ty=ty; w3_have_tile=1;
  w3_vbuf terr={0},bld={0};
  const float gT[3]={0.34f,0.52f,0.26f}, gB[3]={0.62f,0.60f,0.58f};
  int nt_tiles=0,nb=0;
  for(int dy=-W3_RAD;dy<=W3_RAD;dy++)for(int dx=-W3_RAD;dx<=W3_RAD;dx++){
    osmmesh_tile t={0};
    if(osmmesh_fetch_tile(w3_osm,W3_Z,tx+dx,ty+dy,&t)!=OSMMESH_OK) continue;
    if(t.terrain){ w3_emit(&terr,t.terrain,gT,0.45f,1.05f); nt_tiles++; }
    for(uint32_t b=0;b<t.n_buildings;b++){ w3_emit(&bld,&t.buildings[b],gB,0.55f,1.05f); nb++; }
    osmmesh_free_tile(&t);
  }
  if(w3_vTerr) glDeleteBuffers(1,&w3_vTerr);
  if(w3_vBld)  glDeleteBuffers(1,&w3_vBld);
  w3_upload_terrain(terr.v,(int)(terr.n/6));
  w3_upload_buildings(bld.v,(int)(bld.n/6));
  printf("[world3d] streamed tile %u/%u (%d terr tiles, %d buildings, %u+%u verts)\n",
    tx,ty,nt_tiles,nb,(unsigned)(terr.n/6),(unsigned)(bld.n/6));
  free(terr.v); free(bld.v);
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
  if(w3_osm) return;              /* geometry comes from world3d_stream() */
#endif
  w3_build_procedural();
}
static void world3d_render(const telem_packet_t*t,int W,int H,int have){
  float RAD=(float)M_PI/180.f;
  glViewport(0,0,W,H); glEnable(GL_DEPTH_TEST); glClearColor(0.55f,0.70f,0.90f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  float px=have?t->x:0, py=(have&&t->alt>2)?t->alt:120, pz=have?-t->y:0;
  float yaw=have?t->yaw*RAD:0, pitch=have?t->pitch*RAD:0, roll=have?t->roll*RAD:0;
  float f[3]={cosf(pitch)*sinf(yaw),sinf(pitch),-cosf(pitch)*cosf(yaw)};
  float wup[3]={0,1,0},s[3]; v_cross(s,f,wup); v_norm(s); float u[3]; v_cross(u,s,f);
  float up[3]={u[0]*cosf(roll)-s[0]*sinf(roll),u[1]*cosf(roll)-s[1]*sinf(roll),u[2]*cosf(roll)-s[2]*sinf(roll)};
  float eye[3]={px,py,pz},ctr[3]={px+f[0],py+f[1],pz+f[2]};
  float view[16],proj[16],mvp[16]; m_lookat(view,eye,ctr,up); m_persp(proj,72*RAD,(float)W/H,1.5f,7000.f); m_mul(mvp,proj,view);
  glUseProgram(w3_pW); glUniformMatrix4fv(w3_wMVP,1,GL_FALSE,mvp); glEnableVertexAttribArray(w3_wPos); glEnableVertexAttribArray(w3_wCol);
  glBindBuffer(GL_ARRAY_BUFFER,w3_vTerr); glVertexAttribPointer(w3_wPos,3,GL_FLOAT,GL_FALSE,24,0); glVertexAttribPointer(w3_wCol,3,GL_FLOAT,GL_FALSE,24,(void*)12); glDrawArrays(GL_TRIANGLES,0,w3_nTerr);
  glBindBuffer(GL_ARRAY_BUFFER,w3_vBld); glVertexAttribPointer(w3_wPos,3,GL_FLOAT,GL_FALSE,24,0); glVertexAttribPointer(w3_wCol,3,GL_FLOAT,GL_FALSE,24,(void*)12); glDrawArrays(GL_TRIANGLES,0,w3_nBld);
  glDisable(GL_DEPTH_TEST); w3_build_hud(t,W,H,have);
  glBindBuffer(GL_ARRAY_BUFFER,w3_hVBO); glBufferData(GL_ARRAY_BUFFER,w3_hudN*4,w3_hud,GL_DYNAMIC_DRAW);
  glUseProgram(w3_pH); glUniform2f(w3_hScale,2.0f/W,2.0f/H); glEnableVertexAttribArray(w3_hPos); glEnableVertexAttribArray(w3_hCol);
  glVertexAttribPointer(w3_hPos,2,GL_FLOAT,GL_FALSE,20,0); glVertexAttribPointer(w3_hCol,3,GL_FLOAT,GL_FALSE,20,(void*)8); glDrawArrays(GL_LINES,0,w3_hudN/5);
}
#endif
