/* FlightBox simulation — shared wire protocol.
 * "Radio" = UDP between the aircraft-sim container and the flightbox container.
 * The same structs travel flightbox <-> browser over WebSocket (binary).
 * Everything little-endian, packed; both ends are C. */
#ifndef FB_PROTOCOL_H
#define FB_PROTOCOL_H
#include <stdint.h>

/* UDP ports of the fake radio link */
#define FB_UP_PORT    6001   /* flightbox -> aircraft : control uplink   */
#define FB_DOWN_PORT  6002   /* aircraft -> flightbox : telemetry+video  */

/* "Camera" = artificial horizon, small enough for one UDP datagram */
#define VID_W 160
#define VID_H 120

#define FB_MAGIC_CTRL   0x314C5443u  /* 'CTL1' */
#define FB_MAGIC_TELEM  0x314D4C54u  /* 'TLM1' */
#define FB_MAGIC_VIDEO  0x31444956u  /* 'VID1' */

/* flight states (see README §2.4) */
enum { ST_DISARMED = 0, ST_ARMED, ST_CLIMB, ST_LOITER, ST_MANUAL, ST_RTH };

#pragma pack(push, 1)

/* Uplink: pilot correction requests (normalized, CRSF-like) */
typedef struct {
    uint32_t magic;      /* FB_MAGIC_CTRL */
    float    roll;       /* -1..1 correction request */
    float    pitch;      /* -1..1 */
    float    yaw;        /* -1..1 */
    float    throttle;   /*  0..1 */
    uint8_t  arm;        /* 0/1 */
    uint8_t  link_up;    /* 1 = pilot link present (0 => RTH failsafe) */
    uint16_t seq;
} ctrl_packet_t;

/* Downlink telemetry (drives the HUD, rendered in the command center) */
typedef struct {
    uint32_t magic;          /* FB_MAGIC_TELEM */
    float    roll, pitch, yaw;   /* deg */
    float    alt;                /* m  */
    float    x, y;               /* local position, m (home = 0,0) */
    float    gs;                 /* ground speed m/s */
    float    batt;               /* volts */
    float    home_dist;          /* m */
    float    home_bearing;       /* deg, relative to nose (-180..180) */
    float    glideslope_err;     /* deg; + = above the ideal approach path */
    uint8_t  state;              /* ST_* */
    uint8_t  rssi;               /* 0..100 link quality */
    uint16_t seq;
} telem_packet_t;

/* Downlink video: one artificial-horizon frame, RGB565 (160*120*2 = 38400 B) */
typedef struct {
    uint32_t magic;      /* FB_MAGIC_VIDEO */
    uint16_t w, h;
    uint16_t seq;
    uint16_t _pad;
    uint16_t pix[VID_W * VID_H];   /* RGB565 */
} video_packet_t;

#pragma pack(pop)

#define RGB565(r, g, b) \
    ((uint16_t)(((((uint32_t)(r)) & 0xF8u) << 8) | \
                (((((uint32_t)(g)) & 0xFCu)) << 3) | \
                (((uint32_t)(b)) >> 3)))

#endif /* FB_PROTOCOL_H */
