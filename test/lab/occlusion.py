"""NOTHING THAT IS NOT SEEN IS EVER COMPUTED.

PHOTOREALISM FROM OSM IS AN OPTIMISATION PROBLEM BEFORE IT IS A GEOMETRY ONE. A facade worth
looking at costs thousands of triangles and seconds of generation; a town has thousands of
facades; a frame has 16.7 ms. The only way both numbers can be true is that the frame touches
almost none of the town -- measured on Rothenburg, 8 of 1 054 bodies are ever seen from a street
and 86 from 26 m up. So the question a generator must answer FIRST, before it computes anything
at all, is which bodies those are.

A RAY MARCH ANSWERS IT AND IS THE WRONG SHAPE. Marching a horizon per screen column -- Voxel
Space's own trick -- costs 535 040 samples whatever the scene holds, because it walks the ground
rather than the data. OSM is not a field: it is a set of footprints with heights, and a set has a
HIERARCHY. A quadtree over it, each node carrying the greatest ridge below it, answers the same
question by REJECTING SUBTREES: a node whose highest possible silhouette stands under the horizon
already drawn cannot contain anything visible, and its whole subtree is never touched. That is
hierarchical occlusion culling with a world-space Hi-Z, which is what the references do, and the
tree here is a QUADTREE and not an octree because OSM has no overhangs -- a footprint and a
height is 2.5D by construction, and a third axis would be a dimension with nothing in it.

WHAT THE TRAVERSAL GUARANTEES, and it is the part that has to be conservative:

    a node is rejected only if its GREATEST possible screen angle -- its max ridge seen from its
    NEAREST corner -- stands below the horizon over every column it spans
    only a LEAF occludes, and it occludes by its FURTHEST corner, which under-states what it
    really covers
    the order is front to back by nearest corner, so the horizon is as high as it can be when
    the next node is tested

Both bounds err towards drawing, so the set this returns is a SUPERSET of what is visible and
never a subset -- which is the only direction a culler may be wrong in.
"""
import math

import numpy as np

LEAF_MOST = 8             # [SET] bodies in a leaf: below this the tree costs more than it saves
# A CULLER MUST SEE WHAT A GENERATOR WILL ADD: a cornice, an eaves and a balcony stand off the
# mass, so every bound is grown by the deepest relief a generator may add.
RELIEF_M = 1.20


class Node:
    __slots__ = ("x0", "y0", "x1", "y1", "top", "kids", "bodies")

    def __init__(self, x0, y0, x1, y1):
        self.x0, self.y0, self.x1, self.y1 = x0, y0, x1, y1
        self.top = -1e30
        self.kids = []
        self.bodies = []

    def near_far(self, ex, ey):
        """The nearest and furthest distance from the eye to this box, in plan."""
        dx_near = max(self.x0 - ex, 0.0, ex - self.x1)
        dy_near = max(self.y0 - ey, 0.0, ey - self.y1)
        dx_far = max(abs(self.x0 - ex), abs(self.x1 - ex))
        dy_far = max(abs(self.y0 - ey), abs(self.y1 - ey))
        return math.hypot(dx_near, dy_near), math.hypot(dx_far, dy_far)


class Quadtree:
    """THE OSM EXTRACT AS A HIERARCHY, built from footprints and heights alone -- no mesh, and no
    body built to make it."""

    def __init__(self, boxes, tops, leaf_most=LEAF_MOST):
        self.boxes = np.asarray(boxes, dtype=np.float64).reshape(-1, 4)
        self.tops = np.asarray(tops, dtype=np.float64).reshape(-1)
        self.nodes = 0
        if not len(self.boxes):
            self.root = Node(0.0, 0.0, 0.0, 0.0)
            return
        self.boxes = self.boxes + np.array([-RELIEF_M, -RELIEF_M, RELIEF_M, RELIEF_M])
        self.tops = self.tops + RELIEF_M
        x0, y0 = self.boxes[:, 0].min(), self.boxes[:, 1].min()
        x1, y1 = self.boxes[:, 2].max(), self.boxes[:, 3].max()
        self.root = self._split(list(range(len(self.boxes))), x0, y0, x1, y1, leaf_most, 0)

    def _split(self, which, x0, y0, x1, y1, leaf_most, depth):
        self.nodes += 1
        node = Node(x0, y0, x1, y1)
        node.top = float(self.tops[which].max()) if which else -1e30
        if len(which) <= leaf_most or depth > 16:
            node.bodies = which
            return node
        mx, my = 0.5 * (x0 + x1), 0.5 * (y0 + y1)
        # a body goes to the quadrant its CENTRE falls in, so nothing is stored twice; a node's
        # bounds are then the union of its children's, which is what the reject test reads
        buckets = ([], [], [], [])
        for i in which:
            cx = 0.5 * (self.boxes[i, 0] + self.boxes[i, 2])
            cy = 0.5 * (self.boxes[i, 1] + self.boxes[i, 3])
            buckets[(1 if cx > mx else 0) + (2 if cy > my else 0)].append(i)
        if max(len(b) for b in buckets) == len(which):
            node.bodies = which                      # every centre in one quadrant: stop
            return node
        for k, got in enumerate(buckets):
            if not got:
                continue
            b = self.boxes[got]
            node.kids.append(self._split(got, float(b[:, 0].min()), float(b[:, 1].min()),
                                         float(b[:, 2].max()), float(b[:, 3].max()),
                                         leaf_most, depth + 1))
        return node


class Horizon:
    """ONE ANGLE PER SCREEN COLUMN: how high this column is already covered. A world-space Hi-Z
    with one row, which is all a 2.5D city needs."""

    __slots__ = ("width", "fov", "bearing", "eye", "eye_z", "up", "far")

    def __init__(self, eye_xy, eye_z, bearing_deg, fov_deg=55.0, width=1280):
        self.width, self.fov = int(width), math.radians(fov_deg)
        self.bearing = math.radians(bearing_deg)
        self.eye, self.eye_z = eye_xy, float(eye_z)
        self.up = np.full(self.width, -1e30)
        # AND HOW FAR AWAY THE THING IS THAT COVERS THE COLUMN. The body walk needs no distance
        # because it goes front to back: whatever is tested has not yet been hidden by anything
        # behind it. A point asked about AFTERWARDS has lost that order, and without a depth the
        # horizon would hide the road in FRONT of a facade as readily as the road behind it. The
        # occluder's FAR corner is the honest number: a point beyond that is certainly behind it.
        self.far = np.full(self.width, np.inf)

    def sees(self, x, y, z):
        """Is this ground point in the frustum and not behind a facade? Vectorised over arrays.

        CONSERVATIVE BY CONSTRUCTION, like every other rule in this file: the column is the one
        the point falls in, the occluder's depth is its FAR corner, and anything the test cannot
        settle is drawn. A culler may only err towards drawing."""
        x, y, z = np.atleast_1d(x), np.atleast_1d(y), np.atleast_1d(z)
        ex, ey = self.eye
        a = np.arctan2(x - ex, y - ey) - self.bearing
        a = (a + math.pi) % (2 * math.pi) - math.pi
        col = np.floor((a / self.fov + 0.5) * self.width).astype(np.int64)
        inside = (col >= 0) & (col < self.width)
        col = np.clip(col, 0, self.width - 1)
        d = np.hypot(x - ex, y - ey)
        angle = (z - self.eye_z) / np.maximum(d, 1e-6)
        hidden = (angle <= self.up[col]) & (d > self.far[col])
        return inside & ~hidden

    def columns(self, node):
        """The column range a box spans, or None when it falls outside the frustum."""
        ex, ey = self.eye
        lo, hi = 1e30, -1e30
        for (x, y) in ((node.x0, node.y0), (node.x1, node.y0), (node.x1, node.y1),
                       (node.x0, node.y1)):
            a = math.atan2(x - ex, y - ey) - self.bearing
            a = (a + math.pi) % (2 * math.pi) - math.pi
            lo, hi = min(lo, a), max(hi, a)
        if hi - lo > math.pi:                        # the eye is inside: every column
            return 0, self.width
        c0 = int(math.floor((lo / self.fov + 0.5) * self.width))
        c1 = int(math.ceil((hi / self.fov + 0.5) * self.width))
        if c1 <= 0 or c0 >= self.width:
            return None
        return max(0, c0), min(self.width, c1)


def visible(tree, horizon, bodies_top=None):
    """WHICH BODIES ARE SEEN, and how much work it took to say so.

    Returns (set of body indices, node tests, leaf tests)."""
    ex, ey = horizon.eye
    tested = leaves = 0
    seen = set()
    tops = tree.tops if bodies_top is None else np.asarray(bodies_top, dtype=np.float64)
    stack = [(tree.root.near_far(ex, ey)[0], tree.root)]
    while stack:
        # FRONT TO BACK: the nearest node first, so the horizon is as high as it can be
        stack.sort(key=lambda r: -r[0])
        _, node = stack.pop()
        tested += 1
        span = horizon.columns(node)
        if span is None:
            continue
        near, far = node.near_far(ex, ey)
        # THE GREATEST ANGLE ANYTHING UNDER THIS NODE COULD REACH, and the distance that gives it
        # DEPENDS ON THE SIGN. Above the roofs the numerator is negative and a bigger distance
        # makes the angle HIGHER, not lower -- taken the other way round the culler over-states
        # what it can reject and drops things that are visible: measured 2026-09-07 from 26 m up,
        # 48 of the 82 bodies a ray march found were culled. A culler may only err towards
        # drawing, so each bound takes the distance that makes it worst for the culler.
        rise = node.top - horizon.eye_z
        most = rise / max(near if rise > 0.0 else far, 1e-6)
        if most <= horizon.up[span[0]:span[1]].min():
            continue                                  # the whole subtree is behind the horizon
        if node.kids:
            for kid in node.kids:
                stack.append((kid.near_far(ex, ey)[0], kid))
            continue
        for i in node.bodies:
            leaves += 1
            b = tree.boxes[i]
            leaf = Node(b[0], b[1], b[2], b[3])
            leaf.top = float(tops[i])
            got = horizon.columns(leaf)
            if got is None:
                continue
            lnear, lfar = leaf.near_far(ex, ey)
            lrise = leaf.top - horizon.eye_z
            if lrise / max(lnear if lrise > 0.0 else lfar, 1e-6) \
                    <= horizon.up[got[0]:got[1]].min():
                continue
            seen.add(int(i))
            # A LEAF OCCLUDES ONLY IF IT RISES ABOVE THE EYE, and this is the model's own
            # assumption rather than a tuning: a horizon says "this column is covered up to here
            # and everything under it is behind a facade", which is true when the occluder's wall
            # reaches the ground BELOW the line of sight -- and false the moment the eye is above
            # its roof, because then the ground BETWEEN the buildings is seen under their tops.
            # Measured 2026-09-07 from 26 m up: with a low roof allowed to occlude, 12 of the 88
            # bodies a fine march finds were culled, and every one of them stood below the eye.
            # A culler may only err towards drawing.
            if lrise <= 0.0:
                continue
            band = lrise / max(lfar, 1e-6)
            lift = band > horizon.up[got[0]:got[1]]
            np.maximum(horizon.up[got[0]:got[1]], band, out=horizon.up[got[0]:got[1]])
            np.copyto(horizon.far[got[0]:got[1]], lfar, where=lift)
    return seen, tested, leaves
