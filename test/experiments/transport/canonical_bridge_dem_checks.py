"""Bound real DEM checks and independent requested-port sets for bridge heights."""

import unittest
from dataclasses import replace

import numpy as np
from canonical_bridge_cases import bound_dem_bridge
from outshine_ref.transport.canonical_ports import derive_canonical_ports


class CanonicalBridgeDem(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source, cls.demands, terrain, cls.binding = bound_dem_bridge()
        cls.terrain = staticmethod(terrain)

    def derive(self, demands, source=None):
        return derive_canonical_ports(
            source or self.source,
            tuple(demands),
            self.terrain,
            terrain_binding=self.binding,
            maximum_prior_change_m=0.5,
            bridge_second_derivative_per_m=0.002,
        )

    def test_source_fixed_lift_and_pending_are_independent_of_core_order_and_subset(
        self,
    ):
        result = self.derive(self.demands)
        reversed_result = self.derive(
            self.demands[::-1],
            replace(
                self.source,
                nodes=self.source.nodes[::-1],
                spans=self.source.spans[::-1],
            ),
        )
        self.assertEqual(result.values, reversed_result.values[::-1])
        self.assertEqual(result.reasons, reversed_result.reasons[::-1])
        self.assertTrue(any(result.values))
        field = result.report["components"][0]["field"]
        lift = field["coarse_DEM_lift"]
        self.assertGreater(lift["amplitude_m"], 0)
        self.assertGreater(lift["maximum_observed_intrusion_before_m"], 2)
        self.assertLess(lift["maximum_observed_intrusion_after_m"], 1e-6)
        self.assertLess(lift["maximum_grade_bound"], field["maximum_source_grade"])
        self.assertLess(lift["maximum_second_derivative_bound_per_m"], 0.002)
        for demand, value, reason in zip(
            self.demands, result.values, result.reasons, strict=True
        ):
            one = self.derive([demand])
            self.assertEqual(one.values[0], value)
            self.assertEqual(one.reasons[0], reason)
            self.assertEqual(
                one.report["components"][0]["field"]["coarse_DEM_lift"], lift
            )
            if value is None:
                self.assertIn("bridge_port_below_bound_DEM", reason)
            else:
                height = float(
                    self.terrain(np.array([value.anchor_longitude_latitude]))[0]
                )
                self.assertGreaterEqual(value.height_m + 1e-6, height)
                self.assertLessEqual(abs(value.longitudinal_grade), 0.12)
                self.assertLessEqual(abs(np.tan(value.crossfall_rad)), 0.025)


if __name__ == "__main__":
    unittest.main()
