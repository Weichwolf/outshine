import unittest
from dataclasses import FrozenInstanceError, dataclass

import numpy as np
from outshine_ref.buildings.modelling.faces import Face, compile_faces, plane


def courtyard():
    return np.array(
        [
            [0, 0, 0],
            [6, 0, 0],
            [6, 4, 0],
            [0, 4, 0],
            [1, 1, 0],
            [2, 1, 0],
            [2, 2, 0],
            [1, 2, 0],
        ],
        dtype=float,
    )


def face(**options):
    return Face((0, 1, 2, 3), ((4, 5, 6, 7),), **options)


class OuterOnlyVertices(np.ndarray):
    def __new__(cls, values):
        result = np.asarray(values).view(cls)
        result.original = result
        return result

    def __array_finalize__(self, source):
        self.original = getattr(source, "original", None)

    def __getitem__(self, index):
        if (
            self is self.original
            and isinstance(index, (list, np.ndarray))
            and np.any(np.asarray(index) >= 4)
        ):
            raise AssertionError("Rejected outer plane must not read hole vertices")
        return super().__getitem__(index)


@dataclass(frozen=True)
class NoProjectedUVs(Face):
    def __post_init__(self):
        super().__post_init__()
        if self.uvs:
            raise ValueError("Subclass does not accept projected UVs")


class PolygonPlaneTests(unittest.TestCase):
    def test_hole_reorientation_keeps_authored_uvs_and_immutable_metadata(self):
        uv = (
            ((0.1, 0.2), (4, 0), (3.8, 5), (0.2, 5.6)),
            ((0.8, 1.3), (1.1, 1.5), (1.4, 1.9), (0.7, 1.7)),
        )
        extra = tuple(tuple((u + 2, v - 3) for u, v in ring) for ring in uv)
        source = face(
            material=37,
            tags={"extrude.side.outer"},
            uvs=uv,
            group=9,
            uv_sets=((3, extra),),
        )
        normal, polygon, _coordinates, prepared = plane(courtyard(), source)
        np.testing.assert_array_equal(normal, [0, 0, 1])
        self.assertEqual(polygon.area, 23)
        self.assertIsNot(prepared, source)
        self.assertEqual(prepared.holes, ((7, 6, 5, 4),))
        self.assertEqual(prepared.uvs, (uv[0], uv[1][::-1]))
        self.assertEqual(prepared.uv_sets, ((3, (extra[0], extra[1][::-1])),))
        self.assertEqual((prepared.material, prepared.group), (37, 9))
        self.assertEqual(prepared.tags, frozenset({"extrude.side.outer"}))
        self.assertEqual(
            (source.holes, source.uvs, source.uv_sets),
            (((4, 5, 6, 7),), uv, ((3, extra),)),
        )
        with self.assertRaises(FrozenInstanceError):
            prepared.material = 99

    def test_derived_uvs_match_physical_corners_with_both_array_layouts(self):
        for dtype in (np.float32, np.float64):
            for order in ("C", "F"):
                with self.subTest(dtype=dtype, order=order):
                    vertices = np.array(courtyard(), dtype=dtype, order=order)
                    _normal, _polygon, _coordinates, prepared = plane(vertices, face())
                    for ring, values in zip(
                        (prepared.outer, *prepared.holes), prepared.uvs, strict=True
                    ):
                        np.testing.assert_array_equal(values, vertices[list(ring), :2])

    def test_planarity_boundary_and_explicit_warp_permission_stay_distinct(self):
        tolerance = 1e-9
        for height in (0, np.nextafter(tolerance, 0), tolerance):
            vertices = courtyard()
            vertices[4, 2] = height
            plane(vertices, face())
        vertices = courtyard()
        vertices[4, 2] = np.nextafter(tolerance, np.inf)
        with self.assertRaisesRegex(ValueError, "requires planar faces"):
            plane(vertices, face())
        normal, polygon, _coordinates, _prepared = plane(
            vertices, face(), allow_warped=True
        )
        np.testing.assert_array_equal(normal, [0, 0, 1])
        self.assertEqual(polygon.area, 23)

    def test_bad_outer_plane_fails_before_reading_hole_coordinates(self):
        vertices = courtyard()
        vertices[2, 2] = 0.5
        with self.assertRaisesRegex(ValueError, "requires planar faces"):
            plane(OuterOnlyVertices(vertices), face())

    def test_subclass_constructor_invariants_are_preserved(self):
        source = NoProjectedUVs((0, 1, 2, 3), ((4, 5, 6, 7),))
        with self.assertRaisesRegex(
            ValueError, "Subclass does not accept projected UVs"
        ):
            plane(courtyard(), source)

    def test_new_untrusted_face_values_still_require_validation(self):
        for values in (
            {"uvs": (((np.nan, 0),) * 4, ((0, 0),) * 4)},
            {"uv_sets": ((2, (((0, 0),) * 4, ((0, 0),) * 3)),)},
            {"material": 65536},
        ):
            with self.subTest(values=values), self.assertRaises(ValueError):
                face(**values)

    def test_compiled_courtyard_keeps_area_corner_uv_channels_and_attributes(self):
        vertices = courtyard()
        uv = tuple(
            tuple(tuple(vertices[index, :2] / [3, 2]) for index in ring)
            for ring in ((0, 1, 2, 3), (4, 5, 6, 7))
        )
        extra = tuple(tuple((u + 7, v - 9) for u, v in ring) for ring in uv)
        mesh, topology = compile_faces(
            vertices,
            [
                face(
                    material=37,
                    tags={"extrude.side.outer"},
                    group=9,
                    uvs=uv,
                    uv_sets=((3, extra),),
                )
            ],
        )
        points = mesh["vertices"][mesh["faces"]]
        normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        self.assertTrue(np.all(normals[:, 2] > 0))
        self.assertAlmostEqual(float(normals[:, 2].sum()) / 2, 23)
        self.assertTrue(np.all(mesh["materials"] == 37))
        self.assertTrue(np.all(mesh["groups"] == 9))
        self.assertTrue(np.all(mesh["face_tag_mask"] == 2))
        self.assertTrue(np.all(mesh["parts"] == 0))
        for index in mesh["faces"].ravel():
            expected = mesh["vertices"][index, :2] / [3, 2]
            np.testing.assert_array_equal(mesh["uv"][index], expected)
            np.testing.assert_array_equal(mesh["uv3"][index], expected + [7, -9])
        self.assertEqual(topology[0].holes, ((7, 6, 5, 4),))


if __name__ == "__main__":
    unittest.main()
