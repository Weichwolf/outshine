/* FlightBox renderer -- MAX7456.h: emulation of the character-generator OSD chip that real FPV
 * ground stations use. A font ROM of 8x8 pixel tiles is baked once into an atlas texture; the OSD
 * is then composed as textured-quad blits of those tiles (one quad per glyph), exactly as the chip
 * paints characters from its tile ROM. This replaces the old vector-line font (many GL_LINE segments
 * per lit pixel -> a per-frame vertex explosion), so the per-frame HUD geometry is small and bounded
 * (visible chars x 4 verts), which is what kills the tape-label flicker at its root.
 *
 * The tile ROM is generated from readable glyph art (scratchpad/genfont.py) -- the art is the spec,
 * the bytes below are its output, so there is no transcription drift. NEAREST sampling + alpha-test
 * discard keeps the glyphs crisp and blocky (authentic chip look), and they resolve inside the same
 * HUD MSAA FBO as the line primitives, so the glow composite is unchanged. */
#ifndef W3_MAX7456_H
#define W3_MAX7456_H

/* Font tile ROM: 8x8 tiles, one row per byte, bit7 = leftmost column. Charset order below. */
static const char*MX_CS=" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/+\xb0";
static const unsigned char MX_FONT[43][8]={
 {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
 {0x78,0x88,0x98,0xa8,0xc8,0x88,0x78,0x00},
 {0x20,0x60,0x20,0x20,0x20,0x20,0x70,0x00},
 {0x78,0x88,0x08,0x10,0x20,0x40,0xf8,0x00},
 {0xf8,0x10,0x20,0x10,0x08,0x88,0x70,0x00},
 {0x10,0x30,0x50,0x90,0xf8,0x10,0x10,0x00},
 {0xf8,0x80,0xf0,0x08,0x08,0x88,0x70,0x00},
 {0x30,0x40,0x80,0xf0,0x88,0x88,0x70,0x00},
 {0xf8,0x08,0x10,0x20,0x40,0x40,0x40,0x00},
 {0x70,0x88,0x88,0x70,0x88,0x88,0x70,0x00},
 {0x70,0x88,0x88,0x78,0x08,0x10,0x60,0x00},
 {0x70,0x88,0x88,0x88,0xf8,0x88,0x88,0x00},
 {0xf0,0x88,0x88,0xf0,0x88,0x88,0xf0,0x00},
 {0x78,0x88,0x80,0x80,0x80,0x88,0x78,0x00},
 {0xe0,0x90,0x88,0x88,0x88,0x90,0xe0,0x00},
 {0xf8,0x80,0x80,0xf0,0x80,0x80,0xf8,0x00},
 {0xf8,0x80,0x80,0xf0,0x80,0x80,0x80,0x00},
 {0x78,0x88,0x80,0xb8,0x88,0x88,0x78,0x00},
 {0x88,0x88,0x88,0xf8,0x88,0x88,0x88,0x00},
 {0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00},
 {0x38,0x10,0x10,0x10,0x90,0x90,0x60,0x00},
 {0x88,0x90,0xa0,0xc0,0xa0,0x90,0x88,0x00},
 {0x80,0x80,0x80,0x80,0x80,0x80,0xf8,0x00},
 {0x88,0xd8,0xa8,0xa8,0x88,0x88,0x88,0x00},
 {0x88,0xc8,0xa8,0x98,0x88,0x88,0x88,0x00},
 {0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00},
 {0xf0,0x88,0x88,0xf0,0x80,0x80,0x80,0x00},
 {0x70,0x88,0x88,0x88,0xa8,0x90,0x68,0x00},
 {0xf0,0x88,0x88,0xf0,0xa0,0x90,0x88,0x00},
 {0x78,0x80,0x80,0x70,0x08,0x08,0xf0,0x00},
 {0xf8,0x20,0x20,0x20,0x20,0x20,0x20,0x00},
 {0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00},
 {0x88,0x88,0x88,0x88,0x88,0x50,0x20,0x00},
 {0x88,0x88,0x88,0xa8,0xa8,0xd8,0x88,0x00},
 {0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00},
 {0x88,0x88,0x50,0x20,0x20,0x20,0x20,0x00},
 {0xf8,0x08,0x10,0x20,0x40,0x80,0xf8,0x00},
 {0x00,0x00,0x00,0xf8,0x00,0x00,0x00,0x00},
 {0x00,0x00,0x00,0x00,0x00,0x60,0x60,0x00},
 {0x00,0x60,0x60,0x00,0x60,0x60,0x00,0x00},
 {0x08,0x08,0x10,0x20,0x40,0x80,0x80,0x00},
 {0x00,0x20,0x20,0xf8,0x20,0x20,0x00,0x00},
 {0x60,0x90,0x90,0x60,0x00,0x00,0x00,0x00},
};
#define MX_NGLYPH 43
#define MX_TILE   8
#define MX_ATLAS_W (MX_NGLYPH*MX_TILE)
/* Visual metrics vs the old vector font's scale `s`: advance kept at 4*s so every existing HUD text
 * position lines up unchanged; the drawn tile is a touch larger (6*s) so the 5x7 glyph reads at the
 * old height. Content (5 cols) < advance (4*s wide in tiles) -> a clear gap, no overlap. */
#define MX_ADV  4.0f
#define MX_QS   6.0f

/* One batched vertex stream: x,y,u,v,r,g,b per vertex, 6 verts (2 tris) per glyph. Regenerated each
 * frame; bounded by the visible character count, never accumulates. */
static float mx_v[32768]; static int mx_vN;
static struct { GLuint prog,tex,vbo; GLint pos,uv,col,scale; } mx_gl;

static void mx_reset(void){ mx_vN=0; }

static void mx_vert(float x,float y,float u,float vv,float r,float g,float b){ if(mx_vN>32761)return;
  mx_v[mx_vN++]=x; mx_v[mx_vN++]=y; mx_v[mx_vN++]=u; mx_v[mx_vN++]=vv; mx_v[mx_vN++]=r; mx_v[mx_vN++]=g; mx_v[mx_vN++]=b; }

/* Blit one glyph: quad at (x,y) size qs px, UV over glyph `gi`'s 8x8 tile (inset 0.05 texel so NEAREST
 * never lands on a cell boundary and picks a neighbour). */
static void mx_glyph(float x,float y,float qs,int gi,float r,float g,float b){
  float u0=((float)gi*MX_TILE+0.05f)/MX_ATLAS_W, u1=((float)gi*MX_TILE+MX_TILE-0.05f)/MX_ATLAS_W;
  float v0=0.05f/MX_TILE, v1=(MX_TILE-0.05f)/MX_TILE, x1=x+qs, y1=y+qs;
  mx_vert(x,y,u0,v0,r,g,b);  mx_vert(x1,y,u1,v0,r,g,b);  mx_vert(x1,y1,u1,v1,r,g,b);
  mx_vert(x,y,u0,v0,r,g,b);  mx_vert(x1,y1,u1,v1,r,g,b); mx_vert(x,y1,u0,v1,r,g,b); }

/* Drop-in for the old w3_text: same (x,y top-left, scale s, colour, string) contract, but composes
 * from tile blits. Unknown chars fall to the blank tile (index 0). */
static void mx_text(float x,float y,float s,float r,float g,float b,const char*t){
  float adv=MX_ADV*s, qs=MX_QS*s;
  for(;*t;t++){ char u=*t; if(u>='a'&&u<='z')u-=32; const char*p=strchr(MX_CS,u); int gi=p?(int)(p-MX_CS):0;
    if(gi>0) mx_glyph(x,y,qs,gi,r,g,b); x+=adv; } }

/* Textured-glyph shader: same NDC mapping as the HUD line shader (uScale), samples the atlas .r as
 * coverage and alpha-tests -- crisp blocky pixels, and empty texels never overwrite the lines under
 * them, so no blend state is needed inside the opaque HUD FBO pass. */
static const char*MX_VS=
 "attribute vec2 aPos; attribute vec2 aUV; attribute vec3 aCol; uniform vec2 uScale; varying vec2 vUV; varying vec3 vCol;"
 "void main(){ gl_Position=vec4(aPos.x*uScale.x-1.0, 1.0-aPos.y*uScale.y, 0.0,1.0); vUV=aUV; vCol=aCol; }";
static const char*MX_FS=
 "precision mediump float; varying vec2 vUV; varying vec3 vCol; uniform sampler2D uAtlas;"
 "void main(){ if(texture2D(uAtlas,vUV).r<0.5) discard; gl_FragColor=vec4(vCol,1.0); }";

static void mx_init(void){
  unsigned char*a=(unsigned char*)malloc(MX_ATLAS_W*MX_TILE);
  for(int gi=0;gi<MX_NGLYPH;gi++) for(int row=0;row<MX_TILE;row++){ unsigned char m=MX_FONT[gi][row];
    for(int c=0;c<MX_TILE;c++) a[row*MX_ATLAS_W + gi*MX_TILE + c] = (m&(0x80>>c))?255:0; }
  glGenTextures(1,&mx_gl.tex); glBindTexture(GL_TEXTURE_2D,mx_gl.tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE,MX_ATLAS_W,MX_TILE,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,a);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  free(a);
  mx_gl.prog=w3_prog(MX_VS,MX_FS);
  mx_gl.pos=glGetAttribLocation(mx_gl.prog,"aPos"); mx_gl.uv=glGetAttribLocation(mx_gl.prog,"aUV");
  mx_gl.col=glGetAttribLocation(mx_gl.prog,"aCol"); mx_gl.scale=glGetUniformLocation(mx_gl.prog,"uScale");
  glUseProgram(mx_gl.prog); glUniform1i(glGetUniformLocation(mx_gl.prog,"uAtlas"),0);   /* sampler on unit 0, program must be bound */
  glGenBuffers(1,&mx_gl.vbo);
}

/* Draw the accumulated glyph quads into the bound (HUD MSAA) framebuffer at W x H pixel coords. */
static void mx_render(int W,int H){
  if(mx_vN<=0)return;
  glUseProgram(mx_gl.prog); glUniform2f(mx_gl.scale,2.0f/W,2.0f/H);
  glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,mx_gl.tex);
  glBindBuffer(GL_ARRAY_BUFFER,mx_gl.vbo); glBufferData(GL_ARRAY_BUFFER,mx_vN*4,mx_v,GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(mx_gl.pos); glEnableVertexAttribArray(mx_gl.uv); glEnableVertexAttribArray(mx_gl.col);
  glVertexAttribPointer(mx_gl.pos,2,GL_FLOAT,GL_FALSE,28,0);
  glVertexAttribPointer(mx_gl.uv, 2,GL_FLOAT,GL_FALSE,28,(void*)8);
  glVertexAttribPointer(mx_gl.col,3,GL_FLOAT,GL_FALSE,28,(void*)16);
  glDrawArrays(GL_TRIANGLES,0,mx_vN/7);
  glDisableVertexAttribArray(mx_gl.col);
}
#endif
