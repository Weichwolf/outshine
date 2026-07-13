/* FlightBox — aircraft simulator container.
 * Simulates the whole flyer: FBW dynamics, state machine (§2.4), sensors,
 * GPS, battery and the "camera" (artificial horizon). Talks to the flightbox
 * over UDP (the fake radio): receives control uplink, sends telemetry+video.
 *
 * Env: FLIGHTBOX_ADDR (default 127.0.0.1) — where to send the downlink. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "protocol.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG (180.0f / (float)M_PI)
#define RAD ((float)M_PI / 180.0f)
#define TARGET_ALT   100.0f   /* auto-climb target, m */
#define TARGET_GLIDE 7.0f     /* ideal approach angle, deg */
#define LOITER_BANK  15.0f
#define CRUISE       12.0f

static float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }
static float wrap180(float a){ while(a>180)a-=360; while(a<-180)a+=360; return a; }

static void render_horizon(video_packet_t *v, float roll_deg, float pitch_deg){
    v->magic = FB_MAGIC_VIDEO; v->w = VID_W; v->h = VID_H; v->_pad = 0;
    float r = roll_deg * RAD, sr = sinf(r), cr = cosf(r);
    float cx = VID_W/2.0f, cy = VID_H/2.0f;
    float ppd = VID_H/50.0f;               /* pixels per degree of pitch */
    float thr = pitch_deg * ppd;
    uint16_t sky = RGB565(70,130,220), gnd = RGB565(60,150,60), line = RGB565(245,245,245);
    for (int y=0; y<VID_H; y++){
        for (int x=0; x<VID_W; x++){
            float dx = x-cx, dy = y-cy;
            float d = (dx*sr + dy*cr) - thr;   /* signed dist below horizon */
            uint16_t c = (d < 0.0f) ? sky : gnd;
            if (fabsf(d) < 1.2f) c = line;
            v->pix[y*VID_W + x] = c;
        }
    }
}

int main(void){
    const char *fb = getenv("FLIGHTBOX_ADDR"); if(!fb) fb = "127.0.0.1";

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0){ perror("socket"); return 1; }
    struct sockaddr_in me = {0};
    me.sin_family = AF_INET; me.sin_addr.s_addr = INADDR_ANY; me.sin_port = htons(FB_UP_PORT);
    if (bind(sock, (struct sockaddr*)&me, sizeof me) < 0){ perror("bind"); return 1; }
    int fl = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, fl | O_NONBLOCK);

    /* resolve the flightbox (host name in containers) with retry */
    struct sockaddr_in down = {0};
    for(;;){
        struct addrinfo hints = {0}, *res;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
        char ps[16]; snprintf(ps, sizeof ps, "%d", FB_DOWN_PORT);
        if (getaddrinfo(fb, ps, &hints, &res) == 0){ memcpy(&down, res->ai_addr, sizeof down); freeaddrinfo(res); break; }
        fprintf(stderr, "[aircraft] waiting to resolve %s ...\n", fb); sleep(1);
    }

    /* aircraft state */
    float roll=0, pitch=0, yaw=0, x=0, y=0, alt=0, speed=0, batt=12.6f;
    int state = ST_DISARMED, armed = 0;
    ctrl_packet_t ctrl = {0}; ctrl.link_up = 0;
    uint16_t seq = 0;
    const float dt = 0.05f;                 /* 20 Hz */

    fprintf(stderr, "[aircraft] downlink -> %s:%d, listening ctrl on :%d\n", fb, FB_DOWN_PORT, FB_UP_PORT);

    for(;;){
        /* drain control packets, keep the latest valid one */
        ctrl_packet_t c;
        while (recv(sock, &c, sizeof c, 0) == (ssize_t)sizeof c)
            if (c.magic == FB_MAGIC_CTRL) ctrl = c;

        /* ---- state machine + FBW ---- */
        if (!armed){
            if (ctrl.arm){ armed = 1; x = y = 0; alt = 0; speed = CRUISE*0.5f; state = ST_CLIMB; }
        }
        float tgt_bank = 0, tgt_pitch = 0, tgt_speed = CRUISE;

        if (armed){
            float home_dist = sqrtf(x*x + y*y);
            int link = ctrl.link_up;

            if (!link){                              /* RTH failsafe */
                state = (home_dist < 40.0f) ? ST_LOITER : ST_RTH;
            } else if (state == ST_CLIMB){
                if (alt >= TARGET_ALT) state = ST_MANUAL;
            } else {
                state = ST_MANUAL;
            }

            if (state == ST_CLIMB){
                tgt_bank = 0; tgt_pitch = 12; tgt_speed = CRUISE;
            } else if (state == ST_MANUAL){
                tgt_bank  = ctrl.roll  * 35.0f;
                tgt_pitch = ctrl.pitch * 15.0f;
                tgt_speed = 7.0f + ctrl.throttle * 9.0f;
            } else if (state == ST_LOITER){
                tgt_bank = LOITER_BANK; tgt_pitch = 0;   /* hold alt, circle */
            } else if (state == ST_RTH){
                float brg = atan2f(-x, -y) * DEG;        /* bearing to home (0,0) */
                float herr = wrap180(brg - yaw);
                tgt_bank = clampf(herr * 0.7f, -30.0f, 30.0f);
                tgt_pitch = 0;
            }
        }
        /* envelope protection */
        tgt_bank  = clampf(tgt_bank, -45.0f, 45.0f);
        tgt_pitch = clampf(tgt_pitch, -20.0f, 20.0f);

        /* first-order attitude response */
        float k = 3.0f * dt;
        roll  += (tgt_bank  - roll ) * k;
        pitch += (tgt_pitch - pitch) * k;
        if (armed) speed += (tgt_speed - speed) * (1.5f*dt);

        /* kinematics: coordinated turn, N-up heading */
        if (speed > 0.1f){
            float yaw_rate = (9.81f * tanf(roll*RAD) / speed) * DEG;  /* deg/s */
            yaw = wrap180(yaw + yaw_rate * dt);
        }
        float climb = speed * sinf(pitch*RAD);
        alt += climb * dt; if (alt < 0) alt = 0;
        x += speed * sinf(yaw*RAD) * dt;   /* x = east */
        y += speed * cosf(yaw*RAD) * dt;   /* y = north */
        if (armed) batt -= 0.0004f;        /* slow drain */

        /* ---- telemetry ---- */
        float home_dist = sqrtf(x*x + y*y);
        float brg = atan2f(-x, -y) * DEG;
        float need = (home_dist > 1.0f) ? atan2f(alt, home_dist) * DEG : 0.0f;

        telem_packet_t t = {0};
        t.magic = FB_MAGIC_TELEM;
        t.roll = roll; t.pitch = pitch; t.yaw = yaw;
        t.alt = alt; t.x = x; t.y = y; t.gs = speed; t.batt = batt;
        t.home_dist = home_dist;
        t.home_bearing = wrap180(brg - yaw);
        t.glideslope_err = need - TARGET_GLIDE;
        t.state = (uint8_t)state; t.rssi = ctrl.link_up ? 96 : 0;
        t.seq = seq;
        sendto(sock, &t, sizeof t, 0, (struct sockaddr*)&down, sizeof down);

        /* ---- video (artificial horizon) ---- */
        static video_packet_t v;
        render_horizon(&v, roll, pitch);
        v.seq = seq;
        sendto(sock, &v, sizeof v, 0, (struct sockaddr*)&down, sizeof down);

        seq++;
        struct timespec ts = {0, (long)(dt*1e9)};
        nanosleep(&ts, NULL);
    }
    return 0;
}
