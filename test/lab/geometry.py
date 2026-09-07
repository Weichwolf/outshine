"""THE DOOR'S OWN GEOMETRY, IN PYTHON. `include/scene/Geometry.h`, field for field.

The lab held a mesh as a list of Python tuples. Measured 2026-09-07: a million vertices costs
144 MB that way and 12 MB as `float32`, a million triangles 156 MB against 12 MB -- TWELVE TIMES
-- so a town twin of 6.9 million triangles was 2.1 GB where it is 163 MB, and the system killed
the renderer for want of memory. The C++ never had that problem because the door has always said
what a mesh IS:

    setPositions(part, span<const float> metres)      three floats a vertex, FLAT
    setNormals(part, span<const float> unit)          three, unit length
    setTexture(part, span<const float> uv, set)       two, per UV set
    setColours(part, span<const float> rgba)          FOUR, and rgba means rgba
    setTriangles(part, span<const uint32_t> indices)  three per triangle, uint32, FLAT

So the lab says the same thing, with the same names, the same shapes and the same index type,
and the conversion of a generator to C++ is a copy rather than a translation. `RenderableManager`,
`TransformManager` and `MaterialInstance` are the door's words and they are used here unchanged.

WHAT A SCENE COSTS, and it is arithmetic rather than a hope:

    a vertex   12 B position + 12 B normal + 8 B uv + 16 B colour = 48 B at its richest,
               12 B at its plainest
    a triangle 12 B of index
    OldTown at a 90 m reach: 1.9 M triangles over 1.6 M vertices
               = 23 MB of index + 19 MB of position, 42 MB carrying nothing else
    the same twin at its worst measured   6.9 M triangles, 6.7 M vertices = 163 MB

`Budget` states that arithmetic and `Geometry.bytes()` measures what was actually built, so a
generator that overshoots says so in a number rather than in a kill signal.
"""
import numpy as np

POSITION_B = 12           # three float32
NORMAL_B = 12
UV_B = 8
COLOUR_B = 16             # RGBA, as the door says
INDEX_B = 12              # three uint32 per triangle


def budget(triangles, vertices, normals=False, uv=False, colours=False):
    """What a mesh of that size MUST cost, in bytes. The number a generator is held to."""
    per = POSITION_B + (NORMAL_B if normals else 0) + (UV_B if uv else 0) \
        + (COLOUR_B if colours else 0)
    return int(vertices) * per + int(triangles) * INDEX_B


class MaterialInstance:
    """What `addSurface` returns: an index into the geometry's own surfaces, not a copy of one.
    Two parts that share a surface share this, which is what makes them one draw."""

    __slots__ = ("at",)

    def __init__(self, at):
        self.at = int(at)

    def __eq__(self, other):
        return isinstance(other, MaterialInstance) and other.at == self.at

    def __hash__(self):
        return hash(("MaterialInstance", self.at))

    def __repr__(self):
        return f"MaterialInstance({self.at})"


class RenderableManager:
    """`Geometry::renderables()`: what a part IS, asked without reaching into it."""

    __slots__ = ("of",)

    def __init__(self, of):
        self.of = of

    def count(self):
        return self.of.parts()

    def nameOf(self, part):
        return self.of.nameOf(part)

    def getMaterial(self, part):
        return self.of.materialOf(part)

    def setMaterial(self, part, surface):
        return self.of.setMaterial(part, surface)

    def vertexCount(self, part):
        return len(self.of.positionsOf(part)) // 3

    def triangleCount(self, part):
        return len(self.of.trianglesOf(part)) // 3


class Geometry:
    """PARTS, each with its own arrays and one surface. The door's class, in the door's words."""

    __slots__ = ("_named", "_material", "_position", "_normal", "_uv", "_colour", "_triangle",
                 "_surface", "_surfaceName")

    def __init__(self):
        self.clear()

    def clear(self):
        self._named, self._material = [], []
        self._position, self._normal, self._uv, self._colour, self._triangle = [], [], [], [], []
        self._surface, self._surfaceName = [], []

    # -- surfaces
    def addSurface(self, named, surface):
        self._surface.append(surface)
        self._surfaceName.append(str(named))
        return MaterialInstance(len(self._surface) - 1)

    def surfaces(self):
        return len(self._surface)

    def surfaceAt(self, surface):
        return self._surface[surface.at]

    def surfaceNameOf(self, surface):
        return self._surfaceName[surface.at]

    # -- parts
    def addPart(self, named, material):
        self._named.append(str(named))
        self._material.append(material)
        for held in (self._position, self._normal, self._uv, self._colour, self._triangle):
            held.append(None)
        return len(self._named) - 1

    def parts(self):
        return len(self._named)

    def nameOf(self, part):
        return self._named[part]

    def materialOf(self, part):
        return self._material[part]

    def setMaterial(self, part, surface):
        self._material[part] = surface
        return True

    def renderables(self):
        return RenderableManager(self)

    # -- the arrays, flat, exactly as the door takes them
    def setPositions(self, part, metres):
        self._position[part] = np.ascontiguousarray(metres, dtype=np.float32).reshape(-1)
        return True

    def setNormals(self, part, unit):
        self._normal[part] = np.ascontiguousarray(unit, dtype=np.float32).reshape(-1)
        return True

    def setTexture(self, part, uv, uvSet=0):
        held = self._uv[part] or {}
        held[int(uvSet)] = np.ascontiguousarray(uv, dtype=np.float32).reshape(-1)
        self._uv[part] = held
        return True

    def setColours(self, part, rgba):
        self._colour[part] = np.ascontiguousarray(rgba, dtype=np.float32).reshape(-1)
        return True

    def setTriangles(self, part, indices):
        self._triangle[part] = np.ascontiguousarray(indices, dtype=np.uint32).reshape(-1)
        return True

    def positionsOf(self, part):
        return self._position[part] if self._position[part] is not None \
            else np.zeros(0, dtype=np.float32)

    def normalsOf(self, part):
        return self._normal[part] if self._normal[part] is not None \
            else np.zeros(0, dtype=np.float32)

    def textureOf(self, part, uvSet=0):
        held = self._uv[part] or {}
        return held.get(int(uvSet), np.zeros(0, dtype=np.float32))

    def coloursOf(self, part):
        return self._colour[part] if self._colour[part] is not None \
            else np.zeros(0, dtype=np.float32)

    def trianglesOf(self, part):
        return self._triangle[part] if self._triangle[part] is not None \
            else np.zeros(0, dtype=np.uint32)

    # -- what the door checks for itself
    def wellFormed(self):
        """Every part's arrays agree on how many vertices there are, and every index reaches one."""
        for part in range(self.parts()):
            n = len(self.positionsOf(part))
            if n % 3:
                return False
            v = n // 3
            for held, wide in ((self.normalsOf(part), 3), (self.coloursOf(part), 4)):
                if len(held) and len(held) != v * wide:
                    return False
            t = self.trianglesOf(part)
            if len(t) % 3 or (len(t) and (v == 0 or int(t.max()) >= v)):
                return False
        return True

    def windingAgainstNormals(self, part):
        """How many of a part's triangles are wound against their own vertex normals -- the door's
        own words, and the door's own number. A mesh built to the standard reads 0."""
        P = self.positionsOf(part).reshape(-1, 3)
        N = self.normalsOf(part)
        T = self.trianglesOf(part).reshape(-1, 3)
        if not len(T) or not len(N):
            return 0
        N = N.reshape(-1, 3)
        a, b, c = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
        face = np.cross(b - a, c - a)
        area = np.linalg.norm(face, axis=1)
        keep = area > 1e-12
        want = N[T[:, 0]] + N[T[:, 1]] + N[T[:, 2]]
        return int((np.einsum("ij,ij->i", face[keep], want[keep]) < 0.0).sum())

    # -- what a budget is held against
    def bytes(self):
        """What this geometry actually costs, by the same arithmetic `budget` states."""
        total = 0
        for part in range(self.parts()):
            for held in (self.positionsOf(part), self.normalsOf(part), self.coloursOf(part)):
                total += int(held.nbytes)
            for held in (self._uv[part] or {}).values():
                total += int(held.nbytes)
            total += int(self.trianglesOf(part).nbytes)
        return total

    def triangleCount(self):
        return sum(len(self.trianglesOf(p)) // 3 for p in range(self.parts()))

    def vertexCount(self):
        return sum(len(self.positionsOf(p)) // 3 for p in range(self.parts()))
