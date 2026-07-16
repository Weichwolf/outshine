#include "tassert.h"
#include "../../tiles/tilemap.h"

void test_tilemap(void) {
    fb_tm_rect r;
    unsigned char bits[4096];

    tsection("tilemap: the plain cases");
    {

        char s[] = "{\"data\":[1,1,1,1],\"location\":{\"height\":2,\"left\":2154,\"top\":1351,\"width\":2},\"valid\":true}";
        int n = fb_tm_parse(s, sizeof s, &r, bits, sizeof bits);
        ck(n == 4, "2x2 -> 4 entries");
        ck(r.left == 2154 && r.top == 1351, "location left/top read back");
        ck(r.width == 2 && r.height == 2, "location width/height read back");
        ck(bits[0] && bits[1] && bits[2] && bits[3], "all present -> all 1");

        char t[] = "{\"data\":[0,0,0,0],\"location\":{\"height\":2,\"left\":692042,\"top\":1103250,\"width\":2},\"valid\":true}";
        n = fb_tm_parse(t, sizeof t, &r, bits, sizeof bits);
        ck(n == 4, "z21 reply parses");
        ck(!bits[0] && !bits[1] && !bits[2] && !bits[3], "above coverage -> all 0");
    }

    tsection("tilemap: ADJUSTED — the reply describes its own rectangle, not the request");
    {

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

        char few[] = "{\"data\":[1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(few, sizeof few, &r, bits, sizeof bits) == 0,
           "fewer entries than the rectangle claims -> unusable, not a smaller rectangle");
        char many[] = "{\"data\":[1,1,1,1,1],\"location\":{\"height\":2,\"left\":0,\"top\":0,\"width\":2},\"valid\":true}";
        ck(fb_tm_parse(many, sizeof many, &r, bits, sizeof bits) == 0, "more entries than claimed -> unusable");

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

        char s[] = "{\"data\":[1,0,0,0],\"location\":{\"height\":2,\"left\":10,\"top\":20,\"width\":2},\"valid\":true}";
        int n = fb_tm_parse(s, sizeof s, &r, bits, sizeof bits);
        ck(n == 4, "parses");
        ck(bits[0] == 1 && bits[1] == 0 && bits[2] == 0 && bits[3] == 0,
           "data[0] is (left, top); index = row*width + col");
    }
}
