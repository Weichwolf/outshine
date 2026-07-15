/* Extracted from xp_bridge.c: pure astronomy has no business living in a god file next to
 * sockets and PID loops. Now unit-testable against known sun positions. */
#include "ephemeris.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG (180.0/M_PI)
#define RAD (M_PI/180.0)

double fb_julian_day(time_t t){ return t/86400.0 + 2440587.5; }
void fb_sun_pos(double jd,double lat,double lon,double*el,double*az){
    double n=jd-2451545.0;
    double L=fmod(280.460+0.9856474*n,360.0); if(L<0)L+=360;
    double g=fmod(357.528+0.9856003*n,360.0)*RAD;
    double lam=(L+1.915*sin(g)+0.020*sin(2*g))*RAD;
    double eps=(23.439-4.0e-7*n)*RAD;
    double ra=atan2(cos(eps)*sin(lam),cos(lam));
    double dec=asin(sin(eps)*sin(lam));
    double gmst=fmod(280.46061837+360.98564736629*n,360.0);
    double lst=fmod(gmst+lon,360.0)*RAD, H=lst-ra;
    double sl=sin(lat*RAD), cl=cos(lat*RAD);
    *el=asin(sl*sin(dec)+cl*cos(dec)*cos(H))*DEG;
    double a=atan2(-cos(dec)*sin(H), sin(dec)*cl - cos(dec)*sl*cos(H));
    *az=fmod(a*DEG+360.0,360.0);
}
/* simplified lunar position (Schlyter) + illuminated fraction; visual accuracy only */
void fb_moon_pos(double jd,double lat,double lon,double*el,double*az,double*illum){
    double d=jd-2451545.0;
    double N=(125.1228-0.0529538083*d)*RAD, inc=5.1454*RAD, w=(318.0634+0.1643573223*d)*RAD;
    double a=60.2666, e=0.054900, M=fmod(115.3654+13.0649929509*d,360.0)*RAD;
    double E=M+e*sin(M)*(1+e*cos(M)); for(int k=0;k<3;k++) E=E-(E-e*sin(E)-M)/(1-e*cos(E));
    double xv=a*(cos(E)-e), yv=a*sqrt(1-e*e)*sin(E);
    double v=atan2(yv,xv), r=hypot(xv,yv);
    double xh=r*(cos(N)*cos(v+w)-sin(N)*sin(v+w)*cos(inc));
    double yh=r*(sin(N)*cos(v+w)+cos(N)*sin(v+w)*cos(inc));
    double zh=r*(sin(v+w)*sin(inc));
    double lon_e=atan2(yh,xh), lat_e=atan2(zh,hypot(xh,yh));
    double ecl=(23.4393-3.563e-7*d)*RAD;
    double xe=cos(lon_e)*cos(lat_e);
    double ye=sin(lon_e)*cos(lat_e)*cos(ecl)-sin(lat_e)*sin(ecl);
    double ze=sin(lon_e)*cos(lat_e)*sin(ecl)+sin(lat_e)*cos(ecl);
    double ra=atan2(ye,xe), dec=atan2(ze,hypot(xe,ye));
    double gmst=fmod(280.46061837+360.98564736629*d,360.0);
    double lst=fmod(gmst+lon,360.0)*RAD, H=lst-ra;
    double sl=sin(lat*RAD), cl=cos(lat*RAD);
    *el=asin(sl*sin(dec)+cl*cos(dec)*cos(H))*DEG;
    double A=atan2(-cos(dec)*sin(H), sin(dec)*cl-cos(dec)*sl*cos(H));
    *az=fmod(A*DEG+360.0,360.0);
    /* illuminated fraction from sun-moon elongation */
    double se,sa; fb_sun_pos(jd,lat,lon,&se,&sa);
    double sunlon=fmod(280.460+0.9856474*(jd-2451545.0),360.0)*RAD;
    double elong=acos(cos(lon_e-sunlon)*cos(lat_e));
    *illum=(1.0-cos(elong))/2.0;
}
