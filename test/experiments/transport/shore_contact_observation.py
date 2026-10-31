"""Observe current captured shore/stamp geometry before constructing a contact.

Ring membership is not road/nav geometry or a replay of global deformation.
"""

import argparse
import gzip
import hashlib
import json
import runpy
import subprocess
from pathlib import Path

import numpy as np
import shapely

FIXTURE = Path(__file__).parent / "fixtures/feldkirch-shore-contact"


def polygon(stamp):
    return shapely.Polygon(
        np.asarray(stamp["ring"]).reshape(-1, 2),
        [np.asarray(hole).reshape(-1, 2) for hole in stamp["holes"]],
    )


def plane_at(plane, points):
    if len(plane) != 6 or plane[5] != 0:
        raise ValueError("This observation requires the captured affine plane path")
    points = np.asarray(points, float)
    return (
        plane[2]
        + (points[:, 0] - plane[0]) * plane[3]
        + (points[:, 1] - plane[1]) * plane[4]
    )


def observe(root, page_id):
    receipt = runpy.run_path(str(FIXTURE / "verify.py"))["verify"](root)
    manifest_raw = (root / "input.json").read_bytes()
    metadata = json.loads(manifest_raw)
    with gzip.open(root / "contact.json.gz", "rt") as stream:
        capture = json.load(stream)
    water = capture["water"]
    if water["anchor"] != capture["frame"]["anchor"]:
        raise ValueError("Different water/job frames need explicit conversion")
    vertices, faces = np.asarray(water["vertices"]), np.asarray(water["faces"])
    stamps = {s["ordinal"]: s for s in capture["stamps"]}
    observation = metadata["native_observation"]
    basin, corridor = (stamps[i] for i in observation["winning_stamps"])
    if basin["kind"] != 2 or corridor["kind"] != 1 or corridor["profile"] is not None:
        raise ValueError("Different native paths require their own observation")
    pairs, offsets = [], []
    for stamp in capture["stamps"]:
        if stamp["kind"] != 2:
            continue
        face = stamp["ordinal"] - capture["pads"]
        if not 0 <= face < len(faces):
            raise ValueError("Basin original ordinal does not identify a water face")
        xyz = vertices[faces[face, :3]]
        if not np.array_equal(xyz[:, :2], np.asarray(stamp["ring"]).reshape(-1, 2)):
            raise ValueError("Water face and basin ring words differ")
        offsets.extend((xyz[:, 2] - plane_at(stamp["plane"], xyz[:, :2])).tolist())
        pairs.append(face)
    page = next(p for p in metadata["pages"] if p["page"] == page_id)
    edge = metadata["edge"]
    row, column = edge["row"] + page["halo"], edge["col"] + page["halo"]
    with np.load(root / "deformed-pages.npz", allow_pickle=False) as arrays:
        actual = arrays[page["array"]][row, column : column + 2]
        expected = np.asarray(edge["native_pair_m"], dtype="<f4")
        if actual.tobytes() != expected.tobytes():
            raise ValueError("Page/halo indices do not bind the reported edge")
    points = np.asarray(observation["points_enu_m"])
    queries = shapely.points(points)
    members = [
        s
        for s in capture["stamps"]
        if s["kind"] == 1 and s["corridor_key"] == corridor["corridor_key"]
    ]
    joined = shapely.union_all([polygon(s) for s in members])
    water_union = shapely.union_all(shapely.polygons(vertices[faces[:, :3], :2]))
    face = basin["ordinal"] - capture["pads"]
    result = {
        "capture": receipt,
        "manifest_sha256": hashlib.sha256(manifest_raw).hexdigest(),
        "page": page_id,
        "stored_indices_with_halo": [row, column],
        "page_witness_words_match": True,
        "water_basin_faces_checked": len(pairs),
        "water_minus_basin_vertex_m_range": [min(offsets), max(offsets)],
        "basin_ring_covers": shapely.covers(polygon(basin), queries).tolist(),
        "water_projection_covers": shapely.covers(water_union, queries).tolist(),
        "corridor_ring_covers": shapely.covers(polygon(corridor), queries).tolist(),
        "corridor_ring_distance_m": shapely.distance(
            polygon(corridor), queries
        ).tolist(),
        "corridor_apron_m": corridor["apron"],
        "corridor_profile": corridor["profile"],
        "corridor_key": corridor["corridor_key"],
        "corridor_key_stamps": len(members),
        "corridor_key_ring_parts": len(shapely.get_parts(joined)),
        "whole_key_ring_distance_m": shapely.distance(joined, queries).tolist(),
        "basin_wants_plane_m": plane_at(basin["plane"], points).tolist(),
        "corridor_wants_plane_m": plane_at(corridor["plane"], points).tolist(),
        "observed_final_local_m": observation["native_after_local"],
        "winning_water_face": face,
        "winning_face_source_incidence": water["face_sources"][
            water["face_source_offsets"][face] : water["face_source_offsets"][face + 1]
        ],
        "scope": __doc__
        + " Plane values are not final apron blends. Source incidence stays opaque in this observation, not an inferred OSM ID. Selected stamps do not qualify global refusal/admission. Rendered roads and primitive memberships are supplied separately; exterior source-way rows, navigation and exterior DEM remain incomplete. No repaired mesh, World or device admission.",
    }
    return result, (water_union, joined, polygon(basin), polygon(corridor), points)


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    result, geometry = observe(args.inputs, args.page)
    args.output.mkdir(parents=True)
    result["producer_commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    result["producer_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    plot(geometry, result, args.output)
    print(json.dumps(result))


def plot(geometry, report, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    water, corridor_key, basin, corridor, points = geometry
    figure, axes = plt.subplots(1, 2, figsize=(11, 5), layout="constrained")
    for shape, colour, label in (
        (water, "lightblue", "Water geometry projection"),
        (corridor_key, "lightgreen", "Whole captured corridor-key rings"),
        (basin, "blue", "Winning basin ring"),
        (corridor, "green", "Winning corridor ring"),
    ):
        for i, part in enumerate(shapely.get_parts(shape)):
            axes[0].plot(
                *part.exterior.xy, color=colour, label=label if i == 0 else None
            )
            for hole in part.interiors:
                axes[0].plot(*hole.xy, color=colour)
    axes[0].scatter(points[:, 0], points[:, 1], c=["red", "black"])
    centre = points.mean(axis=0)
    axes[0].set(
        xlim=(centre[0] - 12, centre[0] + 12),
        ylim=(centre[1] - 12, centre[1] + 12),
        title="Actual stamp rings; not inferred road edges",
    )
    axes[0].set_aspect("equal")
    axes[0].legend(fontsize=7)
    distance = np.linalg.norm(points - points[0], axis=1)
    for name, label in (
        ("basin_wants_plane_m", "Basin plane"),
        ("corridor_wants_plane_m", "Corridor plane"),
        ("observed_final_local_m", "Native final samples"),
    ):
        axes[1].plot(distance, report[name], "o-", label=label)
    axes[1].set(
        xlabel="ENU sample separation [m]",
        ylabel="Job-local up [m]",
        title="Plane targets and actual mixed output",
    )
    axes[1].legend()
    figure.suptitle("Current native inputs: geometry observation, no repaired contact")
    figure.savefig(output / "observation.png", dpi=130)
    plt.close(figure)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--page", required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
