"""THE ROOF SHAPES, as a registry.

A roof is a HEIGHT FIELD over the footprint and nothing else: `z(x, y)` above the eaves. Every
shape in this file is one function of the polygon's own distance function, its principal axis, or
both, and every one is registered by the name OSM uses for it. ADDING A SHAPE IS ADDING ONE
FUNCTION AND ONE LINE -- no dispatcher to edit, no if-chain to extend, and `known()` says what the
bed can build so a caller can refuse what it cannot.

The seven the C++ generator already has (`RoofKind`: Flat, Gable, Hip, Shed, Mansard, Sawtooth,
Dome) are here under OSM's spelling and behave as they do there; the rest are refinements this
bed adds and the C++ side has yet to take. That direction matters: the C++ building pipeline is a
GOOD BASE and this file corrects and extends it, never replaces it.
"""
import math

SHAPES = {}


def register(name, *, revolution=False, needs_axis=False, c_kind=None, note=""):
    """Put a shape in the registry. `revolution` says it is turned about one axis and therefore
    needs a COMPACT plan; `c_kind` names the C++ `RoofKind` it corresponds to, or None where the
    C++ side has no such shape yet."""
    def take(fn):
        fn.shape_name = name
        fn.revolution = revolution
        fn.needs_axis = needs_axis
        fn.c_kind = c_kind
        fn.note = note
        SHAPES[name] = fn
        return fn
    return take


def known(name):
    return name in SHAPES


def revolution(name):
    return bool(SHAPES[name].revolution) if name in SHAPES else False


def height_at(name, ctx):
    """The roof's height above the eaves, from the registered shape. `ctx` carries everything a
    shape may ask for and is built once per point by the caller."""
    fn = SHAPES.get(name)
    return 0.0 if fn is None else fn(ctx)


class Ctx:
    """What a shape is given. One object rather than nine arguments, so a new shape may ask for
    something the others do not without changing every signature in the file."""

    __slots__ = ("poly", "x", "y", "d", "eaves", "ridge", "axis", "inradius", "half_v", "half_u",
                 "across", "along", "pitch")

    def __init__(self, poly, x, y, d, eaves, ridge, axis, inradius, half_v, half_u,
                 across, along, pitch):
        self.poly, self.x, self.y, self.d = poly, x, y, d
        self.eaves, self.ridge, self.axis = eaves, ridge, axis
        self.inradius, self.half_v, self.half_u = inradius, half_v, half_u
        self.across, self.along, self.pitch = across, along, pitch

    @property
    def rise(self):
        return self.ridge - self.eaves


# ----------------------------------------------------------------------------- the shapes

@register("flat", c_kind="Flat", note="no rise at all; the parapet is a facade element")
def _flat(c):
    return 0.0


@register("pyramidal", revolution=True, c_kind=None,
          note="the apex over the centroid: the distance function normalised by its own maximum")
def _pyramidal(c):
    return c.rise * c.d / max(c.inradius, 1e-6)


@register("hipped", c_kind="Hip",
          note="one pitch off every edge; its ridge set IS the straight skeleton")
def _hipped(c):
    return min(c.d * math.tan(c.pitch), c.rise)


@register("gabled", needs_axis=True, c_kind="Gable",
          note="the distance to the two LONG sides only, so the ridge runs along the long axis")
def _gabled(c):
    return max(0.0, min((c.half_v - c.across) * math.tan(c.pitch), c.rise))


@register("skillion", needs_axis=True, c_kind="Shed", note="one plane, falling across the axis")
def _skillion(c):
    signed = (c.x - c.poly.centroid.x) * c.axis[1][0] + (c.y - c.poly.centroid.y) * c.axis[1][1]
    return c.rise * (signed + c.half_v) / max(2 * c.half_v, 1e-6)


@register("mansard", c_kind="Mansard",
          note="two pitches in series off the EDGE: steep to 0.6 of the rise, then shallow")
def _mansard(c):
    steep_t = math.tan(math.radians(70.0))
    shallow_t = math.tan(math.radians(20.0))
    knee = c.rise * 0.6
    if c.d * steep_t < knee:
        return max(0.0, min(c.d * steep_t, c.rise))
    return min(knee + max(0.0, c.d - knee / steep_t) * shallow_t, c.rise)


@register("half-hipped", needs_axis=True, c_kind=None,
          note="Krueppelwalm: a gable whose top is cut back by a small hip")
def _half_hipped(c):
    rise = (c.half_v - c.across) * math.tan(c.pitch)
    clip = max(0.0, c.half_u - c.along) * math.tan(c.pitch) + c.rise * 0.55
    return max(0.0, min(rise, clip, c.rise))


@register("gambrel", needs_axis=True, c_kind=None,
          note="two pitches across the WIDTH rather than off the edge, the barn's own")
def _gambrel(c):
    knee = c.half_v * 0.55
    steep, shallow = math.tan(math.radians(65.0)), math.tan(math.radians(28.0))
    if c.across > knee:
        return max(0.0, (c.half_v - c.across) * steep)
    return (c.half_v - knee) * steep + (knee - c.across) * shallow


@register("sawtooth", needs_axis=True, c_kind="Sawtooth",
          note="Sheddach: the roof a factory hall wears, its glazing facing north")
def _sawtooth(c):
    bay = max(6.0, 2 * c.half_u / max(1, round(2 * c.half_u / 12.0)))
    signed = (c.x - c.poly.centroid.x) * c.axis[0][0] + (c.y - c.poly.centroid.y) * c.axis[0][1]
    return c.rise * (((signed + c.half_u) % bay) / bay)


@register("barrel", needs_axis=True, c_kind=None, note="a half cylinder along the axis")
def _barrel(c):
    return c.rise * math.sqrt(max(0.0, 1.0 - (c.across / max(c.half_v, 1e-6)) ** 2))


@register("spire", revolution=True, c_kind=None, note="Turmhelm: a steep pyramid, the church's")
def _spire(c):
    return c.rise * c.d / max(c.inradius, 1e-6)


@register("onion", revolution=True, c_kind=None,
          note="a ZWIEBELHAUBE has a BULB, a SHOULDER and a LANTERN; a cone has none of them")
def _onion(c):
    # a height field cannot hold the bulb's overhang -- the drum's radius is its widest -- so the
    # flare is spent on the RATE: steep at the rim, flat through the shoulder, steep at the top.
    # Drawn as a cone it read as a rocket (measured on the baroque church sheet)
    u = min(1.0, c.d / max(c.inradius, 1e-6))
    return c.rise * (0.60 * math.sin(math.pi / 2 * u ** 0.42) + 0.40 * u ** 7)


@register("butterfly", needs_axis=True, c_kind=None, note="two planes falling INWARD to a valley")
def _butterfly(c):
    return c.rise * (c.across / max(c.half_v, 1e-6))


@register("dome", revolution=True, c_kind="Dome", note="a hemisphere over the inradius")
def _dome(c):
    r = c.inradius
    return c.rise * math.sqrt(max(0.0, 1.0 - ((r - c.d) / max(r, 1e-6)) ** 2))


def catalogue():
    """What the bed can build, with what the C++ side calls it. A row with no `c_kind` is a
    shape the C++ `RoofKind` has yet to take -- which is the list this lab owes it."""
    return sorted((n, f.c_kind or "-", f.note) for n, f in SHAPES.items())
