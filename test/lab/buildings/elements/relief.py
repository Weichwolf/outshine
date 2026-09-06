"""THE FACADE'S RELIEF: what a raking light catches on a wall, as geometry.

Mass simple, skin deep -- CLAUDE.md's own rule for the generators. A Gruenderzeit block IS a box;
everything a viewer reads it by stands within 400 mm of the wall plane, and every one of these is
a band or a box in the wall's own frame. The projections are the ones a bricklayer builds:

    SOCKEL          the plinth, one to two courses proud, to the ground floor's sill
    GURTGESIMS      the string course at each floor line, 60 to 120 mm proud
    KRANZGESIMS     the eaves cornice, the deepest band on the wall at 250 to 400 mm
    LISENE          the flat pilaster strip that divides a long facade into fields
    SOHLBANK        the window sill, proud and wider than its opening, with a drip
    STURZ           the lintel or the window head, the band over an opening
    FENSTERLEIBUNG  the REVEAL -- the opening set back into the wall, which is the one element
                    that makes a hole read as a window rather than as a dark rectangle

Every projection is a NUMBER WITH ITS ORIGIN, and the origins are a brick's dimensions: a course
is 71.5 mm plus 12.5 mm of joint, and a band proud by less than half a brick does not cast a
shadow a camera can see at 30 m.
"""
import math

from .base import Place, register

COURSE_M = 0.0840          # [SET] DIN 1053: a brick course is 71.5 mm plus a 12.5 mm joint
HEADER_M = 0.115           # [SET] half a brick, the smallest projection that reads at 30 m
REVEAL_M = 0.140           # [SET] a window set back by one header plus its frame


def _fields(ctx):
    """The bays of one wall, as (centre, width) in the wall's own frame."""
    n = max(1, int(ctx.bays))
    step = ctx.place.length / n
    return [((k + 0.5) * step, step) for k in range(n)]


@register("sockel", lod=2, role="plinth",
          note="the plinth: two courses proud, stopping at the ground floor's sill")
def _sockel(ctx):
    if ctx.epoch in ("industrial", "hall", "contemporary", "bungalow"):
        return ()
    top = min(ctx.sill_m, ctx.level_m * 0.55)
    if top <= 0.15:
        return ()
    v, t = ctx.place.box(0.0, 0.0, ctx.place.length, top, 0.0, 2 * COURSE_M)
    return (("plinth", v, t),)


@register("gurtgesims", lod=2, role="stone", note="a string course on every floor line")
def _gurtgesims(ctx):
    if not ctx.cornice or ctx.levels < 2:
        return ()
    out = []
    for level in range(1, ctx.levels):
        z = level * ctx.level_m
        if z >= ctx.place.height - COURSE_M:
            break
        v, t = ctx.place.box(0.0, z - COURSE_M, ctx.place.length, z + COURSE_M / 2,
                             0.0, HEADER_M * 0.8)
        out.append(("stone", v, t))
    return tuple(out)


@register("kranzgesims", lod=1, role="stone",
          note="the eaves cornice: the deepest band on the wall, and the SILHOUETTE's own edge")
def _kranzgesims(ctx):
    if ctx.epoch in ("industrial", "bungalow", "siedlungshaus"):
        return ()
    depth = 0.40 if ctx.cornice else 0.18
    top = ctx.place.height
    v, t = ctx.place.box(-depth, top - 3 * COURSE_M, ctx.place.length + depth, top,
                         0.0, depth)
    return (("stone", v, t),)


@register("lisene", lod=2, role="wall",
          note="the flat pilaster strip that breaks a long facade into fields")
def _lisene(ctx):
    if not ctx.cornice or ctx.place.length < 14.0:
        return ()
    out = []
    every = max(2, int(round(ctx.bays / 4.0)))
    step = ctx.place.length / max(1, int(ctx.bays))
    for k in range(0, int(ctx.bays) + 1, every):
        s = k * step
        v, t = ctx.place.box(s - 0.28, 0.0, s + 0.28, ctx.place.height, 0.0, HEADER_M * 0.5)
        out.append(("wall", v, t))
    return tuple(out)


@register("fensterleibung", lod=9, role="wall",
          note="SUPERSEDED by facade.holed_wall -- a recess inside a solid wall is invisible")
def _leibung(ctx):
    """A window is not a dark rectangle painted on a wall -- it is a HOLE with sides. The reveal
    is drawn as the four faces of the recess plus the glass at its back, so the opening carries a
    shadow on one jamb and a highlight on the other, which is what the eye reads depth from."""
    out = []
    for (mid, width) in _fields(ctx):
        w = min(ctx.win_w, width * 0.68)
        for level in range(ctx.levels):
            z = level * ctx.level_m + ctx.sill_m
            if z + ctx.win_h > ctx.place.height - COURSE_M:
                break
            s0, s1 = mid - w / 2, mid + w / 2
            z1 = z + ctx.win_h
            # the recess: four thin blocks standing IN the wall, and the glass at the back
            out.append(("glass", *ctx.place.box(s0, z, s1, z1, -REVEAL_M, -REVEAL_M + 0.02)))
            for (a0, b0, a1, b1) in ((s0 - 0.02, z, s0, z1), (s1, z, s1 + 0.02, z1),
                                     (s0, z1, s1, z1 + 0.02), (s0, z - 0.02, s1, z)):
                out.append(("wall", *ctx.place.box(a0, b0, a1, b1, -REVEAL_M, 0.0)))
    return tuple(out)


@register("sohlbank", lod=2, role="stone", note="the sill, proud of the wall and wider than the hole")
def _sohlbank(ctx):
    out = []
    for (mid, width) in _fields(ctx):
        w = min(ctx.win_w, width * 0.68)
        for level in range(ctx.levels):
            z = level * ctx.level_m + ctx.sill_m
            if z + ctx.win_h > ctx.place.height - COURSE_M:
                break
            v, t = ctx.place.box(mid - w / 2 - 0.06, z - 0.07, mid + w / 2 + 0.06, z,
                                 0.0, HEADER_M * 0.7)
            out.append(("stone", v, t))
    return tuple(out)


@register("sturz", lod=2, role="stone", note="the lintel over an opening, a band the width of it")
def _sturz(ctx):
    if not ctx.cornice:
        return ()
    out = []
    for (mid, width) in _fields(ctx):
        w = min(ctx.win_w, width * 0.68)
        for level in range(ctx.levels):
            z = level * ctx.level_m + ctx.sill_m + ctx.win_h
            if z > ctx.place.height - COURSE_M:
                break
            v, t = ctx.place.box(mid - w / 2 - 0.10, z, mid + w / 2 + 0.10, z + 0.13,
                                 0.0, HEADER_M * 0.6)
            out.append(("stone", v, t))
    return tuple(out)
