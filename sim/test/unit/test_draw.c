/* Unit tests — tiles/draw.h
 *
 * The software rasteriser that turns OSM vector data into a ground texture. It used to live in the
 * renderer and could only be judged by looking at the result; now it runs on the tile server and
 * its input is a plain struct, so it can be asserted.
 *
 * These are not golden-pixel tests. The palette and the widths are taste and will change; what
 * must not change is the behaviour that makes a map either true or a lie:
 *
 *   - a filled polygon covers its inside and nothing else
 *   - a hole is a hole
 *   - a scanline whose crossings are wrong is REFUSED, never guessed
 *
 * That last one is the reason this file exists. Lose one crossing and the remaining ones pair up
 * shifted: "inside" and "outside" swap and the fill runs to the next crossing — across the whole
 * tile if there isn't one. It is not a subtle artifact, it is a solid rectangle over the map, and
 * it once buried a power station.
 */
#include "tassert.h"
#include "../../tiles/draw.h"
#include <stdlib.h>
#include <string.h>

#define W 64
#define H 64

static uint8_t g_im[W*H*3];
static void clear(void){ memset(g_im, 0, sizeof g_im); }
static int at(int x, int y, int c){ return g_im[((size_t)y*W + x)*3 + c]; }
static int painted(int x, int y){ return at(x,y,0) || at(x,y,1) || at(x,y,2); }
static int count_painted(void){
    int n = 0;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++) if(painted(x,y)) n++;
    return n;
}

/* Build a feature from a coord list. rings==NULL -> a single implicit ring. */
static osmmesh_mvt_feature mk(const osmmesh_mvt_coord *co, size_t n,
                              const uint32_t *rings, size_t nr, osmmesh_mvt_geom_type gt){
    osmmesh_mvt_feature f;
    memset(&f, 0, sizeof f);
    f.geom_type = gt; f.coords = co; f.n_coords = n;
    f.n_rings = nr; f.ring_offsets = rings;
    return f;
}

void test_draw(void){
    tsection("draw: pixels + bounds");
    {
        clear();
        fb_draw_px(g_im, W, H, 10, 10, 1, 2, 3);
        ck(at(10,10,0)==1 && at(10,10,1)==2 && at(10,10,2)==3, "a pixel lands where asked");
        ck(count_painted()==1, "and only there");

        /* Out of bounds must be silently dropped, not wrapped or crashed: MVT coordinates
         * legitimately fall outside the tile (the clip buffer), so this is the normal case. */
        fb_draw_px(g_im, W, H, -1, 10, 9,9,9);
        fb_draw_px(g_im, W, H, W, 10, 9,9,9);
        fb_draw_px(g_im, W, H, 10, -1, 9,9,9);
        fb_draw_px(g_im, W, H, 10, H,  9,9,9);
        fb_draw_px(g_im, W, H, -5, -5, 9,9,9);
        ck(count_painted()==1, "out-of-bounds pixels are dropped, never wrapped");
    }

    tsection("draw: disk + thick line");
    {
        clear();
        fb_draw_disk(g_im, W, H, 32, 32, 5, 200, 0, 0);
        ck(painted(32,32), "a disk covers its centre");
        ck(painted(32,36) && painted(36,32), "...and reaches its radius");
        ck(!painted(32,40) && !painted(40,32), "...and stops there");
        int n = count_painted();
        ck(n > 60 && n < 100, "a radius-5 disk is about pi*r^2 pixels");   /* ~78.5 */

        /* a disk hanging off the edge must clip, not wrap to the other side */
        clear();
        fb_draw_disk(g_im, W, H, 0, 0, 4, 1,1,1);
        ck(painted(0,0) && painted(3,0), "an edge disk paints what is on screen");
        ck(!painted(W-1,H-1), "...and does not wrap around");

        clear();
        fb_draw_thick(g_im, W, H, 10, 32, 50, 32, 4, 0, 200, 0);
        ck(painted(10,32) && painted(30,32) && painted(50,32), "a thick line is continuous end to end");
        ck(painted(30,33) && painted(30,31), "...and has width");
        ck(!painted(30,40), "...but not unbounded width");

        /* Roads at the far tier are sub-pixel wide; they must not vanish. A map with no roads
         * looks fine and is useless. */
        clear();
        fb_draw_thick(g_im, W, H, 10, 20, 50, 20, 0.01f, 1,1,1);
        ck(count_painted() > 0, "a hairline-width line still draws (far tier roads survive)");

        /* a zero-length line is a dot, not a division by zero */
        clear();
        fb_draw_thick(g_im, W, H, 32, 32, 32, 32, 3, 1,1,1);
        ck(painted(32,32) && count_painted() > 0, "a zero-length line is a dot, not a crash");
    }

    tsection("draw: polygon fill");
    {
        /* a plain square, 10..30 in both axes */
        const osmmesh_mvt_coord sq[] = { {10,10}, {30,10}, {30,30}, {10,30} };
        osmmesh_mvt_feature f = mk(sq, 4, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &f, 1.0f, 7, 8, 9);
        ck(painted(20,20), "the inside is filled");
        ck(at(20,20,0)==7 && at(20,20,1)==8 && at(20,20,2)==9, "...with the colour asked for");
        ck(!painted(5,20) && !painted(35,20), "the outside is not filled (x)");
        ck(!painted(20,5) && !painted(20,35), "the outside is not filled (y)");
        int n = count_painted();
        ck(n > 340 && n < 460, "a 20x20 square fills about 400 pixels");

        /* The ring must close implicitly: only 4 coords are given for 4 edges. If the last->first
         * edge were dropped, the fill would leak out of the open side. */
        ck(!painted(20,32), "the ring closes implicitly (no leak past the last edge)");

        /* degenerate input must be ignored, not drawn */
        const osmmesh_mvt_coord two[] = { {10,10}, {30,30} };
        osmmesh_mvt_feature d = mk(two, 2, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &d, 1.0f, 1,1,1);
        ck(count_painted()==0, "a 2-point 'polygon' draws nothing");

        osmmesh_mvt_feature nul = mk(0, 0, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &nul, 1.0f, 1,1,1);
        ck(count_painted()==0, "a polygon with no coords draws nothing (no crash)");
    }

    tsection("draw: holes, scale, clipping");
    {
        /* outer square 8..40, inner ring 16..32 -> a hole */
        const osmmesh_mvt_coord co[] = {
            {8,8}, {40,8}, {40,40}, {8,40},          /* ring 0: outer */
            {16,16}, {32,16}, {32,32}, {16,32},      /* ring 1: hole   */
        };
        const uint32_t rings[] = { 0, 4, 8 };
        osmmesh_mvt_feature f = mk(co, 8, rings, 2, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &f, 1.0f, 5,5,5);
        ck(painted(12,24), "the ring between outer and inner is filled");
        ck(!painted(24,24), "the interior ring is a HOLE (even-odd), not filled over");
        ck(!painted(4,24), "outside the outer ring stays empty");

        /* scale factor: MVT extent -> texture pixels. Halving it must halve the geometry. */
        const osmmesh_mvt_coord sq[] = { {10,10}, {30,10}, {30,30}, {10,30} };
        osmmesh_mvt_feature s = mk(sq, 4, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &s, 0.5f, 1,1,1);
        ck(painted(10,10), "at sc=0.5 the square covers its halved position");
        ck(!painted(28,28), "...and not its full-scale one");

        /* Coordinates outside the tile are NORMAL in MVT (the clip buffer). The parts on screen
         * must be drawn and nothing may wrap. */
        const osmmesh_mvt_coord big[] = { {-50,-50}, {200,-50}, {200,200}, {-50,200} };
        osmmesh_mvt_feature b = mk(big, 4, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &b, 1.0f, 3,3,3);
        ck(count_painted()==W*H, "a polygon covering the whole tile fills every pixel");

        /* ...and one entirely off-screen fills nothing */
        const osmmesh_mvt_coord away[] = { {200,200}, {260,200}, {260,260}, {200,260} };
        osmmesh_mvt_feature a = mk(away, 4, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &a, 1.0f, 3,3,3);
        ck(count_painted()==0, "a polygon entirely outside the tile draws nothing");
    }

    tsection("draw: a wrong scanline is REFUSED, never guessed");
    {
        /* THE test this file exists for. A ring with an odd crossing count is degenerate; pairing
         * it up anyway swaps inside for outside and runs the fill to the tile edge -- a solid
         * rectangle over whatever was there. Refusing the scanline loses that row; guessing loses
         * the map. */
        long before = fb_draw_refused;

        /* A polygon with a duplicated vertex chain that yields an odd crossing count on some rows:
         * a "bowtie" whose rings are declared but degenerate. */
        const osmmesh_mvt_coord bow[] = { {10,10}, {30,30}, {10,30}, {30,10} };
        osmmesh_mvt_feature f = mk(bow, 4, 0, 0, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        fb_draw_fill(g_im, W, H, &f, 1.0f, 9,9,9);
        /* A bowtie is legal geometry with an EVEN crossing count, so it must still draw. */
        ck(count_painted() > 0, "a self-intersecting bowtie still draws (even crossings)");
        ck(fb_draw_refused == before, "...and refuses nothing: even crossings are not an error");

        /* Now force an overflow: more crossings on one scanline than FB_XS_MAX. A comb with
         * FB_XS_MAX+100 teeth crossing a single row. */
        int teeth = FB_XS_MAX/2 + 50;
        osmmesh_mvt_coord *comb = malloc(sizeof(osmmesh_mvt_coord) * (size_t)teeth * 4);
        size_t nc = 0;
        for(int i=0;i<teeth;i++){          /* each tooth is its own 4-point box crossing y=20 */
            comb[nc++] = (osmmesh_mvt_coord){ i*2,    10 };
            comb[nc++] = (osmmesh_mvt_coord){ i*2+1,  10 };
            comb[nc++] = (osmmesh_mvt_coord){ i*2+1,  30 };
            comb[nc++] = (osmmesh_mvt_coord){ i*2,    30 };
        }
        uint32_t *ro = malloc(sizeof(uint32_t) * ((size_t)teeth + 1));
        for(int i=0;i<=teeth;i++) ro[i] = (uint32_t)(i*4);
        osmmesh_mvt_feature c = mk(comb, nc, ro, (size_t)teeth, OSMMESH_MVT_GEOM_POLYGON);
        clear();
        before = fb_draw_refused;
        fb_draw_fill(g_im, W, H, &c, 1.0f, 9,9,9);
        ck(fb_draw_refused > before, "an overflowing scanline is COUNTED, not silently guessed");
        /* the refused rows must be blank rather than a rectangle across the tile */
        int wide_rows = 0;
        for(int y=0;y<H;y++){ int n=0; for(int x=0;x<W;x++) if(painted(x,y)) n++; if(n==W) wide_rows++; }
        ck(wide_rows == 0, "no refused scanline became a full-width bar (the Grohnde failure mode)");
        free(comb); free(ro);
    }

    tsection("draw: linestrings");
    {
        const osmmesh_mvt_coord line[] = { {5,5}, {25,5}, {25,25} };
        osmmesh_mvt_feature f = mk(line, 3, 0, 0, OSMMESH_MVT_GEOM_LINESTRING);
        clear();
        fb_draw_line(g_im, W, H, &f, 1.0f, 2, 4, 4, 4);
        ck(painted(15,5), "the first segment is drawn");
        ck(painted(25,15), "the second segment is drawn");
        ck(painted(25,5), "the corner joins (no gap at the vertex)");
        ck(!painted(5,25), "the line is NOT closed into a polygon");

        /* multi-ring linestrings: each ring is its own polyline, and they must not be joined */
        const osmmesh_mvt_coord two[] = { {5,10}, {15,10},   {40,50}, {50,50} };
        const uint32_t rings[] = { 0, 2, 4 };
        osmmesh_mvt_feature m = mk(two, 4, rings, 2, OSMMESH_MVT_GEOM_LINESTRING);
        clear();
        fb_draw_line(g_im, W, H, &m, 1.0f, 2, 1,1,1);
        ck(painted(10,10) && painted(45,50), "both polylines are drawn");
        ck(!painted(28,30), "...and are NOT joined to each other");

        osmmesh_mvt_feature one = mk(line, 1, 0, 0, OSMMESH_MVT_GEOM_LINESTRING);
        clear();
        fb_draw_line(g_im, W, H, &one, 1.0f, 2, 1,1,1);
        ck(count_painted()==0, "a 1-point linestring draws nothing (no crash)");
    }

    tsection("draw: aerial mosaic layout");
    {
        /* Exact, not a resample: a 256 px photo tile is one child, so a TS-px texture wants
         * (TS/256)^2 of them at z+log2(TS/256). Getting this wrong fetches the wrong ground. */
        ck(fb_mosaic_factor(1024) == 4, "1024 px texture = 4x4 photo children");
        ck(fb_mosaic_factor(512)  == 2, "512 px = 2x2");
        ck(fb_mosaic_factor(256)  == 1, "256 px = the tile itself");
        ck(fb_mosaic_factor(128)  == 1, "below one child, still one (never zero)");

        ck(fb_mosaic_zoom(14, 1024) == 16, "the near tier: z14 at 1024 px -> z16 children");
        ck(fb_mosaic_zoom(11, 512)  == 12, "the far tier: z11 at 512 px -> z12 children");
        ck(fb_mosaic_zoom(14, 256)  == 14, "256 px needs no zoom step");
        ck(fb_mosaic_zoom(0, 1024)  == 2,  "the arithmetic holds at z0");

        /* factor and zoom must agree: 4^n children means n zoom steps */
        int ok = 1;
        for(int TS = 256; TS <= 4096; TS *= 2){
            int f = fb_mosaic_factor(TS), dz = fb_mosaic_zoom(10, TS) - 10;
            if((1 << dz) != f) ok = 0;
        }
        ck(ok, "factor and zoom step agree at every texture size (2^dz == f)");
    }
}
