/* xpjsb — XP-JSBSim bridge: all tunables and physical constants, named from their derivation.
 * No magic numbers anywhere else in the bridge. */
#ifndef XPJSB_CONSTANTS_H
#define XPJSB_CONSTANTS_H

/* --- loop / timing --- */
#define XPJSB_DT_S            0.01     /* World step + sensor exchange period (100 Hz) */
#define XPJSB_ESC_SPINUP_S    0.5      /* ESC ramps the motor over this; a throttle STEP blows a light prop's RPM ODE */

/* --- wire ports (iNav --sim=xp side) --- */
#define XPJSB_XP_PORT         49000    /* X-Plane UDP dataref port iNav connects to (--simport) */
#define XPJSB_MSP_PORT        5760     /* iNav SITL MSP over TCP (UART1) — telemetry/diag read */

/* --- geodesy --- */
#define XPJSB_M_PER_DEG       111320.0 /* metres per degree of latitude (spherical) */
#define XPJSB_EARTH_RADIUS_M  6371000.0
#define XPJSB_DEG             57.29577951308232   /* rad -> deg */
#define XPJSB_RAD             0.017453292519943295 /* deg -> rad */

/* --- geomagnetic field (for the --useimu magnetometer feed) ---
 * iNav's AHRS needs a DIPPED field, not a flat horizontal north, or its Mahony yaw
 * correction misconverges. Values are a mid-latitude Central-Europe default (IGRF-ish);
 * override per site via env if needed. Declination small (~+3E) → folded into the field. */
#define XPJSB_MAG_INCLINATION_DEG  64.0   /* dip angle (EDNY/Central Europe) */
#define XPJSB_MAG_DECLINATION_DEG   3.0   /* east-positive */
#define XPJSB_MAG_UNIT             1024.0f /* iNav fakeMagSet scale: unit field * this */

/* --- actuator input validation (X-Plane yoke ratios) ---
 * iNav's SITL derives the yoke from servo PWM as (servo-1500)/500; a momentarily-unwritten
 * servo of 0 arrives as EXACTLY -3.0 (3x full deflection). Reject out-of-range, hold last good. */
#define XPJSB_YOKE_MAX_ABS        1.05f  /* |roll/pitch/yaw ratio| above this is a glitch, not a command */
#define XPJSB_THR_MIN            -0.05f
#define XPJSB_THR_MAX             1.05f
#define XPJSB_PWM_MIN             1000   /* servo PWM range (us) for aux-actuator normalization */
#define XPJSB_PWM_MAX             2000

/* --- GPS (fed to iNav; fix dynamics, not wire format) --- */
#define XPJSB_GPS_FIX_3D          2      /* iNav enum: GPS_FIX_3D == 2, NOT 3 */
#define XPJSB_GPS_NUM_SATS        16

/* --- XITL dataref protocol (real-mag path for --useimu) --- */
#define XPJSB_XITL_DREF_VERSION   2      /* >= iNav's XITL_DREF_VERSION → iNav uses the fed magnetometer */

/* --- flight log --- */
#define XPJSB_FLT_LOG_S_DEFAULT   1.0    /* seconds per [flt] pilot-log line (env FLT_LOG_S) */

#endif /* XPJSB_CONSTANTS_H */
