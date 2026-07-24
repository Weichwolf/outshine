/* xpjsb/sensors — JSBSim world state -> X-Plane datarefs iNav (--sim=xp, --useimu) reads.
 * The magnetometer is the load-bearing piece: iNav's AHRS needs a DIPPED geomagnetic field
 * (inclination), not a flat horizontal north, or its yaw correction misconverges (~224 deg off).
 * We feed the real body-frame field via the XITL path (version 2) so iNav uses it directly. */
#include <string.h>
#include <math.h>
#include "constants.h"
#include "sensors.h"
#include "../sim_state.h"

static double baro_inhg(double alt_m){ return 29.92126 * pow(1.0 - 2.25577e-5*alt_m, 5.25588); }

float xpjsb_sensor_value(const char *d){
    /* --- attitude (truth; also drives iNav's computed-mag fallback if XITL were off) --- */
    if(!strcmp(d,"sim/flightmodel/position/phi"))   return (float)S.roll;
    if(!strcmp(d,"sim/flightmodel/position/theta")) return (float)(-S.pitch);  /* X-Plane theta sign is opposite iNav pitch */
    if(!strcmp(d,"sim/flightmodel/position/psi"))   return (float)(S.yaw<0?S.yaw+360:S.yaw);
    if(!strcmp(d,"sim/flightmodel/position/hpath")){                            /* GPS ground track != heading in a turn/wind */
        if(S.gs < 1.0) return (float)(S.yaw<0?S.yaw+360:S.yaw);
        double trk = atan2(S.vx, -S.vz) * XPJSB_DEG;                            /* vx=east, -vz=north */
        return (float)(trk<0 ? trk+360 : trk);
    }
    /* --- gyro (body rates, deg/s) + gust --- */
    if(!strcmp(d,"sim/flightmodel/position/P")) return (float)(S.p+ATM.gustP);
    if(!strcmp(d,"sim/flightmodel/position/Q")) return (float)(S.q+ATM.gustQ);
    if(!strcmp(d,"sim/flightmodel/position/R")) return (float)S.r;
    /* --- body specific forces (g): fed to iNav's IMU under --useimu --- */
    if(!strcmp(d,"sim/flightmodel/forces/g_axil")) return g_nx;
    if(!strcmp(d,"sim/flightmodel/forces/g_side")) return g_ny;
    if(!strcmp(d,"sim/flightmodel/forces/g_nrml")) return g_nz;
    /* --- kinematics / position --- */
    if(!strcmp(d,"sim/flightmodel/position/latitude"))     return (float)S.lat;
    if(!strcmp(d,"sim/flightmodel/position/longitude"))    return (float)S.lon;
    if(!strcmp(d,"sim/flightmodel/position/elevation"))    return (float)S.elev;
    if(!strcmp(d,"sim/flightmodel/position/y_agl"))        return (float)S.agl;
    if(!strcmp(d,"sim/flightmodel/position/local_vx"))     return (float)S.vx;
    if(!strcmp(d,"sim/flightmodel/position/local_vy"))     return (float)S.vy;
    if(!strcmp(d,"sim/flightmodel/position/local_vz"))     return (float)S.vz;
    if(!strcmp(d,"sim/flightmodel/position/groundspeed"))  return (float)S.gs;
    if(!strcmp(d,"sim/flightmodel/position/true_airspeed"))return (float)S.speed;
    if(!strcmp(d,"sim/weather/barometer_current_inhg"))    return (float)baro_inhg(S.elev);
    if(!strcmp(d,"sim/joystick/has_joystick"))             return 1.0f;

    /* --- plain X-Plane (XITL version 0): iNav sends the throttle/servo DREFs itself and computes the
     *     magnetometer from the truth attitude. We run truth-attitude, so iNav uses the given yaw as
     *     heading; the mag need only be PRESENT (SENSOR_MAG) for navIsHeadingUsable — no dipped field,
     *     no --useimu, no iNav patch. (XITL version 2 would suppress the throttle DREF.) --- */
    if(!strcmp(d,"inav_xitl/plugin/xitlDrefVersion")) return 0.0f;
    if(!strcmp(d,"inav_xitl/gps/numSats"))   return (float)XPJSB_GPS_NUM_SATS;
    if(!strcmp(d,"inav_xitl/gps/fix"))       return (float)XPJSB_GPS_FIX_3D;
    if(!strcmp(d,"inav_xitl/gps/latitude"))  return (float)S.lat;
    if(!strcmp(d,"inav_xitl/gps/longitude")) return (float)S.lon;
    if(!strcmp(d,"inav_xitl/gps/elevation")) return (float)S.elev;
    if(!strcmp(d,"inav_xitl/gps/groundspeed"))return (float)S.gs;
    if(!strcmp(d,"inav_xitl/gps/velocities[0]")) return (float)(-S.vz);  /* north */
    if(!strcmp(d,"inav_xitl/gps/velocities[1]")) return (float)S.vx;     /* east */
    if(!strcmp(d,"inav_xitl/gps/velocities[2]")) return (float)(-S.vy);  /* down */
    if(!strcmp(d,"inav_xitl/sensors/airspeed"))        return (float)S.speed;
    if(!strcmp(d,"inav_xitl/sensors/battery_voltage")) return 12.0f;
    if(!strcmp(d,"inav_xitl/sensors/battery_current")) return 1.0f;
    if(!strcmp(d,"inav_xitl/sensors/rangefinder"))     return -1.0f;
    if(!strcmp(d,"inav_xitl/rc/rssi"))       return 100.0f;
    if(!strcmp(d,"inav_xitl/rc/failsafe"))   return 0.0f;
    return 0.0f;
}
