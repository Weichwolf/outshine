/* Unit-test runner. All module tests link into this ONE binary so gcov produces a single,
 * correctly merged coverage report — see tassert.h for why that matters. */
#include "tassert.h"

int fb_t_run = 0, fb_t_fail = 0;

int main(void) {
    test_tilemath();
    test_tilesrc();
    test_route();
    test_atmosphere();
    test_mat4();
    test_style();
    test_lru();
    test_prefetch();
    test_draw();
    printf("\n== %d/%d assertions passed ==\n", fb_t_run - fb_t_fail, fb_t_run);
    return fb_t_fail ? 1 : 0;
}
