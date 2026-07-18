/* Tiny assertion helpers shared by the unit tests.
 *
 * All tests link into ONE binary on purpose: gcov reports coverage per binary and cannot merge
 * across several, so separate binaries meant every file that a given test didn't exercise was
 * reported at 0% — and the "coverage" number depended on which report you looked at first.
 * One binary, one .gcda, one honest number.
 */
#ifndef FB_TASSERT_H
#define FB_TASSERT_H
#include <stdio.h>
#include <string.h>

extern int fb_t_run, fb_t_fail;

static inline void ck(int cond, const char *what) {
    fb_t_run++;
    if (!cond) { fb_t_fail++; printf("  [FAIL] %s\n", what); }
}
static inline void ck_near(double got, double want, double tol, const char *what) {
    fb_t_run++;
    double d = got - want; if (d < 0) d = -d;
    if (d > tol) { fb_t_fail++; printf("  [FAIL] %s: got %.6f want %.6f\n", what, got, want); }
}
static inline void ck_str(const char *got, const char *want, const char *what) {
    fb_t_run++;
    if (!got || !want || strcmp(got, want)) {
        fb_t_fail++; printf("  [FAIL] %s: got '%s' want '%s'\n", what, got ? got : "(null)", want);
    }
}

/* Section banner, so a failure says which group it came from. */
static inline void tsection(const char *name) { printf("\n== %s ==\n", name); }

/* Each module's tests. Return non-zero on failure. */
int test_tilemath(void);
int test_tilesrc(void);
int test_route(void);
void test_atmosphere(void);
void test_mat4(void);
void test_chunkmesh_ecef(void);
void test_style(void);
void test_tilemap(void);
void test_lru(void);
void test_prefetch(void);
void test_draw(void);
void test_w3atmo(void);
void test_camera(void);
void test_stars(void);
void test_geo_ecef(void);
void test_terrain_ecef(void);

#endif /* FB_TASSERT_H */
