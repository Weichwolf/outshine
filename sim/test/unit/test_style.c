/* Unit tests — tiles/style.h
 *
 * The map's cartography: a Shortbread "kind" string -> colour and stroke width. Pure mapping, so
 * it is assertable. What these pin is not "this exact RGB" (the palette is a taste decision that
 * may change) but the RELATIONS that make a map readable: a motorway is wider than a footpath,
 * water is blue-dominant, forest is green-dominant, and an unknown kind still looks like ground.
 * Pinning exact bytes would make every palette tweak a test failure for no gain.
 */
#include "tassert.h"
#include "../../tiles/style.h"

/* The reference denominator the widths are expressed against. It comes FROM style.h — this file
 * used to carry its own `#define REF 1024`, which made it a third private copy of a number that
 * also sat in style.h and (hand-inlined) in raster.c. A test with its own copy of the constant
 * does not catch a change to it, it AGREES with it. */
#define REF ((int)FB_STYLE_REF_TEX)

void test_style(void){
    tsection("style: the unit is TILE-EDGE FRACTIONS — not pixels, not metres");
    {
        /* This section exists because reading the old comment ("stroke width in texture pixels")
         * and the call site, without reading the function between them, produced a confident and
         * entirely wrong bug report: "every doubling of the texture halves each road's ground
         * width; a motorway at 2048 would be 4.4 m". It does not. `u` cancels the resolution out.
         * The numbers below are the refutation, kept as a test so the claim cannot come back. */
        uint8_t r, g, b; int rail;
        const double SPAN_Z14 = 1504.0;   /* m, one z14 tile at 52 N — the aircraft's home tile */

        /* ACROSS TEXTURE SIZE: the ground width must not move at all. This is what makes a texture
         * LOD ramp (256/512/1024/2048) possible in the first place: the same road, sharper. */
        double w_ref = -1;
        for (int ts = 256; ts <= 2048; ts *= 2) {
            double wpx = w3_roadstyle("motorway", ts, &r, &g, &b, &rail);
            double wm  = wpx * (SPAN_Z14 / ts);          /* texels -> metres on the ground */
            if (w_ref < 0) w_ref = wm;
            ck_near(wm, w_ref, 1e-3, "motorway keeps its ground width across texture sizes");
        }
        ck_near(w_ref, 8.81, 0.05, "and that width is ~8.8 m — a plausible motorway, not a footpath");

        /* Same for the narrowest kind: a bug that only scales the wide ones would slip past. */
        double p_ref = -1;
        for (int ts = 256; ts <= 2048; ts *= 2) {
            double wpx = w3_roadstyle("footway", ts, &r, &g, &b, &rail);
            double wm  = wpx * (SPAN_Z14 / ts);
            if (p_ref < 0) p_ref = wm;
            ck_near(wm, p_ref, 1e-3, "a footpath keeps its ground width across texture sizes too");
        }

        /* ACROSS ZOOM: the ground width MUST scale with the tile's span, and that is deliberate
         * cartography, not a bug. A z8 tile is 96 km across and is drawn ~200 km away; its
         * motorway is ~564 m wide, which is about one screen pixel. At 8.8 m it would not exist.
         * Small-scale maps draw roads wider than the ground truth on purpose. */
        double span_z8 = SPAN_Z14 * 64.0;               /* z14 -> z8 is 6 doublings */
        double w14 = w3_roadstyle("motorway", 512, &r, &g, &b, &rail) * (SPAN_Z14 / 512);
        double w8  = w3_roadstyle("motorway", 512, &r, &g, &b, &rail) * (span_z8  / 512);
        ck_near(w8 / w14, 64.0, 1e-6, "a motorway is 64x wider on the ground at z8 than at z14");
        ck(w8 > 500 && w8 < 600, "z8 motorway is ~564 m — about one screen pixel at 200 km");
    }

    tsection("style: landcover colours");
    {
        uint8_t r, g, b;

        w3_landcolor("wood", &r, &g, &b);
        ck(g > r && g > b, "wood is green-dominant");
        uint8_t fr = r, fg = g, fb = b;
        w3_landcolor("forest", &r, &g, &b);
        ck(r == fr && g == fg && b == fb, "forest and wood are the same colour (aliases)");

        w3_landcolor("grass", &r, &g, &b);
        uint8_t gr = r, gg = g, gb = b;
        w3_landcolor("grassland", &r, &g, &b);
        ck(r == gr && g == gg && b == gb, "grass and grassland are aliases");

        /* farmland must read as soil, not as meadow — they are adjacent in every rural tile */
        uint8_t mr, mg, mb;
        w3_landcolor("farmland", &r, &g, &b);
        w3_landcolor("meadow", &mr, &mg, &mb);
        ck(!(r == mr && g == mg && b == mb), "farmland is distinguishable from meadow");
        ck(r > g, "farmland reads as soil (red channel leads), not as vegetation");

        w3_landcolor("sand", &r, &g, &b);
        ck(r > 200 && g > 200 && b < r && b < g, "sand is pale and yellow-ish");

        w3_landcolor("wood", &r, &g, &b);
        uint8_t wr = r;
        w3_landcolor("residential", &r, &g, &b);
        ck(r > wr, "built-up ground is brighter than forest");

        /* the fallback: an unstyled kind must still look like ground, never like a hole */
        uint8_t ur, ug, ub;
        w3_landcolor("some_kind_we_have_never_heard_of", &ur, &ug, &ub);
        ck(ug > ur && ug > ub, "an unknown kind falls back to neutral green (still ground)");
        w3_landcolor("", &r, &g, &b);
        ck(r == ur && g == ug && b == ub, "an empty kind takes the same fallback");
    }

    tsection("style: road widths + colours");
    {
        uint8_t r, g, b; int rail;

        float wmot = w3_roadstyle("motorway", REF, &r, &g, &b, &rail);
        ck(rail == 0, "a motorway is not a railway");
        float wpri = w3_roadstyle("primary",   REF, &r, &g, &b, &rail);
        float wsec = w3_roadstyle("secondary", REF, &r, &g, &b, &rail);
        float wter = w3_roadstyle("tertiary",  REF, &r, &g, &b, &rail);
        float wres = w3_roadstyle("residential", REF, &r, &g, &b, &rail);
        float wpath= w3_roadstyle("footway",   REF, &r, &g, &b, &rail);
        ck(wmot > wpri && wpri > wsec && wsec > wter && wter > wres && wres > wpath,
           "width follows the road hierarchy, motorway down to footway");
        ck(wpath > 0, "even the humblest path has a positive width");

        /* trunk shares the motorway style; a dual carriageway should not shrink at a tag change */
        float wtrunk = w3_roadstyle("trunk", REF, &r, &g, &b, &rail);
        ck(wtrunk == wmot, "trunk renders like a motorway");

        /* rails must be flagged, because they are drawn in a separate second pass */
        w3_roadstyle("rail", REF, &r, &g, &b, &rail);
        ck(rail == 1, "rail sets the rail flag (drawn in the 2nd pass, stays visible)");
        w3_roadstyle("tram", REF, &r, &g, &b, &rail);
        ck(rail == 1, "tram counts as rail");
        w3_roadstyle("service", REF, &r, &g, &b, &rail);
        ck(rail == 0, "the rail flag is reset for a non-rail (no leak between calls)");

        /* a rail must not look like tarmac */
        uint8_t rr, rg, rb, sr, sg, sb;
        w3_roadstyle("rail", REF, &rr, &rg, &rb, &rail);
        w3_roadstyle("residential", REF, &sr, &sg, &sb, &rail);
        ck(rr < sr, "rails are darker than a residential street");

        /* unknown kinds get the track/path style rather than vanishing */
        float wunknown = w3_roadstyle("nonexistent_kind", REF, &r, &g, &b, &rail);
        ck(wunknown == wpath, "an unknown street kind falls back to the path style");
        ck(rail == 0, "the fallback is not a railway");
    }

    tsection("style: widths scale with the texture resolution");
    {
        uint8_t r, g, b; int rail;
        /* This is the whole reason tex_res is a parameter: the far tier bakes at 256 px, and a
         * road drawn at 1024-px widths there is a grey mat, not a road. */
        float at1024 = w3_roadstyle("motorway", 1024, &r, &g, &b, &rail);
        float at256  = w3_roadstyle("motorway",  256, &r, &g, &b, &rail);
        float at2048 = w3_roadstyle("motorway", 2048, &r, &g, &b, &rail);
        ck_near(at256,  at1024/4.0f, 1e-4, "a quarter-size texture gets quarter-width roads");
        ck_near(at2048, at1024*2.0f, 1e-4, "a double-size texture gets double-width roads");

        /* the ratio must hold for every class, not just motorways */
        float p1024 = w3_roadstyle("footway", 1024, &r, &g, &b, &rail);
        float p256  = w3_roadstyle("footway",  256, &r, &g, &b, &rail);
        ck_near(p256, p1024/4.0f, 1e-4, "scaling applies to the fallback class too");

        /* colour must NOT depend on resolution — only geometry scales */
        uint8_t ar, ag, ab, br, bg, bb;
        w3_roadstyle("primary", 1024, &ar, &ag, &ab, &rail);
        w3_roadstyle("primary",  256, &br, &bg, &bb, &rail);
        ck(ar == br && ag == bg && ab == bb, "resolution changes width, never colour");
    }
}
