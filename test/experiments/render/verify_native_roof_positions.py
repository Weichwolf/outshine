"""Verify the position recipe against original accepted single-source Shell products."""

import argparse
import hashlib
import json
import tempfile
from collections import Counter
from pathlib import Path

import numpy as np
from building_roof_fusion_selection import seed_groups
from cached_building_streams import STREAM_BYTES, product


def oriented_triangles(triangles):
    result = Counter()
    for triangle in triangles:
        points = [tuple(float(value) for value in point) for point in triangle]
        result[min(tuple(points[i:] + points[:i]) for i in range(3))] += 1
    return result


def verify(native_directory, surface_directory, products):
    manifest = json.loads((products / "manifest.json").read_text())
    receipts = []
    for cohort in manifest["cohorts"]:
        parent = cohort["parent"]
        selection = json.loads((native_directory / f"{parent}.json").read_text())
        surfaces = json.loads((surface_directory / f"{parent}.json").read_text())
        for row in selection["results"][1:]:
            members = [
                source
                for source in selection["sources"]
                if source["cell"] == row["cell"]
            ]
            calls = row["mesh_calls"]
            if len(members) != 1 or row["projection_calls"] or len(calls) != 1:
                continue
            call = calls[0]
            if (
                not row["accepted"]
                or not call["accepted"]
                or call["detail"] != 1
                or call["openings"]
            ):
                continue
            source = members[0]
            index = source["index"]
            assert call["source"] == index
            actual = surfaces["sources"][index]
            for key in ("index", "id", "kind", "cell"):
                assert actual[key] == source[key]
            record = {key: source[key] for key in ("index", "id", "kind", "cell")}
            record["parent"] = parent
            groups = [
                group
                for shape in seed_groups(actual, vertex_recipe="snapped-f32")
                for group in shape
            ]
            if not groups:
                receipts.append(record | {"state": "no_supported_planar_roof_control"})
                continue
            expected = np.concatenate(groups)
            entry = next(
                entry for entry in cohort["products"] if entry["cell"] == row["cell"]
            )
            path = products / f"{entry['key']}.structure"
            raw = path.read_bytes()
            assert hashlib.sha256(raw).hexdigest() == entry["sha256"]
            parsed = product(raw, entry["key"])
            assert parsed["footprints"] == 1 and parsed["cell"] == row["cell"]
            assert STREAM_BYTES[1] == 24
            vertices = np.frombuffer(parsed["streams"][1], dtype="<f4").reshape(-1, 6)[
                :, :3
            ]
            indices = np.frombuffer(parsed["streams"][3], dtype="<u4").reshape(-1, 3)
            native_triangles = vertices[indices]
            if oriented_triangles(expected) != oriented_triangles(native_triangles):
                raise ValueError(
                    f"original native roof triangles differ: {parent}/{index}"
                )
            receipts.append(
                record
                | {
                    "state": "exact_oriented_position_triangles",
                    "triangles": len(expected),
                    "unique_positions": len(np.unique(expected.reshape(-1, 3), axis=0)),
                    "artifact": str(path),
                    "artifact_sha256": entry["sha256"],
                }
            )
    if not any(row["state"] == "exact_oriented_position_triangles" for row in receipts):
        raise ValueError("at least one actual native Shell control required")
    return receipts


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--surfaces", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    receipts = verify(args.native, args.surfaces, args.products)
    target.mkdir(parents=True)
    report = {
        "scope": "Geometric positions and winding of original accepted singleton Shell controls only; no universal mesher, material/attribute, GPU or world parity. Unsupported controls remain explicit. StoredVertex stride24, original schema10 artifact hashes verified.",
        "controls": receipts,
    }
    (target / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(receipts), flush=True)
