/* FlightBox — X-Plane UDP link layer: dataref subscription table + sensor readout.
 * iNav (RREF) subscribes to datarefs; we answer each from the physics state. */
#ifndef FB_XP_LINK_H
#define FB_XP_LINK_H

typedef struct { int id; char dref[96]; } xp_sub_t;
extern xp_sub_t subs[128];
extern int      nsubs;

void  add_sub(int id, const char *dref);   /* register/refresh an iNav RREF subscription */
float sensor_value(const char *d);         /* current value for a subscribed dataref, from physics */

#endif /* FB_XP_LINK_H */
