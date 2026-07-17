/* FlightBox FDM — live weather ingest (Open-Meteo). See weather.h. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "weather.h"
#include "atmosphere.h"
#include "../sim_state.h"

float g_cloud=0.25f, g_vis=30000.0f;  /* live cloud cover 0..1 + horizontal visibility, m */
int g_wx_live=1;

/* --- live weather via Open-Meteo (background thread; never blocks the FDM) --- */
static double jnum(const char*s,const char*key){ char p[64]; snprintf(p,sizeof p,"\"%s\":",key);
    const char*q=strstr(s,p); if(!q) return NAN; return atof(q+strlen(p)); }
static double jnum_in(const char*s,const char*sect,const char*key){ const char*b=strstr(s,sect); return jnum(b?b:s,key); }
static double jarr(const char*s,const char*key,int idx){ char p[64]; snprintf(p,sizeof p,"\"%s\":[",key);
    const char*q=strstr(s,p); if(!q) return NAN; q+=strlen(p);
    for(int i=0;i<idx;i++){ q=strchr(q,','); if(!q) return NAN; q++; } return atof(q); }
static void wx_fetch(void){
    char url[640];
    snprintf(url,sizeof url,
      "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
      "&current=wind_speed_10m,wind_direction_10m,wind_gusts_10m,cloud_cover,visibility,temperature_2m"
      "&hourly=boundary_layer_height,shortwave_radiation"
      "&wind_speed_unit=ms&forecast_days=1", HOME_LAT, HOME_LON);
    char cmd[760]; snprintf(cmd,sizeof cmd,"curl -s --max-time 8 \"%s\"",url);
    FILE*f=popen(cmd,"r"); if(!f) return;
    static char buf[32768]; size_t n=fread(buf,1,sizeof buf-1,f); buf[n]=0; pclose(f);
    if(n<40 || !strstr(buf,"wind_speed_10m")) { fprintf(stderr,"[wx] no data (offline?) — keeping current weather\n"); return; }
    double wsp =jnum_in(buf,"\"current\":{","wind_speed_10m");
    double wdir=jnum_in(buf,"\"current\":{","wind_direction_10m");
    double gust=jnum_in(buf,"\"current\":{","wind_gusts_10m");
    double cc  =jnum_in(buf,"\"current\":{","cloud_cover");
    double vis =jnum_in(buf,"\"current\":{","visibility");
    time_t tt=time(NULL); struct tm gm; gmtime_r(&tt,&gm); int hr=gm.tm_hour;
    double blh=jarr(buf,"boundary_layer_height",hr), swr=jarr(buf,"shortwave_radiation",hr);
    if(!isfinite(wsp)||!isfinite(wdir)) { fprintf(stderr,"[wx] parse failed\n"); return; }
    if(!isfinite(gust)) gust=wsp*1.4;
    /* wind at flight altitude ~ a bit stronger than 10 m (log profile); FROM dir -> blows toward */
    double v=wsp*1.25;
    /* turbulence from the gust factor; Dryden sigma scales with it */
    double gf=fmax(0.0,gust-wsp);
    /* realistic low-altitude turbulence: MIL-F-8785C puts sigma_w ~ 0.1*W20; add a
     * modest gust-spread term. Capped so a strong-gust day is rough but still flyable. */
    fb_atmo_observe(&ATM,
        -v*cos(wdir*RAD), -v*sin(wdir*RAD),
        fmax(0.3,fmin(1.6, 0.30+0.13*gf)),
        fmin(1.0, 0.08*wsp + 0.11*gf),          /* capped so even a very gusty day stays flyable */
        isfinite(blh)?fmax(300.0,blh):800.0,
        isfinite(swr)?fmax(0.0,fmin(4.5, swr/230.0)):0.0);  /* ~1000 W/m2 midday -> strong lift */
    /* NO snap here: the new weather is a TARGET. physics_step eases the air toward it over
     * FB_ATMO_TAU, because a 15-minute refresh that assigned the wind directly stepped the
     * aircraft's relative airflow instantly — a jolt out of a clear sky. */
    if(isfinite(cc))  g_cloud=(float)fmax(0.0,fmin(1.0,cc/100.0));
    if(isfinite(vis)) g_vis=(float)fmax(1500.0,fmin(60000.0,vis));
    fprintf(stderr,"[wx] LIVE wind=%.1fm/s@%.0f gust=%.1f cloud=%.0f%% vis=%.0fkm BL=%.0fm sun=%.0fW/m2 -> turb=%.2f therm=%.1f\n",
        wsp,wdir,gust,cc,vis/1000.0,blh,swr,ATM.turb,ATM.thermal_W);
}
void* wx_thread(void*arg){ (void)arg;
    for(;;){ wx_fetch(); sleep(900); }   /* refresh every 15 min */
}
