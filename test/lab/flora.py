"""TREES FROM THE SAME DECLARATION THE ENGINE READS -- `src/assets/world/species/*.json`.

CLAUDE.md's second question before writing anything: does this already exist here, unreachable?
It did. `src/generators/flora/` holds a declarative tree generator with a 23-field `Growth`
record, a 24-field `Leaf` record and a `Shading` record, and 31 species declared as JSON with
every number carrying its origin. A second generator with a different model was written in this
lab before that grep was run, and it was the worse of the two -- so it is gone, and this reads the
SAME files and implements the SAME growth, which is what makes the lab an oracle for the C++
rather than a rival to it.

    THE GROWTH IS A QUEUE. A shoot walks in `step_len` steps, tapering by `taper`, wandering by
    `wander` degrees and biased upward by `leader_bias` (a leader) or `branch_up_bias` (a
    branch). At each step it may spawn a lateral with probability `branch_chance`, or -- where
    `whorl_count` is set -- a whorl of them every `whorl_spacing` steps, which is what makes a
    conifer a conifer
    THE CROWN IS AN ENVELOPE, not a shape: `Reach(t) = t^A (1-t)^B` normalised at its own peak,
    with (A, B) per envelope -- conical (0, 1), columnar (0.1, 0.1), ovoid (0.55, 0.95), domed
    (0.30, 0.55), vase (1.20, 0.25), weeping (0.15, 0.30), umbrella (2.50, 0.30), flat-topped
    (1.60, 0.10). A shoot that escapes it is bent back, and beyond a limit it stops
    LEAVES ARE CARDS emitted where the radius falls under `twig_radius * foliage_factor`
    THE LOD IS `max_order`: the same seed at a lower order is the same tree, coarser

The lab's job here is what the lab's job always is: hold the answer the C++ is measured against,
and carry the species the engine does not have yet.
"""
import json
import math
import pathlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
DECLARED = ROOT / "src" / "assets" / "world" / "species"
LAB_SPECIES = pathlib.Path(__file__).resolve().parent / "species"

# (A, B) per envelope, from GrowthForm.cpp's own table -- ONE source, and the lab reads the same
# shape the engine grows.
ENVELOPES = {"free": None, "conical": (0.00, 1.00), "columnar": (0.10, 0.10),
             "ovoid": (0.55, 0.95), "domed": (0.30, 0.55), "vase": (1.20, 0.25),
             "weeping": (0.15, 0.30), "umbrella": (2.50, 0.30), "flat_topped": (1.60, 0.10),
             "cut": None}
FLOOR = 0.05
GOLDEN = math.pi * (3.0 - math.sqrt(5.0))


def reach(envelope, t):
    """The crown's half width at height fraction t, normalised so its own peak is 1."""
    ab = ENVELOPES.get(envelope)
    if ab is None:
        return 1.0
    if t > 1.0:
        return 0.0
    a, b = ab
    t = max(t, FLOOR)
    peak = a / (a + b) if (a + b) > 0 else 0.5
    denom = (peak ** a) * ((1.0 - peak) ** b)
    return 1.0 if denom <= 0.0 else (t ** a) * ((1.0 - t) ** b) / denom


UNSAID = dict(seed=1, trunk_sides=10, base_radius=0.07, step_len=0.16, trunk_steps=26,
              taper=0.955, min_radius=0.005, twig_radius=0.013, branch_chance=0.85,
              max_order=3, terminal_fork=1, branch_angle=55.0, branch_angle_var=12.0,
              order_len=0.62, order_radius=0.52, wander=7.0, leader_bias=0.18,
              branch_up_bias=0.12, whorl_count=0, whorl_spacing=4, foliage_factor=5.5,
              foliage_on_leader=0, shade_prune=0.0, height_m=20.0, spread_m=10.0,
              bole_frac=0.35, crown="free", form="single_stem_tree",
              leaf_card_w=0.075, leaf_card_h=0.10, leaf_cards=3, lai=4.0,
              bark_r=0.40, bark_g=0.31, bark_b=0.23, leaf_kind="broad")


def load(where=None):
    """Every declared species, the engine's own and the lab's, by name."""
    out = {}
    for holds in (DECLARED, LAB_SPECIES) if where is None else (pathlib.Path(where),):
        if not holds.is_dir():
            continue
        for f in sorted(holds.glob("*.json")):
            got = dict(UNSAID)
            got.update({k: v for k, v in json.loads(f.read_text()).items()
                        if not k.endswith("_origin")})
            got["file"] = str(f)
            out[got.get("name", f.stem)] = got
    return out


class _Rng:
    """The stream, deterministic per seed -- the same tree, every time, everywhere."""

    def __init__(self, seed):
        self.s = (int(seed) * 1103515245 + 12345) & 0x7FFFFFFF

    def unit(self):
        self.s = (self.s * 1103515245 + 12345) & 0x7FFFFFFF
        return (self.s >> 8) / 8388608.0

    def signed(self):
        return self.unit() * 2.0 - 1.0


def _frame(d):
    d = np.asarray(d, dtype=float)
    n = float(np.linalg.norm(d))
    d = d / n if n > 1e-9 else np.array([0.0, 0.0, 1.0])
    up = np.array([0.0, 0.0, 1.0])
    if abs(float(np.dot(d, up))) > 0.95:
        up = np.array([1.0, 0.0, 0.0])
    u = np.cross(d, up)
    u /= float(np.linalg.norm(u)) or 1.0
    return d, u, np.cross(d, u)


class Shoot:
    __slots__ = ("pos", "dir", "radius", "step", "steps", "order", "bare", "roll", "leader")

    def __init__(self, pos, direction, radius, step, steps, order, bare, roll, leader):
        self.pos, self.dir, self.radius = np.asarray(pos, dtype=float), np.asarray(direction, dtype=float), radius
        self.step, self.steps, self.order = step, steps, order
        self.bare, self.roll, self.leader = bare, roll, leader


def grow(species, seed=None, lod=None):
    """ONE TREE from a declared species: (nodes, cards) where a node is (a, b, r0, r1) and a card
    is (centre, radius). The mesher below turns them into geometry; keeping them apart is what
    lets a rung change the mesh without regrowing the tree."""
    g = dict(species)
    rng = _Rng(seed if seed is not None else g["seed"])
    order_most = int(g["max_order"] if lod is None else min(int(g["max_order"]), max(0, int(lod))))
    envelope = g.get("crown", "free")
    height = float(g["height_m"])
    half = float(g["spread_m"]) * 0.5
    bole = float(g.get("bole_frac", 0.35))
    leaf_at = float(g["twig_radius"]) * float(g["foliage_factor"])
    bare_steps = int(round(bole * float(g["trunk_steps"])))
    nodes, cards = [], []

    # THE ENVELOPE IS IN THE TREE'S OWN UNITS while it grows, and the whole thing is scaled to
    # `height_m` at the end -- which is how the C++ does it (`NormalizeToUnitHeight`).
    grow_h = float(g["trunk_steps"]) * float(g["step_len"])
    crown_base = bole * grow_h
    crown_span = max(1e-3, grow_h - crown_base)
    crown_half = half * (grow_h / max(height, 1e-3))

    def escape(p):
        if envelope in ("free", "cut") or crown_half <= 0.0:
            return 0.0
        want = crown_half * reach(envelope, (p[2] - crown_base) / crown_span)
        if want <= 1e-6:
            return 2.0
        return float(math.hypot(p[0], p[1])) / want

    queue = [Shoot((0.0, 0.0, 0.0), (0.0, 0.0, 1.0), float(g["base_radius"]),
                   float(g["step_len"]), int(g["trunk_steps"]), 0, bare_steps, 0.0, 0)]
    at = 0
    while at < len(queue) and len(nodes) < 60000:
        t = queue[at]
        at += 1
        for s in range(t.steps):
            old = t.pos.copy()
            d, u, v = _frame(t.dir)
            wr = math.radians(float(g["wander"]))
            ub = float(g["leader_bias"]) if t.order == 0 else float(g["branch_up_bias"])
            want = d + u * (rng.signed() * wr) + v * (rng.signed() * wr) + np.array([0.0, 0.0, ub])
            n = float(np.linalg.norm(want))
            t.dir = want / n if n > 1e-9 else d
            t.pos = t.pos + t.dir * t.step
            r0, t.radius = t.radius, t.radius * float(g["taper"])
            nodes.append((tuple(old), tuple(t.pos), r0, t.radius))
            out = escape(t.pos)
            if out > 1.0:
                pull = min(1.0, (out - 1.0) / 0.25) * 0.35
                inward = -np.array([t.pos[0], t.pos[1], 0.0])
                m = float(np.linalg.norm(inward))
                if m > 1e-9:
                    t.dir = t.dir + inward / m * pull
                    t.dir /= float(np.linalg.norm(t.dir)) or 1.0
                if t.order > 0 and out > 1.6:
                    break
            leader_ok = (t.order != 0) or (s >= int(bole * t.steps))
            if leader_ok and t.radius < leaf_at and (t.order >= 1 or int(g["foliage_on_leader"])):
                cards.append((tuple(t.pos), t.radius))
            if t.bare > 0:
                t.bare -= 1
            elif t.order < order_most:
                whorl = int(g["whorl_count"])
                if whorl > 0 and t.order == 0:
                    if (s - bare_steps) % max(1, int(g["whorl_spacing"])) == 0:
                        for w in range(whorl):
                            _spawn(queue, t, g, rng, 2 * math.pi * w / whorl + rng.signed() * 0.2,
                                   escape)
                elif rng.unit() < float(g["branch_chance"]):
                    t.roll += GOLDEN + rng.signed() * 0.35
                    _spawn(queue, t, g, rng, t.roll, escape)
            if t.radius < float(g["min_radius"]):
                break
    # THE TREE IS GROWN IN ITS OWN UNITS AND SCALED TO ITS DECLARED HEIGHT, exactly as the engine
    # does: `NormalizeToUnitHeight`. Without it `step_len` and `height_m` are two statements of
    # the same thing and they disagree.
    top = max((max(a[2], b[2]) for (a, b, _, _) in nodes), default=1.0) or 1.0
    k = height / top
    nodes = [((a[0] * k, a[1] * k, a[2] * k), (b[0] * k, b[1] * k, b[2] * k), r0 * k, r1 * k)
             for (a, b, r0, r1) in nodes]
    cards = [((p[0] * k, p[1] * k, p[2] * k), r * k) for (p, r) in cards]
    return nodes, cards


def _spawn(queue, parent, g, rng, roll, escape):
    if parent.order + 1 > int(g["max_order"]):
        return
    d, u, v = _frame(parent.dir)
    turn = math.radians(float(g["branch_angle"]) + rng.signed() * float(g["branch_angle_var"]))
    side = u * math.cos(roll) + v * math.sin(roll)
    direction = d * math.cos(turn) + side * math.sin(turn)
    n = float(np.linalg.norm(direction))
    direction = direction / n if n > 1e-9 else d
    radius = parent.radius * float(g["order_radius"])
    if radius <= float(g["min_radius"]):
        return
    # SHADE PRUNE: a branch turning INWARD on a mature crown is a branch the canopy has shaded
    # out, and a tree that keeps them is a tree with a solid middle
    if float(g["shade_prune"]) > 0.0 and parent.order >= 2:
        outward = np.array([parent.pos[0], parent.pos[1], 0.0])
        m = float(np.linalg.norm(outward))
        if m > 1e-6 and float(np.dot(direction[:2], outward[:2] / m)) < 0.15:
            if rng.unit() < float(g["shade_prune"]):
                return
    steps = max(2, int(parent.steps * float(g["order_len"])))
    queue.append(Shoot(parent.pos, direction, radius, parent.step * float(g["order_len"]),
                       steps, parent.order + 1, 1, roll, parent.leader))


# WHICH SPECIES, from what OSM says and what a region plants. `genus` and `species` are tagged on
# a minority of trees; `leaf_type` separates a needle from a broadleaf and is tagged far more
# often, and that separation is most of a silhouette.
BY_GENUS = {"quercus": "oak", "fagus": "beech", "betula": "birch", "pinus": "pine",
            "picea": "spruce", "abies": "fir", "populus": "poplar", "salix": "willow",
            "tilia": "lime", "acer": "sycamore", "fraxinus": "ash", "ulmus": "elm",
            "carpinus": "hornbeam", "corylus": "hazel", "sorbus": "rowan",
            "aesculus": "chestnut", "castanea": "chestnut", "crataegus": "hawthorn",
            "prunus": "blackthorn", "sambucus": "elder", "cornus": "dogwood",
            "ligustrum": "privet", "buxus": "box", "euonymus": "spindle",
            "viburnum": "guelder_rose", "rosa": "dog_rose"}


def pick_species(declared, tags, seed=0):
    """The species this tree IS. A genus where the surveyor gave one, a needle-or-broadleaf where
    they gave only that, and otherwise a draw from what the street is planted with."""
    tags = tags or {}
    for key in ("genus", "species", "taxon", "genus:en"):
        told = str(tags.get(key, "")).strip().lower().split()
        if told and told[0] in BY_GENUS and BY_GENUS[told[0]] in declared:
            return BY_GENUS[told[0]]
    if tags.get("leaf_type") == "needleleaved":
        pool = [n for n in ("pine", "spruce", "fir") if n in declared]
    else:
        pool = [n for n in ("lime", "sycamore", "oak", "beech", "birch", "hornbeam", "ash")
                if n in declared]
    pool = pool or sorted(declared)
    return pool[seed % len(pool)]


def mesh(nodes, cards, species, at=(0.0, 0.0, 0.0), sides=5):
    """THE TREE AS GEOMETRY: tapered tubes for the wood, alpha-masked cards for the leaves, sized
    from the species' own `leaf_card_w`, `leaf_card_h` and `leaf_cards`."""
    ox, oy, oz = at
    bark_v, bark_t = [], []
    for (a, b, r0, r1) in nodes:
        d = np.asarray(b) - np.asarray(a)
        n = float(np.linalg.norm(d))
        if n < 1e-6:
            continue
        _, u, v = _frame(d / n)
        base = len(bark_v)
        for k in range(sides):
            ang = 2 * math.pi * k / sides
            off = u * math.cos(ang) + v * math.sin(ang)
            bark_v += [tuple(np.asarray(a) + off * max(r0, 0.004) + (ox, oy, oz)),
                       tuple(np.asarray(b) + off * max(r1, 0.003) + (ox, oy, oz))]
        for k in range(sides):
            i, j = base + 2 * k, base + 2 * ((k + 1) % sides)
            bark_t += [(i, j, j + 1), (i, j + 1, i + 1)]
    cw = float(species.get("leaf_card_w", 0.075))
    ch = float(species.get("leaf_card_h", 0.10))
    per = max(1, int(species.get("leaf_cards", 3)))
    rng = _Rng(int(species.get("seed", 1)) * 7919)
    leaf_v, leaf_t = [], []
    for (p, r) in cards:
        for _ in range(per):
            d = np.array([rng.signed(), rng.signed(), rng.signed() * 0.7])
            n = float(np.linalg.norm(d))
            d = d / n if n > 1e-6 else np.array([1.0, 0.0, 0.0])
            _, u, v = _frame(d)
            c = np.asarray(p) + d * max(r, 0.01) * 2.0 + (ox, oy, oz)
            w, h = cw * (0.8 + 0.5 * rng.unit()), ch * (0.8 + 0.5 * rng.unit())
            base = len(leaf_v)
            leaf_v += [tuple(c - u * w - v * h), tuple(c + u * w - v * h),
                       tuple(c + u * w + v * h), tuple(c - u * w + v * h)]
            leaf_t += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
    return (("bark", bark_v, bark_t), ("leaf", leaf_v, leaf_t))
