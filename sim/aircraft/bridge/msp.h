/* FlightBox — MSP client to iNav SITL (TCP 5760): RC inject + telemetry read. */
#ifndef FB_MSP_H
#define FB_MSP_H
#include <stdint.h>

extern int      msp_fd;            /* <0 until connected */
extern float    t_roll, t_pitch;   /* iNav attitude, deg (MSP_ATTITUDE) */
extern int      t_batt10;          /* battery voltage, tenths of a volt */
extern uint32_t t_armflags;        /* MSP2_INAV_STATUS arming flags */

int  msp_connect(void);
void msp1(uint8_t cmd, const uint8_t *p, uint8_t n);   /* MSPv1 request */
void msp2(uint16_t fn);                                 /* MSPv2 request (no payload) */
void msp_poll(void);                                    /* drain socket, parse replies */
int  mode_active(int boxid);                            /* is a mode box currently active? */

#endif /* FB_MSP_H */
