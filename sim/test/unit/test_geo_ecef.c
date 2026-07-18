/* Unit tests for osmmesh geo_ecef.c — WGS84 ECEF conversions + the
 * full-precision fractional-tile projector.
 *
 * Target: 100% line coverage of geo_ecef.c. Pure maths, verified by known
 * anchors, round-trips, and an EXACT curvature reference (equatorial arc).
 */
#include <math.h>
#include "osmmesh/geo.h"
#include "../../tiles/tilemath.h"   /* header-only; independent projection to pin against */
#include "tassert.h"

static double vlen(double x, double y, double z) { return sqrt(x*x + y*y + z*z); }

void test_geo_ecef(void) {
    printf("== geo_ecef unit tests ==\n");

    const double a = osmmesh_wgs84_a;
    const double f = osmmesh_wgs84_f;
    const double b = a * (1.0 - f);         /* semi-minor axis */

    ck_near(a, 6378137.0, 1e-6, "wgs84 a");
    ck_near(f, 1.0 / 298.257223563, 1e-15, "wgs84 f");

    /* ---- geo_to_ecef known anchors ---- */
    osmmesh_ecef e;
    e = osmmesh_geo_to_ecef((osmmesh_geo){ 0.0, 0.0, 0.0 });        /* lon0 lat0 */
    ck_near(e.x, a, 1e-6, "equator/greenwich -> X=a");
    ck_near(e.y, 0.0, 1e-6, "equator/greenwich -> Y=0");
    ck_near(e.z, 0.0, 1e-6, "equator/greenwich -> Z=0");

    e = osmmesh_geo_to_ecef((osmmesh_geo){ 90.0, 0.0, 0.0 });       /* lon90 lat0 */
    ck_near(e.x, 0.0, 1e-6, "lon=90E,lat=0 -> X=0");
    ck_near(e.y, a, 1e-6, "lon=90E,lat=0 -> Y=a");
    ck_near(e.z, 0.0, 1e-6, "lon=90E,lat=0 -> Z=0");

    e = osmmesh_geo_to_ecef((osmmesh_geo){ 0.0, 90.0, 0.0 });       /* north pole */
    ck_near(e.x, 0.0, 1e-6, "north pole -> X=0");
    ck_near(e.y, 0.0, 1e-6, "north pole -> Y=0");
    ck_near(e.z, b, 1e-6, "north pole -> Z=b (polar radius)");

    e = osmmesh_geo_to_ecef((osmmesh_geo){ 0.0, -90.0, 0.0 });      /* south pole */
    ck_near(e.z, -b, 1e-6, "south pole -> Z=-b");

    /* height adds along the outward normal: at the equator that is +X. */
    e = osmmesh_geo_to_ecef((osmmesh_geo){ 0.0, 0.0, 1000.0 });
    ck_near(e.x, a + 1000.0, 1e-6, "equator +1000m -> X=a+1000");

    /* ---- round-trip geo->ecef->geo, sub-mm ---- */
    struct { double lat, lon, alt; const char *name; } pts[] = {
        { 52.045,   9.385,  113.0, "Hameln (home)" },
        { 47.283,   7.524,  430.0, "Grenchen (CH)" },
        { 45.833,   6.865, 4808.0, "Mont Blanc" },
        {-33.868, 151.209,   58.0, "Sydney (S/E)" },
        { 64.146, -21.942,    0.0, "Reykjavik (N/W)" },
        {-89.5,     12.0,   2800.0, "near south pole" },
    };
    for (size_t i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        osmmesh_geo g0 = { pts[i].lon, pts[i].lat, pts[i].alt };
        osmmesh_ecef p = osmmesh_geo_to_ecef(g0);
        osmmesh_geo g1 = osmmesh_ecef_to_geo(p);
        /* Convert the geodetic residual to a metric one via re-projection so a
         * single tolerance is meaningful regardless of latitude. */
        osmmesh_ecef p1 = osmmesh_geo_to_ecef(g1);
        double d = vlen(p.x - p1.x, p.y - p1.y, p.z - p1.z);
        char m[96];
        snprintf(m, sizeof m, "round-trip <1mm: %s", pts[i].name);
        ck(d < 1e-3, m);
        snprintf(m, sizeof m, "round-trip lat %s", pts[i].name);
        ck_near(g1.lat, pts[i].lat, 1e-8, m);
        snprintf(m, sizeof m, "round-trip lon %s", pts[i].name);
        ck_near(g1.lon, pts[i].lon, 1e-8, m);
        snprintf(m, sizeof m, "round-trip alt %s", pts[i].name);
        ck_near(g1.alt, pts[i].alt, 1e-3, m);
    }

    /* ---- ecef_to_geo polar branch (pxy < 1e-9): fed exactly on the spin axis ---- */
    osmmesh_geo gp = osmmesh_ecef_to_geo((osmmesh_ecef){ 0.0, 0.0, b });
    ck_near(gp.lat, 90.0, 1e-9, "ECEF (0,0,b) -> lat=90");
    ck_near(gp.lon, 0.0, 1e-12, "ECEF (0,0,b) -> lon=0 (defined)");
    ck_near(gp.alt, 0.0, 1e-6, "ECEF (0,0,b) -> alt=0");
    osmmesh_geo gp2 = osmmesh_ecef_to_geo((osmmesh_ecef){ 0.0, 0.0, -(b + 500.0) });
    ck_near(gp2.lat, -90.0, 1e-9, "ECEF (0,0,-(b+500)) -> lat=-90");
    ck_near(gp2.alt, 500.0, 1e-6, "ECEF south pole +500m -> alt=500");

    /* ---- curvature of the earth: EXACT equatorial reference arc ----
     * Two points at lat=0, h=0 lie on a circle of radius exactly a. An arc of
     * s=100 km subtends dlon = s/a; the straight ECEF chord is 2a sin(dlon/2),
     * shorter than the arc by ~ a*dlon^3/24 = s^3/(24 a^2) ~ 1.02 m. */
    double s = 100000.0;
    double dlon_rad = s / a;
    double dlon_deg = dlon_rad * 180.0 / 3.14159265358979323846;
    osmmesh_ecef c0 = osmmesh_geo_to_ecef((osmmesh_geo){ 0.0, 0.0, 0.0 });
    osmmesh_ecef c1 = osmmesh_geo_to_ecef((osmmesh_geo){ dlon_deg, 0.0, 0.0 });
    double chord = vlen(c1.x - c0.x, c1.y - c0.y, c1.z - c0.z);
    double deficit = s - chord;
    ck(chord < s, "curvature: chord shorter than arc");
    ck_near(deficit, s * s * s / (24.0 * a * a), 0.05, "curvature deficit ~ s^3/24a^2");

    /* ---- tile_frac_to_geo: pin against the independent tilemath projection ----
     * frac (fx,fy) over tile (z,x,y) must equal fb_tile_to_geo(x+fx, y+fy, z).
     * Two independent implementations of the same Web-Mercator inverse; if
     * either drifts, this bites (the class of bug that once cost 1032 cases). */
    struct { uint8_t z; uint32_t x, y; double fx, fy; } fs[] = {
        { 14, 8555, 5424, 0.0, 0.0 }, { 14, 8555, 5424, 1.0, 1.0 },
        { 14, 8555, 5424, 0.5, 0.5 }, { 13, 4277, 2712, 0.37, 0.81 },
        {  0,    0,    0, 0.5, 0.5 }, { 10,  511,  340, 0.0, 1.0 },
    };
    for (size_t i = 0; i < sizeof fs / sizeof fs[0]; i++) {
        osmmesh_geo g = osmmesh_tile_frac_to_geo(fs[i].z, fs[i].x, fs[i].y, fs[i].fx, fs[i].fy);
        double rlat, rlon;
        fb_tile_to_geo((double)fs[i].x + fs[i].fx, (double)fs[i].y + fs[i].fy, fs[i].z, &rlat, &rlon);
        char m[96];
        snprintf(m, sizeof m, "frac_to_geo lon == tilemath [%zu]", i);
        ck_near(g.lon, rlon, 1e-9, m);
        snprintf(m, sizeof m, "frac_to_geo lat == tilemath [%zu]", i);
        ck_near(g.lat, rlat, 1e-9, m);
        ck_near(g.alt, 0.0, 1e-15, "frac_to_geo alt=0");
    }
}
