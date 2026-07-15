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

/* the tile size the widths are tuned for */
#define REF 1024

void test_style(void){
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
