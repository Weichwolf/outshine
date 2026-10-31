"""Bind a reported geographic conflict to a whole historical water footprint.

No current stamp/source binding, water datum or contact construction is inferred.
"""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
import shapely
from native_river_inputs import surface_rings
from native_water_observation import NO_INPUT, read_v3


def whole_feature(data, selected):
    """Retain all observed fragments of one explicit provider feature/epoch/class."""

    if type(selected) is not int or not 0 <= selected < len(data["surfaces"]):
        raise ValueError("Explicit observed surface index required")

    def key(index):
        row = data["surfaces"][index]
        if int(row["input"]) == NO_INPUT:
            return None
        info = data["inputs"][int(row["input"])]
        source = data["sources"][info["source"]]
        identity = info.get("provider_feature_id")
        if identity is None or not source.get("provider") or not source.get("revision"):
            return None
        return (
            source["provider"],
            source["revision"],
            identity,
            info["layer"],
            info["kind"],
            info["bridge"],
            info["tunnel"],
            info["crossing"],
        )

    selected_key = key(selected)
    if selected_key is None:
        raise ValueError("Explicit feature and provider epoch required")
    ids = [i for i in range(len(data["surfaces"])) if key(i) == selected_key]
    polygons = []
    for i in ids:
        rings = [ring[:, ::-1] for ring in surface_rings(data, i)]
        polygon = shapely.Polygon(rings[0], rings[1:])
        if not polygon.is_valid or polygon.is_empty:
            raise ValueError("Whole valid source rings required")
        polygons.append(polygon)
    body = shapely.union_all(polygons)
    if not body.is_valid or body.is_empty:
        raise ValueError("Whole valid observed feature required")
    return body, ids, selected_key


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    manifest_raw, witness_raw = args.inputs.read_bytes(), args.witness.read_bytes()
    metadata, witness = json.loads(manifest_raw), json.loads(witness_raw)
    item = metadata["observed_v3"]
    data = read_v3(args.inputs.parent / item["file"], item["sha256"])
    body, ids, identity = whole_feature(data, args.surface)
    points = np.array(
        [[r["longitude_deg"], r["latitude_deg"]] for r in witness["samples"]]
    )
    if points.shape != (2, 2) or not np.isfinite(points).all():
        raise ValueError("Two finite reported geographic points required")
    origin = points[0]
    scale = np.deg2rad(1) * 6378137 * np.array([np.cos(np.deg2rad(origin[1])), 1])
    projected = shapely.transform(body, lambda xy: (xy - origin) * scale)
    queries = shapely.points((points - origin) * scale)
    args.output.mkdir(parents=True)
    (args.output / "whole-observed-feature.wkb").write_bytes(body.wkb)
    levels = [float(data["surfaces"][i]["level_m"]) for i in ids]
    result = {
        "producer_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "producer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "work": {
            "observed_surfaces": len(data["surfaces"]),
            "matched_fragments": len(ids),
            "union_wkb_bytes": len(body.wkb),
            "point_queries": len(points),
        },
        "manifest_sha256": hashlib.sha256(manifest_raw).hexdigest(),
        "witness_sha256": hashlib.sha256(witness_raw).hexdigest(),
        "historical_native_sha256": item["sha256"],
        "feature_key": identity,
        "surface_ids": ids,
        "parts": len(shapely.get_parts(body)),
        "holes": sum(len(p.interiors) for p in shapely.get_parts(body)),
        "observed_source_levels_m": levels,
        "covers_samples": shapely.covers(projected, queries).tolist(),
        "boundary_distance_m_approx": shapely.distance(
            projected.boundary, queries
        ).tolist(),
        "scope": __doc__
        + " Distances use a local equirectangular diagnostic projection. All observed fragments are retained; capture/tile cuts are not declared natural shores. Historical surface levels do not supply current native water/profile height. No native picture, repair, World or device qualification.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    plot(projected, (points - origin) * scale, witness, args.output)
    print(json.dumps(result))


def plot(body, points, witness, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 3, figsize=(15, 5), layout="constrained")
    for axis in axes[:2]:
        for polygon in shapely.get_parts(body):
            axis.fill(*polygon.exterior.xy, color="lightblue")
            for hole in polygon.interiors:
                axis.fill(*hole.xy, color="white", edgecolor="black")
        axis.scatter(points[:, 0], points[:, 1], c=["red", "green"])
        axis.set_aspect("equal")
    axes[0].set_title("Whole observed feature / holes retained")
    axes[1].set(
        xlim=(-3, 3), ylim=(-3, 3), title="Reported points near historical outline"
    )
    distance = np.linalg.norm(points - points[0], axis=1)
    for key, label in (
        ("original_geodetic_m", "Original DEM"),
        ("stored_geodetic_m", "Reported deformed edge"),
    ):
        axes[2].plot(distance, [r[key] for r in witness["samples"]], "o-", label=label)
    axes[2].legend()
    axes[2].set(
        xlabel="Approximate separation [m]",
        ylabel="Reported geodetic height [m]",
        title="Two-point witness; no new contact geometry",
    )
    figure.suptitle(
        "Historical footprint context; current native shore/road input still required"
    )
    figure.savefig(output / "context.png", dpi=130)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--witness", type=Path, required=True)
    parser.add_argument("--surface", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
