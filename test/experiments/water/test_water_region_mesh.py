"""Independent local jobs retain exact contacts and the whole root height field."""

import dataclasses
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from test_water_edge_profiles import owner
from water_edge_profiles import prepare_profile_owner, restrict_profile
from water_region_mesh import cook_region

A, B = (4, 5, 5), (4, 6, 5)
EDGE = (4, 0, 6, 5)


def prepared(mask=None):
    return prepare_profile_owner(
        owner(),
        anchor=[9.6, 47.2],
        mask=shapely.box(0, 0, 1000, 1000) if mask is None else mask,
        owner_version="independent-regions-v1",
    )


def cook(root, polygons, *, cell=A, profiles=None, **options):
    if profiles is None:
        profiles = {EDGE: restrict_profile(root, [np.array([[250, 0], [250, 1000]])])}
    return cook_region(
        root,
        polygons,
        cell=cell,
        source_keys=tuple(range(len(polygons))),
        required_edges=(EDGE,),
        profiles=profiles,
        **options,
    )


class WaterRegionMesh(unittest.TestCase):
    def test_four_independent_cells_share_the_same_multiway_corner(self):
        root = prepared()
        c, d = (4, 5, 6), (4, 6, 6)
        lines = {
            EDGE: [[500, 0], [500, 500]],
            (4, 1, 5, 6): [[0, 500], [500, 500]],
            (4, 0, 6, 6): [[500, 500], [500, 1000]],
            (4, 1, 6, 6): [[500, 500], [1000, 500]],
        }
        from water_shared_borders import adjacent_cells

        profiles = {
            edge: restrict_profile(root, [np.array(line)])
            for edge, line in lines.items()
        }
        masks = {
            A: shapely.box(0, 0, 500, 500),
            B: shapely.box(500, 0, 1000, 500),
            c: shapely.box(0, 500, 500, 1000),
            d: shapely.box(500, 500, 1000, 1000),
        }
        for index, cell in enumerate((d, A, c, B)):
            contacts = {
                edge: p for edge, p in profiles.items() if cell in adjacent_cells(edge)
            }
            product = cook_region(
                root,
                [masks[cell]],
                cell=cell,
                source_keys=(cell,),
                required_edges=tuple(contacts),
                profiles=contacts,
                refine_interior=bool(index % 2),
            )
            vertex = product.mesh.vertices[
                np.all(product.mesh.vertices[:, :2] == [500, 500], axis=1)
            ]
            np.testing.assert_array_equal(vertex, [[500, 500, 502.5]])
            self.assertEqual(len(product.contacts), 2)

    def test_independent_interior_density_keeps_exact_crease_and_shared_edges(self):
        root = prepared()
        frozen = root.mesh.vertices.copy()
        left = cook(root, [shapely.box(0, 0, 250, 1000)])
        right = cook(
            root, [shapely.box(250, 0, 1000, 1000)], cell=B, refine_interior=True
        )
        profile = restrict_profile(root, [np.array([[250, 0], [250, 1000]])])
        for product in (left, right):
            np.testing.assert_array_equal(
                product.mesh.vertices[product.contacts[EDGE]],
                profile.points[profile.segments],
            )
            vertices = product.mesh.vertices[product.mesh.triangles]
            # Every interior barycentre follows the analytic crease, not a chord.
            centres = vertices.mean(axis=1)
            np.testing.assert_allclose(
                centres[:, 2],
                500 + 0.005 * np.minimum(centres[:, 0], centres[:, 1]),
                atol=1e-12,
                rtol=0,
            )
            self.assertEqual(product.report["maximum_parent_plane_up_difference_m"], 0)
            self.assertFalse(product.parent_normals.flags.writeable)
            for index, normal in zip(
                product.parent_faces, product.parent_normals, strict=True
            ):
                slope = [-0.005, 0, 1] if index == 1 else [0, -0.005, 1]
                np.testing.assert_allclose(
                    normal, np.array(slope) / np.linalg.norm(slope), atol=1e-16, rtol=0
                )
        self.assertGreater(len(right.mesh.triangles), len(left.mesh.triangles))
        np.testing.assert_array_equal(root.mesh.vertices, frozen)

    def test_holes_disconnected_parts_and_every_source_identity_survive(self):
        outer = shapely.box(0, 0, 250, 1000)
        hole = shapely.box(20, 400, 100, 600)
        island = shapely.box(600, 600, 800, 800)
        first = outer.difference(hole)
        mask = shapely.union_all([first, island, shapely.box(250, 0, 1000, 1000)])
        root = prepared(mask)
        product = cook(root, [first, island])
        result = shapely.union_all(
            shapely.polygons(product.mesh.vertices[product.mesh.triangles, :2])
        )
        self.assertEqual(result.symmetric_difference(first.union(island)).area, 0)
        self.assertEqual(result.intersection(hole).area, 0)
        self.assertEqual(set(product.membership.members), {0, 1})
        self.assertEqual(product.source_keys, (0, 1))

    def test_job_and_source_arrival_order_do_not_change_the_product(self):
        root = prepared()
        polygons = [shapely.box(0, 0, 250, 500), shapely.box(0, 500, 250, 1000)]
        profiles = {
            EDGE: restrict_profile(
                root, [np.array([[250, 0], [250, 500], [250, 1000]])]
            )
        }
        first = cook(root, polygons, profiles=profiles)
        cook(root, [shapely.box(250, 0, 1000, 1000)], cell=B)
        reversed_sources = cook(root, polygons[::-1], profiles=profiles)
        np.testing.assert_array_equal(
            first.mesh.vertices, reversed_sources.mesh.vertices
        )
        np.testing.assert_array_equal(
            first.mesh.triangles, reversed_sources.mesh.triangles
        )
        np.testing.assert_array_equal(
            first.contacts[EDGE], reversed_sources.contacts[EDGE]
        )

    def test_missing_stale_and_corrupt_contacts_refuse_atomic_local_product(self):
        root = prepared()
        polygon = [shapely.box(0, 0, 250, 1000)]
        profile = restrict_profile(root, [np.array([[250, 0], [250, 1000]])])
        with self.assertRaisesRegex(ValueError, "missing or extra"):
            cook(root, polygon, profiles={})
        with self.assertRaisesRegex(ValueError, "different functional owner"):
            cook(
                root,
                polygon,
                profiles={EDGE: dataclasses.replace(profile, context="stale")},
            )
        with self.assertRaisesRegex(ValueError, "content digest"):
            cook(
                root,
                polygon,
                profiles={
                    EDGE: dataclasses.replace(profile, segments=profile.segments[:1])
                },
            )
        with self.assertRaisesRegex(ValueError, "nonempty complete"):
            cook(
                root,
                polygon,
                profiles={
                    EDGE: dataclasses.replace(profile, segments=profile.segments[:0])
                },
            )
        self.assertIsInstance(
            cook(root, [shapely.box(250, 0, 1000, 1000)], cell=B).mesh, Mesh
        )

    def test_context_includes_owner_frame_and_version(self):
        root = prepared()
        profile = restrict_profile(root, [np.array([[250, 0], [250, 1000]])])
        for changed in (
            prepare_profile_owner(
                root.mesh,
                anchor=[9.7, 47.2],
                mask=root.mask,
                owner_version="independent-regions-v1",
            ),
            prepare_profile_owner(
                root.mesh, anchor=root.anchor, mask=root.mask, owner_version="v2"
            ),
        ):
            with self.assertRaisesRegex(ValueError, "different functional owner"):
                cook(changed, [shapely.box(0, 0, 250, 1000)], profiles={EDGE: profile})

    def test_contact_vertices_alone_do_not_admit_an_interior_line(self):
        root = prepared()
        # Both cells' union contains these points, but the line is internal to A.
        profile = restrict_profile(root, [np.array([[0, 0], [1000, 1000]])])
        with self.assertRaisesRegex(ValueError, "not a local boundary edge"):
            cook(root, [root.mask], profiles={EDGE: profile})
        with self.assertRaisesRegex(ValueError, "does not belong"):
            cook(root, [root.mask], cell=(4, 7, 5), profiles={EDGE: profile})

    def test_unpublished_extra_boundary_subdivision_is_not_silently_accepted(self):
        root = prepared()
        with self.assertRaisesRegex(ValueError, "not a local boundary edge"):
            cook(root, [shapely.box(0, 0, 250, 500), shapely.box(0, 500, 250, 1000)])

    def test_mask_and_work_limits_refuse_locally_without_dropping_sources(self):
        root = prepared()
        polygon = [shapely.box(0, 0, 250, 1000)]
        for options, message in (
            ({"face_limit": 1}, "budget exhausted"),
            ({"vertex_limit": 1}, "vertex budget"),
            ({"candidate_limit": 1}, "intersection budget"),
            ({"coordinate_limit": 1}, "source polygons"),
            ({"face_limit": 0}, "positive regional"),
        ):
            with self.assertRaisesRegex(ValueError, message):
                cook(root, polygon, **options)
        root = prepared(shapely.box(0, 0, 900, 1000))
        with self.assertRaisesRegex(ValueError, "authoritative water mask"):
            cook(root, [shapely.box(0, 0, 1000, 1000)])


if __name__ == "__main__":
    unittest.main()
