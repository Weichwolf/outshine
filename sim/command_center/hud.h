/* FlightBox renderer — the HUD/OSD: a 2D line overlay in pixel coords, regenerated every frame and
 * drawn on top of the decoded video (never encoded into it). Bitmap font, then the full avionics OSD. */
#ifndef W3_HUD_H
#define W3_HUD_H
/* ---- HUD (2D lines, pixel coords) ---- */
/* Regenerated every frame (glBufferData DYNAMIC). Sized for the full OSD: the bitmap
 * font draws ~2 line segments per lit pixel, so all the text + arrows + ladders add up
 * to a few thousand segments. Too small a buffer silently drops the LAST-drawn elements. The
 * MIL-STD-1787 pitch ladder + its numbers pushed the count up, so this is sized well above it:
 * 131072 floats = ~13000 segments. */
static float w3_hud[131072]; static int w3_hudN;
static const char*W3_CS=" 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.:/";
static const unsigned char W3_FONT[41][5]={
 {0,0,0,0,0},{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
 {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,7,4,7},{7,4,7,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},{5,5,6,5,5},{4,4,4,4,7},
 {5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},{7,5,7,4,4},{7,5,5,7,1},{6,5,6,5,5},{7,4,7,1,7},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7},
 {0,0,7,0,0},{0,0,0,0,2},{0,2,0,2,0},{1,1,2,4,4}};
static const char*W3_STN[]={"DISARM","ARMED","CLIMB","LOITER","MANUAL","RTH"};
static void w3_line(float x0,float y0,float x1,float y1,float r,float g,float b){ if(w3_hudN>131052)return;
  w3_hud[w3_hudN++]=x0;w3_hud[w3_hudN++]=y0;w3_hud[w3_hudN++]=r;w3_hud[w3_hudN++]=g;w3_hud[w3_hudN++]=b;
  w3_hud[w3_hudN++]=x1;w3_hud[w3_hudN++]=y1;w3_hud[w3_hudN++]=r;w3_hud[w3_hudN++]=g;w3_hud[w3_hudN++]=b; }
static void w3_gpx(float x,float y,float s,float r,float g,float b){ w3_line(x,y,x+s,y,r,g,b); w3_line(x,y+s*0.5f,x+s,y+s*0.5f,r,g,b); }
static void w3_text(float x,float y,float s,float r,float g,float b,const char*t){
  for(;*t;t++){ char u=*t; if(u>='a'&&u<='z')u-=32; const char*p=strchr(W3_CS,u); int ix=p?(int)(p-W3_CS):0;
    for(int row=0;row<5;row++){ unsigned char m=W3_FONT[ix][row]; for(int c=0;c<3;c++) if(m&(4>>c)) w3_gpx(x+c*s,y+row*s,s,r,g,b);} x+=4*s; } }
static void w3_printf(float x,float y,float s,float r,float g,float b,const char*fmt,...){ char bb[96]; va_list a; va_start(a,fmt); vsnprintf(bb,96,fmt,a); va_end(a); w3_text(x,y,s,r,g,b,bb); }

/* Rotate (x,y) about (ox,oy) by a screen-space angle given as (ca=cos,sa=sin); y is DOWN. Banks the
 * conformal horizon line with roll, its endpoints turned about the boresight. */
static void w3_rot(float ox,float oy,float ca,float sa,float x,float y,float*rx,float*ry){
  float dx=x-ox,dy=y-oy; *rx=ox+dx*ca-dy*sa; *ry=oy+dx*sa+dy*ca; }
/* A line whose two endpoints are first rotated about (ox,oy) by (ca,sa). */
static void w3_rline(float ox,float oy,float ca,float sa,float x0,float y0,float x1,float y1,float r,float g,float b){
  float ax,ay,bx,by; w3_rot(ox,oy,ca,sa,x0,y0,&ax,&ay); w3_rot(ox,oy,ca,sa,x1,y1,&bx,&by); w3_line(ax,ay,bx,by,r,g,b); }
/* Approximate a circle as `seg` chords -- the Flight-Path-Marker ring. */
static void w3_circle(float cx,float cy,float rad,int seg,float r,float g,float b){
  float px=cx+rad,py=cy; for(int i=1;i<=seg;i++){ float a=(float)i/(float)seg*6.2831853f;
    float x=cx+rad*cosf(a),y=cy+rad*sinf(a); w3_line(px,py,x,y,r,g,b); px=x; py=y; } }
/* A rectangle outline -- the boxed current value on a tape. */
static void w3_box(float x0,float y0,float x1,float y1,float r,float g,float b){
  w3_line(x0,y0,x1,y0,r,g,b); w3_line(x1,y0,x1,y1,r,g,b); w3_line(x1,y1,x0,y1,r,g,b); w3_line(x0,y1,x0,y0,r,g,b); }
/* Full OSD: every telemetry field, computed/derived correctly. The bitmap font only
 * has [ 0-9 A-Z - . : / ], so no '%'/'+': percent is implied by the label, sign shown
 * via '-' plus colour (green=climb/good, amber=caution, red=warning). */
static void w3_build_hud(const telem_packet_t*t,int W,int H,int have){
  w3_hudN=0; float cx=W/2,cy=H/2;
  const float RAD=(float)M_PI/180.f, HG_R=0.30f,HG_G=1.0f,HG_B=0.40f;   /* monochrome HUD green */
  /* Waterline / boresight: the FIXED aircraft reference (nose / longitudinal axis), screen-locked. */
  w3_line(cx-46,cy,cx-16,cy,HG_R,HG_G,HG_B); w3_line(cx+16,cy,cx+46,cy,HG_R,HG_G,HG_B);
  w3_line(cx-16,cy,cx,cy+9,HG_R,HG_G,HG_B);  w3_line(cx,cy+9,cx+16,cy,HG_R,HG_G,HG_B);
  if(!have){ w3_text(cx-60,30,3,1,0.8f,0.2f,"NO TELEMETRY"); return; }

  /* ==== Primary attitude field: Flight-Path-Marker + 0-deg horizon line (MIL-STD-1787) ====
   * Kept: waterline, FPM, and ONE conformal 0-deg horizon line. Dropped: the numbered climb-dive
   * rungs (busy clutter -- the FPV video already shows the real ladder of terrain). Conformal vertical
   * scale K (from the 80 deg camera FOV): an angle e above the boresight sits at cy - K*tan(e). */
  float K=(H*0.5f)/tanf(40.f*RAD), pitch=t->pitch;
  /* Flight-Path-Marker: where the velocity vector points. gamma = climb angle from vs/gs; AoA =
   * pitch - gamma sets it below the waterline. Horizontal drift is deliberately not modelled -> centred. */
  float gamma=atan2f(t->vs, t->gs>0.5f?t->gs:0.5f)/RAD;
  float fx=cx, fy=cy - K*tanf((gamma-pitch)*RAD);
  if(fy<cy-H*0.45f)fy=cy-H*0.45f; if(fy>cy+H*0.45f)fy=cy+H*0.45f;
  w3_circle(fx,fy,7,10,HG_R,HG_G,HG_B);
  w3_line(fx-7,fy,fx-18,fy,HG_R,HG_G,HG_B); w3_line(fx+7,fy,fx+18,fy,HG_R,HG_G,HG_B); w3_line(fx,fy-7,fx,fy-15,HG_R,HG_G,HG_B);
  /* Horizon line: the single 0-deg pitch reference, conformal (sits on the video horizon), a long bar
   * with a central gap for the boresight, banked with roll. Only this 0-deg line stays -- the numbered
   * climb-dive rungs are omitted as clutter. Roll sign verified against the terrain horizon at a shot. */
  float ca=cosf(t->roll*RAD), sa=sinf(t->roll*RAD);
  float horY=cy - K*tanf((0.f-pitch)*RAD);
  /* Draw it ~3 px thick (three perpendicular-offset copies): a single 1 px GL_LINE at a bank angle
   * rasterises stair-stepped -- and MSAA antialiases polygon edges, not thin lines -- so a tilted
   * 1 px horizon breaks into visible dashes. Stacking three offset lines gives a solid bar. */
  for(float off=-1.f; off<=1.f; off+=1.f){
    w3_rline(cx,cy,ca,sa, cx-260,horY+off, cx-40,horY+off, HG_R,HG_G,HG_B);
    w3_rline(cx,cy,ca,sa, cx+40,horY+off, cx+260,horY+off, HG_R,HG_G,HG_B);
  }
  float hdg=t->yaw<0?t->yaw+360:t->yaw;

  /* ===== Heading tape (top): moving scale centred on heading, boxed value + up-caret, home pointer.
   * Ticks every 5 deg, labels every 30 (N/03/06/E...). home_bearing is relative to the nose, so on a
   * nose-centred tape the home marker sits at that offset from centre. ===== */
  { float hpd=5.f, hy1=40;
    for(int d=-45; d<=45; d+=5){
      float sx=cx+(float)d*hpd; int hh=(((int)lroundf(hdg)+d)%360+360)%360;
      float tk=(hh%30==0)?11.f:6.f; w3_line(sx,hy1,sx,hy1-tk,HG_R,HG_G,HG_B);
      if(hh%30==0){ char nb[4]; const char*L=nb;
        if(hh==0)L="N"; else if(hh==90)L="E"; else if(hh==180)L="S"; else if(hh==270)L="W"; else snprintf(nb,4,"%02d",hh/10);
        w3_text(sx-(L[1]?7.f:3.f),hy1-tk-15,2.f,HG_R,HG_G,HG_B,L); } }
    w3_line(cx-200,hy1,cx+200,hy1,HG_R,HG_G,HG_B);
    w3_line(cx-7,hy1+7,cx,hy1,HG_R,HG_G,HG_B); w3_line(cx,hy1,cx+7,hy1+7,HG_R,HG_G,HG_B);       /* up-caret */
    w3_box(cx-26,hy1+8,cx+26,hy1+30,HG_R,HG_G,HG_B); w3_printf(cx-22,hy1+13,2.f,HG_R,HG_G,HG_B,"%03.0f",hdg);
    float hb=t->home_bearing; if(hb>44)hb=44; if(hb<-44)hb=-44; float hsx=cx+hb*hpd;
    w3_line(hsx,hy1-1,hsx-6,hy1-11,HG_R,HG_G,HG_B); w3_line(hsx,hy1-1,hsx+6,hy1-11,HG_R,HG_G,HG_B); w3_line(hsx-6,hy1-11,hsx+6,hy1-11,HG_R,HG_G,HG_B);
    w3_text(hsx-3,hy1-25,1.4f,HG_R,HG_G,HG_B,"H"); }

  /* ===== Airspeed tape (left): moving vertical scale, boxed TAS + caret; GS secondary below ===== */
  { float apx=5.f, ax=70.f, as=t->airspeed;
    for(int d=-24; d<=24; d+=4){
      float av=as+(float)d; if(av<0.f) continue; float sy=cy-(float)d*apx; int ai=(int)lroundf(av);
      float tk=(ai%10==0)?11.f:6.f; w3_line(ax,sy,ax-tk,sy,HG_R,HG_G,HG_B);
      if(ai%10==0) w3_printf(ax-tk-26,sy-4,1.7f,HG_R,HG_G,HG_B,"%3d",ai); }
    w3_line(ax,cy-150,ax,cy+150,HG_R,HG_G,HG_B);
    w3_box(ax+3,cy-11,ax+63,cy+11,HG_R,HG_G,HG_B); w3_printf(ax+9,cy-7,2.f,HG_R,HG_G,HG_B,"%3.0f",as);
    w3_line(ax,cy,ax+3,cy-6,HG_R,HG_G,HG_B); w3_line(ax,cy,ax+3,cy+6,HG_R,HG_G,HG_B);           /* caret at the rail */
    w3_text(ax+3,cy-30,1.4f,HG_R,HG_G,HG_B,"TAS"); w3_printf(ax+3,cy+18,1.7f,HG_R,HG_G,HG_B,"GS %2.0f",t->gs); }
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
