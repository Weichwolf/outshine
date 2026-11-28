import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from outshine.materials.detail_coordinates import (
    WARP_METRES,
    WAVE_STRETCH,
    WAVES,
    DetailFrame,
)
from outshine.materials.ground_detail import detail_scale, ground_detail
from outshine.terrain.granodiorite import cube_surface, rock_field
from scipy.ndimage import map_coordinates
from shapely.geometry import box


def texture_position(frame, xyz, span, stretch):
    local = xyz * frame.scale
    wave = (local / stretch) @ WAVES.T + frame.phase(stretch)
    shifted = local + frame.offset(span) + stretch * WARP_METRES * np.sin(wave)
    return np.remainder(shifted / span, 1)


def ground(x0, x1):
    xy = np.array(((x0, -23), (x1, -23), (x1, 23), (x0, 23)))
    return np.column_stack((xy, xy @ [0.11, 0.02])), np.array(((0, 1, 2), (0, 2, 3)))


class NaturalMaterialsTest(unittest.TestCase):
    def test_detail_keeps_its_geographic_phase_after_recentring(self):
        points = np.array([[6, 10, 1], [59, -34, -0.3], [-29, -17, 8]])
        a = DetailFrame(
            np.array([-12930050.17, 3776200.53, 1231.31]), np.array([1.204, 1.0, 1.0])
        )
        b = DetailFrame(a.origin + [77.8, -72.2, 3.7], np.array([1.205, 1.0, 1.0]))
        same_positions = (points * a.scale + a.origin - b.origin) / b.scale
        for span, stretch in zip((3.4, 54.0), WAVE_STRETCH, strict=True):
            np.testing.assert_allclose(
                texture_position(a, points, span, stretch),
                texture_position(b, same_positions, span, stretch),
                atol=2e-8,
            )

    def test_neighbouring_detail_spans_do_not_copy_the_gravel_pattern(self):
        frame = DetailFrame(np.array([-12930050.17, 3776200.53, 1231.31]), np.ones(3))
        texture = ground_detail("gravelly soil")
        for band, stretch in enumerate(WAVE_STRETCH):
            span = detail_scale("gravelly soil")[band]
            x, y = np.meshgrid(np.linspace(0, span, 120), np.linspace(0, span, 120))
            xy = np.column_stack((x.ravel(), y.ravel(), np.zeros(x.size)))
            channel = texture[:, :, 2 if band == 0 else 3]

            positions = np.vstack((xy, xy + [span, 0, 0]))
            uv = texture_position(frame, positions, span, stretch)
            first, next_tile = map_coordinates(
                channel, uv[:, :2].T * len(channel), order=1, mode="grid-wrap"
            ).reshape(2, -1)
            self.assertLess(abs(np.corrcoef(first, next_tile)[0, 1]), 0.35)

    def test_block_has_closed_outward_faces_and_never_occupies_the_route(self):
        vertices, faces = cube_surface(5)
        edges = np.sort(
            np.concatenate((faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]])),
            axis=1,
        )
        _, count = np.unique(edges, axis=0, return_counts=True)
        np.testing.assert_array_equal(count, np.full(len(count), 2))
        tri = vertices[faces]
        normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        self.assertTrue((np.einsum("ni,ni->n", normals, tri.mean(axis=1)) > 0).all())
        source = SimpleNamespace(
            lon=-116.17, lat=34.01, metres=np.array([92000, 111000])
        )
        atlas = SimpleNamespace(
            evidence_windows=lambda *_: [(-117, 33, -116, 35)],
            geology_cells=lambda lat, lon: np.ones(len(lat), dtype=int),
            geology_mask=lambda indices, codes: np.full(
                len(indices), "JOTR-GRI:Khv" in codes
            ),
            observed_cells=lambda lat, lon: (np.ones(len(lat), dtype=int), None),
            observation_mask=lambda indices, codes: np.ones(len(indices), dtype=bool),
        )
        xy = np.array([[0.0, 0], [13.0, 2]])
        with patch(
            "outshine.terrain.granodiorite.tile_sites",
            side_effect=lambda *args, **kwargs: iter([(xy, np.zeros((2, 3)))]),
        ):
            field = rock_field(
                atlas,
                source,
                {"ground": ground(-27, 27), "road": ground(-2, 2)},
                np.zeros(2),
                39,
            )
        self.assertIsNotNone(field)
        self.assertGreater(field.footprints.distance(box(-2, -23, 2, 23)), 0.28)
