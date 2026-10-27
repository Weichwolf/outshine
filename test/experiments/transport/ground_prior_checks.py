"""Necessary whole-port bounds, conservative frame alternatives and fit bypass."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from ground_prior_examples import examples
from outshine_ref.contracts import Mesh
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._ground import fit_ground
from outshine_ref.transport.canonical_ports._ground_prior import (
    incompatible,
    prior_contradiction,
)
from outshine_ref.transport.port_heights import PortHeightInput


def source(frames=None):
    vertices = np.array(
        [[0, 0, 0], [0, 1, 10], [1, 0, 0], [1, 1, 10], [2, 0, 0], [2, 1, 10]],
        dtype=float,
    )
    faces = np.array([[0, 2, 1], [2, 3, 1], [2, 4, 3], [4, 5, 3]])
    return PortHeightInput(
        Mesh(vertices, faces),
        np.array([0, 0, 1, 1]),
        np.array([0, 1]),
        np.array([[2, 3]]),
        np.array([[0, 1]]),
        np.tile(np.eye(2), (4, 1, 1)) if frames is None else frames,
    )


class GroundPriorChecks(unittest.TestCase):
    def test_actual_complete_port_yields_a_negative_certificate(self):
        witness, work = prior_contradiction(source(), 0.5, PortBudget())
        self.assertEqual(witness["vertices"], (2, 3))
        self.assertEqual(witness["possible_frames"], 1)
        self.assertEqual(
            work,
            {"ports": 1, "face_visits": 4, "frame_checks": 1, "state": "contradiction"},
        )

    def test_one_compatible_frame_per_part_prevents_unsafe_rejection(self):
        frames = np.tile(np.eye(2), (4, 1, 1))
        frames[[1, 3]] *= 0.01
        witness, work = prior_contradiction(source(frames), 0.5, PortBudget())
        self.assertIsNone(witness)
        self.assertEqual(work["state"], "no_conclusion")

    def test_dual_frame_is_transposed_before_inversion(self):
        self.assertFalse(incompatible((1, 2, 0, 1), (0, 0, 0), (1, 0, 3), 0.5))

    def test_exact_border_and_extra_reserve_do_not_reject_feasible_data(self):
        self.assertFalse(incompatible((1, 0, 0, 1), (0, 0, 0), (1, 0, 2), 0.5))
        self.assertFalse(incompatible((1, 0, 0, 1), (0, 0, 0), (1, 0, 2.000001), 0.5))
        self.assertTrue(incompatible((1, 0, 0, 1), (0, 0, 0), (1, 0, 2.0001), 0.5))

    def test_work_exhaustion_never_invents_a_certificate(self):
        frames = np.tile(np.eye(2), (4, 1, 1))
        frames[[1, 3]] *= 2
        budget = PortBudget()
        budget = replace(budget, surface=replace(budget.surface, max_queries=1))
        witness, work = prior_contradiction(source(frames), 0.5, budget)
        self.assertIsNone(witness)
        self.assertEqual(work["state"], "work_limit")
        self.assertEqual(work["frame_checks"], 1)

    def test_existing_zero_time_budget_is_preserved(self):
        budget = PortBudget()
        budget = replace(budget, surface=replace(budget.surface, time_limit_s=0))
        field, report = fit_ground(source(), 0.5, "cone", budget)
        self.assertIsNone(field)
        self.assertEqual(report["reason"], "port_time_budget")

    def test_negative_certificate_avoids_the_c1_solver(self):
        with patch(
            "outshine_ref.transport.canonical_ports._ground.plan_port_heights",
            side_effect=AssertionError("unnecessary C1 solver"),
        ):
            field, report = fit_ground(source(), 0.5, "cone", PortBudget())
        self.assertIsNone(field)
        self.assertEqual(report["reason"], "ground_prior_error_limit")
        self.assertEqual(report["prior_preflight"]["work"]["state"], "contradiction")

    def test_successful_affine_path_skips_preflight(self):
        with patch(
            "outshine_ref.transport.canonical_ports._ground.prior_contradiction",
            side_effect=AssertionError("successful affine path changed"),
        ):
            field, report = fit_ground(source(), 20, "auto", PortBudget())
        self.assertIsNotNone(field)
        self.assertEqual(report["selected_method"], "affine")

    def test_non_port_prior_is_not_treated_as_a_protected_edge(self):
        inputs = source()
        vertices = inputs.surface.vertices.copy()
        vertices[:, 2] = 0
        vertices[0, 2] = 100
        inputs = replace(inputs, surface=Mesh(vertices, inputs.surface.triangles))
        witness, work = prior_contradiction(inputs, 0.5, PortBudget())
        self.assertIsNone(witness)
        self.assertEqual(work["face_visits"], 0)


class ActualGroundPriorChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inputs = {name: inputs for name, inputs, _ in examples()}

    def test_actual_chain_rejects_before_any_c1_fit(self):
        with patch(
            "outshine_ref.transport.canonical_ports._ground.plan_port_heights",
            side_effect=AssertionError("actual impossible source reached solver"),
        ):
            field, report = fit_ground(self.inputs["chain"], 0.5, "auto", PortBudget())
        self.assertIsNone(field)
        witness = report["prior_preflight"]["witness"]
        self.assertEqual(witness["vertices"], (4, 5))
        self.assertEqual(report["prior_preflight"]["work"]["frame_checks"], 1)

    def test_actual_star_remains_without_a_negative_certificate(self):
        witness, work = prior_contradiction(self.inputs["star"], 0.5, PortBudget())
        self.assertIsNone(witness)
        self.assertEqual(work["ports"], 8)
        self.assertEqual(work["frame_checks"], 0)


if __name__ == "__main__":
    unittest.main()
