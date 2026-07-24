/* FlightBox — MSP client to iNav SITL (TCP 5760): RC inject + telemetry read. */
#ifndef FB_MSP_H
#define FB_MSP_H
#include <stdint.h>

extern int      msp_fd;            /* <0 until connected */
extern float    t_roll, t_pitch;   /* iNav attitude, deg (MSP_ATTITUDE) */
extern int      t_batt10;          /* battery voltage, tenths of a volt */
extern uint32_t t_armflags;        /* MSP2_INAV_STATUS arming flags */
extern uint32_t t_modeflags;       /* MSP_STATUS flightModeFlags: iNav confirmed active modes (bit0=ARM,1=ANGLE,2=NAV RTH per box order) */
extern int t_navstate, t_navwp, t_naverr;   /* MSP_NAV_STATUS: nav state machine, active WP number, nav error code */
extern int t_servo[16], t_nservo;           /* MSP_SERVO: iNav's mixer servo outputs (PWM us) — the aux-actuator tap */
extern int    t_yaw, t_fix, t_sats;         /* iNav heading (deg), GPS fix type, sat count */
extern int    t_inav_dth, t_inav_dir;       /* iNav's own distance (m) / direction (deg) to home (MSP_COMP_GPS) */
extern int    t_estalt;                      /* iNav estimated altitude, cm (MSP_ALTITUDE) */
extern double t_inav_lat, t_inav_lon;        /* iNav's own GPS position (MSP_RAW_GPS) */

int  msp_connect(void);
void msp1(uint8_t cmd, const uint8_t *p, uint8_t n);   /* MSPv1 request */
void msp2(uint16_t fn);                                 /* MSPv2 request (no payload) */
void msp_poll(void);                                    /* drain socket, parse replies */
int  mode_active(int boxid);                            /* is a mode box currently active? */

#endif /* FB_MSP_H */
