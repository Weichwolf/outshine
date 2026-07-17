/* FlightBox renderer — the procedural fallback world (no-OSM build): a sine heightfield plus
 * scattered boxes, uploaded into the shared w3_gl.vTerr/w3_gl.vBld VBOs that world3d.h declares. */
#ifndef W3_PROCEDURAL_H
#define W3_PROCEDURAL_H
/* world uses OSM/terrain meshes if loaded via w3_load_mesh(), else procedural. */
#define W3_GRID 48
#define W3_CELL 120.0f
static float w3_hgt(float e,float n){ return 20.0f*sinf(e/900.f)*cosf(n/1100.f)+8.0f*sinf(e/300.f+n/250.f); }

static void w3_upload_terrain(const float*v,int nverts){ glGenBuffers(1,&w3_gl.vTerr); glBindBuffer(GL_ARRAY_BUFFER,w3_gl.vTerr); glBufferData(GL_ARRAY_BUFFER,nverts*6*4,v,GL_STATIC_DRAW); w3_gl.nTerr=nverts; }
static void w3_upload_buildings(const float*v,int nverts){ glGenBuffers(1,&w3_gl.vBld); glBindBuffer(GL_ARRAY_BUFFER,w3_gl.vBld); glBufferData(GL_ARRAY_BUFFER,nverts*6*4,v,GL_STATIC_DRAW); w3_gl.nBld=nverts; }

static void w3_build_procedural(void){
  float*v=malloc(W3_GRID*W3_GRID*6*6*sizeof(float)); if(!v)return; int o=0;
  for(int j=0;j<W3_GRID;j++)for(int i=0;i<W3_GRID;i++){
    float e0=(i-W3_GRID/2)*W3_CELL,n0=(j-W3_GRID/2)*W3_CELL,e1=e0+W3_CELL,n1=n0+W3_CELL;
    float p[4][3]={{e0,w3_hgt(e0,n0),-n0},{e1,w3_hgt(e1,n0),-n0},{e1,w3_hgt(e1,n1),-n1},{e0,w3_hgt(e0,n1),-n1}};
    int idx[6]={0,1,2,0,2,3};
    for(int k=0;k<6;k++){ float*P=p[idx[k]]; float g=0.35f+0.0045f*P[1]; if(g<0.25f)g=0.25f; if(g>0.6f)g=0.6f;
      v[o++]=P[0];v[o++]=P[1];v[o++]=P[2]; v[o++]=0.20f+g*0.2f;v[o++]=g;v[o++]=0.18f; } }
  w3_upload_terrain(v,o/6); free(v);
  float*b=malloc(600*36*6*sizeof(float)); if(!b)return; int bo=0; unsigned s=12345;
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
#endif
