/* FlightBox — headless offscreen renderer for visual evaluation.
 * Renders the SAME world3d scene the WASM command center shows (shared world3d.h),
 * streaming REAL OSM+terrain geometry live from osmmesh around a given GPS pose,
 * into an offscreen EGL/GLES2 FBO, and dumps the framebuffer as raw RGB (-> PNG via
 * a tiny Python step) so renderings can be inspected without a browser.
 *
 * Usage: render_native <out.rgb> <vec.pmtiles> <terr.pmtiles> \
 *          [origin_lat origin_lon] [W H] [roll pitch yaw alt east north]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define W3_USE_OSM
#include "world3d.h"

static EGLDisplay egl_headless_display(void){
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if(getPlatformDisplay){
        EGLDisplay d = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
        if(d != EGL_NO_DISPLAY) return d;
    }
    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

int main(int argc, char** argv){
    if(argc<4){ fprintf(stderr,"usage: %s out.rgb vec.pmtiles terr.pmtiles [lat lon] [W H] [roll pitch yaw alt e n]\n",argv[0]); return 1; }
    const char* out = argv[1];
    const char* vec = argv[2];
    const char* terr= argv[3];
    double olat = argc>5?atof(argv[4]):52.045, olon = argc>5?atof(argv[5]):9.385;
    int W = argc>7?atoi(argv[6]):640, H = argc>7?atoi(argv[7]):480;
    telem_packet_t t = {0}; t.magic=FB_MAGIC_TELEM;
    t.roll = argc>13?atof(argv[8]):-8; t.pitch=argc>13?atof(argv[9]):-3; t.yaw=argc>13?atof(argv[10]):40;
    t.alt  = argc>13?atof(argv[11]):130; t.x = argc>13?atof(argv[12]):0; t.y=argc>13?atof(argv[13]):-300;
    t.gs=28; t.batt=11.4f; t.state=4; t.rssi=92;
    /* home indicator / glideslope from pose */
    t.home_dist=(float)hypot(t.x,t.y); t.home_bearing=(float)(atan2(-t.x,-t.y)*180/M_PI - t.yaw);
    t.glideslope_err=1.0f;

    EGLDisplay dpy = egl_headless_display();
    EGLint major,minor; if(!eglInitialize(dpy,&major,&minor)){ fprintf(stderr,"eglInitialize failed\n"); return 2; }
    fprintf(stderr,"EGL %d.%d — %s\n", major,minor, eglQueryString(dpy,EGL_VENDOR));
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint cfgattr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8, EGL_NONE };
    EGLConfig cfg; EGLint ncfg=0;
    if(!eglChooseConfig(dpy,cfgattr,&cfg,1,&ncfg) || ncfg<1){ fprintf(stderr,"no EGL config\n"); return 2; }
    EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION,2, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,ctxattr);
    if(!eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,ctx)){ fprintf(stderr,"eglMakeCurrent 0x%x\n",eglGetError()); return 2; }
    fprintf(stderr,"GL_RENDERER: %s\n", (const char*)glGetString(GL_RENDERER));

    #define GL_RGBA8_OES 0x8058
    GLuint fbo,color,depth;
    glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenRenderbuffers(1,&color); glBindRenderbuffer(GL_RENDERBUFFER,color);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8_OES,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,color);
    glGenRenderbuffers(1,&depth); glBindRenderbuffer(GL_RENDERBUFFER,depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,W,H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){ fprintf(stderr,"FBO incomplete\n"); return 2; }

    if(!world3d_osm_open(vec,terr,olat,olon)) return 3;
    world3d_init();
    /* aircraft geographic position from ENU pose offset (east=x, north=y) */
    double lat = olat + (double)t.y/111320.0;
    double lon = olon + (double)t.x/(111320.0*cos(olat*M_PI/180.0));
    world3d_stream(lat, lon);
    world3d_render(&t, W, H, 1);
    glFinish();
    GLenum e=glGetError(); if(e) fprintf(stderr,"glGetError 0x%x\n",e);

    unsigned char* px = malloc((size_t)W*H*4);
    glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,px);
    unsigned char* rgb = malloc((size_t)W*H*3);
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){ unsigned char*s=px+((size_t)(H-1-y)*W+x)*4,*d=rgb+((size_t)y*W+x)*3; d[0]=s[0];d[1]=s[1];d[2]=s[2]; }
    FILE* f = fopen(out,"wb"); fwrite(rgb,1,(size_t)W*H*3,f); fclose(f);
    fprintf(stderr,"wrote %s (%dx%d)\n", out, W, H);
    free(px); free(rgb);
    eglTerminate(dpy);
    return 0;
}
