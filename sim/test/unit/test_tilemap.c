/* Unit tests — tiles/tilemap.h: parsing Esri's "which tiles exist" reply.
 *
 * This parser is the only thing standing between us and caching Esri's placeholder card as ground
 * texture. It cannot be checked by looking at a screenshot: a wrong bit means one tile is fetched
 * that should not have been, or skipped that should have been -- somewhere, later, once.
 *
 * The fixtures are REAL replies, captured from the live service, not invented ones. The important
 * one is `adjusted`: a 32x32 request at a coastline came back describing a 32x4 rectangle. An
 * invented fixture would never have had that in it, because nobody expects a server to answer a
 * different question than the one asked.
 */
#include "tassert.h"
#include "../../tiles/tilemap.h"

void test_tilemap(void) {
    fb_tm_rect r;
    unsigned char bits[4096];

    tsection("tilemap: the plain cases");
    {
        /* Real, z12 over Hameln: everything exists. */
        char s[] = "{\"data\":[1,1,1,1],\"location\":{\"height\":2,\"left\":2154,\"top\":1351,\"width\":2},\"valid\":true}";
        int n = fb_tm_parse(s, sizeof s, &r, bits, sizeof bits);
        ck(n == 4, "2x2 -> 4 entries");
        ck(r.left == 2154 && r.top == 1351, "location left/top read back");
        ck(r.width == 2 && r.height == 2, "location width/height read back");
        ck(bits[0] && bits[1] && bits[2] && bits[3], "all present -> all 1");

        /* Real, z21 over the same ground: nothing exists -- this is exactly where Esri serves the
         * 2521-byte placeholder with a 200. The whole point of the oracle. */
        char t[] = "{\"data\":[0,0,0,0],\"location\":{\"height\":2,\"left\":692042,\"top\":1103250,\"width\":2},\"valid\":true}";
        n = fb_tm_parse(t, sizeof t, &r, bits, sizeof bits);
        ck(n == 4, "z21 reply parses");
        ck(!bits[0] && !bits[1] && !bits[2] && !bits[3], "above coverage -> all 0");
    }

    tsection("tilemap: ADJUSTED — the reply describes its own rectangle, not the request");
    {
        /* THE trap, and a real capture: 32x32 was asked for, 32x4 came back with "adjusted": true.
         * A parser that trusts the request indexes bits[row*32+col] into a 128-entry array. */
        char s[600] = "{\"adjusted\":true,\"data\":[";
        for (int i = 0; i < 128; i++) strcat(s, i ? ",1" : "1");
        strcat(s, "],\"location\":{\"height\":4,\"left\":5407,\"top\":9852,\"width\":32},\"valid\":true}");
        int n = fb_tm_parse(s, strlen(s) + 1, &r, bits, sizeof bits);
        ck(n == 128, "adjusted reply yields 128 entries, not the 1024 requested");
        ck(r.width == 32 && r.height == 4, "the rectangle is 32x4 — read from location, not assumed");
        ck(r.left == 5407 && r.top == 9852, "adjusted rectangle keeps its own origin");
    }

    tsection("tilemap: a reply that contradicts itself teaches NOTHING (never 'absent')");
    {
        /* 0 must mean "learned nothing" and never "no tiles". Reading a broken reply as absence is
         * the overloaded-404 bug again: one value standing for two facts, the wrong one permanent. */
        char few[] = "{\"data\":[1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(few, sizeof few, &r, bits, sizeof bits) == 0,
           "fewer entries than the rectangle claims -> unusable, not a smaller rectangle");
        char many[] = "{\"data\":[1,1,1,1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(many, sizeof many, &r, bits, sizeof bits) == 0, "more entries than claimed -> unusable");
        /* This fixture is SELF-CONSISTENT on purpose: 4 entries for a 2x2 rectangle. Only the
         * `valid: false` check can reject it.
         * The first version had `"data":[]` here and was worthless: the parser rejected it for the
         * length mismatch, so deleting the valid-check entirely left the test green. Caught by
         * mutating the check away and watching nothing happen — which is the only way this kind of
         * hole is ever found. A test that passes for a reason other than the one it names is not a
         * weaker test, it is not a test. */
        char inval[] = "{\"data\":[1,1,1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":false}";
        ck(fb_tm_parse(inval, sizeof inval, &r, bits, sizeof bits) == 0,
           "valid:false -> unusable, even when the data itself is fine");
        char noloc[] = "{\"data\":[1,1,1,1],\"valid\":true}";
        ck(fb_tm_parse(noloc, sizeof noloc, &r, bits, sizeof bits) == 0, "no location -> unusable");
        char nodata[] = "{\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(nodata, sizeof nodata, &r, bits, sizeof bits) == 0, "no data array -> unusable");
        char junk[] = "{\"data\":[2,2,2,2],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(junk, sizeof junk, &r, bits, sizeof bits) == 0, "values outside {0,1} -> unusable");
        char multi[] = "{\"data\":[10,1,1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(multi, sizeof multi, &r, bits, sizeof bits) == 0, "multi-digit token -> unusable");
        char neg[] = "{\"data\":[1],\"location\":{\"height\":-1,\"left\":0,\"top\":0,\"width\":1},\"valid\":true}";
        ck(fb_tm_parse(neg, sizeof neg, &r, bits, sizeof bits) == 0, "negative height -> unusable");
        char huge[] = "{\"data\":[1],\"location\":{\"height\":99999,\"left\":0,\"top\":0,\"width\":99999},\"valid\":true}";
        ck(fb_tm_parse(huge, sizeof huge, &r, bits, sizeof bits) == 0, "absurd rectangle -> refused, not trusted");
    }

    tsection("tilemap: it does not write past the buffer it was given");
    {
        char s[] = "{\"data\":[1,1,1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        unsigned char small[3];
        ck(fb_tm_parse(s, sizeof s, &r, small, 3) == 0, "rectangle larger than maxbits -> refused");
        ck(fb_tm_parse(s, sizeof s, &r, small, 4) == 4, "exactly maxbits -> accepted");
    }

    tsection("tilemap: row-major from top-left");
    {
        /* The ordering IS the meaning: transposed, every coastline would be mirrored and the cache
         * would hold the right answers at the wrong coordinates. */
        char s[] = "{\"data\":[1,0,0,0],\"location\":{\"height\":2,\"left\":10,\"top\":20,\"width\":2},\"valid\":true}";
        int n = fb_tm_parse(s, sizeof s, &r, bits, sizeof bits);
        ck(n == 4, "parses");
        ck(bits[0] == 1 && bits[1] == 0 && bits[2] == 0 && bits[3] == 0,
           "data[0] is (left, top); index = row*width + col");
    }
}
