/* xpjsb/xplink — the X-Plane UDP dataref protocol iNav (--sim=xp) speaks. Single responsibility:
 * the wire. Owns the RREF subscription table; routes inbound DREF controls to actuators and
 * answers subscriptions from sensors. */
#ifndef XPJSB_XPLINK_H
#define XPJSB_XPLINK_H

/* Open the UDP socket iNav connects to (bind XPJSB_XP_PORT). Returns fd or <0. */
int  xpjsb_xp_open(int port);

/* Drain inbound packets: RREF -> subscription table, DREF -> actuators. Learns iNav's address
 * from the first packet so replies are addressed. Non-blocking. */
void xpjsb_xp_recv(int fd);

/* Send the current value of every subscribed dataref back to iNav (one RREF reply). */
void xpjsb_xp_send(int fd);

#endif /* XPJSB_XPLINK_H */
