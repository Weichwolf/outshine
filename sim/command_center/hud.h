/* FlightBox renderer — the HUD/OSD: a 2D line overlay in pixel coords, regenerated every frame and
 * drawn on top of the decoded video (never encoded into it). Bitmap font, then the full avionics OSD. */
#ifndef W3_HUD_H
#define W3_HUD_H
/* ---- HUD (2D lines, pixel coords) ---- */
/* Regenerated every frame (glBufferData DYNAMIC). Sized for the full OSD: the bitmap
 * font draws ~2 line segments per lit pixel, so all the text + arrows + ladders add up
 * to a few thousand segments. Too small a buffer silently drops the LAST-drawn elements
 * (home arrow, glideslope). 65536 floats = ~6500 segments, comfortably above the OSD. */
static float w3_hud[65536]; static int w3_hudN;
static const char*W3_CS=" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/";
static const unsigned char W3_FONT[41][5]={
 {0,0,0,0,0},{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
 {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,7,4,7},{7,4,7,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},{5,5,6,5,5},{4,4,4,4,7},
 {5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},{7,5,7,4,4},{7,5,5,7,1},{6,5,6,5,5},{7,4,7,1,7},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7},
 {0,0,7,0,0},{0,0,0,0,2},{0,2,0,2,0},{1,1,2,4,4}};
static const char*W3_STN[]={"DISARM","ARMED","CLIMB","LOITER","MANUAL","RTH"};
static void w3_line(float x0,float y0,float x1,float y1,float r,float g,float b){ if(w3_hudN>65516)return;
  w3_hud[w3_hudN++]=x0;w3_hud[w3_hudN++]=y0;w3_hud[w3_hudN++]=r;w3_hud[w3_hudN++]=g;w3_hud[w3_hudN++]=b;
  w3_hud[w3_hudN++]=x1;w3_hud[w3_hudN++]=y1;w3_hud[w3_hudN++]=r;w3_hud[w3_hudN++]=g;w3_hud[w3_hudN++]=b; }
static void w3_gpx(float x,float y,float s,float r,float g,float b){ w3_line(x,y,x+s,y,r,g,b); w3_line(x,y+s*0.5f,x+s,y+s*0.5f,r,g,b); }
static void w3_text(float x,float y,float s,float r,float g,float b,const char*t){
  for(;*t;t++){ char u=*t; if(u>='a'&&u<='z')u-=32; const char*p=strchr(W3_CS,u); int ix=p?(int)(p-W3_CS):0;
    for(int row=0;row<5;row++){ unsigned char m=W3_FONT[ix][row]; for(int c=0;c<3;c++) if(m&(4>>c)) w3_gpx(x+c*s,y+row*s,s,r,g,b);} x+=4*s; } }
static void w3_printf(float x,float y,float s,float r,float g,float b,const char*fmt,...){ char bb[96]; va_list a; va_start(a,fmt); vsnprintf(bb,96,fmt,a); va_end(a); w3_text(x,y,s,r,g,b,bb); }
/* Full OSD: every telemetry field, computed/derived correctly. The bitmap font only
 * has [ 0-9 A-Z - . : / ], so no '%'/'+': percent is implied by the label, sign shown
 * via '-' plus colour (green=climb/good, amber=caution, red=warning). */
static void w3_build_hud(const telem_packet_t*t,int W,int H,int have){
  w3_hudN=0; float cx=W/2,cy=H/2;
  w3_line(cx-24,cy,cx-8,cy,0.4f,1,0.4f); w3_line(cx+8,cy,cx+24,cy,0.4f,1,0.4f); w3_line(cx,cy-8,cx,cy+8,0.4f,1,0.4f);
  /* Data attribution -- a licence obligation, so drawn ALWAYS (before the no-telemetry return): OSM
   * vector (ODbL), Esri imagery, HYG star catalogue (CC-BY-SA). The bitmap font is upper-case only
   * and has no '(c)', so the credits read in the plain characters it can draw. */
  w3_text(14, H-10, 1.5f, 0.45f,0.45f,0.55f, "OPENSTREETMAP CONTRIBUTORS - ESRI IMAGERY - HYG DATABASE CC-BY-SA");
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
  { int evs=(w3_ground.mode==W3_GROUND_PHOTO);
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
/* Draw the 2D HUD (line overlay) into the bound framebuffer at W×H pixel coords. */
static void world3d_render_hud(const telem_packet_t*t,int W,int H,int have){
  glViewport(0,0,W,H); glDisable(GL_DEPTH_TEST); w3_build_hud(t,W,H,have);
  glBindBuffer(GL_ARRAY_BUFFER,w3_gl.hVBO); glBufferData(GL_ARRAY_BUFFER,w3_hudN*4,w3_hud,GL_DYNAMIC_DRAW);
  glUseProgram(w3_gl.pH); glUniform2f(w3_gl.hScale,2.0f/W,2.0f/H); glEnableVertexAttribArray(w3_gl.hPos); glEnableVertexAttribArray(w3_gl.hCol);
  glVertexAttribPointer(w3_gl.hPos,2,GL_FLOAT,GL_FALSE,20,0); glVertexAttribPointer(w3_gl.hCol,3,GL_FLOAT,GL_FALSE,20,(void*)8); glDrawArrays(GL_LINES,0,w3_hudN/5);
}
#endif
