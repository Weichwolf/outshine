"""THE DOOR'S OWN LOD LADDER, MIRRORED. `include/generate/Generate.h` is the source; this is a copy.

RAGE decides three things here and the door already took them: the levels are HD, LOD, SLOD1 and
SLOD2/3 -- `Fine`, `Shell`, `Massed`, `Skyline`; a coarser entity REPLACES the finer ones under it
rather than standing beside them; and every level is BAKED when the geometry is built, never in a
frame. Unreal agrees on all three and differs only in what picks the level, and the door has taken
Nanite's answer over `lodDistance`: the ERROR a simplification introduces, not the SIZE of what it
simplifies.

    IT IS THE ERROR AND NOT THE SIZE. What a subject MEASURES on screen answers "is it visible at
    all"; what its simplification MOVES answers "is the simplification visible", and only the
    second licenses replacing geometry.  -- Generate.h

`test/lab/visible.py:rung_for` judged by size, which is the rule that document records as already
paid for and thrown away. This file is what replaces it, and the assertions below are the door's
own `static_assert`s, case for case, so the two cannot drift without a red.
"""
import math

FINE, SHELL, MASSED, SKYLINE = 0, 1, 2, 3        # Detail, in the door's order
NAMES = ("Fine", "Shell", "Massed", "Skyline")

# The most ERROR a simplification may project and still be invisible: one pixel.
ERROR_PX = 1.0


def at_rung(rungs_coarser):
    """`DetailAtRung`: the detail a tile carries, given how many rungs coarser than the finest."""
    if rungs_coarser <= 0:
        return FINE
    if rungs_coarser == 1:
        return SHELL
    if rungs_coarser == 2:
        return MASSED
    return SKYLINE


def coarser(one, two):
    """`Coarser`: a subject is never finer than the coarsest thing that bounds it."""
    return one if one > two else two


def unseen(error_m, focal_px, away_m):
    """`Unseen`: is a simplification that moves geometry by `error_m` invisible from `away_m`?

    `focal_px / away_m` is pixels per metre at that range -- the same quantity Nanite's
    `errorPerMetre / distance` computes and Unreal's ScreenSize is a bounding-sphere form of."""
    if not error_m > 0.0:
        return True
    if not focal_px > 0.0 or not away_m > 0.0:
        return False
    return error_m * focal_px <= ERROR_PX * away_m


def focal_px(fov_deg, width_px):
    """THE PINHOLE'S FOCAL LENGTH IN PIXELS, and not the small-angle stand-in. `visible.py` used
    `fov / width` radians per pixel, which is the tangent's first term: at 55 degrees over 1280 it
    is 3.4 % short at the centre and wrong by a quarter at the edge of the frame."""
    return 0.5 * width_px / math.tan(0.5 * math.radians(fov_deg))


def rung_for(error_m, away_m, fov_deg=55.0, width_px=1280):
    """THE COARSEST RUNG WHOSE ERROR IS STILL UNDER A PIXEL, given what each rung MOVES.

    `error_m` is the error of each rung above the finest, in order -- for a building, the depth of
    the relief that rung drops -- so the caller states what its own simplification costs and this
    states which of those the eye cannot tell apart. A rung whose error is visible is never taken,
    however far away the subject is."""
    f = focal_px(fov_deg, width_px)
    got = FINE
    for k, err in enumerate(error_m):
        if not unseen(err, f, away_m):
            break
        got = at_rung(k + 1)
    return got


# --- the door's own static_asserts, case for case ---------------------------------------------
assert at_rung(-1) == FINE, "a finer rung than the finest is still the finest"
assert at_rung(0) == FINE
assert at_rung(1) == SHELL
assert at_rung(2) == MASSED
assert at_rung(9) == SKYLINE, "every rung beyond is the horizon"
assert coarser(FINE, MASSED) == MASSED
assert coarser(SKYLINE, SHELL) == SKYLINE
assert unseen(0.3, 691.0, 300.0), "a gable's depth at 300 m is under a pixel"
assert not unseen(0.3, 691.0, 100.0), "at 100 m it is two pixels and has to be drawn"
assert not unseen(100.0, 691.0, 10000.0), "a block a hundred metres across is seven pixels wrong at ten kilometres"
assert unseen(100.0, 691.0, 100000.0), "and under one at a hundred"
assert unseen(0.0, 691.0, 1.0), "a simplification that moves nothing is always invisible"
assert not unseen(1.0, 0.0, 1.0), "with no projection nothing may be claimed invisible"
