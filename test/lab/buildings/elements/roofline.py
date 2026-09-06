"""THE ROOFLINE: what stands against the sky, as geometry.

At 200 m a building IS its silhouette, and the silhouette is not the roof plane -- it is what
breaks it. A street of gabled roofs with nothing on them reads as a diagram; the same street with
chimneys at the party walls, dormers on the slope and a ridge that carries its tiles reads as a
town. That is the cheapest detail in the whole ladder by triangles per metre of read, which is why
it is rung L1 and not L3.

    SCHORNSTEIN  a chimney stands ON THE PARTY WALL and rises above the ridge, because a flue that
                 does not clear the ridge draws badly -- DIN 18160 asks 400 mm over a 20 degree
                 roof and 1 m measured horizontally from the surface
    GAUBE        a dormer: cheeks, a face, its own little roof, set back from the eaves by a
                 course and stopping short of the ridge
    ATTIKA       the parapet a flat roof needs, 900 mm because that is a guard rail's height
    FIRSTZIEGEL  the ridge, a half-round course along it; two hundred millimetres that turn a
                 crease into a line
    DACHRINNE    the gutter at the eaves, and the one thing that gives a roof edge thickness
"""
import math

import numpy as np

from .base import Place, register

FLUE_OVER_RIDGE_M = 0.40   # [SET] DIN 18160-1: a flue clears the ridge by this on a pitched roof
PARAPET_M = 0.90           # [SET] a guard against falling, which is what a parapet is
GUTTER_M = 0.12            # [SET] a half-round gutter's radius, and the eaves' own thickness


@register("dachrinne", lod=1, role="metal",
          note="the gutter: what gives an eaves edge thickness against the sky")
def _dachrinne(ctx):
    if ctx.roof == "flat":
        return ()
    p = ctx.place
    v, t = p.box(0.0, p.height - GUTTER_M, p.length, p.height,
                 ctx.eaves_over_m, ctx.eaves_over_m + 2 * GUTTER_M)
    return (("metal", v, t),)


@register("attika", lod=1, role="stone", note="a flat roof's parapet, at a guard rail's height")
def _attika(ctx):
    if ctx.roof != "flat":
        return ()
    p = ctx.place
    v, t = p.box(0.0, p.height, p.length, p.height + PARAPET_M, -0.10, 0.14)
    return (("stone", v, t),)


@register("firstziegel", lod=1, role="roof", note="the ridge course, along the ridge")
def _firstziegel(ctx):
    if ctx.roof not in ("gabled", "half-hipped", "gambrel", "hipped") or ctx.ridge_run is None:
        return ()
    (a, b) = ctx.ridge_run
    a, b = np.asarray(a, dtype=float), np.asarray(b, dtype=float)
    span = float(np.linalg.norm(b - a))
    if span < 1.0:
        return ()
    ridge = Place(a, b - a, (0.0, 0.0, 1.0), span, 0.30, up=np.cross(b - a, (0.0, 0.0, 1.0)))
    v, t = ridge.box(0.0, -0.11, span, 0.11, -0.16, 0.16)
    return (("roof", v, t),)


@register("schornstein", lod=1, role="stone",
          note="a chimney ON THE RIDGE, clearing it by DIN 18160's 400 mm")
def _schornstein(ctx):
    """A CHIMNEY STANDS ON THE RIDGE, NOT IN A WALL. Built in the wall's own frame it came out as
    two thin white posts shooting through the roof with their caps floating above them (rendered
    and looked at, 2026-09-06) -- the shaft was inside the body and only the part above the tiles
    was visible. A stack is a solid block that starts inside the roof and ends over the ridge, and
    it belongs to the RIDGE's frame, so it is built once per body and not once per wall."""
    if ctx.roof == "flat" or not ctx.heated or ctx.ridge_run is None:
        return ()
    (a, b) = ctx.ridge_run
    a, b = np.asarray(a, dtype=float), np.asarray(b, dtype=float)
    span = float(np.linalg.norm(b - a))
    if span < 3.0:
        return ()
    along = (b - a) / span
    across = np.cross((0.0, 0.0, 1.0), along)
    out = []
    shaft = 0.50 if ctx.levels <= 2 else 0.72
    top = ctx.ridge_z + FLUE_OVER_RIDGE_M
    for frac in ((0.18, 0.82) if span > 14.0 else (0.72,)):
        seat = a + along * (span * frac)
        base = np.array([seat[0], seat[1], ctx.ridge_z - 1.4])
        stack = Place(base - along * (shaft / 2) - across * (shaft / 2), along, across,
                      shaft, top - base[2])
        out.append(("stone", *stack.box(0.0, 0.0, shaft, top - base[2], 0.0, shaft)))
        out.append(("stone", *stack.box(-0.09, top - base[2], shaft + 0.09,
                                        top - base[2] + 0.15, -0.09, shaft + 0.09)))
    return tuple(out)


@register("gaube", lod=1, role="roof",
          note="a dormer PROJECTING FROM THE SLOPE, with its own face, cheeks and roof")
def _gaube(ctx):
    """A DORMER PROJECTS FROM THE ROOF and does not sit in the wall below it. Built in the wall's
    frame it went 1.3 m INTO the body and 5 cm out of it, so nothing was visible but a sliver
    (rendered and looked at, 2026-09-06) -- the same defect as the reveal and the shopfront, three
    times over: geometry written in the wrong frame is geometry inside something else.

    The dormer stands OUT of the wall plane by its own depth, seated above the eaves where the
    slope has risen enough to carry it, and its face is vertical."""
    if ctx.roof not in ("gabled", "hipped", "mansard", "half-hipped", "gambrel"):
        return ()
    if not ctx.faces_slope or ctx.rise_m < 2.2 or ctx.bays < 2:
        return ()
    out = []
    p = ctx.place
    # A DORMER IS SMALL AGAINST ITS ROOF and sits LOW on the slope. At 0.40 of the rise on a
    # 2.8 m roof it reached most of the way to the ridge and read as a box parked up there
    # (looked at, 2026-09-06); a dormer covers about a quarter of the slope's height and its
    # cill sits a course or two above the gutter.
    wide, high, deep = 1.25, min(1.35, ctx.rise_m * 0.30), 0.95
    seat = p.height + max(0.22, ctx.rise_m * 0.08)
    step = p.length / max(1, int(ctx.bays))
    for bay in range(0, int(ctx.bays), 2):
        s = (bay + 0.5) * step
        if s < 1.4 or s > p.length - 1.4:
            continue
        # the box stands PROUD of the wall by its depth and reaches back into the slope
        out.append(("roof", *p.box(s - wide / 2, seat, s + wide / 2, seat + high, -0.9, deep)))
        out.append(("roof", *p.box(s - wide / 2 - 0.12, seat + high, s + wide / 2 + 0.12,
                                   seat + high + 0.13, -1.0, deep + 0.16)))
        out.append(("glass", *p.box(s - wide / 2 + 0.15, seat + 0.20, s + wide / 2 - 0.15,
                                    seat + high - 0.18, deep - 0.06, deep - 0.04)))
    return tuple(out)
