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
 * SPEC CHANGE (commit 4cee44d), and why these pins moved. `day` used to exist THREE times — a CPU
 * curve linear in degrees, the sky shader's own on sin(el), and the star cutoff — up to 13 points
 * apart through twilight. They are now ONE physics-based curve, w3_daylight(sun_el): a smoothstep
 * over [-9, +3] deg, so deep night (below nautical twilight, ~-9 deg) is genuinely DARK for EVS.
 * With it the light floor dropped 0.20 -> 0.08 on purpose: 0.20 kept the night grey. This file
 * FOLLOWS that spec — it does not hold the old numbers to keep the gate green (that would be
 * forgery); it re-pins the new curve the renderer actually ships. The 0.20 floor once blinded
 * pngstat's daylight-calibrated threshold (a night world scored 1.1 %, an empty one 1.0 %); that
 * predicate now scales its margin with pixel brightness (e2d7dad), so 0.08 is checked below AND the
 * night-screenshot separation is verified out-of-band — the constant no longer silently owns a gate.
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
    tsection("w3_daylight: the twilight smoothstep over [-9, +3] deg, dark by nautical twilight");
    {
        /* The new spec: a smoothstep t*t*(3-2t), t = clamp((sun_el+9)/12, 0, 1). Center at -3 deg,
         * half-width 6 deg. Ramp START -9 (nautical twilight, genuinely dark) and END +3 (full day)
         * pin the offset; the -3 midpoint and the -6/0 shoulders pin the SHAPE. The shoulders are
         * where the S-curve earns its keep: at sun_el -6 (t=0.25) smoothstep gives 0.15625, where a
         * LINEAR ramp would give 0.25 — pinning that value is what catches a linear-vs-smoothstep
         * regression, which endpoints alone cannot. Tested directly on w3_daylight (pure) AND through
         * w3_atmo_from().day (they must agree — atmo reads the same curve). */
        ck_near(w3_daylight(-9.f), 0.0f,     1e-6, "sun -9 deg (nautical twilight) -> day 0, ramp start");
        ck_near(w3_daylight(-6.f), 0.15625f, 1e-6, "sun -6 deg -> 0.15625 (smoothstep shoulder, NOT linear 0.25)");
        ck_near(w3_daylight(-3.f), 0.5f,     1e-6, "sun -3 deg -> 0.5 (curve centre)");
        ck_near(w3_daylight( 0.f), 0.84375f, 1e-6, "sun on the horizon -> 0.84375 (upper shoulder)");
        ck_near(w3_daylight( 3.f), 1.0f,     1e-6, "sun +3 deg -> day 1, ramp end");
        ck_near(w3_daylight(-14.f), 0.0f,    1e-6, "deep night clamps at 0, does not go negative");
        ck_near(w3_daylight(60.f),  1.0f,    1e-6, "high noon clamps at 1, does not exceed");
        /* monotone non-decreasing across the whole ramp — no dip, no overshoot */
        for (float e = -12.f; e <= 6.f; e += 0.5f)
            ck(w3_daylight(e) <= w3_daylight(e + 0.5f) + 1e-6f, "w3_daylight is monotone in sun elevation");
        /* atmo.day IS this curve, not a second copy */
        telem_packet_t t = mk(0.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).day, w3_daylight(0.f), 1e-6, "w3_atmo_from().day == w3_daylight(sun_el)");
    }

    tsection("w3_daylight: the <0.6 star cutoff (sky.h) sits in nautical twilight");
    {
        /* The renderer draws stars only where day < 0.6 (sky.h: `if(A->day>=0.6f...) return;`). With
         * this curve that boundary is sun_el ~ -2.2 deg (day(-2.2)=0.599, day(-2.0)=0.623), i.e.
         * stars appear in nautical/late-civil twilight and are gone well before the horizon. Pinning
         * the day value either side of 0.6 fixes WHEN the sky first shows stars — a thing no
         * screenshot gate can see (pngstat scores ground). */
        ck(w3_daylight(-3.f) < 0.6f,  "at sun -3 deg day<0.6 -> stars drawn (twilight sky)");
        ck(w3_daylight(-2.f) >= 0.6f, "by sun -2 deg day>=0.6 -> stars suppressed");
        ck(w3_daylight( 0.f) >= 0.6f, "at the horizon day>=0.6 -> no stars");
    }

    tsection("w3_atmo: the light floor is 0.08 — EVS night is genuinely dark now");
    {
        /* light = 0.08 + 0.92*day (was 0.20 + 0.80*day). The floor dropped so deep night actually
         * goes dark for EVS; 0.08 is not zero (the world is dim, not absent). This is the constant
         * the header warns about — verified against the night screenshot separately, not left to
         * silently move a gate. */
        telem_packet_t t = mk(-14.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 0.08f, 1e-6,
                "deep-night light is 0.08 (dark but not absent), NOT the old 0.20");
        t = mk(60.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 1.00f, 1e-6, "full day -> light = 1.00");
        t = mk(0.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 0.85625f, 1e-5, "horizon -> 0.08 + 0.92*0.84375");
        t = mk(-3.f, 0, 0);
        ck_near(w3_atmo_from(&t, 1).light, 0.54f, 1e-5, "curve centre -> 0.08 + 0.92*0.5");
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

    tsection("w3_atmo_synthetic: constant map-like daylight, per RTCA DO-315B SVS");
    {
        /* The values pinned here are the SVS convention, not tuning: full day, full light, no
         * cloud, NO sun disc (map-blue, not a photo of the sky). If any drifts, the synthetic
         * vision mode stops looking like a PFD and starts looking like the real sky. */
        w3_atmo s = w3_atmo_synthetic();
        ck_near(s.day,     1.0f, 1e-6, "SVS is always full day");
        ck_near(s.light,   1.0f, 1e-6, "SVS is always full light");
        ck_near(s.cloud,   0.0f, 1e-6, "SVS has no cloud");
        ck_near(s.moon_ph, 0.0f, 1e-6, "SVS has no moon phase");
        ck_near(s.sun_disc, 0.0f, 1e-6, "SVS draws NO sun disc — plain map-blue, the point of SVS");

        /* Sun fixed at el=55, az=160 (high, for even relief shading). Pin the vector so the
         * directional light cannot wander. */
        ck_near(s.sun[0],  0.19617f, 1e-4, "SVS sun x = cos55*sin160");
        ck_near(s.sun[1],  0.81915f, 1e-4, "SVS sun y = sin55 (high, above horizon)");
        ck_near(s.sun[2],  0.53900f, 1e-4, "SVS sun z = -cos55*cos160");
        float sl = sqrtf(s.sun[0]*s.sun[0] + s.sun[1]*s.sun[1] + s.sun[2]*s.sun[2]);
        ck_near(sl, 1.0f, 1e-5, "SVS sun direction is unit length");
        ck(s.sun[1] > 0, "SVS sun is above the horizon");

        /* Moon deliberately below the horizon: no moon disc in synthetic vision. */
        ck_near(s.moon[0],  0.0f, 1e-6, "SVS moon x = 0");
        ck_near(s.moon[1], -1.0f, 1e-6, "SVS moon points straight down — below horizon, no disc");
        ck_near(s.moon[2],  0.0f, 1e-6, "SVS moon z = 0");

        ck_near(s.haze[0], 0.72f, 1e-6, "SVS horizon r (clear-day blue)");
        ck_near(s.haze[1], 0.82f, 1e-6, "SVS horizon g");
        ck_near(s.haze[2], 0.92f, 1e-6, "SVS horizon b (bluest channel)");
        ck(s.haze[2] > s.haze[0], "SVS horizon is blue-dominant");
    }

    tsection("w3_atmo_synthetic: TIME-INDEPENDENT — the architectural contract");
    {
        /* THE contract for SVS: the synthetic atmosphere ignores time and telemetry entirely.
         * The signature already says so — w3_atmo_synthetic() takes void, so no packet and no
         * clock can reach it. This asserts the consequence: whatever the world is doing, whatever
         * SIM_UTC reads, two calls are byte-identical. That is what makes SVS a stable reference
         * view instead of a second sky renderer. */
        w3_atmo x = w3_atmo_synthetic();
        w3_atmo y = w3_atmo_synthetic();
        ck(memcmp(&x, &y, sizeof x) == 0, "synthetic atmosphere is byte-identical across calls");

        /* And it must NOT equal a real atmosphere driven by live telemetry — a positive check
         * that SVS is genuinely a different, fixed view, not accidentally the daytime default. */
        telem_packet_t night = mk(-14.f, 300.f, 0.9f);   /* deep night, heavy cloud */
        w3_atmo real = w3_atmo_from(&night, 1);
        ck(memcmp(&x, &real, sizeof x) != 0,
           "synthetic ignores telemetry: stays daylight even when the world is night");
        ck(x.sun_disc != real.sun_disc || x.day != real.day,
           "synthetic day/sun_disc differ from a live night atmosphere");
    }
}
