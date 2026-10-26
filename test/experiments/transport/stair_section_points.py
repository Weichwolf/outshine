"""Exact finite vertical cuts keyed by original indexed mesh vertices and edges."""

from fractions import Fraction


class SectionPoints:
    def __init__(self, vertices, used, axis):
        self.vertices = {
            int(i): tuple(Fraction(float(value)) for value in vertices[i]) for i in used
        }
        self.start, end = (
            tuple(Fraction(float(value)) for value in point) for point in axis
        )
        self.dx, self.dy = end[0] - self.start[0], end[1] - self.start[1]
        self.square = self.dx * self.dx + self.dy * self.dy
        self.signed = {
            i: (point[0] - self.start[0]) * self.dy
            - (point[1] - self.start[1]) * self.dx
            for i, point in self.vertices.items()
        }
        self.identities, self.xyz, self.parameters = {}, [], []
        self.edge_queries = self.edge_constructions = 0

    def parameter(self, point):
        return (
            (point[0] - self.start[0]) * self.dx + (point[1] - self.start[1]) * self.dy
        ) / self.square

    def node(self, identity, point):
        if identity not in self.identities:
            self.identities[identity] = len(self.xyz)
            self.xyz.append(point)
            self.parameters.append(self.parameter(point))
        return self.identities[identity]

    def edge(self, first, second):
        self.edge_queries += 1
        a, b = sorted((int(first), int(second)))
        da, db = self.signed[a], self.signed[b]
        if da == 0:
            return self.node(("vertex", a), self.vertices[a])
        if db == 0:
            return self.node(("vertex", b), self.vertices[b])
        if da * db >= 0:
            return None
        identity = ("edge", a, b)
        if identity not in self.identities:
            fraction = da / (da - db)
            point = tuple(
                x + fraction * (y - x)
                for x, y in zip(self.vertices[a], self.vertices[b], strict=True)
            )
            self.edge_constructions += 1
            return self.node(identity, point)
        return self.identities[identity]

    def face(self, vertices):
        if all(self.signed[int(i)] == 0 for i in vertices):
            parameters = [self.parameter(self.vertices[int(i)]) for i in vertices]
            if max(parameters) < 0 or min(parameters) > 1:
                return None
            raise ValueError("coplanar face does not define one route section")
        hits = set()
        for first, second in zip(vertices, [*vertices[1:], vertices[0]], strict=True):
            if self.signed[int(first)] == self.signed[int(second)] == 0:
                self.edge_queries += 1
                for vertex in (int(first), int(second)):
                    hits.add(self.node(("vertex", vertex), self.vertices[vertex]))
            else:
                hit = self.edge(first, second)
                if hit is not None:
                    hits.add(hit)
        if len(hits) < 2:
            return None
        if len(hits) != 2:
            raise ValueError("one noncoplanar triangle must cut in one segment")
        return self.clip(*hits)

    def _limit(self, a, b, target):
        if self.parameters[a] == target:
            return a
        if self.parameters[b] == target:
            return b
        identity = ("limit", target, *sorted((a, b)))
        if identity not in self.identities:
            fraction = (target - self.parameters[a]) / (
                self.parameters[b] - self.parameters[a]
            )
            point = tuple(
                x + fraction * (y - x)
                for x, y in zip(self.xyz[a], self.xyz[b], strict=True)
            )
            return self.node(identity, point)
        return self.identities[identity]

    def clip(self, first, second):
        a, b = sorted(
            (first, second), key=lambda i: (self.parameters[i], self.xyz[i][2])
        )
        low, high = self.parameters[a], self.parameters[b]
        if high < 0 or low > 1:
            return None
        if low < 0:
            a = self._limit(a, b, 0)
        if high > 1:
            b = self._limit(a, b, 1)
        return None if a == b else tuple(sorted((a, b)))
