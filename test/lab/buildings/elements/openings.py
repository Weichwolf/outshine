"""THE GROUND FLOOR AND ITS OPENINGS, as geometry.

In every one of the references the DETAIL BUDGET GOES TO THE STREET LEVEL. A player walks there,
and it is where a building stops being a facade and becomes a place you can enter: a shopfront
with its stallriser and fascia, a door in a frame with steps up to it, a canopy over the door, a
cellar light at the pavement, a railing where the ground falls away. A block whose ground floor is
the same wall as its third floor reads as a model of a building.

    SCHAUFENSTER  a shopfront: a stallriser, glass to the fascia, a fascia board over it
    HAUSTUER      the door, set in a frame, in a reveal deeper than a window's
    TREPPE        the steps up to it where the floor stands above the pavement
    VORDACH       the canopy over the door
    KELLERFENSTER the cellar light at the pavement, which is what says a building has a below
"""
from .base import register
from .relief import COURSE_M, HEADER_M, REVEAL_M

STALL_M = 0.60             # [SET] a stallriser, the sill a shop window stands on
FASCIA_M = 0.55            # [SET] the board over a shopfront, where the sign goes
DOOR_W_M = 1.26            # [SET] DIN 18040: a leaf a wheelchair passes, plus its frame
RISER_M = 0.17             # [SET] DIN 18065: a public stair's riser


def _shopping(ctx):
    return ctx.epoch in ("gruenderzeit", "jugendstil", "commercial", "hall", "interwar",
                         "contemporary", "late20") and ctx.street and ctx.levels >= 2


@register("schaufenster", lod=2, role="glass",
          note="a shopfront: stallriser, glass, fascia -- the street level's own wall")
def _schaufenster(ctx):
    if not _shopping(ctx):
        return ()
    p = ctx.place
    top = ctx.level_m - FASCIA_M
    if top <= STALL_M + 0.4:
        return ()
    out = [("stone", *p.box(0.0, 0.0, p.length, STALL_M, 0.0, HEADER_M * 0.6)),
           ("glass", *p.box(0.30, STALL_M, p.length - 0.30, top, -0.16, -0.13)),
           ("stone", *p.box(-0.06, top, p.length + 0.06, top + FASCIA_M, 0.0, HEADER_M))]
    n = max(1, int(p.length // 3.2))
    for k in range(1, n):
        s = p.length * k / n
        out.append(("wall", *p.box(s - 0.09, STALL_M, s + 0.09, top, -0.16, 0.02)))
    return tuple(out)


@register("haustuer", lod=2, role="wall", note="the door in its frame, in a deeper reveal")
def _haustuer(ctx):
    if not ctx.street or ctx.epoch in ("industrial", "hall"):
        return ()
    p = ctx.place
    mid = p.length * 0.5
    high = min(2.35, ctx.level_m * 0.86)
    seat = max(0.0, ctx.floor_over_ground_m)
    out = [("glass", *p.box(mid - DOOR_W_M / 2, seat, mid + DOOR_W_M / 2, seat + high,
                            -REVEAL_M - 0.09, -REVEAL_M - 0.07))]
    for (a0, a1) in ((mid - DOOR_W_M / 2 - 0.11, mid - DOOR_W_M / 2),
                     (mid + DOOR_W_M / 2, mid + DOOR_W_M / 2 + 0.11)):
        out.append(("stone", *p.box(a0, seat, a1, seat + high + 0.11, -REVEAL_M - 0.09, 0.05)))
    out.append(("stone", *p.box(mid - DOOR_W_M / 2 - 0.11, seat + high,
                                mid + DOOR_W_M / 2 + 0.11, seat + high + 0.11,
                                -REVEAL_M - 0.09, 0.05)))
    return tuple(out)


@register("treppe", lod=2, role="stone",
          note="the steps to the door where the floor stands over the pavement")
def _treppe(ctx):
    if not ctx.street or ctx.floor_over_ground_m < RISER_M:
        return ()
    p = ctx.place
    mid = p.length * 0.5
    steps = max(1, int(round(ctx.floor_over_ground_m / RISER_M)))
    out = []
    for k in range(steps):
        z = ctx.floor_over_ground_m * (k + 1) / steps
        depth = 0.30 * (steps - k)
        out.append(("stone", *p.box(mid - DOOR_W_M / 2 - 0.30, 0.0,
                                    mid + DOOR_W_M / 2 + 0.30, z, 0.0, depth)))
    return tuple(out)


@register("vordach", lod=3, role="metal", note="the canopy over the door")
def _vordach(ctx):
    if not ctx.street or ctx.epoch in ("industrial", "gothic", "sacral", "baroque"):
        return ()
    p = ctx.place
    mid = p.length * 0.5
    z = min(2.55, ctx.level_m * 0.92) + max(0.0, ctx.floor_over_ground_m)
    out = [("metal", *p.box(mid - 1.35, z, mid + 1.35, z + 0.10, 0.0, 1.25))]
    for at in (mid - 1.15, mid + 1.15):
        out.append(("metal", *p.box(at - 0.045, z - 1.05, at + 0.045, z, 1.05, 1.15)))
    return tuple(out)


@register("kellerfenster", lod=3, role="glass",
          note="the cellar light at the pavement: what says a building has a below")
def _kellerfenster(ctx):
    if ctx.epoch not in ("gruenderzeit", "jugendstil", "interwar", "siedlungshaus"):
        return ()
    p = ctx.place
    out = []
    step = p.length / max(1, int(ctx.bays))
    for bay in range(int(ctx.bays)):
        s = (bay + 0.5) * step
        out.append(("glass", *p.box(s - 0.36, 0.12, s + 0.36, 0.52, -0.10, -0.08)))
    return tuple(out)
