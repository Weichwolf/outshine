"""Native source metadata -> complete geometry/field candidate cuts.

The field bound is for a centred capture camera, not arbitrary World poses.
Native pose/material/visibility checks still decide whether a product may replace
its parents. No mesh payloads, occlusion or native frame budgets are modelled.
"""

import math
import struct
from dataclasses import dataclass
from functools import lru_cache


@dataclass(frozen=True)
class Product:
    node: int
    kind: str
    revision: str
    pixels: int = 0
    centred_error_px: float = 0.0

    @property
    def address(self):
        return self.revision, self.node, self.kind, self.pixels


def centred_error(focal, half_extent, distance):
    """L1 error for |u|,|v|<=h, |depth|<=2h and the central perspective eye.

    Each axis differs by F*|u*depth| / (d*|d-depth|).
    The sum is at most 4*F*h*h/(d*(d-2*h)); near-plane cases cannot qualify.
    This real-arithmetic model is not the native interval/GPU proof.
    """
    if distance <= 2 * half_extent:
        return math.inf
    return 4 * focal * half_extent**2 / (distance * (distance - 2 * half_extent))


class SourceProducts:
    def __init__(self, data, cells, supported=None, *, revision):
        if not isinstance(revision, str) or not revision:
            raise ValueError("immutable source revision required")
        self.revision = revision
        if len(data) < 16:
            raise ValueError("complete native header required")
        magic, count, sources = struct.unpack_from("<QII", data)
        if magic != 0x3158444945474150 or not sources or count != 2 * sources - 1:
            raise ValueError("nonempty native page-index-v1 required")
        if len(data) != 16 + count * 64 + sources * 4 or len(cells) != sources:
            raise ValueError("complete native index and source-cell inventory required")
        supported = bytes(sources) if supported is None else supported
        if len(supported) != sources or any(s not in (0, 1) for s in supported):
            raise ValueError("complete explicit source-support inventory required")
        self.nodes = tuple(struct.iter_unpack("<6d4I", data[16 : 16 + count * 64]))
        self.order = struct.unpack_from("<" + "I" * sources, data, 16 + count * 64)
        if sorted(self.order) != list(range(sources)) or any(
            not isinstance(c, int) or c < 0 for c in cells
        ):
            raise ValueError("unique source ordinals and nonnegative cohort tags required")
        self.cohorts = [None] * count
        self.supported = [False] * count
        self.centres = [None] * count
        self.radii = [0.0] * count
        owners = [0] * count
        for at in range(count - 1, -1, -1):
            node = self.nodes[at]
            lo, hi = node[:3], node[3:6]
            first, size, left, right = node[6:]
            if not all(math.isfinite(x) for x in node[:6]) or any(a > b for a, b in zip(lo, hi)):
                raise ValueError("finite ordered source bounds required")
            self.centres[at] = tuple((a + b) / 2 for a, b in zip(lo, hi))
            self.radii[at] = math.hypot(*(b - a for a, b in zip(lo, hi))) / 2
            if not size or first + size > sources:
                raise ValueError("source interval outside immutable order")
            if size == 1:
                if left or right:
                    raise ValueError("native leaves have no children")
                self.cohorts[at] = cells[self.order[first]]
                self.supported[at] = bool(supported[self.order[first]])
                continue
            if not at < left < right < count:
                raise ValueError("forward native child indices required")
            a, b = self.nodes[left], self.nodes[right]
            if a[6] != first or b[6] != first + a[7] or a[7] + b[7] != size:
                raise ValueError("children must partition the parent interval")
            for child in (left, right):
                owners[child] += 1
                bounds = self.nodes[child]
                if any(bounds[i] < lo[i] or bounds[i + 3] > hi[i] for i in range(3)):
                    raise ValueError("parent bounds must enclose both children")
            a, b = self.cohorts[left], self.cohorts[right]
            self.cohorts[at] = a if a == b else None
            self.supported[at] = self.supported[left] and self.supported[right]
        if owners != [0] + [1] * (count - 1) or self.nodes[0][6:8] != (0, sources):
            raise ValueError("one complete connected root required")

    def cut(self, eye, focal, error, *, cell_cohorts=True):
        if len(eye) != 3 or not all(math.isfinite(x) for x in eye):
            raise ValueError("finite spatial eye required")
        if not math.isfinite(focal) or focal <= 0 or not math.isfinite(error) or error < 0:
            raise ValueError("finite projection and nonnegative error required")
        pending, products, visited = [0], [], 0
        while pending:
            at = pending.pop()
            node = self.nodes[at]
            visited += 1
            pixels, bound = 0, math.inf
            uniform = self.cohorts[at] is not None
            if self.supported[at] and error > 0 and (uniform or not cell_cohorts):
                radius = self.radii[at]
                centre = self.centres[at]
                distance = math.hypot(centre[0] - eye[0], centre[1] - eye[1], centre[2] - eye[2])
                if radius > 0 and distance > radius:
                    required = math.ceil(4 * focal * radius / (distance - radius) + 2)
                    if required <= 4096:
                        pixels = 1 << (max(4, required) - 1).bit_length()
                        half = radius * pixels / (pixels - 2)
                        bound = centred_error(focal, half, distance)
            if bound <= error:
                products.append(Product(at, "field", self.revision, pixels, bound))
            elif node[7] == 1:
                products.append(Product(at, "geometry" if self.supported[at] else "unresolved",
                                        self.revision))
            else:
                pending.extend((node[9], node[8]))
        return products, visited

    def validate_cut(self, products):
        cursor = 0
        for product in products:
            if product.revision != self.revision or not 0 <= product.node < len(self.nodes):
                raise ValueError("product does not belong to this native hierarchy")
            first, size = self.nodes[product.node][6:8]
            if first != cursor:
                raise ValueError("cut must be canonical, with no missing or multiply owned sources")
            cursor += size
        if cursor != len(self.order):
            raise ValueError("cut does not cover every original source")

    def publish(self, previous, desired, ready, *, valid_previous):
        """Keep complete coverage using independently validated current products."""
        previous, desired = tuple(previous), tuple(desired)
        ready, valid_previous = tuple(ready), tuple(valid_previous)
        self.validate_cut(previous)
        self.validate_cut(desired)
        old = {p.node: p for p in previous}
        wanted = {p.node: p for p in desired}
        if any(p.kind == "unresolved" for p in (*ready, *valid_previous)):
            raise ValueError("unresolved sources grant no replacement authority")
        ready = {p.address for p in ready}
        valid_previous = {p.address for p in valid_previous}
        if not valid_previous <= {p.address for p in previous}:
            raise ValueError("fallback validity belongs to the previous cut")
        if not ready <= {p.address for p in desired}:
            raise ValueError("readiness belongs to this exact desired cut")

        @lru_cache(None)
        def available(at):
            if at in wanted:
                return wanted[at].address in ready
            node = self.nodes[at]
            return node[7] > 1 and available(node[8]) and available(node[9])

        pending, output = [0], []
        while pending:
            at = pending.pop()
            if at in wanted and wanted[at].address in ready:
                output.append(wanted[at])
            elif at in old and old[at].address in valid_previous and not available(at):
                output.append(old[at])
            else:
                node = self.nodes[at]
                if node[7] == 1:
                    raise ValueError("no complete current fallback")
                pending.extend((node[9], node[8]))
        self.validate_cut(output)
        return output
