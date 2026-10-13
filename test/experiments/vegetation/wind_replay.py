"""Fresh-process wind replay from cached assets, with no generator or mesh rebuild."""

import hashlib
import json
from pathlib import Path

import numpy as np
from wind_field import WindField
from wind_products import deform


def read_arrays(path):
    with np.load(path) as data:
        return {key: data[key] for key in data.files}


def main():
    root = Path("build/experiments/vegetation/wind")
    field = WindField(**read_arrays(root / "wind-plan.npz"))
    report = {"exact_replayed_products": 0, "all_edges_maximum_stretch": 0.0}
    for detail, times in (("near", ("rest", "0.5", "2")), ("coarse", ("0.5",))):
        for suffix in times:
            scenario = json.loads((root / f"wind-{detail}-{suffix}.json").read_text())
            for index, product in enumerate(scenario["products"]):
                mesh = read_arrays(root / f"{detail}-{index}-rest.npz")
                stored = read_arrays(root / f"{detail}-{index}-binding.npz")
                binding = (stored["values"], stored["gradients"])
                actual = deform(
                    field,
                    mesh,
                    binding,
                    scenario["wind_seconds"],
                    scenario["wind_strength"],
                )
                expected = read_arrays(root / product["mesh"])
                assert actual.keys() == expected.keys()
                assert all(np.array_equal(actual[k], expected[k]) for k in actual)
                report["exact_replayed_products"] += 1
                if suffix != times[0]:
                    continue
                rest = mesh["vertices"][mesh["faces"]]
                rest_edges = np.roll(rest, -1, axis=1) - rest
                rest_lengths = np.linalg.norm(rest_edges, axis=2)
                for seconds in np.linspace(0, 10, 41):
                    positions, _ = field.move(mesh["vertices"], binding, seconds)
                    points = positions[mesh["faces"]]
                    edges = np.roll(points, -1, axis=1) - points
                    ratio = np.linalg.norm(edges, axis=2) / rest_lengths
                    report["all_edges_maximum_stretch"] = max(
                        report["all_edges_maximum_stretch"], float(ratio.max())
                    )
    report["implementation_sha256"] = {
        name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
        for name in ("wind_replay.py", "wind_field.py", "wind_products.py")
    }
    (root / "replay.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
