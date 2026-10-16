"""Real contact semantics, Float32 failures and bounded partial station planning."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder
from stair_support_audit import Reference, body_caps, domain, quantized
from stair_support_plan import admit, footprint, plan, stations


def rectangle(x0, y0, x1, y1, z):
    xy = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]], float)
    values = z(xy) if callable(z) else np.full(4, z)
    return Mesh(np.c_[xy, values], np.array([[0, 1, 2], [0, 2, 3]]))


def surfaces():
    lower = rectangle(
        990,
        990,
        1010,
        1010,
        lambda xy: 165.123 + 0.12 * (xy[:, 0] - 1000) + 0.23 * (xy[:, 1] - 1000),
    )
    upper = Mesh(lower.vertices + [0, 0, 3], lower.triangles)
    shape = shapely.box(1000.123, 1000.234, 1000.923, 1000.684)
    product, note = SupportVolumeBuilder(lower, upper).build(
        shape, minimum_height_m=0.1001
    )
    if product is None:
        raise ValueError(str(note))
    return lower, upper, product


def crossing():
    rows = [
        {
            "source_id": "bridge",
            "xy": np.array([[0, 0], [40, 0]]),
            "original": {"bridge": True},
        },
        {
            "source_id": "ground",
            "xy": np.array([[6, -5], [6, 5]]),
            "original": {"bridge": False},
        },
    ]
    bridge, road = rectangle(0, -1, 40, 1, 3), rectangle(5, -5, 7, 5, 0.25)
    faces = np.vstack([bridge.triangles, road.triangles + 4])
    data = {
        "body_vertices": np.vstack([bridge.vertices, road.vertices]),
        "top_faces": faces,
        "bottom_faces": faces,
        "face_sources": np.array([1, 1, 2, 2]),
    }
    return rows, data, rectangle(-5, -10, 45, 10, 0)


class WholeContacts(unittest.TestCase):
    def test_sloped_contact_and_actual_rounding_have_a_derived_envelope(self):
        lower, upper, product = surfaces()
        note = body_caps(product)
        self.assertGreater(note["float32"]["minimum_height_m"], 2.99)
        for cap, source in (
            (product.lower_contact, lower),
            (product.upper_contact, upper),
        ):
            result = Reference(source).check(cap)
            self.assertLess(result["double_contact_error_m"], 1e-10)
            self.assertGreater(result["derived_float32_contact_bound_m"], 0)
            self.assertLessEqual(
                result["float32_contact_error_m"],
                result["derived_float32_contact_bound_m"] + 1e-10,
            )

    def test_corner_only_contact_misses_a_real_interior_peak(self):
        flat = rectangle(-1, -1, 1, 1, 0)
        outer = rectangle(-2, -2, 2, 2, 0)
        faces = []
        for i in range(4):
            j = (i + 1) % 4
            faces.extend([[i, j, j + 4], [i, j + 4, i + 4], [i + 4, j + 4, 8]])
        source = Mesh(
            np.vstack([outer.vertices, flat.vertices, [0, 0, 0.5]]), np.array(faces)
        )
        np.testing.assert_array_equal(source.vertices[4:8], flat.vertices)
        with self.assertRaisesRegex(ValueError, "Double contact differs"):
            Reference(source).check(flat)

    def test_whole_double_body_is_insufficient_when_float32_collapses(self):
        lower = rectangle(1e8 - 2, 1e8 - 2, 1e8 + 2, 1e8 + 2, 0)
        upper = Mesh(lower.vertices + [0, 0, 3], lower.triangles)
        product, _note = SupportVolumeBuilder(lower, upper).build(
            shapely.box(1e8 - 0.4, 1e8 - 0.225, 1e8 + 0.4, 1e8 + 0.225),
            minimum_height_m=0.1,
        )
        self.assertIsNotNone(product)
        with self.assertRaisesRegex(ValueError, "projection collapses"):
            body_caps(product)

    def test_moving_both_caps_does_not_preserve_contact(self):
        lower, _upper, product = surfaces()
        shifted = Mesh(
            product.lower_contact.vertices + [0, 0, 0.001],
            product.lower_contact.triangles,
        )
        with self.assertRaisesRegex(ValueError, "Double contact differs"):
            Reference(lower).check(shifted)

    def test_known_traffic_and_previous_piers_are_excluded_in_both_precisions(self):
        lower, upper, product = surfaces()
        shape = domain(product.lower_contact)
        empty = shapely.Polygon()
        for excluded, old, message in (
            ([shape, empty], [[], []], "selected source"),
            ([empty, shape], [[], []], "selected source"),
            ([empty, empty], [[shape], []], "earlier pier"),
            (
                [empty, empty],
                [[], [domain(quantized(product.lower_contact))]],
                "earlier pier",
            ),
        ):
            with self.assertRaisesRegex(ValueError, message):
                admit(product, Reference(lower), Reference(upper), excluded, old)

    def test_rounded_buffer_corners_cannot_understate_the_physical_distance(self):
        lower, upper, product = surfaces()
        shape = domain(product.lower_contact)
        x, y = shape.bounds[2:]
        angle = np.pi / 32
        near = shapely.Point(x + 0.1999 * np.cos(angle), y + 0.1999 * np.sin(angle))
        self.assertFalse(near.intersects(shape.buffer(0.2, quad_segs=8)))
        self.assertLess(near.distance(shape), 0.2)
        with self.assertRaisesRegex(ValueError, "selected source"):
            admit(product, Reference(lower), Reference(upper), [near, near], [[], []])


class BoundedStations(unittest.TestCase):
    def test_shift_recovers_a_real_excluded_station_without_losing_the_others(self):
        rows, road, lower = crossing()
        fixed, before = plan(rows, road, lower, method="fixed")
        shifted, after = plan(rows, road, lower, method="shifted")
        self.assertEqual((len(fixed), len(shifted)), (2, 3))
        self.assertEqual(
            before["stations"][0]["reason"], "other_selected_source_exclusion"
        )
        self.assertEqual(after["stations"][0]["shift_m"], -2)
        self.assertTrue(all(not r["reason"] for r in after["stations"]))

    def test_budget_exhaustion_keeps_a_complete_explicit_omission_ledger(self):
        rows, road, lower = crossing()
        bodies, report = plan(rows, road, lower, method="shifted", seconds=1e-9)
        self.assertFalse(bodies)
        self.assertEqual(len(report["stations"]), 3)
        self.assertTrue(
            all(
                r["reason"] == "support_time_or_trial_budget"
                for r in report["stations"]
            )
        )

    def test_station_and_finite_source_budgets(self):
        rows, _road, _lower = crossing()
        long = dict(rows[0], xy=np.array([[0, 0], [2000, 0]]))
        with self.assertRaisesRegex(ValueError, "station budget"):
            stations([long])
        for station in (1, 39):
            with self.assertRaisesRegex(ValueError, "outside finite source"):
                footprint(shapely.LineString(rows[0]["xy"]), station)


if __name__ == "__main__":
    unittest.main()
