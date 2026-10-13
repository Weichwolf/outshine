"""Synthetic junction cases and actual cached profile invariants."""

import hashlib
import json
from pathlib import Path

import numpy as np
from leaf_terminal_profile import terminal_profile


def checks():
    p = np.array([[0.0, 0], [0, 0.001], [0, 0.002], [0, 0.003]])
    parents = np.array([-1, 0, 1, 2])
    radii = np.full(4, 0.001)
    edges = np.array([[0, 1], [1, 2], [2, 3]])
    free, chains = terminal_profile(p, parents, radii, p, edges, 0.0002)
    t = np.array([1, 2 / 3, 1 / 3, 0])
    expected = 0.00005 + 0.00095 * t**2 * (3 - 2 * t)
    assert np.allclose(free, expected, atol=1e-18, rtol=0)
    connected = np.vstack((p, [0, 0.004]))
    attached, _ = terminal_profile(
        p, parents, radii, connected, np.vstack((edges, [3, 4])), 0.0002
    )
    assert attached[-1] == 0.0002 and attached[0] == radii[0]
    split = np.vstack((connected, [0.001, 0.003]))
    preserved, _ = terminal_profile(
        p, parents, radii, split, np.vstack((edges, [3, 4], [3, 5])), 0.0002
    )
    assert np.array_equal(preserved, radii)
    interior = np.vstack((p, [0, 0.0025], [0.001, 0.0025]))
    interior_edges = np.array([[0, 1], [1, 2], [2, 4], [4, 3], [4, 5]])
    preserved, _ = terminal_profile(p, parents, radii, interior, interior_edges, 0.0002)
    assert np.array_equal(preserved, radii)
    return {
        "free_profile": True,
        "attached_minor_radius": True,
        "branching_tip_preserved": True,
        "interior_junction_preserved": True,
        "chain_length_m": chains[0]["length_m"],
    }


def main():
    folder = Path("build/experiments/vegetation/terminal-leaves")
    products = []
    for index in range(3):
        recipe = json.loads((folder / f"developed-{index}-material.json").read_text())
        path = folder / recipe["material_radius_profile"]
        assert (
            hashlib.sha256(path.read_bytes()).hexdigest()
            == recipe["material_radius_profile_sha256"]
        )
        with np.load(path) as profile, np.load(folder / recipe["vein_plan"]) as base:
            before, after = base["radii"], profile["radii"]
            assert (
                before.shape == after.shape
                and np.all(after > 0)
                and np.all(after <= before)
            )
            assert before[0] == after[0]
            with np.load(folder / recipe["minor_network"]) as network:
                degree = np.bincount(
                    network["edges"].ravel(), minlength=len(network["positions"])
                )
                junctions = network["positions"][degree > 2]
            a = base["positions"][base["parents"][1:]]
            v = base["positions"][1:] - a
            count = 0
            for junction in junctions:
                t = np.sum((junction - a) * v, axis=1) / np.sum(v * v, axis=1)
                inside = (
                    (t >= 0)
                    & (t <= 1)
                    & (np.linalg.norm(a + t[:, None] * v - junction, axis=1) <= 1e-12)
                )
                old = before[base["parents"][1:]] * (1 - t) + before[1:] * t
                new = after[base["parents"][1:]] * (1 - t) + after[1:] * t
                assert np.allclose(old[inside], new[inside], atol=1e-15, rtol=0)
                count += int(np.any(inside))
            products.append(
                {
                    "changed_radii": int(np.count_nonzero(before != after)),
                    "junctions_independently_preserved": count,
                }
            )
    report = {"checks": checks(), "products": products}
    (folder / "profile-checks.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
