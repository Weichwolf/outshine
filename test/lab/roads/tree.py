"""A TREE IS ITS BRANCHING, and the crown's silhouette follows from it.

Three smooth ellipsoids on a stick is a tree from 2005: the silhouette is regular, every tree in a
row is the same shape, and the eye reads "placeholder" instantly (rendered a street of them and
looked at, 2026-09-06). A real tree's outline is irregular BECAUSE its branches are, so the crown
is not modelled -- it is what the branching leaves behind.

    THE STRUCTURE IS RECURSIVE, which is what this engine's generators are for: a trunk splits
    into two or three, each child is shorter and thinner by the species' own ratio, turns by its
    own angle, and the recursion stops when a branch is thinner than a twig
    THE TAPER IS THE SPECIES'. Leonardo's rule -- the cross section is conserved across a fork --
    gives r_child = r_parent * n^(-1/2.49) for n children, and 2.49 is the exponent measured on
    real trees (Bentley et al., Nature 2013). A tree that ignores it is a tree with a trunk like
    a broom handle
    THE LEAVES SIT AT THE TIPS, one cluster per terminal branch, so the canopy's outline is the
    branch tips' hull and not an ellipsoid
    THE LOD IS THE RECURSION DEPTH: stop early and the same seed gives the same tree, coarser

Species are a registry: a plane, a lime, a birch, a pine, a poplar differ in the fork angle, the
child count, the length ratio and the leaf cluster's own shape, and those five numbers are most of
what tells them apart at fifty metres.
"""
import math

import numpy as np

SPECIES = {}


def register(name, **kw):
    SPECIES[name] = dict(name=name, **kw)
    return SPECIES[name]


# fork: how far a child turns off its parent. spread: how far children turn from each other.
# ratio: a child's length as a share of its parent's. children: how many at a fork.
# leaf: the terminal cluster's radius as a share of the terminal branch's length.
register("platane", fork=32.0, spread=118.0, ratio=0.76, children=3, depth=6,
         trunk=0.62, leaf=0.90, lean=0.10, note="a London plane, the street tree of Europe")
register("linde", fork=28.0, spread=132.0, ratio=0.78, children=3, depth=6,
         trunk=0.55, leaf=1.00, lean=0.08, note="a lime: dense, rounded, the avenue's own")
register("eiche", fork=42.0, spread=104.0, ratio=0.72, children=3, depth=6,
         trunk=0.72, leaf=0.85, lean=0.16, note="an oak: heavy limbs, a broad irregular crown")
register("birke", fork=22.0, spread=140.0, ratio=0.80, children=2, depth=7,
         trunk=0.34, leaf=0.62, lean=0.22, note="a birch: slender, weeping, an open crown")
register("pappel", fork=14.0, spread=96.0, ratio=0.82, children=2, depth=7,
         trunk=0.42, leaf=0.55, lean=0.04, note="a poplar: upright, narrow, a windbreak's own")
register("kiefer", fork=48.0, spread=112.0, ratio=0.66, children=2, depth=5,
         trunk=0.58, leaf=1.25, lean=0.12, note="a pine: a bare stem and a flat crown on top")
register("fichte", fork=62.0, spread=100.0, ratio=0.70, children=4, depth=5,
         trunk=0.50, leaf=0.75, lean=0.02,
         note="a spruce: whorled, conical, and it keeps its lower limbs")

# WHAT OSM SAYS, where it says anything. `species` and `genus` are tagged on a minority of trees;
# `leaf_type` is tagged more often and separates a needle from a broadleaf, which is most of the
# silhouette. Everything else falls to the region's own common street tree.
BY_GENUS = {"platanus": "platane", "tilia": "linde", "quercus": "eiche", "betula": "birke",
            "populus": "pappel", "pinus": "kiefer", "picea": "fichte", "acer": "linde",
            "fraxinus": "linde", "aesculus": "platane", "salix": "birke", "fagus": "eiche"}


def species_of(tags, seed=0):
    """WHICH TREE. A genus where the surveyor gave one, a needle-or-broadleaf where they gave
    only that, and otherwise the seed picks from what a street is planted with."""
    tags = tags or {}
    for key in ("genus", "species", "genus:en", "taxon"):
        told = str(tags.get(key, "")).strip().lower().split()
        if told and told[0] in BY_GENUS:
            return SPECIES[BY_GENUS[told[0]]]
    if tags.get("leaf_type") == "needleleaved":
        return SPECIES[("kiefer", "fichte")[seed % 2]]
    return SPECIES[("platane", "linde", "eiche", "birke")[seed % 4]]


def _rand(seed):
    """A deterministic stream: the same tree every time, which is compulsory here."""
    state = (seed * 1103515245 + 12345) & 0x7FFFFFFF
    while True:
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        yield (state >> 8) / 8388608.0 - 1.0     # in -1 .. 1


def _frame(direction):
    d = np.asarray(direction, dtype=float)
    d /= float(np.linalg.norm(d)) or 1.0
    up = np.array([0.0, 0.0, 1.0])
    if abs(float(np.dot(d, up))) > 0.95:
        up = np.array([1.0, 0.0, 0.0])
    u = np.cross(d, up)
    u /= float(np.linalg.norm(u)) or 1.0
    return d, u, np.cross(d, u)


def _tube(a, b, r0, r1, sides=6):
    """A tapered tube from a to b. Six sides: a branch is read by its silhouette and its taper,
    never by its roundness, and a street holds hundreds of these."""
    d, u, v = _frame(np.asarray(b) - np.asarray(a))
    verts, tris = [], []
    for k in range(sides):
        ang = 2 * math.pi * k / sides
        off = u * math.cos(ang) + v * math.sin(ang)
        verts += [tuple(np.asarray(a) + off * r0), tuple(np.asarray(b) + off * r1)]
    for k in range(sides):
        i, j = 2 * k, 2 * ((k + 1) % sides)
        tris += [(i, j, j + 1), (i, j + 1, i + 1)]
    return verts, tris


def _cluster(centre, radius, seed, cards=7):
    """A LEAF SPRAY: alpha-masked CARDS, which is what every one of the references uses and what
    `research/world.md` already says ("leaves as alpha-tested").

    Foliage modelled as geometry balls cannot read as foliage at any polygon count: a canopy of
    them is a bunch of faceted spheres and the eye names them instantly (rendered seven species
    twice and looked at, 2026-09-06). A spray is a handful of crossed quads at random attitudes,
    each carrying a leaf mask -- the SILHOUETTE then comes from the mask and the branching, and
    the triangle count falls by an order of magnitude at the same read.

    The material is `leaf`: alphaMode MASK, doubleSided -- glTF's own way of saying this."""
    rng = _rand(seed)
    verts, tris = [], []
    for k in range(cards):
        # a random attitude, biased outward from the branch so a spray fans rather than crosses
        dx, dy, dz = next(rng), next(rng), next(rng) * 0.7
        d = np.array([dx, dy, dz], dtype=float)
        n = float(np.linalg.norm(d))
        d = d / n if n > 1e-6 else np.array([1.0, 0.0, 0.0])
        _, u, v = _frame(d)
        at = np.asarray(centre, dtype=float) + d * radius * (0.25 + 0.55 * abs(next(rng)))
        w = radius * (0.85 + 0.45 * abs(next(rng)))
        h = w * (0.66 + 0.4 * abs(next(rng)))
        base = len(verts)
        verts += [tuple(at - u * w - v * h), tuple(at + u * w - v * h),
                  tuple(at + u * w + v * h), tuple(at - u * w + v * h)]
        tris += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    return verts, tris


def build(x, y, z, height=None, tags=None, seed=0, lod=3):
    """ONE TREE: (role, vertices, triangles) per part. `lod` is the recursion depth kept, which is
    what makes the ladder recursive -- the same seed at a lower rung is the same tree, coarser."""
    kind = species_of(tags, seed)
    rng = _rand(seed * 2654435761 & 0x7FFFFFFF)
    high = float(height or (9.0 + 5.0 * next(rng)))
    depth = max(1, min(int(kind["depth"]), 2 + int(lod) * 2))
    bark, leaf = [], []

    def limb(a, direction, length, radius, level):
        b = np.asarray(a) + np.asarray(direction) * length
        bark.append(_tube(a, b, radius, radius * kind["ratio"] ** 0.5))
        if level >= depth or length < 0.35:
            # THE CANOPY IS A MASS, NOT A ROW OF BALLS. One cluster per tip, sized to the tip's
            # own length, left each cluster separately readable and the crown looked like balls
            # on sticks (rendered seven species and looked at, 2026-09-06). Three smaller ones
            # along the last branch, overlapping, merge into one outline -- and the outline is
            # still the BRANCHING's, which is the whole point.
            r = max(0.34, length * kind["leaf"] * 0.70)
            for u in (0.35, 0.62, 0.86, 1.06):
                at = np.asarray(a) + (np.asarray(b) - np.asarray(a)) * u
                leaf.append(_cluster(tuple(at), r, seed + level * 977 + len(leaf) * 31))
            return
        n = kind["children"]
        # LEONARDO'S RULE: the cross section is conserved across a fork, with the measured
        # exponent 2.49 (Bentley et al., Nature 2013), so r_child = r_parent * n^(-1/2.49)
        child_r = radius * n ** (-1.0 / 2.49)
        d, u, v = _frame(direction)
        for k in range(n):
            turn = math.radians(kind["fork"] * (0.75 + 0.5 * abs(next(rng))))
            about = math.radians(kind["spread"] * k + 40.0 * next(rng))
            side = u * math.cos(about) + v * math.sin(about)
            nd = d * math.cos(turn) + side * math.sin(turn)
            nd = nd + np.array([0.0, 0.0, -kind["lean"] * math.sin(turn)])
            nd /= float(np.linalg.norm(nd)) or 1.0
            limb(b, nd, length * kind["ratio"] * (0.88 + 0.24 * abs(next(rng))), child_r,
                 level + 1)

    stem = high * kind["trunk"]
    limb((x, y, z), (0.04 * next(rng), 0.04 * next(rng), 1.0), stem,
         max(0.09, high * 0.030), 1)
    out = []
    for (verts, tris) in bark:
        out.append(("bark", verts, tris))
    for (verts, tris) in leaf:
        out.append(("leaf", verts, tris))
    return tuple(out)
