/* FlightBox — downlink to flightbox: telemetry packet + artificial-horizon video. */
#ifndef FB_TELEMETRY_H
#define FB_TELEMETRY_H
#include <netinet/in.h>

void telem_send(int fbfd, struct sockaddr_in fbdst, int link_up);  /* pose/state telem, one datagram */
void telem_video(int fbfd, struct sockaddr_in fbdst);              /* artificial-horizon RGB565 frame */

#endif /* FB_TELEMETRY_H */
