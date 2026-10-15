"""Render accepted Python transport products using four fixed Place cameras."""

import argparse
import hashlib
import json
import math
import subprocess
import time
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np
import shapely
from capture_height_targets import GIRTH_M, sample_targets
from outshine_ref.contracts import Mesh
from outshine_ref.transport import close_surface
from PIL import Image
from port_height_library_comparison import load
from port_plane_fixture import load_reference

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PLACES = (
    ("Wien", "Vienna"),
    ("Tokyo", "Tokyo"),
    ("CentralPark", "CentralPark"),
    ("Feldkirch", "Feldkirch"),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def mercator(latitude, longitude):
    return np.array(
        [
            longitude / 360 * GIRTH_M,
            math.asinh(math.tan(math.radians(latitude))) * GIRTH_M / (2 * math.pi),
        ]
    )


def camera(name, origin, sources):
    path = ROOT / "src/assets/places" / (name + ".scenario")
    scenario = ET.parse(path).getroot()
    view = scenario.find("./views/view")
    at = view.find("at").attrib
    latitude, longitude = float(at["lat"]), float(at["lon"])
    xy = (mercator(latitude, longitude) - mercator(*origin)) * math.cos(
        math.radians(origin[0])
    )
    heights, receipts = sample_targets(origin, np.array([xy, [0.0, 0.0]]), sources, 11)
    elevation = float(at["heightM"]) + (
        heights[0] if at["samplesHeight"] == "yes" else 0.0
    )
    eye = np.r_[xy, elevation]
    bearing, pitch = map(math.radians, (float(at["bearingDeg"]), float(at["pitchDeg"])))
    forward = np.array(
        [
            math.sin(bearing) * math.cos(pitch),
            math.cos(bearing) * math.cos(pitch),
            math.sin(pitch),
        ]
    )
    frame = scenario.find("render").attrib
    result = {
        "projection": "perspective",
        "eye": eye.tolist(),
        "target": (eye + forward * 1000).tolist(),
        "vertical_fov_deg": float(view.attrib["fovDeg"]),
        "clip_near_m": 0.05,
        "clip_far_m": float(scenario.find("world").attrib["sightM"]),
        "profile": [int(frame["widthPx"]), int(frame["heightPx"])],
    }
    proof = {
        "scenario": str(path.relative_to(ROOT)),
        "scenario_sha256": digest(path),
        "geographic_pose": at,
        "local_origin_lat_lon": origin,
        "camera_dem_height_m": float(heights[0]),
        "local_flat_datum_m": float(heights[1]),
        "dem_receipts": receipts,
        "reference_image_sha256": digest(ROOT / "doc/images/places" / (name + ".png")),
        "height_sampling": "Bilinear pixel-centre Mapterhorn z11 at the unchanged declared geographic pose; sampled-height cameras add their declared height. This is not the native engine's final fitted contact height.",
    }
    return result, proof


def pinned(path, release, pins):
    if digest(path) != pins[str(path.relative_to(release))]:
        raise ValueError("accepted flat release bytes changed: " + str(path))
    return path


def flat_model(name, release, pins, datum, camera_products=None):
    if camera_products is None:
        base = release / "geometry/components"
    else:
        release = base = camera_products
        pins = json.loads((base / "generation.json").read_text())["files"]
    directory = base / "results" / name / "components"
    report_path = pinned(directory / "result.json", release, pins)
    report = json.loads(report_path.read_text())
    if not report["report"]["valid_components"] or report["products"] < 1:
        raise ValueError("accepted nonempty flat component products required")
    meshes, colours, sources = [], [], []
    for index in range(report["products"]):
        path = pinned(directory / f"product-{index}.npz", release, pins)
        with np.load(path) as data:
            top = Mesh(
                np.c_[data["vertices"], np.full(len(data["vertices"]), datum)],
                data["indices"],
            )
            slots = data["part_roles"][data["face_owners"]]
        meshes.append(top)
        colours.append(slots)
        sources.append({"path": str(path), "sha256": digest(path)})
    offsets = np.cumsum([0] + [len(m.vertices) for m in meshes])
    mesh = Mesh(
        np.vstack([m.vertices for m in meshes]),
        np.vstack([m.triangles + offset for m, offset in zip(meshes, offsets)]),
    )
    proof = {
        "input_sha256": report["input_sha256"],
        "products": sources,
        "report_sha256": digest(report_path),
        "source_states": report["source_states"],
        "source_reasons": report["source_reasons"],
        "complete": report["report"]["complete"],
        "recipe": "Accepted flat recipe rigidly placed at the coarse DEM height of its captured local origin. No terrain fit, bank, tier or drivable-turn claim.",
        "flat_datum_m": datum,
    }
    return mesh, np.concatenate(colours), proof


def vienna_model(directory):
    report, data = load(directory)
    verification = json.loads((directory / "verification.json").read_text())
    if (
        not verification["valid"]
        or verification["mesh_sha256"] != report["mesh_sha256"]
    ):
        raise ValueError("verified accepted Vienna plan required")
    product, _captured, _metadata, _sources = load_reference()
    road = data["parts"] >= 0
    slots = np.full(len(road), 5, int)
    slots[road] = product["part_roles"][data["parts"][road]]
    mesh = Mesh(np.c_[data["xy"], data["heights"]], data["faces"])
    proof = {
        "plan_sha256": report["mesh_sha256"],
        "generation_commit": report["generation_commit"],
        "verification_sha256": digest(directory / "verification.json"),
        "frame": report["frame"],
        "source_states": {
            state: sum(row["status"] == state for row in report["source_usage"])
            for state in ("used", "not_used")
        },
        "recipe": "Accepted installed PR181 C1 analytic road field and its actual conforming C0 road/soil mesh. Original captured scope; not a complete Place/tier/traffic generator.",
    }
    return mesh, slots, proof


def append_camera_window(top, slots, near, near_slots):
    reserved = shapely.box(
        *top.vertices[:, :2].min(axis=0), *top.vertices[:, :2].max(axis=0)
    )
    polygons = shapely.polygons(near.vertices[near.triangles, :2])
    hits = shapely.STRtree(polygons).query(reserved, predicate="intersects")
    overlap = float(shapely.area(shapely.intersection(polygons[hits], reserved)).sum())
    if overlap > 1e-7:
        raise ValueError("new camera window intersects the retained verified ground")
    mesh = Mesh(
        np.vstack((top.vertices, near.vertices)),
        np.vstack((top.triangles, near.triangles + len(top.vertices))),
    )
    return mesh, np.r_[slots, near_slots], overlap


def prepare(args):
    if args.output.exists() and any(args.output.iterdir()):
        raise ValueError("fresh empty external render output required")
    args.output.mkdir(parents=True, exist_ok=True)
    pins = json.loads((args.flat_release / "sha256.json").read_text())
    rows, reports = [], {}
    for name, source in PLACES:
        input_path = (
            args.flat_release / "geometry/components/inputs" / (source + "-input.json")
        )
        original = json.loads(pinned(input_path, args.flat_release, pins).read_text())
        view, camera_proof = camera(name, original["origin_lat_lon"], args.sources)
        if name == "Wien":
            top, slots, model_proof = vienna_model(args.port_plan)
            if args.camera_products is not None:
                near, near_slots, near_proof = flat_model(
                    source,
                    args.flat_release,
                    pins,
                    camera_proof["local_flat_datum_m"],
                    args.camera_products,
                )
                top, slots, overlap = append_camera_window(top, slots, near, near_slots)
                model_proof.update(
                    additional_camera_window=near_proof,
                    retained_ground_overlap_m2=overlap,
                    uncoupled_windows=True,
                )
        else:
            top, slots, model_proof = flat_model(
                source,
                args.flat_release,
                pins,
                camera_proof["local_flat_datum_m"],
                args.camera_products,
            )
        body = close_surface(top, 0.25)
        materials = np.full(len(body.triangles), 2, int)
        materials[: len(top.triangles)] = slots
        path = args.output / (name + ".npz")
        np.savez_compressed(
            path, vertices=body.vertices, triangles=body.triangles, materials=materials
        )
        rows.append(
            dict(
                id=name,
                mesh=str(path),
                mesh_sha256=digest(path),
                output=str(args.output / (name + ".png")),
                background=[0.34, 0.51, 0.68],
                **view,
            )
        )
        reports[name] = {
            "camera": camera_proof,
            "model": model_proof,
            "mesh_sha256": digest(path),
            "input_vertices": len(top.vertices),
            "input_triangles": len(top.triangles),
            "display_body_depth_m": 0.25,
            "source_scope": "Existing accepted Python products only; absent terrain/world classes are not supplied by native screenshots.",
        }
    scene = args.output / "scenes.json"
    scene.write_text(json.dumps(rows, indent=2) + "\n")
    (args.output / "inputs.json").write_text(json.dumps(reports, indent=2) + "\n")
    return scene


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--flat-release", type=Path, required=True)
    parser.add_argument("--port-plan", type=Path, required=True)
    parser.add_argument("--camera-products", type=Path)
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    began = time.monotonic()
    scene = prepare(args)
    if args.build_only:
        return
    subprocess.run(
        [
            "blender",
            "--background",
            "--factory-startup",
            "--threads",
            "2",
            "--python-exit-code",
            "1",
            "--python",
            str(HERE / "render_transport_solid.py"),
            "--",
            str(scene),
        ],
        check=True,
        cwd=ROOT,
        timeout=180,
    )
    for name, _ in PLACES:
        with Image.open(args.output / (name + ".png")) as image:
            if image.format != "PNG" or image.size != (1280, 720):
                raise ValueError("four complete 1280x720 Place PNGs required")
            image.verify()
    output = {
        name: {
            "sha256": digest(args.output / (name + ".png")),
            "camera": json.loads((args.output / (name + "-camera.json")).read_text()),
        }
        for name, _ in PLACES
    }
    result = {
        "generation_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    "lib",
                    "test/experiments/transport",
                ],
                cwd=ROOT,
                text=True,
            ).strip()
        ),
        "seconds": time.monotonic() - began,
        "images": output,
        "scope": "Four actual Python renders with fixed Place camera declarations; manual image review required before presentation.",
    }
    (args.output / "proof.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    main()
