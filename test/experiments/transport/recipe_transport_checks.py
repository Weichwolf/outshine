"""Actual whole-source recipe ports and physical failures beyond formal graph types."""

import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from recipe_graph import Prepared, external
from recipe_graph_data import Budget, Files, canonical, digest, plain
from recipe_transport_inputs import SOURCE_TYPE, SOUTH, inputs_for
from recipe_transport_operators import (
    CAPABILITIES,
    PROFILE,
    ROOT,
    body_operator,
    checked_body,
    operators,
    prepare,
)
from recipe_transport_values import descriptor, payload_bytes, sealed
from stair_junction_audit import norm
from stair_junction_body import build
from stair_junction_solve import solve


def changed_source(product, value, **metadata):
    value = sealed(value)
    return external(
        SOURCE_TYPE,
        value,
        descriptor(value),
        payload_bytes(value),
        dict(product.metadata) | metadata,
    )


class SourceSnapshots(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = inputs_for()["source"]

    def test_owned_source_arrays_and_attributes_cannot_be_changed(self):
        xy = self.source.value["rows"][0]["xy"]
        with self.assertRaises(ValueError):
            xy.flags.writeable = True
        with self.assertRaises(TypeError):
            self.source.value["rows"][0]["original"]["bridge"] = False
        original = np.array([1.0, 2.0])
        snapshot = sealed(original)
        original[:] = 3
        np.testing.assert_array_equal(snapshot, [1, 2])

    def test_neighbors_dem_parameters_and_actual_array_dtypes_invalidate(self):
        original = self.source.value
        cases = []
        neighbors = list(original["context"]["sources"])
        neighbor = neighbors[-1]
        neighbors[-1] = dict(
            neighbor,
            properties=dict(neighbor["properties"], name="changed retained neighbor"),
        )
        context = dict(original["context"], sources=neighbors)
        cases.append(dict(original, context=context))
        windows = list(original["dem_windows"])
        window = dict(windows[0])
        arrays = dict(window["arrays"])
        heights = arrays["heights_m"].copy()
        heights.flat[0] += 0.25
        window["arrays"] = dict(arrays, heights_m=heights)
        windows[0] = window
        cases.append(dict(original, dem_windows=windows))
        cases.append(
            dict(original, selection=dict(original["selection"], variant="new"))
        )
        rows = list(original["rows"])
        rows[0] = dict(rows[0], xy=rows[0]["xy"].astype(np.float32))
        cases.append(dict(original, rows=rows))
        for value in cases:
            self.assertNotEqual(
                changed_source(self.source, value).fingerprint, self.source.fingerprint
            )

    def test_required_fourth_dem_window_cannot_be_omitted(self):
        recipe = json.loads(SOUTH.read_text())
        recipe["elevation"] = recipe["elevation"][:3]
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_transport-"
        ) as tmp:
            path = Path(tmp) / "input.json"
            path.write_text(json.dumps(recipe))
            for row in recipe["elevation"]:
                (path.parent / row["window_file"]).symlink_to(
                    SOUTH.with_name(row["window_file"])
                )
            with self.assertRaisesRegex(ValueError, "beyond declared DEM tiles"):
                inputs_for(path)

    def test_wrong_frame_units_datum_or_actual_precision_fail_before_layout(self):
        for metadata in (
            {"frame_id": "wrong"},
            {"units": "cm"},
            {"height_datum": "other"},
        ):
            source = changed_source(self.source, self.source.value, **metadata)
            with (
                patch(
                    "recipe_transport_operators.layout",
                    side_effect=AssertionError("must reject before layout"),
                ),
                self.assertRaises(ValueError),
            ):
                prepare().evaluate({"source": source}, outputs=("plan",))
        rows = list(self.source.value["rows"])
        rows[0] = dict(rows[0], xy=rows[0]["xy"].astype(np.float32))
        source = changed_source(self.source, dict(self.source.value, rows=rows))
        with self.assertRaisesRegex(ValueError, "actual finite binary64"):
            prepare().evaluate({"source": source}, outputs=("plan",))

    def test_product_budget_precedes_every_domain_callback(self):
        recipe = prepare(budget=Budget(max_product_bytes=self.source.nbytes))
        with (
            patch(
                "recipe_transport_operators.layout",
                side_effect=AssertionError("budget too late"),
            ),
            self.assertRaisesRegex(ValueError, "reservation exceeds budget"),
        ):
            recipe.evaluate({"source": self.source}, outputs=("plan",))

    def test_real_definition_errors_precede_domain_callbacks(self):
        recipe_dir = Path(__file__).resolve().parents[1] / "assets/recipes"
        root = json.loads(
            (recipe_dir / "transport-checked-network-v1.json").read_text()
        )
        profile = json.loads(
            (recipe_dir / "transport-profile-network-v1.json").read_text()
        )

        def prepare_data(root_value, profile_value, capabilities=CAPABILITIES):
            with tempfile.TemporaryDirectory(
                prefix="outshine-tests.outshine_transport-"
            ) as tmp:
                a, b = Path(tmp) / "root.json", Path(tmp) / "profile.json"
                a.write_bytes(canonical(root_value))
                b.write_bytes(canonical(profile_value))
                return Prepared(
                    ROOT,
                    Files({ROOT: a, PROFILE: b}),
                    operators(),
                    capabilities=capabilities,
                )

        version = json.loads(json.dumps(root))
        version["dependencies"]["profile"]["version"] = "absent"
        cycle = json.loads(json.dumps(profile))
        cycle["dependencies"] = {
            "cycle": {
                "id": ROOT.id,
                "version": ROOT.version,
                "inputs": {"source": "source"},
            }
        }
        wrong = json.loads(json.dumps(profile))
        wrong["nodes"]["profiles"]["inputs"]["source"] = {"input": "absent"}
        with patch(
            "recipe_transport_operators.layout",
            side_effect=AssertionError("invalid definition evaluated"),
        ):
            for a, b, capabilities in (
                (version, profile, CAPABILITIES),
                (root, cycle, CAPABILITIES),
                (root, wrong, CAPABILITIES),
                (root, profile, ()),
            ):
                with self.assertRaises(ValueError):
                    prepare_data(a, b, capabilities)

    def test_profile_invalidation_keeps_the_same_two_recipe_definitions(self):
        recipe = prepare()
        before = recipe.evaluate({"source": self.source}, outputs=("plan",))
        rows = list(self.source.value["rows"])
        prior = rows[-1]["prior"].copy()
        prior[-1] += 0.05
        rows[-1] = dict(rows[-1], prior=prior)
        source = changed_source(self.source, dict(self.source.value, rows=rows))
        after = recipe.evaluate({"source": source}, outputs=("plan",))
        self.assertEqual(before.definition_fingerprint, after.definition_fingerprint)
        self.assertNotEqual(before.instance_fingerprint, after.instance_fingerprint)
        self.assertNotEqual(
            before.outputs["plan"].fingerprint, after.outputs["plan"].fingerprint
        )


class ActualTransport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inputs = inputs_for()
        cls.recipe = prepare()
        with patch(
            "recipe_transport_operators.build",
            side_effect=AssertionError("plan request built a body"),
        ):
            cls.plan_run = cls.recipe.evaluate(cls.inputs, outputs=("plan",))
        cls.profile = cls.plan_run.outputs["plan"]
        _, cls.data, _ = build(
            cls.profile.value["layout"], cls.profile.value["heights"]
        )
        cls.body = body_operator(
            {}, {"source": cls.inputs["source"], "plan": cls.profile}
        )["body"]

    def test_plan_request_evaluates_one_operator_and_no_body(self):
        self.assertEqual(self.plan_run.stats["evaluated_nodes"], 1)
        self.assertEqual(self.plan_run.stats["loaded_definitions"], 2)
        self.assertEqual(len(self.inputs["source"].value["rows"]), 21)
        self.assertEqual(len(self.profile.value["outer_profiles_unconnected"]), 7)
        self.assertNotIn("body", self.plan_run.outputs)

    def test_body_has_real_double_float32_contacts_underpasses_and_closed_links(self):
        proof = self.body.value["proof"]
        self.assertEqual(len(proof["protected_profiles"]), 6)
        self.assertEqual(len(proof["clearance"]), 6)
        self.assertEqual(sum(p["risers"] for p in proof["treads"].values()), 24)
        self.assertGreater(proof["float32_volume_m3"], 0)
        self.assertEqual(self.body.value["float32_vertices"].dtype, np.dtype("float32"))
        self.assertNotIn("elapsed_ms", canonical(proof).decode())
        with self.assertRaises(ValueError):
            self.body.value["float32_vertices"].flags.writeable = True

    def test_source_binding_is_checked_before_body_construction(self):
        bad = replace(
            self.profile, value=dict(self.profile.value, source_fingerprint="0" * 64)
        )
        with (
            patch(
                "recipe_transport_operators.build",
                side_effect=AssertionError("wrong source built"),
            ),
            self.assertRaisesRegex(ValueError, "different source snapshot"),
        ):
            body_operator({}, {"source": self.inputs["source"], "plan": bad})

    def test_actual_wrong_end_cap_is_not_a_checked_body(self):
        source = self.inputs["source"]
        value = dict(source.value, protected_profiles=((20, 15, -1),))
        source = changed_source(source, value)
        profile = replace(
            self.profile,
            value=dict(self.profile.value, source_fingerprint=source.fingerprint),
        )
        with self.assertRaisesRegex(ValueError, "exactly one actual shared component"):
            checked_body(source, profile, self.data)

    def test_unassigned_or_foreign_face_masks_are_rejected(self):
        for bad in (0, 1 << 21):
            masks = self.data["face_sources"].copy()
            masks[0] = bad
            with self.assertRaisesRegex(ValueError, "source ownership"):
                checked_body(
                    self.inputs["source"],
                    self.profile,
                    dict(self.data, face_sources=masks),
                )

    def test_downcast_xyz_cannot_hide_behind_a_binary64_port(self):
        bad = dict(
            self.data, body_vertices=self.data["body_vertices"].astype(np.float32)
        )
        with self.assertRaisesRegex(ValueError, "cannot hide downcast"):
            checked_body(self.inputs["source"], self.profile, bad)

    def test_real_double_valid_profile_is_rejected_after_actual_float32_cast(self):
        plan = self.profile.value["layout"]
        heights, note = solve(plan, precision=False)
        self.assertIsNotNone(heights, note)
        _, data, _ = build(plan, heights)
        owner = 3
        faces = data["top_faces"][(data["face_sources"] & (1 << owner)) != 0]
        vertices = data["body_vertices"]
        self.assertLess(norm(Mesh(vertices, faces)), 0.06)
        self.assertGreater(
            norm(Mesh(vertices.astype(np.float32).astype(float), faces)), 0.06
        )
        with (
            patch("recipe_transport_operators.solve", return_value=(heights, note)),
            self.assertRaisesRegex(
                ValueError, "actual saved source cap exceeds requested norm"
            ),
        ):
            self.recipe.evaluate(self.inputs, outputs=("body",))

    def test_same_definitions_are_not_expanded_per_source(self):
        definitions = self.recipe.definition_fingerprint
        self.assertEqual(len(definitions), 64)
        self.assertEqual(len(self.recipe.definitions), 2)
        self.assertEqual(len({key.id for key in self.recipe.definitions}), 2)
        self.assertNotIn(
            "39881120",
            canonical(
                plain(self.recipe.definitions[next(iter(self.recipe.definitions))])
            ).decode(),
        )
        self.assertEqual(len(digest(descriptor(self.body.value))), 64)


if __name__ == "__main__":
    unittest.main()
