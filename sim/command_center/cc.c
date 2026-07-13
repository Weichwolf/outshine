/* FlightBox — Command Center (C -> WASM via Emscripten).
 * Served by the flightbox. Draws the video (artificial horizon) + a HUD overlay
 * from ELRS telemetry, reads a game controller OR keyboard, and sends control
 * back over WebSocket. Keyboard fallback so it works without a gamepad:
 *   arrows = roll/pitch, W/S = throttle, A/D = yaw, ENTER = arm, L = drop link. */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/websocket.h>
#include "protocol.h"

#define WIN_W 640
#define WIN_H 480

static SDL_Renderer *ren;
static SDL_Texture  *vtex;
static EMSCRIPTEN_WEBSOCKET_T ws;
static int ws_open = 0;
static telem_packet_t telem;
static int have_telem = 0;
static SDL_GameController *pad = NULL;

/* ---- tiny 3x5 font ---- */
static const char *CS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.";
static const uint8_t FONT[39][5] = {
 {0,0,0,0,0},{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
 {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,7,4,7},{7,4,7,4,4},
 {7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},{5,5,6,5,5},{4,4,4,4,7},
 {5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},{7,5,7,4,4},{7,5,5,7,1},{6,5,6,5,5},
 {7,4,7,1,7},{7,2,2,2,2},{5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},
 {5,5,2,2,2},{7,1,2,4,7},{0,0,7,0,0},{0,0,0,0,2}
};
static void gtext(int x,int y,int s,SDL_Color c,const char*t){
    SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,255);
    for(; *t; t++){
        char u=*t; if(u>='a'&&u<='z') u-=32;
        const char*p=strchr(CS,u); int idx=p?(int)(p-CS):0;
        for(int r=0;r<5;r++){ uint8_t row=FONT[idx][r];
            for(int col=0;col<3;col++) if(row&(4>>col)){ SDL_Rect q={x+col*s,y+r*s,s,s}; SDL_RenderFillRect(ren,&q);} }
        x+=4*s;
    }
}
static void gprintf(int x,int y,int s,SDL_Color c,const char*fmt,...){
    char b[128]; va_list a; va_start(a,fmt); vsnprintf(b,sizeof b,fmt,a); va_end(a); gtext(x,y,s,c,b);
}

/* ---- websocket ---- */
static EM_BOOL on_open(int t,const EmscriptenWebSocketOpenEvent*e,void*u){(void)t;(void)e;(void)u;ws_open=1;return EM_TRUE;}
static EM_BOOL on_close(int t,const EmscriptenWebSocketCloseEvent*e,void*u){(void)t;(void)e;(void)u;ws_open=0;return EM_TRUE;}
static EM_BOOL on_msg(int t,const EmscriptenWebSocketMessageEvent*e,void*u){
    (void)t;(void)u;
    if(e->isText||e->numBytes<4) return EM_TRUE;
    uint32_t mg=*(uint32_t*)e->data;
    if(mg==FB_MAGIC_TELEM && e->numBytes==sizeof(telem_packet_t)){ memcpy(&telem,e->data,sizeof telem); have_telem=1; }
    else if(mg==FB_MAGIC_VIDEO && e->numBytes==sizeof(video_packet_t)){
        video_packet_t*v=(video_packet_t*)e->data;
        SDL_UpdateTexture(vtex,NULL,v->pix,VID_W*2);
    }
    return EM_TRUE;
}

/* ---- HUD ---- */
static const char*STNAME[]={"DISARM","ARMED","CLIMB","LOITER","MANUAL","RTH"};
static void draw_hud(void){
    SDL_Color w={235,235,235,255}, g={120,255,120,255}, y={255,220,60,255};
    int cx=WIN_W/2, cy=WIN_H/2;
    /* center reference */
    SDL_SetRenderDrawColor(ren,g.r,g.g,g.b,255);
    SDL_Rect c1={cx-20,cy-1,14,2},c2={cx+6,cy-1,14,2},c3={cx-1,cy-6,2,12};
    SDL_RenderFillRect(ren,&c1);SDL_RenderFillRect(ren,&c2);SDL_RenderFillRect(ren,&c3);
    if(!have_telem){ gtext(cx-60,20,2,y,"NO TELEMETRY"); return; }

    /* readouts */
    gprintf(12,12,2,w,"ALT %5.0f M",telem.alt);
    gprintf(12,30,2,w,"SPD %5.1f",telem.gs);
    gprintf(12,48,2,w,"HDG %5.0f",telem.yaw<0?telem.yaw+360:telem.yaw);
    gprintf(12,66,2,w,"BAT %4.1f V",telem.batt);
    gprintf(WIN_W-150,12,2,w,"HOME %5.0f M",telem.home_dist);
    SDL_Color sc = (telem.state==ST_RTH)?y:(telem.state==ST_LOITER?y:g);
    gprintf(WIN_W-150,30,2,sc,"%s",STNAME[telem.state%6]);
    gprintf(WIN_W-150,48,2, telem.rssi>0?g:y, "LNK %3d",telem.rssi);

    /* home arrow (points to home, relative to nose) */
    float a=telem.home_bearing*(float)M_PI/180.f;
    int hx=cx, hy=90; float len=26;
    float tx=hx+sinf(a)*len, ty=hy-cosf(a)*len;
    SDL_SetRenderDrawColor(ren,y.r,y.g,y.b,255);
    SDL_RenderDrawLine(ren,hx,hy,(int)tx,(int)ty);
    float la=a+2.6f, ra=a-2.6f;
    SDL_RenderDrawLine(ren,(int)tx,(int)ty,(int)(tx+sinf(la)*8),(int)(ty-cosf(la)*8));
    SDL_RenderDrawLine(ren,(int)tx,(int)ty,(int)(tx+sinf(ra)*8),(int)(ty-cosf(ra)*8));
    gtext(cx-8,hy+18,1,y,"HOME");

    /* glideslope ladder (right side): diamond moves with error */
    int gx=WIN_W-40, gy=cy;
    SDL_SetRenderDrawColor(ren,w.r,w.g,w.b,255);
    for(int i=-2;i<=2;i++){ SDL_Rect t={gx-6,gy+i*24-1,12,2}; SDL_RenderFillRect(ren,&t);}
    int dy=(int)(-telem.glideslope_err*4.0f); if(dy<-48)dy=-48; if(dy>48)dy=48;
    SDL_SetRenderDrawColor(ren,y.r,y.g,y.b,255);
    SDL_Rect d={gx-5,gy+dy-5,10,10}; SDL_RenderFillRect(ren,&d);
    gtext(gx-10,gy+60,1,w,"G/S");
}

/* ---- control input ---- */
static uint16_t seq=0;
static int armed_latch=0, enter_prev=0;
static void send_control(void){
    if(!ws_open) return;
    ctrl_packet_t c; memset(&c,0,sizeof c); c.magic=FB_MAGIC_CTRL; c.link_up=1; c.throttle=0.5f;
    const Uint8*k=SDL_GetKeyboardState(NULL);
    /* keyboard */
    if(k[SDL_SCANCODE_RIGHT])c.roll+=1; if(k[SDL_SCANCODE_LEFT])c.roll-=1;
    if(k[SDL_SCANCODE_UP])c.pitch+=1; if(k[SDL_SCANCODE_DOWN])c.pitch-=1;
    if(k[SDL_SCANCODE_D])c.yaw+=1; if(k[SDL_SCANCODE_A])c.yaw-=1;
    if(k[SDL_SCANCODE_W])c.throttle=1; if(k[SDL_SCANCODE_S])c.throttle=0;
    if(k[SDL_SCANCODE_L])c.link_up=0;                 /* hold L: simulate link loss */
    int enter=k[SDL_SCANCODE_RETURN]||k[SDL_SCANCODE_SPACE];
    if(enter&&!enter_prev) armed_latch=1;             /* latch armed on press */
    enter_prev=enter;
    /* gamepad overrides if present */
    if(pad){
        float rx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX)/32767.f;
        float ry=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY)/32767.f;
        float ly=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY)/32767.f;
        float lx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX)/32767.f;
        if(fabsf(rx)>0.08f)c.roll=rx; if(fabsf(ry)>0.08f)c.pitch=-ry;
        if(fabsf(lx)>0.08f)c.yaw=lx;  if(fabsf(ly)>0.08f)c.throttle=(-ly+1)/2;
        if(SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_A)) armed_latch=1;
        if(SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_B)) c.link_up=0;
    }
    c.arm=armed_latch; c.seq=seq++;
    emscripten_websocket_send_binary(ws,&c,sizeof c);
}

static void frame(void){
    SDL_Event e; while(SDL_PollEvent(&e)){
        if(e.type==SDL_CONTROLLERDEVICEADDED && !pad) pad=SDL_GameControllerOpen(e.cdevice.which);
    }
    send_control();
    SDL_SetRenderDrawColor(ren,10,10,14,255); SDL_RenderClear(ren);
    SDL_RenderCopy(ren,vtex,NULL,NULL);   /* video fills window */
    draw_hud();
    SDL_RenderPresent(ren);
}

int main(void){
    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER);
    SDL_Window*win; SDL_CreateWindowAndRenderer(WIN_W,WIN_H,0,&win,&ren);
    vtex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_RGB565,SDL_TEXTUREACCESS_STREAMING,VID_W,VID_H);
    /* fill blue/green until first frame arrives */
    { static uint16_t init[VID_W*VID_H]; for(int i=0;i<VID_W*VID_H;i++) init[i]=(i<VID_W*VID_H/2)?RGB565(70,130,220):RGB565(60,150,60);
      SDL_UpdateTexture(vtex,NULL,init,VID_W*2); }

    char url[256]; const char*host=emscripten_run_script_string("location.host");
    snprintf(url,sizeof url,"ws://%s/ws",host&&*host?host:"127.0.0.1:8080");
    EmscriptenWebSocketCreateAttributes attr={url,NULL,EM_TRUE};
    ws=emscripten_websocket_new(&attr);
    emscripten_websocket_set_onopen_callback(ws,NULL,on_open);
    emscripten_websocket_set_onclose_callback(ws,NULL,on_close);
    emscripten_websocket_set_onmessage_callback(ws,NULL,on_msg);

    emscripten_set_main_loop(frame,0,1);
    return 0;
}
