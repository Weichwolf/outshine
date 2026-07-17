/* Unit tests — command_center/stars.h  (where a star actually stands)
 *
 * This is the arithmetic that was unverifiable by construction. pngstat scores GROUND; it has no
 * opinion about the sky. And no human can look at a night render and tell Orion in the right place
 * from Orion an hour — or a continent — off. So sidereal time, hour angle and the spherical
 * triangle sat in a render function for as long as they existed, and the only thing anyone could
 * say about them was that the picture had stars in it.
 *
 * Polaris ends that. From latitude phi the pole star stands at altitude phi, due north, at every
 * hour of every night. It is the oldest check in navigation, it is exact, and it pins the entire
 * transform — latitude, hour angle, azimuth convention and the ENU axes — in one case.
 */
#include "tassert.h"
#include "../../command_center/stars.h"
#include <math.h>

/* Polaris, straight out of the catalogue in world3d.h: {37.95f, 89.26f, 1.98f} */
#define POLARIS_RA   37.95
#define POLARIS_DEC  89.26
#define HAMELN_LAT   52.045

void test_stars(void) {
    tsection("stars: Polaris stands at your latitude, due north, ALWAYS");
    {
        /* The whole point: this must hold at EVERY sidereal time. If the hour angle, the azimuth
         * convention or the north axis is wrong, the pole star wanders — and a wandering pole star
         * is the one thing the sky cannot do. Sweeping the clock is what turns this from a lucky
         * sample into a proof. */
        for (int h = 0; h < 24; h++) {
            double lst = h * 15.0;                       /* one sidereal hour = 15 deg */
            w3_stardir d = w3_star_dir(lst, HAMELN_LAT, POLARIS_RA, POLARIS_DEC);
            ck(d.above, "Polaris is above the horizon at 52 N — at every hour");
            /* altitude = latitude, to within Polaris's ~0.74 deg off-pole wobble */
            double alt = asin(d.u) * 180.0 / M_PI;
            ck(fabs(alt - HAMELN_LAT) < 1.0, "Polaris altitude == observer latitude (+-1 deg)");
            /* due north: the north component dominates and east is nearly nothing */
            ck(d.n > 0.55f, "Polaris is NORTH (n > 0, dominant)");
            ck(fabs(d.e) < 0.02f, "Polaris has almost no east/west component");
        }
    }

    tsection("stars: Polaris from other latitudes — the equator and the pole");
    {
        /* At the equator the pole star sits ON the horizon, so the 0.03 cut must reject it.
         * At the north pole it is overhead. Both are famous, both are exact, and together they
         * pin the latitude term that a single mid-latitude case cannot. */
        w3_stardir eq = w3_star_dir(0, 0.0, POLARIS_RA, POLARIS_DEC);
        ck(!eq.above, "at the equator Polaris is on the horizon -> rejected by the 0.03 cut");

        w3_stardir pole = w3_star_dir(0, 90.0, POLARIS_RA, POLARIS_DEC);
        ck(pole.above, "at the north pole Polaris is up");
        ck(pole.u > 0.999f, "at the north pole Polaris is overhead (alt ~ 90)");

        w3_stardir south = w3_star_dir(0, -35.0, POLARIS_RA, POLARIS_DEC);
        ck(!south.above, "from the southern hemisphere Polaris is never visible");
    }

    tsection("stars: a star on the meridian, and which way it sets");
    {
        /* dec == lat and hour angle 0 -> straight overhead. The cleanest possible case: if the
         * spherical triangle is wrong this cannot land at the zenith. */
        w3_stardir z = w3_star_dir(100.0, 40.0, 100.0, 40.0);
        ck(z.above, "dec == lat at hour angle 0: above");
        ck(z.u > 0.9999f, "dec == lat at hour angle 0 -> the ZENITH");

        /* An equatorial star at hour angle EXACTLY +-90 deg is on the horizon by definition
         * (sinAlt = cos(lat)*cos(0)*cos(90) = 0) — from any latitude, which is why the celestial
         * equator rises due east and sets due west everywhere. The first version of this test
         * asserted such a star was "still up"; the code was right and the test was wrong. Left
         * here as the case it actually is: */
        w3_stardir horiz = w3_star_dir(90.0, 45.0, 0.0, 0.0);
        ck(!horiz.above, "equatorial star at hour angle 90 is ON the horizon -> rejected");

        /* Four hours past the meridian it is genuinely up, and it must be in the WEST. Hour angle
         * is the direction of time; a sign flip would have it rise in the west, which looks
         * completely normal in a screenshot. */
        w3_stardir w = w3_star_dir(60.0, 45.0, 0.0, 0.0);     /* H = +60 deg = 4 h past */
        ck(w.above, "equatorial star 4 h past the meridian is up at 45 N");
        ck(w.e < -0.5f, "4 h PAST the meridian -> setting toward the WEST (e < 0)");

        w3_stardir e = w3_star_dir(300.0, 45.0, 0.0, 0.0);    /* H = -60 deg = 4 h before */
        ck(e.above, "equatorial star 4 h before the meridian is up");
        ck(e.e > 0.5f, "4 h BEFORE the meridian -> rising from the EAST (e > 0)");

        /* An equatorial star on the meridian from 45 N: due south, altitude 45. */
        w3_stardir s = w3_star_dir(0.0, 45.0, 0.0, 0.0);
        ck(s.above, "equatorial star on the meridian at 45 N: up");
        ck(s.n < -0.5f, "an equatorial star culminates in the SOUTH from the north (n < 0)");
        ck(fabs(asin(s.u) * 180.0 / M_PI - 45.0) < 0.5, "culminating altitude = 90 - latitude");
    }

    tsection("stars: the direction is a unit vector, or the 40 km placement lies");
    {
        /* The renderer pushes each star 40 000 m along this vector. If it is not unit length the
         * stars land at different distances — invisible at infinity, but the maths would be wrong
         * and the next reader would build on it. */
        double cases[][4] = { {12.0, 52.0, 100.0, 20.0}, {200.0, -33.0, 95.0, -52.0},
                              {330.0, 60.0, 279.0, 38.8}, {45.0, 10.0, 88.8, 7.4} };
        for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            w3_stardir d = w3_star_dir(cases[i][0], cases[i][1], cases[i][2], cases[i][3]);
            if (!d.above) continue;
            double len = sqrt((double)d.e*d.e + (double)d.n*d.n + (double)d.u*d.u);
            ck(fabs(len - 1.0) < 1e-5, "star direction is unit length");
        }
    }

    tsection("stars: below the horizon means DON'T DRAW, and the fields are not to be trusted");
    {
        /* A star opposite the pole from 52 N is far below. `above` is the answer; e/n/u are left
         * at zero rather than a plausible-looking direction, because a caller that forgets to check
         * would otherwise draw a star in the ground and it would look like a light on the terrain. */
        w3_stardir d = w3_star_dir(0.0, 52.045, 0.0, -89.0);
        ck(!d.above, "a south-polar star is below the horizon at 52 N");
        ck(d.e == 0.0f && d.n == 0.0f && d.u == 0.0f, "below the horizon: fields are zeroed, not stale");
    }

    tsection("stars: sidereal time — the 4 minutes a day that nobody sees go wrong");
    {
        /* J2000.0 is jd 2451545.0 = Unix 946728000. GMST there is the polynomial's constant term,
         * so this pins the epoch itself. */
        double g = w3_gmst_deg(946728000.0);
        ck(fabs(g - 280.46061837) < 1e-6, "GMST at J2000.0 is the epoch constant, 280.4606 deg");

        /* One SOLAR day later the sky has turned 360.98565 deg, not 360 — that ~0.9856 deg is the
         * four minutes a star rises earlier each night. A code that used 360 would look perfect
         * tonight and be an hour off in a fortnight. Nothing in the render could ever say so. */
        double g1 = w3_gmst_deg(946728000.0 + 86400.0);
        double turn = fmod(g1 - g + 720.0, 360.0);
        ck(fabs(turn - 0.98564736629) < 1e-4, "one solar day advances GMST by 0.9856 deg, not 0");

        /* One SIDEREAL day (86164.0905 s) is by definition one full turn against the stars, so
         * GMST must come back to exactly where it was. This is the cleanest statement of what the
         * 360.98565 constant means, and it is the test that a naive 360.0 fails.
         * (An earlier version of this test asked for the same after one TROPICAL year and failed —
         * because 365.2422 days is not a whole number of days, so the leftover 0.2422 turns the
         * Earth another ~87 deg. The code was right; the expectation was written without doing the
         * arithmetic. Exactly the mistake this file exists to catch, made while writing it.) */
        double gsid = w3_gmst_deg(946728000.0 + 86164.0905);
        double back = fmod(gsid - g + 720.0, 360.0);
        ck(back < 0.01 || back > 359.99, "one SIDEREAL day returns GMST to where it started");

        ck(w3_gmst_deg(946728000.0) >= 0 && w3_gmst_deg(946728000.0) < 360, "GMST is normalised 0..360");
        /* Before the epoch fmod goes negative — the normalisation must catch it, or every star
         * flips to the far side of the sky for any date before 2000. */
        double gp = w3_gmst_deg(946728000.0 - 100.0 * 86400.0);
        ck(gp >= 0 && gp < 360, "GMST stays 0..360 for dates BEFORE J2000 (negative fmod)");
    }

    tsection("stars: longitude enters exactly once");
    {
        /* +15 deg of longitude = +1 h of local sidereal time. If this were subtracted, every star
         * would sit an hour off per 15 deg east — and Hameln at 9.4 E would look almost right. */
        ck(fabs(w3_lst_deg(100.0, 15.0) - 115.0) < 1e-9, "east longitude ADDS to sidereal time");
        ck(fabs(w3_lst_deg(100.0, -15.0) - 85.0) < 1e-9, "west longitude subtracts");
        ck(fabs(w3_lst_deg(350.0, 20.0) - 10.0) < 1e-9, "LST wraps at 360");
        double l = w3_lst_deg(5.0, -20.0);
        ck(l >= 0 && l < 360, "LST stays 0..360 when it would go negative");
        ck(fabs(l - 345.0) < 1e-9, "and wraps to the right place, not to 0");
    }

    tsection("stars: decode is 6 bytes/star, little-endian <HhBB — the wire format is pinned");
    {
        /* The HYG bands arrive as a raw byte stream from fb-tiles and get decoded ONCE at startup.
         * No screenshot can tell a byte-order or scale-factor mistake here from a correct one: the
         * stars just stand in the wrong places, which is exactly what the sky is allowed to do to a
         * human eye. So the formulas (stars.h:57-60) are pinned against hand-encoded bytes.
         *
         * Three stars, each 6 bytes LE: ra=u16/65536*360, dec=i16/32767*90, mag=u8/255*8-1.5,
         * bv=u8/255*3-0.5. Chosen so a big-endian read or a swapped ra/dec would miss:
         *   A: ra 90  (16384=00 40)  dec +90 (32767=FF 7F)  mag byte 0   bv byte 0
         *   B: ra 180 (32768=00 80)  dec  0  (0    =00 00)  mag byte 255 bv byte 255
         *   C: ra 270 (49152=00 C0)  dec -90 (-32767=01 80) mag byte 128 bv byte 64  */
        const uint8_t buf[18] = {
            0x00,0x40, 0xFF,0x7F, 0x00, 0x00,
            0x00,0x80, 0x00,0x00, 0xFF, 0xFF,
            0x00,0xC0, 0x01,0x80, 0x80, 0x40,
        };
        int n = w3_stars_load(buf, (int)sizeof buf);
        ck(n == 3, "18 bytes -> 3 stars");
        ck(w3_nstars == 3 && w3_stars != 0, "the global catalogue is filled");

        ck_near(w3_stars[0].ra,   90.0, 1e-3, "star A ra = u16 16384/65536*360 = 90 (LE low byte first)");
        ck_near(w3_stars[0].dec,  90.0, 1e-3, "star A dec = i16 32767/32767*90 = +90");
        ck_near(w3_stars[0].mag, -1.5,  1e-3, "mag byte 0 -> -1.5 (the brightest end)");
        ck_near(w3_stars[0].bv,  -0.5,  1e-3, "bv byte 0 -> -0.5 (bluest)");

        ck_near(w3_stars[1].ra,  180.0, 1e-3, "star B ra = u16 32768/65536*360 = 180");
        ck_near(w3_stars[1].dec,   0.0, 1e-3, "star B dec = 0");
        ck_near(w3_stars[1].mag,   6.5, 1e-3, "mag byte 255 -> 6.5 (the faint limit)");
        ck_near(w3_stars[1].bv,    2.5, 1e-3, "bv byte 255 -> 2.5 (reddest)");

        ck_near(w3_stars[2].ra,  270.0,    1e-3, "star C ra = u16 49152/65536*360 = 270");
        ck_near(w3_stars[2].dec, -90.0,    1e-3, "star C dec = i16 -32767 -> -90 (signed, not 65535 wrap)");
        ck_near(w3_stars[2].mag,  2.51569, 1e-3, "mag byte 128 -> 128/255*8-1.5");
        ck_near(w3_stars[2].bv,   0.25294, 1e-3, "bv byte 64 -> 64/255*3-0.5");

        /* Decode preserves order — the bands are pre-sorted brightest-first, so a magnitude limit
         * downstream is a prefix length. It must not reorder. */
        ck(w3_stars[0].mag < w3_stars[1].mag, "order preserved: index follows the byte stream, not re-sorted");
    }

    tsection("stars: decode rejects an empty or truncated buffer");
    {
        /* n = nbytes/6; a partial star at the tail is DROPPED, never read past its end (a 6-byte
         * read on 3 bytes would walk off the buffer). And an empty stream leaves the catalogue as
         * it was — no null deref, no zero-length malloc. */
        const uint8_t one[9] = { 0x00,0x40, 0xFF,0x7F, 0x00, 0x00,  0x11,0x22,0x33 };
        int n = w3_stars_load(one, (int)sizeof one);
        ck(n == 1, "9 bytes -> 1 star, the trailing 3 bytes (a partial star) are dropped");

        ck(w3_stars_load(one, 0) == 0, "0 bytes -> 0 stars (n <= 0 short-circuit, no malloc)");
        ck(w3_stars_load(one, 5) == 0, "5 bytes (< one star) -> 0 stars, never a partial read");
        /* the failed decodes returned before touching the globals -> the last GOOD load still stands */
        ck(w3_nstars == 1, "a rejected decode does not clobber the existing catalogue");
    }
}
