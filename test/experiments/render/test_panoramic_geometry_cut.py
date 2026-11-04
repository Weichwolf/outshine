"""All-direction error envelopes, viewport edges, translation and complete cuts."""

import itertools
import math
import random
import unittest

from panoramic_geometry_cut import PanoramicView
from source_geometry_cut import Camera, GeometrySources, dot, unit
from source_product_cut import SourceProducts
from test_source_geometry_cut import metadata, project
from test_source_product_cut import index


def edge_camera(eye, point, focal, tx, ty):
    base = Camera.toward(eye, point, focal)
    beta = math.atan(ty)
    alpha = math.atan(tx * math.cos(beta))
    right = tuple(math.sin(alpha) * f + math.cos(alpha) * r
                  for f, r in zip(base.forward, base.right))
    forward = tuple(math.cos(alpha) * f - math.sin(alpha) * r
                    for f, r in zip(base.forward, base.right))
    up = tuple(math.sin(beta) * f + math.cos(beta) * u
               for f, u in zip(forward, base.up))
    forward = tuple(math.cos(beta) * f - math.sin(beta) * u
                    for f, u in zip(forward, base.up))
    return Camera(tuple(eye), right, up, forward, focal)


class PanoramicGeometryTests(unittest.TestCase):
    def test_back_facing_camera_does_not_force_originals_in_round_coverage(self):
        tree = SourceProducts(*index(), revision="round-coverage")
        sources = GeometrySources(tree, metadata(tree))
        eye = (15, 1, 1000)
        front, _ = sources.cut(Camera.toward(eye, (15, 1, 1), 720), 1)
        back, _ = sources.cut(Camera.toward(eye, (15, 1, 2000), 720), 1)
        panoramic, _ = sources.cut(PanoramicView(eye, 720, 1280, 720), 1)
        self.assertEqual([p.kind for p in front], ["shell"])
        self.assertEqual([p.kind for p in back], ["original"] * 4)
        self.assertEqual([p.kind for p in panoramic], ["shell"])
        tree.validate_cut(panoramic)

    def test_approach_and_unknown_errors_keep_required_originals(self):
        tree = SourceProducts(*index(), revision="round-coverage")
        sources = GeometrySources(tree, metadata(tree, errors=(-1,) * 4))
        cut, _ = sources.cut(PanoramicView((15, 1, 1000), 720, 1280, 720), 1)
        self.assertEqual([p.kind for p in cut], ["original"] * 4)
        sources = GeometrySources(tree, metadata(tree))
        cut, _ = sources.cut(PanoramicView((15, 1, 1000), 720, 1280, 720, 1000), 1)
        self.assertEqual([p.kind for p in cut], ["original"] * 4)

    def test_projection_bound_at_edges_including_shell_outside_source_box(self):
        rng = random.Random(1955)
        box = (-3, -2, 200, 3, 2, 210)
        view = PanoramicView((0, 0, 0), 720, 1280, 720, radius=40)
        error = 0.5
        shell = view.shell_error(view.bounds(box), error)
        hull = view.hull_error(view.bounds(box))
        largest = 0
        for _ in range(160):
            direction = unit(tuple(rng.uniform(-1, 1) for _ in range(3)))
            eye = tuple(40 * d for d in direction)
            point = tuple(rng.uniform(a, b) for a, b in zip(box[:3], box[3:6]))
            other = tuple(p + error * d for p, d in zip(point, direction))
            opposite = tuple(rng.uniform(a, b) for a, b in zip(box[:3], box[3:6]))
            for visible in (point, other):
                for x, y in itertools.product((-1, 0, 1), repeat=2):
                    camera = edge_camera(eye, visible, 720, x * 1280 / 1440, y / 2)
                    self.assertGreater(dot(tuple(p-e for p,e in zip(point,eye)), camera.forward), 0)
                    a, b, c = (project(camera, p) for p in (point, other, opposite))
                    observed = sum(abs(p-q) for p,q in zip(a,b))
                    largest = max(largest, observed)
                    self.assertLessEqual(observed, shell + 1e-9)
                    self.assertLessEqual(sum(abs(p-q) for p,q in zip(a,c)), hull + 1e-9)
        self.assertGreater(largest, 2)

    def test_eye_domain_and_invalid_projection(self):
        for focal, width, height, radius in ((0,1280,720,0),(720,0,720,0),
                                            (720,1280,math.inf,0),(720,1280,720,-1)):
            with self.assertRaises(ValueError):
                PanoramicView((0,0,0), focal, width, height, radius)
        view = PanoramicView((0,0,0), 720, 1280, 720)
        domain = view.bounds((-1,-1,-1,1,1,1))
        self.assertEqual(view.shell_error(domain, 0), 0)
        self.assertEqual(view.shell_error(domain, 1), math.inf)
        self.assertEqual(view.hull_error(domain), math.inf)
        # A shell endpoint can be nearer than every point of the original source box.
        self.assertEqual(view.shell_error(view.bounds((0,0,2,0,0,3)), 1), math.inf)
        with self.assertRaises(ValueError):
            view.shell_error(domain, -1)

    def test_coupled_displacement_reaches_the_l1_viewport_corner_limit(self):
        view = PanoramicView((0,0,0), 720, 1280, 720)
        tx, ty, depth = 1280/1440, 0.5, 1000000
        point = (tx*depth, ty*depth, depth)
        direction = unit((1,1,-tx-ty))
        other = tuple(p+d for p,d in zip(point,direction))
        camera = Camera((0,0,0),(1,0,0),(0,1,0),(0,0,1),720)
        a,b = project(camera,point),project(camera,other)
        observed = sum(abs(x-y) for x,y in zip(a,b))
        bound = view.shell_error(view.bounds(point+point), 1)
        self.assertLessEqual(observed,bound)
        self.assertGreater(observed,bound*0.9999)


if __name__ == "__main__":
    unittest.main()
