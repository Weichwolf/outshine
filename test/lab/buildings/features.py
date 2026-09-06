"""THE FACADE ELEMENTS, as a registry.

An element is what a viewer reads a period from at two hundred metres: a Kranzgesims with its
four courses, a Sockel with its Bossierung, a Strebepfeiler set back in stages. This file holds
one function per element and nothing else, and each is registered with the LOD RUNG at which it
appears -- L0 the mass, L1 the facade as material parameters, L2 the reveals and the profiles,
L3 the bodies that stand off the wall.

ADDING AN ELEMENT IS ADDING A FUNCTION AND ONE LINE. There is no dispatcher to edit: the sheet
walks the registry, asks each element whether it `applies` here, and draws the ones that say yes,
in the order they were registered. A specialisation -- a region's own variant of a cornice, an
epoch's own window head -- is another entry beside the first, guarded more tightly.

Every function takes ONE context, `d`, which carries the axes, the building, the facade, the wall
being drawn and the numbers the sheet has already worked out. A new element that needs something
the context lacks adds a field there and nothing else changes.
"""
import math

import numpy as np
import matplotlib
import matplotlib.patches
from matplotlib.patches import Polygon as MplPoly, Rectangle

ELEMENTS_BY_LOD = {}
ORDER = []


class Draw:
    """What an element is given. One object rather than seventeen arguments."""

    __slots__ = ("axe", "b", "f", "wall", "el", "ink", "extent", "joints", "street", "rise",
                 "gable_ended", "faces_slope", "heated", "named_wall", "proj", "H", "L")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))


def register(name, lod, note=""):
    """Put an element in the registry at the rung it belongs to."""
    def take(fn):
        fn.element_name = name
        fn.lod = lod
        fn.note = note
        ELEMENTS_BY_LOD.setdefault(lod, []).append(fn)
        ORDER.append(fn)
        return fn
    return take


def draw_all(d, upto=3):
    """Every registered element that applies here, at or below rung `upto`, in order."""
    for fn in ORDER:
        if fn.lod <= upto:
            fn(d)


def catalogue():
    return [(fn.element_name, fn.lod, fn.note) for fn in ORDER]


@register("kranzgesims", lod=2)
def _kranzgesims(d):
    if not (d.b.style.cornice):
        return
    # a KRANZGESIMS is a PROFILE and not a stripe: a bed mould, a row of dentils, the
    # corona that throws the shadow, and a cyma above it. Four courses is what makes a
    # cornice read at 1:200 and it is the single element a viewer reads a period from
    oversail = 0.55
    d.axe.add_patch(Rectangle((-0.10, d.b.eaves - 0.62), d.wall["length"] + 0.20, 0.12,
                            facecolor="0.88", edgecolor=d.ink, lw=0.6))       # Bettgesims
    if d.b.style.epoch in ("gruenderzeit", "baroque", "commercial"):
        for x in np.arange(0.0, d.wall["length"], 0.42):                       # Zahnschnitt
            d.axe.add_patch(Rectangle((x + 0.08, d.b.eaves - 0.50), 0.22, 0.20,
                                    facecolor="0.80", edgecolor=d.ink, lw=0.4))
    d.axe.add_patch(Rectangle((-oversail, d.b.eaves - 0.30), d.wall["length"] + 2 * oversail,
                            0.24, facecolor="0.78", edgecolor=d.ink, lw=0.9))  # Corona
    d.axe.add_patch(Rectangle((-oversail * 0.7, d.b.eaves - 0.06),
                            d.wall["length"] + 1.4 * oversail, 0.10,
                            facecolor="0.86", edgecolor=d.ink, lw=0.6))        # Sima


@register("sockel", lod=2)
def _sockel(d):
    if not ("Sockel" in d.el or d.b.style.epoch in ("gruenderzeit", "baroque", "gothic", "jugendstil")):
        return
    hs = d.b.style.level_m * 0.32
    d.axe.add_patch(Rectangle((-0.12, d.b.pad), d.wall["length"] + 0.24, hs,
                            facecolor="0.86", edgecolor=d.ink, lw=0.9))        # Sockel
    for z in np.arange(d.b.pad + 0.34, d.b.pad + hs - 0.05, 0.34):               # Bossierung
        d.axe.plot([-0.12, d.wall["length"] + 0.12], [z, z], color="0.55", lw=0.45)
    d.axe.add_patch(Rectangle((-0.20, d.b.pad + hs), d.wall["length"] + 0.40, 0.14,
                            facecolor="0.80", edgecolor=d.ink, lw=0.7))        # Sockelgesims


@register("gurtgesims", lod=2)
def _gurtgesims(d):
    if not ("Gurtgesims" in d.el or "Gesims" in d.el):
        return
    for level in range(1, d.f.levels()):
        z = d.b.pad + level * d.b.style.level_m
        d.axe.add_patch(Rectangle((-0.16, z - 0.24), d.wall["length"] + 0.32, 0.18,
                                facecolor="0.84", edgecolor=d.ink, lw=0.7))
        d.axe.plot([-0.16, d.wall["length"] + 0.16], [z - 0.30] * 2, color="0.55", lw=0.5)


@register("pilaster", lod=2)
def _pilaster(d):
    if not ("Pilaster" in d.el):
        return
    for bay in range(d.wall["bays"] + 1):
        x = bay * d.wall["bay_m"]
        w_p = 0.42
        d.axe.add_patch(Rectangle((x - w_p / 2, d.b.pad), w_p, d.b.eaves - d.b.pad,
                                facecolor="0.90", edgecolor=d.ink, lw=0.7))    # Schaft
        d.axe.add_patch(Rectangle((x - w_p * 0.8, d.b.pad), w_p * 1.6, 0.34,
                                facecolor="0.84", edgecolor=d.ink, lw=0.7))    # Basis
        d.axe.add_patch(Rectangle((x - w_p * 0.85, d.b.eaves - 1.05), w_p * 1.7, 0.42,
                                facecolor="0.82", edgecolor=d.ink, lw=0.7))    # Kapitell
        for z in np.arange(d.b.pad + 0.7, d.b.eaves - 1.2, 0.55):                # Kannelur
            d.axe.plot([x - w_p * 0.22, x + w_p * 0.22], [z, z], color="0.6", lw=0.3)


@register("strebepfeiler", lod=2)
def _strebepfeiler(d):
    if not ("Strebepfeiler" in d.el):
        return
    # a STREBEPFEILER carries the vault's thrust and is therefore THICKEST AT THE FOOT,
    # set back in stages, each stage shedding water on a weathering. A plain rectangle is
    # a pilaster with the wrong name and carries nothing (seen in B01)
    d.H = d.b.eaves - d.b.pad
    for bay in range(d.wall["bays"] + 1):
        x = bay * d.wall["bay_m"]
        for k, (wd, z0, z1) in enumerate(((0.62, 0.00, 0.42), (0.48, 0.42, 0.74),
                                          (0.34, 0.74, 0.94))):
            d.axe.add_patch(Rectangle((x - wd, d.b.pad + d.H * z0), 2 * wd, d.H * (z1 - z0),
                                    facecolor="0.90", edgecolor=d.ink, lw=0.9, zorder=5))
            d.axe.add_patch(MplPoly(np.array([(x - wd, d.b.pad + d.H * z1),
                                            (x + wd, d.b.pad + d.H * z1),
                                            (x + wd * 0.72, d.b.pad + d.H * z1 + 0.34),
                                            (x - wd * 0.72, d.b.pad + d.H * z1 + 0.34)]),
                                  closed=True, facecolor="0.80", edgecolor=d.ink,
                                  lw=0.8, zorder=5))                   # Wasserschlag
        d.axe.add_patch(MplPoly(np.array([(x - 0.30, d.b.pad + d.H * 0.94),
                                        (x + 0.30, d.b.pad + d.H * 0.94),
                                        (x, d.b.pad + d.H * 0.94 + 1.4)]), closed=True,
                              facecolor="0.86", edgecolor=d.ink, lw=0.8, zorder=5))  # Fiale


@register("vorhangfassade", lod=1)
def _vorhangfassade(d):
    if not ("Vorhangfassade" in d.el):
        return
    # a CURTAIN WALL is a grid hung in front of the frame: a spandrel band at every
    # floor, a mullion at every module, a SOCKELGESCHOSS of double height at the foot
    # and an ATTIKA that hides the plant. Drawn as 480 loose window rectangles it read
    # as graph paper, which is what a tower without these three reads as
    base_h = d.b.style.level_m * 1.7
    d.axe.add_patch(Rectangle((0, d.b.pad), d.wall["length"], base_h,
                            facecolor="0.40", edgecolor=d.ink, lw=1.0, zorder=3))
    d.axe.add_patch(Rectangle((-0.25, d.b.pad + base_h), d.wall["length"] + 0.5, 0.30,
                            facecolor="0.86", edgecolor=d.ink, lw=0.8, zorder=3))
    for level in range(2, d.f.levels() + 1):
        z = d.b.pad + level * d.b.style.level_m
        if z > d.b.eaves:
            break
        d.axe.add_patch(Rectangle((0, z - 0.75), d.wall["length"], 0.75,
                                facecolor="0.72", edgecolor="none", zorder=3))
        d.axe.plot([0, d.wall["length"]], [z - 0.75, z - 0.75], color=d.ink, lw=0.4, zorder=3)
    for m in np.arange(0.0, d.wall["length"] + 1e-6, d.wall["bay_m"] / 2.0):
        d.axe.plot([m, m], [d.b.pad + base_h + 0.3, d.b.eaves], color=d.ink, lw=0.4, zorder=3)
    d.axe.add_patch(Rectangle((-0.30, d.b.eaves - 1.1), d.wall["length"] + 0.60, 1.1,
                            facecolor="0.80", edgecolor=d.ink, lw=1.0, zorder=3))  # Attika


@register("fachwerk", lod=2)
def _fachwerk(d):
    if not ("Fachwerk" in d.el and d.f.levels() >= 2):
        return
    # a FACHWERK is a frame and not a texture: a SCHWELLE and a RÄHM close each storey,
    # a STÄNDER stands on every bay joint, and the corner bays are braced with a STREBE
    # -- without the brace the frame is a mechanism and the drawing says so
    for level in range(1, d.f.levels()):
        z0 = d.b.pad + level * d.b.style.level_m
        z1 = min(d.b.eaves, z0 + d.b.style.level_m)
        d.axe.add_patch(Rectangle((0, z0), d.wall["length"], 0.16,
                                facecolor="0.72", edgecolor=d.ink, lw=0.6))   # Schwelle
        d.axe.add_patch(Rectangle((0, z1 - 0.16), d.wall["length"], 0.16,
                                facecolor="0.72", edgecolor=d.ink, lw=0.6))   # Raehm
        for bay in range(d.wall["bays"] + 1):
            x = bay * d.wall["bay_m"]
            d.axe.add_patch(Rectangle((x - 0.09, z0), 0.18, z1 - z0,
                                    facecolor="0.72", edgecolor=d.ink, lw=0.6))
        for bay in (0, d.wall["bays"] - 1):
            x0s = bay * d.wall["bay_m"]
            sgn = 1.0 if bay == 0 else -1.0
            d.axe.plot([x0s + sgn * 0.09, x0s + sgn * (d.wall["bay_m"] * 0.55)],
                     [z1 - 0.16, z0 + 0.16], color=d.ink, lw=1.4)              # Strebe


@register("schaufenster", lod=1)
def _schaufenster(d):
    if not ("Schaufenster" in d.el):
        return
    # a SHOPFRONT is about three metres tall whatever the storey height is: tied to
    # `level_m` it grew to eight metres on a retail shed and swallowed the whole facade
    hs_ = min(3.2, d.b.style.level_m - 0.9)
    d.axe.add_patch(Rectangle((0.6, d.b.pad + 0.3), d.wall["length"] - 1.2, hs_,
                            facecolor="0.45", edgecolor=d.ink, lw=1.0, zorder=3))
    d.axe.add_patch(Rectangle((-0.4, d.b.pad + hs_ + 0.45), d.wall["length"] + 0.8, 0.35,
                            facecolor="0.75", edgecolor=d.ink, lw=0.8, zorder=4))  # Vordach


@register("arkade", lod=2)
def _arkade(d):
    if not ("Arkade" in d.el):
        return
    # an ARCADE is a row of PIERS with arches between them, springing at the head of the
    # ground floor and dying into the first floor's band. Struck at mid-storey with a
    # 0.7-bay radius it read as two pencil scribbles over the shopfront (seen in B10)
    spring = d.b.pad + d.b.style.level_m * 0.62
    pier = min(0.6, d.wall["bay_m"] * 0.18)
    for bay in range(d.wall["bays"] + 1):
        x = bay * d.wall["bay_m"]
        d.axe.add_patch(Rectangle((x - pier / 2, d.b.pad), pier, spring - d.b.pad,
                                facecolor="0.86", edgecolor=d.ink, lw=0.9, zorder=4))
    for bay in range(d.wall["bays"]):
        x = (bay + 0.5) * d.wall["bay_m"]
        clear = d.wall["bay_m"] - pier
        d.axe.add_patch(matplotlib.patches.Wedge((x, spring), clear / 2, 0, 180,
                                               width=0.22, facecolor="0.86",
                                               edgecolor=d.ink, lw=0.9, zorder=4))


@register("sheddach", lod=0)
def _sheddach(d):
    if not ("Sheddach" in d.el or d.b.roof == "sawtooth"):
        return
    pass


@register("schornstein_flat", lod=3)
def _schornstein_flat(d):
    if not ("Schornstein" in d.el and d.b.roof in ("flat", "sawtooth", "skillion")):
        return
    d.axe.add_patch(Rectangle((d.wall["length"] * 0.78, d.b.eaves), 1.2,
                            max(6.0, (d.b.eaves - d.b.pad) * 0.5),
                            facecolor="0.8", edgecolor=d.ink, lw=1.0))


@register("erker", lod=3)
def _erker(d):
    if not ("Erker" in d.el and d.f.levels() >= 3 and d.wall["bays"] >= 3):
        return
    # an ERKER is corbelled out over the first floor and stops under the eaves, with its
    # own little roof; in plan it is already there as the projection the mass carries
    xe = (d.wall["bays"] // 2) * d.wall["bay_m"]
    z0e, z1e = d.b.pad + d.b.style.level_m, d.b.eaves - 0.7
    we = d.wall["bay_m"] * 0.88
    # the ORIEL stands BEHIND the openings, or it hides the very windows it exists to
    # carry and reads as a pier (seen in B05). Its cap is a low hip, not a spire
    d.axe.add_patch(MplPoly(np.array([(xe - we / 2 + 0.35, z0e - 0.9), (xe + we / 2 - 0.35, z0e - 0.9),
                                    (xe + we / 2, z0e), (xe - we / 2, z0e)]), closed=True,
                          facecolor="0.88", edgecolor=d.ink, lw=0.9, zorder=6))   # Konsole
    # the oriel's SHAFT is drawn as an outline: filled it hides the windows it carries,
    # behind the wall it vanishes. An elevation shows a projecting body by its edge and
    # its shadow, which is the convention and also the only thing that reads here
    d.axe.add_patch(Rectangle((xe - we / 2, z0e), we, z1e - z0e, facecolor="none",
                            edgecolor=d.ink, lw=1.4, zorder=6))
    d.axe.plot([xe + we / 2 - 0.12] * 2, [z0e, z1e], color="0.45", lw=2.2, zorder=6)
    d.axe.add_patch(MplPoly(np.array([(xe - we / 2 - 0.25, z1e), (xe + we / 2 + 0.25, z1e),
                                    (xe + we / 4, z1e + 0.55), (xe - we / 4, z1e + 0.55)]),
                          closed=True, facecolor="0.84", edgecolor=d.ink, lw=0.9, zorder=6))
    for lv in range(1, d.f.levels()):
        zz = d.b.pad + lv * d.b.style.level_m
        if z0e < zz < z1e - 0.4:
            d.axe.plot([xe - we / 2, xe + we / 2], [zz, zz], color=d.ink, lw=0.5, zorder=6)


@register("stuckband", lod=3)
def _stuckband(d):
    if not ("Stuckband" in d.el):
        return
    zs = d.b.eaves - 1.15
    d.axe.add_patch(Rectangle((0, zs), d.L, 0.55, facecolor="0.90", edgecolor=d.ink,
                            lw=0.7, zorder=4))
    for xs_ in np.arange(0.45, d.L, 0.9):
        d.axe.add_patch(matplotlib.patches.Circle((xs_, zs + 0.275), 0.17,
                                                facecolor="0.82", edgecolor=d.ink,
                                                lw=0.5, zorder=5))


@register("schweifgiebel", lod=3)
def _schweifgiebel(d):
    if not ("geschweifter Giebel" in d.el and d.b.ridge > d.b.eaves + 1.2 and d.street):
        return
    # a VOLUTE GABLE over the middle bays: two S-curves meeting at a small pediment
    xm, wg = d.L / 2, min(d.L * 0.42, d.wall["bay_m"] * 2.4)
    hg = min(d.b.ridge - d.b.eaves, 3.0)
    # a SCHWEIFGIEBEL is two S-curves rising from the shoulders to a small pediment:
    # smoothstep is exactly that curve, concave at the foot and convex at the top
    u = np.linspace(0.0, 1.0, 48)
    left = [(xm - wg / 2 + (wg / 2) * uu, d.b.eaves + hg * (3 * uu ** 2 - 2 * uu ** 3))
            for uu in u]
    volute = [(xm - wg / 2, d.b.eaves)] + left \
        + [(xm + wg / 2 - (x - (xm - wg / 2)), z) for x, z in reversed(left)] \
        + [(xm + wg / 2, d.b.eaves)]
    d.axe.add_patch(MplPoly(np.array(volute), closed=True, facecolor="0.88", edgecolor=d.ink,
                          lw=1.0, zorder=6))


@register("wasserspeier", lod=3)
def _wasserspeier(d):
    if not ("Wasserspeier" in d.el):
        return
    for bay in range(d.wall["bays"] + 1):
        x = bay * d.wall["bay_m"]
        d.axe.add_patch(MplPoly(np.array([(x - 0.14, d.b.eaves - 0.1), (x + 0.14, d.b.eaves - 0.1),
                                        (x + 0.5, d.b.eaves + 0.42), (x + 0.2, d.b.eaves + 0.42)]),
                              closed=True, facecolor="0.80", edgecolor=d.ink, lw=0.7, zorder=7))


@register("kartusche", lod=3)
def _kartusche(d):
    if not ("Kartusche" in d.el and d.street):
        return
    d.axe.add_patch(matplotlib.patches.Ellipse((d.L / 2, d.b.pad + d.b.style.level_m * 1.35),
                                             1.5, 1.1, facecolor="0.86", edgecolor=d.ink,
                                             lw=0.9, zorder=6))


@register("klinkerband", lod=2)
def _klinkerband(d):
    if not ("Klinkerband" in d.el):
        return
    for lv in range(1, d.f.levels() + 1):
        zz = min(d.b.pad + lv * d.b.style.level_m, d.b.eaves)
        d.axe.add_patch(Rectangle((0, zz - 0.42), d.L, 0.30, facecolor="0.78",
                                edgecolor=d.ink, lw=0.5, zorder=3))


@register("loggia", lod=3)
def _loggia(d):
    if not ("Loggia" in d.el and d.f.levels() >= 3 and d.wall["bays"] >= 3 and d.street):
        return
    xl_ = (d.wall["bays"] // 2) * d.wall["bay_m"]
    wl = d.wall["bay_m"] * 0.9
    for lv in range(1, d.f.levels()):
        zz = d.b.pad + lv * d.b.style.level_m
        d.axe.add_patch(Rectangle((xl_ - wl / 2, zz + 0.15), wl, d.b.style.level_m - 0.6,
                                facecolor="0.35", edgecolor=d.ink, lw=0.9, zorder=5))
        d.axe.add_patch(Rectangle((xl_ - wl / 2, zz + 0.15), wl, 1.0, facecolor="0.80",
                                edgecolor=d.ink, lw=0.7, zorder=6))


@register("waschbeton", lod=2)
def _waschbeton(d):
    if not ("Waschbeton" in d.el):
        return
    for lv in range(d.f.levels() + 1):
        zz = min(d.b.pad + lv * d.b.style.level_m, d.b.eaves)
        d.axe.plot([0, d.L], [zz, zz], color="0.55", lw=0.5, zorder=3)
    for bay in range(d.wall["bays"] + 1):
        d.axe.plot([bay * d.wall["bay_m"]] * 2, [d.b.pad, d.b.eaves], color="0.55", lw=0.5, zorder=3)


@register("fensterband", lod=1)
def _fensterband(d):
    if not ("Fensterband" in d.el):
        return
    # a RIBBON WINDOW is one opening per storey and not a row of holes: the band runs
    # between the piers and the spandrel below it is the wall
    for lv in range(d.f.levels()):
        zz = d.b.pad + lv * d.b.style.level_m
        d.axe.add_patch(Rectangle((0.5, zz + d.b.style.level_m * 0.42), d.L - 1.0,
                                d.b.style.level_m * 0.40, facecolor="0.55",
                                edgecolor=d.ink, lw=0.8, zorder=4))
        for m in np.arange(0.5, d.L - 0.5, d.wall["bay_m"] / 2.0):
            d.axe.plot([m, m], [zz + d.b.style.level_m * 0.42,
                              zz + d.b.style.level_m * 0.82], color="white", lw=0.6, zorder=5)


@register("pfosten_riegel", lod=1)
def _pfosten_riegel(d):
    if not ("Pfosten-Riegel" in d.el):
        return
    for m in np.arange(0.0, d.L + 1e-6, d.wall["bay_m"] / 2.0):
        d.axe.plot([m, m], [d.b.pad, d.b.eaves], color=d.ink, lw=0.7, zorder=4)
    for lv in range(d.f.levels() + 1):
        zz = min(d.b.pad + lv * d.b.style.level_m, d.b.eaves)
        d.axe.plot([0, d.L], [zz, zz], color=d.ink, lw=0.7, zorder=4)


@register("franz_balkon", lod=3)
def _franz_balkon(d):
    if not ("franzoesischer Balkon" in d.el):
        return
    for (w, mid, up, ww, hh, kind) in d.f.openings():
        if w is not d.wall or kind not in ("window", "balcony-door") or up - d.b.pad < 2.0:
            continue
        for r in np.linspace(mid - ww / 2, mid + ww / 2, 7):
            d.axe.plot([r, r], [up, up + 1.0], color=d.ink, lw=0.5, zorder=6)
        d.axe.plot([mid - ww / 2, mid + ww / 2], [up + 1.0] * 2, color=d.ink, lw=0.9, zorder=6)


@register("attika", lod=1)
def _attika(d):
    if not ("Attika" in d.el and "Vorhangfassade" not in d.el):
        return
    d.axe.add_patch(Rectangle((-0.25, d.b.eaves), d.L + 0.5, 1.0, facecolor="0.88",
                            edgecolor=d.ink, lw=1.0, zorder=4))


@register("stahlfenster", lod=2)
def _stahlfenster(d):
    if not ("Stahlfenster" in d.el):
        return
    for (w, mid, up, ww, hh, kind) in d.f.openings():
        if w is not d.wall or kind != "window":
            continue
        for r in np.linspace(mid - ww / 2, mid + ww / 2, 4)[1:-1]:
            d.axe.plot([r, r], [up, up + hh], color="white", lw=0.5, zorder=5)
        for zz in np.linspace(up, up + hh, 4)[1:-1]:
            d.axe.plot([mid - ww / 2, mid + ww / 2], [zz, zz], color="white", lw=0.5, zorder=5)


@register("rampe", lod=3)
def _rampe(d):
    if not ("Rampe" in d.el and d.street):
        return
    xr = d.L * 0.5
    d.axe.add_patch(MplPoly(np.array([(xr - 3.0, d.b.pad - 1.2), (xr + 3.0, d.b.pad - 1.2),
                                    (xr + 3.0, d.b.pad + 1.1), (xr - 3.0, d.b.pad + 1.1)]),
                          closed=True, facecolor="0.80", edgecolor=d.ink, lw=0.9, zorder=5))


@register("werbeband", lod=1)
def _werbeband(d):
    if not ("Werbeband" in d.el):
        return
    zw = d.b.pad + min(3.2, d.b.style.level_m - 0.9) + 0.85
    d.axe.add_patch(Rectangle((0, zw), d.L, 0.75, facecolor="0.30", edgecolor=d.ink,
                            lw=0.9, zorder=5))


@register("rosette", lod=3)
def _rosette(d):
    if not ("Rosette" in d.el and d.street and d.b.ridge > d.b.eaves + 2.0):
        return
    d.axe.add_patch(matplotlib.patches.Circle((d.L / 2, d.b.eaves - d.H * 0.22), min(2.2, d.L * 0.11),
                                            facecolor="0.55", edgecolor=d.ink, lw=1.1, zorder=6))
    d.axe.add_patch(matplotlib.patches.Circle((d.L / 2, d.b.eaves - d.H * 0.22),
                                            min(2.2, d.L * 0.11) * 0.45, facecolor="white",
                                            edgecolor=d.ink, lw=0.7, zorder=7))


@register("tor", lod=2)
def _tor(d):
    if not ("Tor" in d.el and d.street):
        return
    xt = (d.wall["bays"] // 2) * d.wall["bay_m"] + d.wall["bay_m"] / 2
    wt = min(d.wall["bay_m"] * 0.9, 4.2)
    ht = min(d.b.style.level_m * 1.6, d.H * 0.7)
    d.axe.add_patch(Rectangle((xt - wt / 2, d.b.pad), wt, ht, facecolor="0.42",
                            edgecolor=d.ink, lw=1.2, zorder=5))
    d.axe.plot([xt, xt], [d.b.pad, d.b.pad + ht], color="white", lw=0.8, zorder=6)


@register("gauben", lod=3)
def _gauben(d):
    if not ((d.b.style.epoch in ("gruenderzeit", "jugendstil", "farm", "baroque", "interwar",
                                "siedlungshaus", "einfamilienhaus") and d.b.roof in ("gabled", "hipped", "mansard", "half-hipped", "gambrel") and d.faces_slope and d.rise > 2.2 and d.wall["bays"] >= 2)):
        return
    hg = min(1.55, d.rise * 0.42)
    zg = d.b.eaves + d.rise * 0.18
    for bay in range(0, d.wall["bays"], 2):
        x = (bay + 0.5) * d.wall["bay_m"]
        d.axe.add_patch(Rectangle((x - 0.72, zg), 1.44, hg, facecolor="0.90",
                                edgecolor=d.ink, lw=0.8))
        d.axe.add_patch(MplPoly(np.array([(x - 1.00, zg + hg), (x, zg + hg + 0.62),
                                        (x + 1.00, zg + hg)]), closed=True,
                              facecolor="0.84", edgecolor=d.ink, lw=0.8))
        d.axe.add_patch(Rectangle((x - 0.42, zg + 0.28), 0.84, hg - 0.55,
                                facecolor="0.55", edgecolor=d.ink, lw=0.5))


@register("vordach", lod=3, note="the canopy over a house door: the type's own front")
def _vordach(d):
    if not (d.b.style.epoch in ("siedlungshaus", "bungalow", "einfamilienhaus", "late20", "hall")
            and d.street and d.wall["bays"] >= 2):
        return
    x = d.wall["length"] * 0.5
    z = d.b.pad + min(2.35, d.b.style.level_m * 0.85)
    d.axe.add_patch(MplPoly(np.array([(x - 1.35, z), (x + 1.35, z), (x + 1.20, z + 0.16),
                                      (x - 1.20, z + 0.16)]), closed=True,
                            facecolor="0.86", edgecolor=d.ink, lw=0.9))
    for at in (x - 1.15, x + 1.15):
        # A RECTANGLE'S THIRD ARGUMENT IS ITS HEIGHT, not the level its top stands at. Passing
        # the absolute z drew a 102 m post and blew the elevation's own limits, so both panels
        # of every bungalow sheet came out as two hairlines (measured 2026-09-06, by looking).
        d.axe.add_patch(Rectangle((at - 0.05, d.b.pad), 0.10, z - d.b.pad, facecolor="0.80",
                                  edgecolor=d.ink, lw=0.6))


@register("carport", lod=3, note="a bungalow parks under a flat slab on two posts, not in a box")
def _carport(d):
    if not (d.b.style.epoch in ("bungalow", "einfamilienhaus") and not d.street
            and d.wall["length"] > 8.0):
        return
    z = d.b.pad + 2.30
    x0 = d.wall["length"] - 6.20
    d.axe.add_patch(MplPoly(np.array([(x0, z), (x0 + 5.80, z), (x0 + 5.80, z + 0.22),
                                      (x0, z + 0.22)]), closed=True,
                            facecolor="0.88", edgecolor=d.ink, lw=1.0))
    for at in (x0 + 0.25, x0 + 5.45):
        d.axe.add_patch(Rectangle((at - 0.07, d.b.pad), 0.14, z - d.b.pad, facecolor="0.80",
                                  edgecolor=d.ink, lw=0.7))


@register("klappladen", lod=3, note="the shutter beside every window of a Siedlungshaus")
def _klappladen(d):
    if not (d.b.style.epoch == "siedlungshaus" and d.rise > 0.5):
        return
    w, h = d.b.style.win_w, d.b.style.win_h
    for level in range(d.f.levels()):
        z = d.b.pad + level * d.b.style.level_m + d.b.style.sill_m
        if z + h > d.b.eaves:
            break
        for bay in range(d.wall["bays"]):
            x = (bay + 0.5) * d.wall["bay_m"]
            for side in (-1, +1):
                d.axe.add_patch(Rectangle((x + side * (w / 2 + 0.02) - (0.26 if side > 0 else 0),
                                           z), 0.26, h, facecolor="0.72", edgecolor=d.ink,
                                          lw=0.5))


@register("schornstein", lod=3)
def _schornstein(d):
    if not (d.b.roof != "flat" and d.heated and d.faces_slope and d.b.style.epoch in ( "gruenderzeit", "jugendstil", "farm", "interwar", "postwar", "baroque",
        "siedlungshaus", "einfamilienhaus", "bungalow")):
        return
    for frac in ((0.30, 0.78) if d.wall["length"] > 14.0 else (0.72,)):
        x = d.wall["length"] * frac
        d.axe.add_patch(Rectangle((x - 0.42, d.b.ridge - 0.4), 0.84,
                                max(1.3, d.rise * 0.55) + 0.4,
                                facecolor="0.86", edgecolor=d.ink, lw=0.9))
        d.axe.add_patch(Rectangle((x - 0.55, d.b.ridge + max(1.3, d.rise * 0.55) - 0.22),
                                1.10, 0.22, facecolor="0.78", edgecolor=d.ink, lw=0.8))


