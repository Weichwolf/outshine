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


def _openings(ctx):
    """Every opening this wall carries, with the storey it belongs to -- ONE source, so a sill
    and its window cannot disagree about where the window is."""
    from .facade import openings_of
    return openings_of(ctx)


@register("sohlbank", lod=2, role="stone",
          note="the sill, proud of the wall and wider than the hole, with a drip")
def _sohlbank(ctx):
    out = []
    for (s0, z0, s1, z1, floor) in _openings(ctx):
        if floor.at == 0 and z1 - z0 > 1.9:
            continue                      # a shop's light stands on its stallriser, not a sill
        out.append(("stone", *ctx.place.box(s0 - 0.06, z0 - 0.07, s1 + 0.06, z0,
                                            0.0, HEADER_M * 0.7)))
    return tuple(out)


@register("sturz", lod=2, role="stone",
          note="the head over an opening: a band, or a PEDIMENT where the storey asks for one")
def _sturz(ctx):
    out = []
    for (s0, z0, s1, z1, floor) in _openings(ctx):
        if floor.head == "none" or (floor.at == 0 and z1 - z0 > 1.9):
            continue
        out.append(("stone", *ctx.place.box(s0 - 0.10, z1, s1 + 0.10, z1 + 0.13,
                                            0.0, HEADER_M * 0.6)))
        if floor.head == "pediment":
            # THE BELETAGE IS THE ONLY FLOOR THAT GETS A PEDIMENT, and that single fact is what
            # a viewer reads a Gruenderzeit block's hierarchy from at a hundred metres.
            mid = 0.5 * (s0 + s1)
            for (a0, a1, zz, hh) in ((s0 - 0.16, s1 + 0.16, z1 + 0.13, 0.10),
                                     (mid - (s1 - s0) * 0.30, mid + (s1 - s0) * 0.30,
                                      z1 + 0.23, 0.12),
                                     (mid - (s1 - s0) * 0.16, mid + (s1 - s0) * 0.16,
                                      z1 + 0.35, 0.11)):
                out.append(("stone", *ctx.place.box(a0, zz, a1, zz + hh, 0.0, HEADER_M * 0.9)))
    return tuple(out)


@register("balkon", lod=3, role="stone",
          note="a balcony on the BELETAGE, over the door's own bay, with its balustrade")
def _balkon(ctx):
    """A BALCONY IS A BODY THAT STANDS OFF THE WALL, and the one thing on a facade that breaks
    its plane. It sits on the piano nobile, over the entrance, because that is where the owner's
    room was -- and a facade with none is a facade with nothing in front of it at all."""
    from . import storeys as rhythm
    if not ctx.cornice or not ctx.street or int(ctx.levels) < 3:
        return ()
    floors = [f for f in rhythm.of(ctx) if f.kind == "beletage"]
    if not floors:
        return ()
    floor = floors[0]
    mid = ctx.place.length * 0.5
    half = min(2.10, ctx.place.length * 0.22)
    z = floor.z0 - 0.02
    out = [("stone", *ctx.place.box(mid - half, z, mid + half, z + 0.16, 0.0, 1.15))]
    for at in (mid - half + 0.05, mid + half - 0.05):
        out.append(("stone", *ctx.place.box(at - 0.05, z + 0.16, at + 0.05, z + 1.02, 0.0, 1.10)))
    out.append(("stone", *ctx.place.box(mid - half, z + 0.94, mid + half, z + 1.02, 1.02, 1.15)))
    n = max(4, int(2 * half / 0.16))
    for k in range(1, n):
        s = mid - half + 2 * half * k / n
        out.append(("stone", *ctx.place.box(s - 0.028, z + 0.16, s + 0.028, z + 0.94,
                                            1.05, 1.11)))
    return tuple(out)
