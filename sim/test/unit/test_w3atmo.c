/* Unit tests — command_center/atmo.h  (the RENDERER's atmosphere: sun, sky colour, light level)
 *
 * Not to be confused with test_atmosphere.c, which covers aircraft/fdm/atmosphere.c — air density,
 * wind and thermals. Two different atmospheres; the prefixes keep them apart (`fb_atmo` is the
 * engine's, `w3_atmo` is the renderer's).
 *
 * These numbers decide every pixel of the image — the day/night ramp, the horizon colour, how dark
 * the world gets — and until this header existed, nothing could check them: they were nine locals
 * welded to glUniform calls inside a 90-line render function. The failure mode is the reason it
 * matters: a wrong constant here does not crash and does not look broken. It looks like weather.
 *
 * One of these constants has already cost a night: `light = 0.20 + 0.80*day` dims the world to
 * 20 % after dusk, which is what made pngstat's daylight-calibrated threshold blind (a real night
 * world scored 1.1 %, an empty one 1.0 %). The 0.20 floor is pinned below — not because it is
 * sacred, but because changing it silently changes what every screenshot gate sees.
 */
#include "tassert.h"
#include "../../command_center/atmo.h"
#include <math.h>

static telem_packet_t mk(float sun_el, float sun_az, float cloud) {
    telem_packet_t t;
    memset(&t, 0, sizeof t);
    t.sun_el = sun_el; t.sun_az = sun_az; t.cloud = cloud;
    t.moon_el = 0; t.moon_az = 0; t.moon_phase = 0.5f;
    return t;
}

void test_w3atmo(void) {
    tsection("w3_atmo: the day ramp is civil twilight, not sunset");
    {
        /* -6..+6 deg is the ramp. Below it nothing gets darker; above it nothing gets brighter.
         * Pinning the endpoints AND the midpoint catches an inverted or rescaled ramp, which a
         * single sample cannot. */
        telem_packet_t t = mk(-6.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).day, 0.0f, 1e-6, "sun at -6 deg -> day = 0 (ramp starts)");
        t = mk(6.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).day, 1.0f, 1e-6, "sun at +6 deg -> day = 1 (ramp ends)");
        t = mk(0.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).day, 0.5f, 1e-6, "sun on the horizon -> day = 0.5 (midpoint)");
        t = mk(-14.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).day, 0.0f, 1e-6, "deep night clamps at 0, does not go negative");
        t = mk(60.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).day, 1.0f, 1e-6, "high noon clamps at 1, does not exceed");
    }

    tsection("w3_atmo: the light floor is 0.20 — the number that blinded a gate");
    {
        telem_packet_t t = mk(-14.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 0.20f, 1e-6,
                "night light is 0.20, NOT 0 — the world is dim, not absent");
        t = mk(60.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 1.00f, 1e-6, "full day -> light = 1.00");
        t = mk(0.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 0.60f, 1e-6, "horizon -> light = 0.20 + 0.80*0.5");
    }

    tsection("w3_atmo: sun direction is ENU (E=+X, up=+Y, N=-Z)");
    {
        /* The axis convention is the one that mirrors silently when it is wrong — the same class
         * mat4.h pins for the camera. North is -Z, and nothing in a screenshot says so. */
        telem_packet_t t = mk(0.f, 0.f, 0);          /* due north, on the horizon */
        w3_atmo a = w3_atmo_from(&t, 1);
        ck_near(a.sun[0],  0.0f, 1e-5, "az=0 (north): x = 0");
        ck_near(a.sun[1],  0.0f, 1e-5, "az=0, el=0: y = 0 (on the horizon)");
        ck_near(a.sun[2], -1.0f, 1e-5, "az=0 (north) points to -Z, not +Z");

        t = mk(0.f, 90.f, 0);                        /* due east */
        a = w3_atmo_from(&t, 1);
        ck_near(a.sun[0], 1.0f, 1e-5, "az=90 (east) points to +X");
        ck_near(a.sun[2], 0.0f, 1e-5, "az=90: z = 0");

        t = mk(90.f, 123.f, 0);                      /* straight up; azimuth must not matter */
        a = w3_atmo_from(&t, 1);
        ck_near(a.sun[1], 1.0f, 1e-5, "el=90 -> straight up (+Y), whatever the azimuth");
        ck_near(a.sun[0], 0.0f, 1e-5, "el=90: x = 0");
        ck_near(a.sun[2], 0.0f, 1e-5, "el=90: z = 0");

        /* unit length at an oblique angle: catches a missing cos(el) factor, which the axis
         * cases above would not — they all have cos or sin equal to 0 or 1. */
        t = mk(37.f, 214.f, 0);
        a = w3_atmo_from(&t, 1);
        float len = sqrtf(a.sun[0]*a.sun[0] + a.sun[1]*a.sun[1] + a.sun[2]*a.sun[2]);
        ck_near(len, 1.0f, 1e-5, "oblique sun direction is unit length");
        t = mk(0, 0, 0); t.moon_el = 37.f; t.moon_az = 214.f;
        a = w3_atmo_from(&t, 1);
        len = sqrtf(a.moon[0]*a.moon[0] + a.moon[1]*a.moon[1] + a.moon[2]*a.moon[2]);
        ck_near(len, 1.0f, 1e-5, "oblique moon direction is unit length too");
    }

    tsection("w3_atmo: haze is the horizon colour, and cloud only pulls it 45 % of the way");
    {
        telem_packet_t t = mk(60.f, 0, 0.f);         /* clear noon */
        w3_atmo a = w3_atmo_from(&t, 1);
        ck_near(a.haze[0], 0.72f, 1e-5, "clear day haze r = 0.05 + 0.67");
        ck_near(a.haze[1], 0.82f, 1e-5, "clear day haze g = 0.06 + 0.76");
        ck_near(a.haze[2], 0.92f, 1e-5, "clear day haze b = 0.13 + 0.79 (bluest channel)");
        ck(a.haze[2] > a.haze[0], "a clear daytime horizon is blue-dominant");

        t = mk(-14.f, 0, 0.f);                       /* clear night */
        a = w3_atmo_from(&t, 1);
        ck_near(a.haze[0], 0.05f, 1e-5, "night haze is nearly black, not black");
        ck(a.haze[2] > a.haze[0], "even a night horizon stays blue-dominant — pngstat leans on this");

        /* Full cover must NOT reach the flat grey: 0.45 is the cap, and an overcast noon that
         * went pure white is exactly what that cap prevents. */
        t = mk(60.f, 0, 1.f);
        a = w3_atmo_from(&t, 1);
        ck_near(a.haze[0], 0.756f, 1e-4, "overcast noon r moves 45 % toward 0.80, not all the way");
        ck_near(a.haze[2], 0.866f, 1e-4, "overcast noon b moves 45 % toward 0.80 (downward)");
        ck(a.haze[2] < 0.92f && a.haze[2] > 0.80f, "cloud pulls b down toward grey but not past it");
    }

    tsection("w3_atmo: cloud is clamped, and the no-telemetry defaults are the ones we ship");
    {
        telem_packet_t t = mk(0, 0, -0.5f);
        ck_near(w3_atmo_from(&t, 1).cloud, 0.0f, 1e-6, "negative cloud clamps to 0");
        t = mk(0, 0, 2.0f);
        ck_near(w3_atmo_from(&t, 1).cloud, 1.0f, 1e-6, "cloud above 1 clamps to 1");

        /* have=0 is the first frame of every page load. If these drift, a fresh browser shows a
         * different world for a second and nobody knows why. */
        t = mk(0, 0, 0);
        w3_atmo a = w3_atmo_from(&t, 0);
        ck_near(a.day,     1.0f, 1e-6, "no telemetry -> daylight (sun_el 35 clamps day to 1)");
        ck_near(a.light,   1.0f, 1e-6, "no telemetry -> full light");
        ck_near(a.cloud,   0.3f, 1e-6, "no telemetry -> 0.3 cloud");
        ck_near(a.moon_ph, 0.5f, 1e-6, "no telemetry -> half moon");
        ck(a.sun[1] > 0, "no telemetry -> the sun is above the horizon");
    }

    tsection("w3_atmo: it is a pure function");
    {
        /* Same input twice must give the same output — no hidden state, no time, no globals.
         * That is the whole reason this can be tested at all. */
        telem_packet_t t = mk(12.f, 200.f, 0.4f);
        w3_atmo a = w3_atmo_from(&t, 1), b = w3_atmo_from(&t, 1);
        ck(memcmp(&a, &b, sizeof a) == 0, "same telemetry -> byte-identical atmosphere");
    }
}
