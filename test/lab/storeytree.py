"""STOREYS PER GROUND SQUARE: the tree an OSM tile builds in milliseconds, and the unit a
generator is asked for work in.

WHY A STOREY AND NOT A METRE. A body is not seen or unseen as a whole. Walking a street you see
the upper floors of what stands behind and none of their ground floors -- and the ground floor is
the most expensive thing a facade generator makes: a shopfront, a door, steps, a kerb crossing.
Culling per BODY therefore builds all of that for a body whose ground floor is behind a wall.
The storey is the right grain for the same reason it is the right grain for the generator:
`elements/storeys.py` already builds sockel, beletage, regel and dach as separate things, so a
node of this tree maps to one call the generator can make on its own.

WHY A GROUND SQUARE. OSM arrives in tiles and a tile is a square, so the plan is already a grid;
a quadtree over it costs nothing to build and its nodes are the tile's own subdivisions. A square
holds a COLUMN of storeys -- which is where the word octree earns itself: the third split is not
geometric but the building's own floor levels, so the leaves are storeys and not cubes of air.

WHAT IT IS BUILT FROM, and it is the whole point: footprints, heights and levels. No mesh, no
body, no facade. Everything a generator would compute comes AFTER this tree has said what to
compute.
"""
import math

import numpy as np

SQUARE_M = 16.0           # [SET] the ground square: `ClassField`'s own fine cell, and about the
                          # width of one building, so a square holds a handful of columns
ROOF = -1                 # the storey index a roof carries
# A CULLER MUST SEE WHAT A GENERATOR WILL ADD. The mass is the footprint, but a cornice, an
# eaves and a balcony stand off it -- CLAUDE.md makes relief depth a number a generator owes,
# and this is the second thing that number is for: a body whose mass is hidden but whose balcony
# pokes past the wall in front is still seen. Every bound below is grown by it.
RELIEF_M = 1.20
TERRAIN_OCCLUDES = True   # a hill hides a quarter -- but only where the square's own slope is known


class Column:
    """One ground square's worth of storeys, ordered from the ground up."""

    __slots__ = ("x0", "y0", "x1", "y1", "rows", "ground", "boxes")

    def __init__(self, x0, y0, x1, y1, ground=-1e30):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.rows = []                              # (body, storey, z_low, z_high)
        self.boxes = {}                             # body -> (its own bounds, its ridge)
        # THE GROUND IS AN OCCLUDER AND USUALLY THE BIGGEST ONE. A hill between the eye and a
        # town hides the whole of it, and a building in a dip hides less than its height says.
        # A square therefore carries its own terrain height, and the square's top is the higher
        # of that and whatever stands on it.
        self.ground = float(ground)

    @property
    def top(self):
        return max(max((r[3] for r in self.rows), default=-1e30), self.ground)


class Node:
    __slots__ = ("x0", "y0", "x1", "y1", "top", "kids", "column")

    def __init__(self, x0, y0, x1, y1):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.top = -1e30
        self.kids = []
        self.column = None

    def near_far(self, ex, ey):
        dxn = max(self.x0 - ex, 0.0, ex - self.x1)
        dyn = max(self.y0 - ey, 0.0, ey - self.y1)
        dxf = max(abs(self.x0 - ex), abs(self.x1 - ex))
        dyf = max(abs(self.y0 - ey), abs(self.y1 - ey))
        return math.hypot(dxn, dyn), math.hypot(dxf, dyf)


class StoreyTree:
    """THE TILE'S OWN TREE. Built from footprints, levels, heights and the TERRAIN."""

    def __init__(self, bodies, square_m=SQUARE_M, ground_at=None, extent=None):
        self.square = float(square_m)
        self.bodies = bodies
        self.columns = {}
        self.nodes = 0
        self.rows = 0
        self.ground_at = ground_at
        if not bodies and extent is None:
            self.root = Node(0.0, 0.0, 0.0, 0.0)
            return
        import shapely
        from shapely.strtree import STRtree
        if bodies:
            bounds = np.array([b.poly.bounds for b in bodies])
            x0, y0 = bounds[:, 0].min(), bounds[:, 1].min()
            x1, y1 = bounds[:, 2].max(), bounds[:, 3].max()
        else:
            x0, y0, x1, y1 = extent
        if extent is not None:
            x0, y0 = min(x0, extent[0]), min(y0, extent[1])
            x1, y1 = max(x1, extent[2]), max(y1, extent[3])
        nx = max(1, int(math.ceil((x1 - x0) / self.square)))
        ny = max(1, int(math.ceil((y1 - y0) / self.square)))
        # ONE STRtree QUERY FOR THE WHOLE GRID: a square's bodies are those whose footprint meets
        # it, and shapely answers that for every square at once.
        cells, boxes = [], []
        for j in range(ny):
            for i in range(nx):
                ax, ay = x0 + i * self.square, y0 + j * self.square
                cells.append((i, j, ax, ay))
                boxes.append(shapely.box(ax, ay, ax + self.square, ay + self.square))
        # EVERY SQUARE EXISTS, because the terrain is everywhere even where no building is
        if ground_at is not None:
            for (i, j, ax, ay) in cells:
                self.columns[(i, j)] = Column(ax, ay, ax + self.square, ay + self.square,
                                              ground_at(ax + self.square * 0.5,
                                                        ay + self.square * 0.5))
        if not bodies:
            self.root = self._split(sorted(self.columns), x0, y0, x0 + nx * self.square,
                                    y0 + ny * self.square, 0)
            return
        tree = STRtree([b.poly for b in bodies])
        pairs = tree.query(np.asarray(boxes, dtype=object), predicate="intersects")
        for k in range(pairs.shape[1]):
            at, body = int(pairs[0, k]), int(pairs[1, k])
            i, j, ax, ay = cells[at]
            col = self.columns.get((i, j))
            if col is None:
                col = self.columns[(i, j)] = Column(ax, ay, ax + self.square, ay + self.square)
            b = bodies[body]
            level = float(b.style.level_m)
            n = max(1, int(b.levels) if getattr(b, "levels", 0) else
                    max(1, int(round((b.eaves - b.pad) / max(level, 1e-3)))))
            step = (b.eaves - b.pad) / n
            for s in range(n):
                col.rows.append((body, s, b.pad + s * step, b.pad + (s + 1) * step))
            col.rows.append((body, ROOF, b.eaves, b.ridge + RELIEF_M))
            box = b.poly.bounds
            col.boxes[body] = ((box[0] - RELIEF_M, box[1] - RELIEF_M,
                                box[2] + RELIEF_M, box[3] + RELIEF_M), float(b.ridge))
            self.rows += n + 1
        self.root = self._split(sorted(self.columns), x0, y0, x0 + nx * self.square,
                                y0 + ny * self.square, 0)

    def _split(self, keys, x0, y0, x1, y1, depth):
        self.nodes += 1
        node = Node(x0, y0, x1, y1)
        node.top = max((self.columns[k].top for k in keys), default=-1e30)
        if len(keys) <= 1 or depth > 16:
            node.column = self.columns[keys[0]] if keys else None
            return node
        mx, my = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
        buckets = ([], [], [], [])
        for k in keys:
            c = self.columns[k]
            buckets[(1 if c.x0 >= mx else 0) + (2 if c.y0 >= my else 0)].append(k)
        if max(len(b) for b in buckets) == len(keys):
            node.column = self.columns[keys[0]]
            return node
        for q, got in enumerate(buckets):
            if not got:
                continue
            qx0 = x0 if q % 2 == 0 else mx
            qy0 = y0 if q < 2 else my
            node.kids.append(self._split(got, qx0, qy0, qx0 + (mx - x0), qy0 + (my - y0),
                                         depth + 1))
        return node


def visible(tree, eye_xy, eye_z, bearing_deg, fov_deg=55.0, width=1280):
    """WHICH (body, storey) ARE SEEN, front to back, rejecting whole subtrees.

    The bounds are the same two the plan tree uses and for the same reason: a node is rejected
    only by the GREATEST angle anything under it could reach, and only something that rises ABOVE
    THE EYE occludes -- below it the ground between the buildings is seen under their tops.
    Returns (set of (body, storey), node tests, row tests)."""
    ex, ey = eye_xy
    fov = math.radians(fov_deg)
    bearing = math.radians(bearing_deg)
    up = np.full(width, -1e30)
    seen = set()
    tested = rows = 0

    def columns(n):
        lo, hi = 1e30, -1e30
        for (x, y) in ((n.x0, n.y0), (n.x1, n.y0), (n.x1, n.y1), (n.x0, n.y1)):
            a = math.atan2(x - ex, y - ey) - bearing
            a = (a + math.pi) % (2 * math.pi) - math.pi
            lo, hi = min(lo, a), max(hi, a)
        if hi - lo > math.pi:
            return 0, width
        c0 = int(math.floor((lo / fov + 0.5) * width))
        c1 = int(math.ceil((hi / fov + 0.5) * width))
        if c1 <= 0 or c0 >= width:
            return None
        return max(0, c0), min(width, c1)

    stack = [(tree.root.near_far(ex, ey)[0], tree.root)]
    while stack:
        stack.sort(key=lambda r: -r[0])
        _, node = stack.pop()
        tested += 1
        span = columns(node)
        if span is None:
            continue
        near, far = node.near_far(ex, ey)
        rise = node.top - eye_z
        if rise / max(near if rise > 0.0 else far, 1e-6) <= up[span[0]:span[1]].min():
            continue
        if node.kids:
            for kid in node.kids:
                stack.append((kid.near_far(ex, ey)[0], kid))
            continue
        col = node.column
        if col is None:
            continue
        g = col.ground - eye_z
        if TERRAIN_OCCLUDES and g > 0.0:
            np.maximum(up[span[0]:span[1]], g / max(far, 1e-6), out=up[span[0]:span[1]])
        # A STOREY DOES NOT OCCLUDE. It is a BAND on a facade, not a facade: what covers what is
        # behind it is the whole BODY, from its ridge down to its pad, and a storey is only the
        # grain at which the answer is asked. Letting each storey occlude as if its own top were
        # a roof culled 7 of the 9 bodies a march sees from a street (measured 2026-09-07) --
        # which is a culler erring in the one direction it may not.
        for (body, storey, lo, hi) in sorted(col.rows, key=lambda r: -r[3]):
            rows += 1
            r = hi - eye_z
            if r / max(near if r > 0.0 else far, 1e-6) <= up[span[0]:span[1]].min():
                continue
            seen.add((body, storey))
        # AND THE OCCLUDER IS THE BODY, NOT THE SQUARE. A 16 m square spans a wide arc and its
        # tallest building may stand in one corner of it; occluding over the square's whole span
        # culled 7 of the 9 bodies a march sees from a street. Each body raises the horizon over
        # ITS OWN arc, by its own furthest corner, which is the rule the plan tree is measured
        # correct with.
        for body in {r[0] for r in col.rows}:
            bounds, ridge = col.boxes[body]
            crown = ridge - eye_z
            if crown <= 0.0:
                continue
            box = Node(bounds[0], bounds[1], bounds[2], bounds[3])
            got = columns(box)
            if got is None:
                continue
            _, bfar = box.near_far(ex, ey)
            np.maximum(up[got[0]:got[1]], crown / max(bfar, 1e-6), out=up[got[0]:got[1]])
    return seen, tested, rows
