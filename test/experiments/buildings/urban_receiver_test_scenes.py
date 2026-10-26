"""Small analytic receiver scenes shared by independent transport controls."""

import argparse

import numpy as np
from test_urban_patches import example
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent


def one_box():
    parent = UrbanParent(
        np.ones((1, 1, 3)),
        np.array([0, 0, 1, 16, 16, 0.75, 0.15]),
        np.array([6.0, 12.0]),
        np.array([0.7, 0.3]),
    )
    resolved = {
        "columns": np.array([[0, 0]], np.int32),
        "spans": np.array([[0.0, 6.0]]),
        "offsets": np.array([0, 1], np.uint32),
        "lookup": np.array([[0]], np.int32),
        "low": np.array([0, 0]),
        "basis": np.eye(3),
        "shape": np.array([1, 1]),
        "height_bounds": np.array([0.0, 6.0]),
        "reference_origin_m": np.zeros(3),
        "header": {"pitch_m": 16.0, "footprint_fraction": 0.75},
    }
    origins = np.array([[8.0, 8.0, 50.0]])
    directions = np.array([[0.0, 0.0, -1.0]])
    profile = {"kind": "sun", "direction": [0, 0, 1], "intensity": 3, "ambient": 0.1}
    return parent, resolved, origins, directions, profile


def fixture():
    parent = UrbanParent(
        np.ones((2, 2, 3)),
        np.array([0, 0, 1, 48, 16, 0.75, 0.15]),
        np.array([6.0, 12.0, 24.0]),
        np.array([0.6, 0.3, 0.1]),
    )
    resolved = prepare_resolved(example())
    view = (np.array([16.0, 16, 300]), np.diag([1.0, -1, -1]), 300.0, 1280, 720)
    profile = {
        "kind": "sun",
        "direction": [0.45, -0.5, 0.7],
        "intensity": 3,
        "ambient": 0.1,
    }
    args = argparse.Namespace(samples=4, seed=None, batch=127, trace_memory=False)
    return parent, resolved, view, profile, args
