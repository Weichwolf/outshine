"""Conservative whole-triangle separation from the unchanged actual base body."""

import numpy as np
import shapely
from tilted_rail_verify import GroundContact


class UpperContact(GroundContact):
    def __init__(self, deck, baseline):
        super().__init__(deck, np.ones(len(deck.triangles), bool))
        self.base = baseline.vertices[baseline.triangles]
        # Hulls retain vertical walls as lines instead of silently dropping them.
        self.base_shapes = shapely.convex_hull(shapely.polygons(self.base[..., :2]))
        self.base_tree = shapely.STRtree(self.base_shapes)
        self.base_tests = 0

    def verify(self, footprint, triangle, *, max_faces=8192):
        proof = super().verify(footprint, triangle, max_faces=max_faces)
        hits = self.base_tree.query(footprint, predicate="intersects")
        self.base_tests += len(hits)
        if len(hits) > 32768 or self.base_tests > 1048576:
            raise ValueError("upper rail actual baseline triangle budget")
        p = np.asarray(triangle)
        gradient = np.linalg.solve(p[1:, :2] - p[0, :2], p[1:, 2] - p[0, 2])
        vertices = self.base[hits]
        expected = p[0, 2] + (vertices[..., :2] - p[0, :2]) @ gradient
        error = float(np.max(vertices[..., 2] - expected, initial=0))
        if error > 1e-7:
            raise ValueError(
                "baseline triangle not certified below selected support plane"
            )
        return dict(
            proof,
            baseline_triangle_candidates=len(hits),
            maximum_baseline_above_bed_m=error,
        )
