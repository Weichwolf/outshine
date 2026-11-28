import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from outshine.materials.aeolian_sand import (
    SAND_DETAIL,
    SAND_PROFILES,
    SAND_TRAIL_SHOULDER_M,
    TREAD_DETAIL,
    mapped_sand,
)
from outshine.materials.detail_coordinates import (
    WARP_METRES,
    WAVE_STRETCH,
    WAVES,
    DetailFrame,
)
from outshine.materials.ground_detail import detail_scale, ground_detail
from outshine.terrain.granodiorite import cube_surface, rock_field
from outshine.world.mesh import Meshes
from outshine.world.scene import ReferenceScene
from scipy.ndimage import map_coordinates
from shapely.geometry import LineString, box


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

    def test_neighbouring_detail_spans_do_not_copy_the_surface_pattern(self):
        for recipe, origin in (
            ("gravelly soil", [-12930050.17, 3776200.53, 1231.31]),
            ("grass", [15380536.2, 4014154.0, 1633.0]),
            ("leaf litter", [15380536.2, 4014154.0, 1633.0]),
            (SAND_DETAIL, [1722241, -2747831, 612]),
            (TREAD_DETAIL, [-4772473, -289249, 37]),
        ):
            frame = DetailFrame(np.array(origin), np.ones(3))
            texture = ground_detail(recipe)
            for band, stretch in enumerate(WAVE_STRETCH):
                with self.subTest(recipe=recipe, band=band):
                    span = detail_scale(recipe)[band]
                    x, y = np.meshgrid(
                        np.linspace(0, span, 120), np.linspace(0, span, 120)
                    )
                    xy = np.column_stack((x.ravel(), y.ravel(), np.zeros(x.size)))
                    channel = texture[:, :, 2 if band == 0 else 3]
                    positions = np.vstack((xy, xy + [span, 0, 0], xy + [0, span, 0]))
                    uv = texture_position(frame, positions, span, stretch)
                    samples = map_coordinates(
                        channel, uv[:, :2].T * len(channel), order=1, mode="grid-wrap"
                    ).reshape(3, -1)
                    self.assertLess(np.max(np.abs(np.corrcoef(samples)[0, 1:])), 0.35)

    def test_sand_overlay_preserves_mesh_support_and_uses_mapped_coverage(self):
        scene = object.__new__(ReferenceScene)
        scene.sand = SAND_PROFILES[103]
        scene.soil_surfaces = {}
        scene.sand_area = mapped_sand(
            [
                ("landcover", box(-2, -3, 0, 3), {"class": "sand"}),
                ("landcover", box(0, -3, 2, 3), {"class": "rock"}),
            ]
        )
        vertices = [
            (-1, 0, 1),
            (-0.5, 0, 2),
            (-1, 1, 1),
            (1, 0, 2),
            (1.5, 0, 2),
            (1, 1, 2),
        ]
        faces = [[0, 1, 2], [3, 4, 5]]
        batches = list(scene.local_material("ground", vertices, faces))
        self.assertEqual([batch[0] for batch in batches], [scene.sand.sand, "ground"])
        for batch, face in zip(batches, faces, strict=True):
            self.assertIs(batch[1], vertices)
            np.testing.assert_array_equal(batch[2], [face])
        self.assertEqual(
            [
                batch[0]
                for batch in scene.local_material("rail ballast", vertices, faces)
            ],
            ["rail ballast"],
        )

        scene.local = Meshes()
        edge = LineString([(0, -2), (0, 2)])
        scene.local.mineral_trail_edges = edge
        width = SAND_TRAIL_SHOULDER_M
        route = [(0, 0, 0), (width / 2, 0, 0), (width, 0, 0)]
        scene.local["unpaved path"] = route, []
        np.testing.assert_allclose(
            scene.ground_blend(scene.sand.tread, route), [1, 0.5, 0]
        )

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
