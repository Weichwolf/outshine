"""Actual native swimming-pool body control; no natural shore or new water fit."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from native_bank_slice import audit
from outshine_ref.water.frame import inverse_frame, model_height_targets
from pool_body import build

FIXTURE = Path(__file__).with_name("fixtures") / "native-bank-contact"


def original(directory):
    raw = (directory / "input.json").read_bytes()
    data = json.loads(raw)
    source = json.loads((directory / "source.json").read_text())
    if hashlib.sha256(raw).hexdigest() != source["bound_input_sha256"]:
        raise ValueError(
            "original pool slice does not match its semantic source binding"
        )
    report, triangles, xy = audit(data)
    replay = []
    for point in report["native_membership_replay"]:
        if not point["hits"]:
            continue
        i = point["node"]
        local = np.r_[xy[i], point["hits"][0]["constant_two_m_bed_up_m"]]
        _, height, _ = inverse_frame(data["water"]["meshAnchor"], local[None])
        expected = np.float32(data["water"]["points"][i]["waterOnlyGeodeticF32M"])
        actual = np.float32(height[0])
        replay.append(
            {
                "node": i,
                "geodetic_f32_m": float(actual),
                "native_word_equal": bool(
                    actual.view(np.uint32) == expected.view(np.uint32)
                ),
            }
        )
    report["single_wet_native_height_replay"] = replay
    if source["source"]["kind"] != "swimming_pool":
        raise ValueError("original source semantics must identify this pool control")
    if [f["id"] for f in data["water"]["sourceSurface344Faces"]] != source["face_ids"]:
        raise ValueError("source component and facet IDs do not match")
    return data, source, report, triangles, xy


def construct(data, source, triangles):
    def reference(xy):
        return model_height_targets(
            data["water"]["meshAnchor"],
            xy,
            np.full(len(xy), source["source"]["levelM"]),
        )

    return build(
        triangles,
        reference,
        kind=source["source"]["kind"],
        ground_role="declared_ground",
        footprint_role="declared_complete_local_component",
    )


def image_body(body, *, with_water):
    from current_visibility import project, raster

    v, f = body["vertices"], body["faces"]
    target = (v.min(axis=0) + v.max(axis=0)) * 0.5
    eye = target + [-45.0, -65.0, 70.0]
    forward = (target - eye) / np.linalg.norm(target - eye)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    view = (
        eye,
        np.array([right, up, forward]),
        720 / (2 * np.tan(np.deg2rad(60) / 2)),
        1280,
        720,
    )
    projected, z = project(v, view)
    if np.any(z <= 0.1):
        raise ValueError("pool diagnostic has near-crossing geometry")
    depth, owners = raster(projected, z, f, 1280, 720, with_faces=True)
    palette = np.array(
        [
            [0.38, 0.38, 0.35],
            [0.65, 0.68, 0.67],
            [0.87, 0.83, 0.72],
            [0.74, 0.77, 0.75],
            [0.52, 0.55, 0.52],
        ]
    )
    triangles = v[f]
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    light = np.array([-0.3, -0.5, 1.0])
    light /= np.linalg.norm(light)
    colour = palette[body["parts"]] * (0.35 + 0.65 * abs(normal @ light))[:, None]
    image = np.full((720, 1280, 3), [0.82, 0.85, 0.88])
    visible = owners >= 0
    image[visible] = colour[owners[visible]]
    if with_water:
        water = body["water_triangles"].reshape(-1, 3)
        water_faces = np.arange(len(water)).reshape(-1, 3)
        pp, zz = project(water, view)
        water_depth = raster(pp, zz, water_faces, 1280, 720)
        front = water_depth > depth
        image[front] = 0.45 * image[front] + 0.55 * np.array([0.10, 0.40, 0.62])
    return image


def display(body, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(14, 5))
    for axis, water in zip(axes, (False, True)):
        axis.imshow(image_body(body, with_water=water))
        axis.set(
            title="Declared pool shell"
            + (
                " + unchanged source water"
                if water
                else " / water hidden to inspect body"
            ),
            xlabel="pixel x",
            ylabel="pixel y",
        )
    figure.suptitle(
        "1280×720 CPU depth geometry diagnostic; recipe dimensions, ground role and display transparency declared; no PBR/device image"
    )
    figure.tight_layout()
    figure.savefig(output / "pool-body.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    data, source, replay, triangles, _ = original(args.input)
    wall, cpu = time.perf_counter(), time.process_time()
    body = construct(data, source, triangles)
    elapsed = [time.perf_counter() - wall, time.process_time() - cpu]
    faces = body["faces"]
    directed = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, counts = np.unique(np.sort(directed, axis=1), axis=0, return_counts=True)
    if not np.all(counts == 2):
        raise ValueError("constructed pool is not a closed indexed body")
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **body)
    report = {
        "source_replay": replay,
        "body_vertices": len(body["vertices"]),
        "body_triangles": len(faces),
        "contact_triangles": len(body["contact_faces"]),
        "body_build_wall_cpu_s_single_observation": elapsed,
        "all_returned_numeric_bytes": sum(v.nbytes for v in body.values()),
        "water_words_preserved": body["water_triangles"].tobytes()
        == triangles.tobytes(),
        "roles": {
            "ground": "explicit controlled recipe declaration, not inferred from kind",
            "footprint": "full local native connected component, no global geographic completeness claim",
        },
        "recipe_m": {
            "depth": 2.0,
            "floor_thickness": 0.2,
            "wall_width": 0.3,
            "freeboard": 0.2,
        },
        "terrain_scope": "Excavation and rim-grade contact surface prepared; actual surrounding DEM/join still pending, no world/terrain admission.",
    }
    if args.render:
        display(body, args.output)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
