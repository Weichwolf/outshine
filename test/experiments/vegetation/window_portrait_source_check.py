"""Actual frozen source and corrupt on-disk window counterexamples, explicit inputs."""

import argparse
import json
import tempfile
from pathlib import Path

import numpy as np
from window_portrait_source import source


def check(whole, execution, catalog, output):
    _, axes, windows, _, evidence = source(whole, execution, catalog)
    cases = []
    with tempfile.TemporaryDirectory(
        prefix="outshine-vegetation-window-input-", dir="/tmp"
    ) as name:
        root = Path(name)
        manifest = json.loads((whole / "plant-product.json").read_text())
        for filename in ("plant-product.json", manifest["recipe"]):
            (root / filename).symlink_to((whole / filename).resolve())
        for field, index, replacement in (
            ("axis_ids", 0, len(axes["origins"])),
            ("weights", 0, 0.0),
            ("first_needle_ids", 0, 1),
            ("frames", (0, 0, 0), np.nan),
        ):
            values = {k: a.copy() if k == field else a for k, a in windows.items()}
            values[field][index] = replacement
            np.savez_compressed(root / "windows.npz", **values)
            try:
                source(root, execution, catalog)
            except (ValueError, AssertionError) as error:
                cases.append(
                    {
                        "field": field,
                        "rejected": True,
                        "reason": str(error).splitlines()[0],
                    }
                )
            else:
                raise AssertionError("corrupted actual windows accepted: " + field)
        values = dict(windows)
        values["frames"] = windows["frames"].copy()
        values["frames"][0, :, 0] *= -1
        np.savez_compressed(root / "windows.npz", **values)
        try:
            source(root, execution, catalog)
        except ValueError as error:
            cases.append(
                {"field": "reflected frame", "rejected": True, "reason": str(error)}
            )
        else:
            raise AssertionError("reflected physical source frame accepted")
        np.savez_compressed(root / "windows.npz", **windows)
        replay = source(root, execution, catalog)
        for key, value in windows.items():
            np.testing.assert_array_equal(value, replay[2][key])
        # Reject a different execution before consuming its decoded arrays.
        (root / "plant-product.json").unlink()
        manifest["input_sha256"] = dict(manifest["input_sha256"])
        key = next(k for k in manifest["input_sha256"] if k.endswith("/axes.npz"))
        manifest["input_sha256"][key] = "0" * 64
        (root / "plant-product.json").write_text(json.dumps(manifest))
        try:
            source(root, execution, catalog)
        except ValueError as error:
            cases.append(
                {
                    "field": "foreign executed axes",
                    "rejected": True,
                    "reason": str(error),
                }
            )
        else:
            raise AssertionError("foreign cached execution accepted")
    result = {
        "identity": evidence["identity"],
        "axes": len(axes["origins"]),
        "windows": len(windows["origins"]),
        "represented_needles": int(windows["needle_counts"].sum()),
        "complete_source_and_exact_replay": True,
        "negative_actual_files": cases,
    }
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("whole", "execution", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    check(args.whole, args.execution, args.catalog, args.output)
