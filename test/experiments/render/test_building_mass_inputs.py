"""Original source-frame heights, atomic unsupported forms and no native mesh reads."""

import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from build_building_mass import cook
from building_mass_inputs import describe, read_masses
from parent_surface_inputs import ParentInputs


def surface(shapes):
    return {
        "origin": [0, 0, 0],
        "east": [1, 0, 0],
        "north": [0, 1, 0],
        "up": [0, 0, 1],
        "minimum_height_m": 0,
        "shapes": shapes,
    }


def shape():
    return {
        "ring": [[0, 0], [1, 0], [1, 1], [0, 1]],
        "holes": [],
        "foot_m": 0,
        "sole_m": 0,
        "roof_kind": 0,
        "rise_m": 0,
        "seat_m": 0,
        "eaves_m": 5,
    }


class BuildingMassInputs(unittest.TestCase):
    def test_tilted_source_retains_actual_height_ranges_and_declared_wall_shift(self):
        angle = 0.1
        c, s = np.cos(angle), np.sin(angle)
        source = surface([shape()]) | {"east": [c, 0, s], "up": [-s, 0, c]}
        mass, shift = describe(17, source, np.eye(3), np.zeros(3))
        np.testing.assert_allclose(mass.bottom, [0, s], atol=1e-12, rtol=0)
        np.testing.assert_allclose(mass.roof, [5 * c, 5 * c + s], atol=1e-12, rtol=0)
        self.assertAlmostEqual(mass.footprint.area, c)
        self.assertAlmostEqual(shift, 5 * s)

    def test_one_unsupported_part_preserves_whole_source_as_original_need(self):
        valid = surface([shape()])
        invalid = surface([shape(), shape() | {"roof_kind": 6}])
        parent = SimpleNamespace(
            plans={
                "summaries": [
                    {
                        "supported": True,
                        "frame": [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]],
                    }
                ]
            },
            iter_forms=lambda _: [(33, [1, 2, 3], [valid, invalid, None])],
            form_reads=1,
            form_bytes=0,
            archive_sha256="analytic",
        )
        masses, frame, report = read_masses(parent, {33})
        self.assertEqual([s.source for s in masses], [1])
        self.assertEqual([n["source"] for n in report["needs"]], [2, 3])
        np.testing.assert_array_equal(frame["selected_sources"], [1, 2, 3])

    def test_original_builder_reads_only_index_and_one_selected_form_block(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        original_member, reads, parents = ParentInputs.member, [], []

        def member(parent, name):
            self.assertIn(name, {parent.metadata["index"], parent.forms[33]})
            if not parents:
                parents.append(parent)
            reads.append(name)
            return original_member(parent, name)

        with patch.object(ParentInputs, "member", member):
            arrays, report = cook(path, {33})
        self.assertEqual(len(reads), 2)
        self.assertNotIn("member", vars(parents[0]))
        self.assertEqual(len(arrays["selected_sources"]), 576)
        np.testing.assert_array_equal(
            np.sort(np.r_[arrays["group_sources"], arrays["needs_sources"]]),
            arrays["selected_sources"],
        )
        self.assertIn(2893, arrays["needs_sources"])
        self.assertGreater(len(arrays["group_sources"]), len(report["groups"]))
        self.assertEqual(report["read"]["form_reads"], 1)
        self.assertTrue(all(len(g["sources"]) >= 2 for g in report["groups"]))


if __name__ == "__main__":
    unittest.main()
