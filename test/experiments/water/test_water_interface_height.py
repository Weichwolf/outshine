"""Canonical source-pair inputs and independently fitted shared water fields."""

import dataclasses
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from test_water_contact_band import grid
from test_water_owner_inputs import EPOCH, receipts
from water_interface_height import (
    fit_interface_region,
    prepare_interface,
    restrict_interface_region,
)
from water_owner_inputs import fixed_interface_inputs, fixed_owner_inputs
from water_region_mesh import cook_region

EDGE = (4, 0, 6, 5)
CELLS = ((4, 5, 5), (4, 6, 5))


def interface_inputs(edge=EDGE, items=None, **options):
    return fixed_interface_inputs(
        edge,
        receipts() if items is None else items,
        epoch=EPOCH,
        model_revision="shared-pair-r1",
        **options,
    )


def make_owner(mask=None, polylines=None, **options):
    mesh, anchor, rows, bounds = grid()
    return prepare_interface(
        mesh,
        anchor,
        rows,
        bounds,
        shapely.box(-80, -30, 80, 30) if mask is None else mask,
        [np.array([[0, -30], [0, 30]])] if polylines is None else polylines,
        edge=EDGE,
        cells=CELLS,
        source_version="pair-source-r1",
        **options,
    )


def region(owner, side, offset=0, mask=None, **options):
    if mask is None:
        mask = (
            shapely.box(-80, -30, 0, 30) if side == 0 else shapely.box(0, -30, 80, 30)
        )
    return fit_interface_region(
        owner,
        mask,
        cell=CELLS[side],
        expected_source_version=owner.source_version,
        prior_offset_m=offset,
        **options,
    )


class InterfaceInputsTests(unittest.TestCase):
    def test_each_edge_has_one_union_of_both_fixed_cell_stencils(self):
        for edge, cells in ((EDGE, CELLS), ((4, 1, 5, 6), ((4, 5, 5), (4, 5, 6)))):
            result = interface_inputs(edge)
            plans = [
                fixed_owner_inputs(c, receipts(), epoch=EPOCH, model_revision="r1")
                for c in cells
            ]
            self.assertEqual(result.state, "ready")
            self.assertEqual(result.cells, cells)
            self.assertEqual(
                set(result.tiles), set(plans[0].tiles) | set(plans[1].tiles)
            )
            self.assertEqual(len(result.required), 24)
            self.assertEqual(result.work["receipt_lookups"], 24)
            subset = {k: receipts()[k] for k in reversed(result.required)}
            self.assertEqual(result, interface_inputs(edge, subset))

    def test_profile_arrival_and_version_do_not_depend_on_recipient_order(self):
        items = receipts()
        initial = interface_inputs(items=items)
        key = (CELLS[0], "water_lines")
        missing = dict(items)
        missing.pop(key)
        self.assertEqual(interface_inputs(items=missing).state, "pending_sources")
        self.assertIsNone(interface_inputs(items=missing).version)
        changed = dict(items)
        changed[key] = dataclasses.replace(changed[key], digest="new-profile")
        self.assertNotEqual(interface_inputs(items=changed).version, initial.version)
        outside = ((4, 3, 3), "water_lines")
        changed = dict(items)
        changed[outside] = dataclasses.replace(changed[outside], digest="irrelevant")
        self.assertEqual(interface_inputs(items=changed), initial)

    def test_pair_budget_and_projection_boundaries_precede_receipt_lookup(self):
        large = interface_inputs(radius=3)
        self.assertEqual(large.state, "input_tile_budget_needs_contacts")
        self.assertEqual(large.work["source_cells"], 56)
        self.assertEqual(large.work["receipt_lookups"], 0)
        for edge in ((4, 0, 0, 5), (4, 0, 16, 5), (4, 1, 5, 0), (4, 1, 5, 16)):
            result = interface_inputs(edge)
            self.assertEqual(result.state, "needs_projection_contacts")
            self.assertIsNone(result.version)
        for edge in ((4, 2, 5, 5), (4, 0, 17, 5), (4, 1, 16, 5)):
            with self.assertRaises(ValueError):
                interface_inputs(edge)


class InterfaceHeightTests(unittest.TestCase):
    def test_common_field_restriction_never_repeats_a_physical_solve(self):
        owner, report = make_owner()
        self.assertIsNotNone(owner, report)
        with patch(
            "water_interface_height.fit_physical_surface",
            side_effect=AssertionError("no additional height fit"),
        ):
            for side, mask in enumerate(
                (shapely.box(-80, -30, 0, 30), shapely.box(0, -30, 80, 30))
            ):
                local, result = restrict_interface_region(
                    owner,
                    mask,
                    cell=CELLS[side],
                    expected_source_version=owner.source_version,
                )
                self.assertIsNotNone(local, result)
                self.assertTrue(local.field.mesh is owner.witness)
                self.assertTrue(local.field.sampler is owner.common.sampler)
                self.assertTrue(local.field.tree is owner.common.tree)
                self.assertTrue(local.field.gradient is owner.common.gradient)
                self.assertEqual(result["local_physical_solves"], 0)
                np.testing.assert_array_equal(
                    local.profile.points, owner.profile.points
                )
                for refined in (False, True):
                    fine = cook_region(
                        local.field,
                        [mask],
                        cell=CELLS[side],
                        source_keys=(side,),
                        required_edges=(EDGE,),
                        profiles={EDGE: local.profile},
                        refine_interior=refined,
                    )
                    np.testing.assert_array_equal(
                        fine.mesh.vertices[fine.contacts[EDGE]],
                        owner.profile.points[owner.profile.segments],
                    )

    def test_distinct_interval_groupings_have_distinct_owner_versions(self):
        xy = np.array([[0, -30], [0, -10], [0, 10], [0, 30]])
        full, first = make_owner(polylines=[xy])
        split, second = make_owner(polylines=[xy[:2], xy[2:]])
        self.assertIsNotNone(full, first)
        self.assertIsNotNone(split, second)
        self.assertNotEqual(full.context, split.context)
        self.assertNotEqual(full.version, split.version)

    def test_independent_objectives_keep_real_shared_edges_and_parent_normals(self):
        owner, report = make_owner()
        self.assertIsNotNone(owner, report)
        frozen = owner.witness.vertices.copy()
        outputs = {}
        for side, offset in ((1, 0.25), (0, -0.25)):
            product, result = region(owner, side, offset)
            self.assertIsNotNone(product, result)
            self.assertTrue(result["feasibility_witness_audit"]["admitted"])
            self.assertTrue(result["physical_audit"]["admitted"])
            np.testing.assert_array_equal(product.profile.points, owner.profile.points)
            np.testing.assert_array_equal(
                product.profile.segments, owner.profile.segments
            )
            self.assertNotEqual(product.field.context, owner.profile.context)
            fine = cook_region(
                product.field,
                [product.field.mask],
                cell=CELLS[side],
                source_keys=(CELLS[side],),
                required_edges=(EDGE,),
                profiles={EDGE: product.profile},
            )
            outputs[side] = fine.mesh.vertices[fine.contacts[EDGE]]
            with self.assertRaisesRegex(ValueError, "different functional owner"):
                cook_region(
                    product.field,
                    [product.field.mask],
                    cell=CELLS[side],
                    source_keys=(CELLS[side],),
                    required_edges=(EDGE,),
                    profiles={EDGE: owner.profile},
                )
        np.testing.assert_array_equal(outputs[0], outputs[1])
        np.testing.assert_array_equal(owner.witness.vertices, frozen)
        self.assertFalse(owner.anchor.flags.writeable)
        self.assertFalse(owner.rows.flags.writeable)

    def test_local_prior_change_moves_free_heights_without_moving_the_interface(self):
        owner, report = make_owner()
        self.assertIsNotNone(owner, report)
        first, _ = region(owner, 0, -0.25)
        second, _ = region(owner, 0, 0.25)
        self.assertIsNotNone(first)
        self.assertIsNotNone(second)
        self.assertGreater(first.report["free_vertices"], 0)
        self.assertGreater(
            float(
                abs(
                    first.field.mesh.vertices[:, 2] - second.field.mesh.vertices[:, 2]
                ).max()
            ),
            0.01,
        )
        np.testing.assert_array_equal(first.profile.points, second.profile.points)
        self.assertNotEqual(first.field.context, second.field.context)

    def test_stale_source_frame_and_witness_are_refused_before_a_local_fit(self):
        owner, report = make_owner()
        self.assertIsNotNone(owner, report)
        with self.assertRaisesRegex(ValueError, "stale interface"):
            fit_interface_region(
                owner, owner.mask, cell=CELLS[0], expected_source_version="old"
            )
        with self.assertRaisesRegex(ValueError, "stale interface"):
            restrict_interface_region(
                owner, owner.mask, cell=CELLS[0], expected_source_version="old"
            )
        for changed in (
            dataclasses.replace(owner, anchor=owner.anchor + [1, 0]),
            dataclasses.replace(
                owner,
                witness=Mesh(
                    owner.witness.vertices + [0, 0, 0.1], owner.witness.triangles
                ),
            ),
        ):
            with self.assertRaisesRegex(ValueError, "context mismatch"):
                region(changed, 0)
        with self.assertRaisesRegex(ValueError, "interface cell"):
            fit_interface_region(
                owner,
                owner.mask,
                cell=(4, 7, 5),
                expected_source_version=owner.source_version,
            )

    def test_hole_on_the_interface_preserves_two_separate_water_intervals(self):
        hole = shapely.box(-20, -15, 20, 15)
        mask = shapely.box(-80, -30, 80, 30).difference(hole)
        lines = [np.array([[0, -30], [0, -15]]), np.array([[0, 15], [0, 30]])]
        owner, report = make_owner(mask, lines)
        self.assertIsNotNone(owner, report)
        for side in (0, 1):
            own = (
                shapely.box(-80, -30, 0, 30)
                if side == 0
                else shapely.box(0, -30, 80, 30)
            ).difference(hole)
            fitted, result = region(owner, side, mask=own)
            self.assertIsNotNone(fitted, result)
            fine = cook_region(
                fitted.field,
                [own],
                cell=CELLS[side],
                source_keys=(side,),
                required_edges=(EDGE,),
                profiles={EDGE: fitted.profile},
            )
            union = shapely.union_all(
                shapely.polygons(fine.mesh.vertices[fine.mesh.triangles, :2])
            )
            self.assertEqual(union.intersection(hole).area, 0)
            self.assertEqual(union.symmetric_difference(own).area, 0)
        with self.assertRaisesRegex(ValueError, "precise water mask"):
            make_owner(mask)

    def test_bad_contact_geometry_and_budget_fail_locally(self):
        with self.assertRaisesRegex(ValueError, "interface budget"):
            make_owner(band_face_limit=1)
        for lines in (
            [],
            [np.array([[0, 0], [0, 0]])],
            [np.array([[0, 0], [np.nan, 1]])],
        ):
            with self.assertRaises(ValueError):
                make_owner(polylines=lines)
        owner, report = make_owner()
        self.assertIsNotNone(owner, report)
        with self.assertRaisesRegex(ValueError, "regional coarse-facet budget"):
            region(owner, 0, face_limit=1)
        with self.assertRaisesRegex(ValueError, "pair water footprint"):
            region(owner, 0, mask=shapely.box(-100, -30, 0, 30))
        valid, result = region(owner, 1)
        self.assertIsNotNone(valid, result)


if __name__ == "__main__":
    unittest.main()
