"""Replay saved candidate/cap/pier/whole-scene products without rebuilding them."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface, replay_contact
from camera_height_inputs import digest
from cycle_bridge_audit import audit
from cycle_bridge_inputs import inputs
from cycle_bridge_problem import band, problem
from foot_ramp_audit import shell_ownership
from foot_ramp_geometry import Ramp
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from surface_solid_checks import audit_closed
from wien_support_verify import unpack


def read_ribbon(path):
    with np.load(path) as data:
        top = Mesh(data["upper_vertices"], data["cap_faces"])
        low = Mesh(data["lower_vertices"], data["cap_faces"])
        body = Mesh(data["body_vertices"], data["body_faces"])
        table = data["table"]
    return Ramp(body, low, top, surface_domain(top)), table


def pier_proof(fixture, p, ribbon, bodies, contacts, fine):
    source = ContactSurface(fine)
    deck = ContactSurface(ribbon.lower)
    lower_error, upper_error, stations = [], [], []
    forbidden = shapely.union_all(
        [
            band(r)
            for r in fixture["sources"]
            if r["role"] in ("ground_joint", "underpass")
        ]
    ).buffer(0.2)
    for body, (lo, hi) in zip(bodies, contacts, strict=True):
        shape = surface_domain(lo)
        product = Ramp(body, lo, hi, shape)
        shell_ownership(product)
        volume = audit_closed(body)
        points = hi.vertices[hi.triangles]
        area = (
            np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])[:, 2] / 2
        )
        expected = float(
            np.sum(
                area
                * (hi.vertices[:, 2] - lo.vertices[:, 2])[hi.triangles].mean(axis=1)
            )
        )
        if volume <= 0 or abs(volume - expected) > 1e-7:
            raise ValueError("saved pier shell/cap volume inconsistent")
        if not ribbon.shape.buffer(1e-8).covers(shape) or shape.intersects(forbidden):
            raise ValueError("saved pier violates deck/traffic footprint")
        lower_error.append(replay_contact(lo, source))
        upper_error.append(replay_contact(hi, deck))
        stations.append(
            float((np.array(shape.centroid.coords[0]) - p["origin"]) @ p["axis"])
        )
        a = body.vertices[body.triangles]
        b = body.vertices.astype(np.float32).astype(float)[body.triangles]
        if np.any(
            np.einsum(
                "ij,ij->i",
                np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0]),
                np.cross(b[:, 1] - b[:, 0], b[:, 2] - b[:, 0]),
            )
            <= 0
        ):
            raise ValueError("saved pier Float32 face collapses or inverts")
    ext = (shapely.get_coordinates(ribbon.shape) - p["origin"]) @ p["axis"]
    shapes = [surface_domain(lo) for lo, _hi in contacts]
    for i, shape in enumerate(shapes):
        if any(shape.intersection(other).area > 1e-8 for other in shapes[i + 1 :]):
            raise ValueError("saved piers overlap each other")
    gap = float(np.max(np.diff(np.r_[ext.min(), sorted(stations), ext.max()])))
    if not bodies or gap > 24:
        raise ValueError("saved pier spacing outside the chosen procedural budget")
    return {
        "piers": len(bodies),
        "maximum_station_gap_m": gap,
        "whole_ground_error_m": max(lower_error),
        "whole_deck_error_m": max(upper_error),
    }


def scene_proof(path, before, bodies, palette):
    with np.load(path) as data:
        after = {k: data[k] for k in data.files}
    for key, old in before.items():
        if not np.array_equal(after[key][: len(old)], old):
            raise ValueError("saved complete scene changed an original array prefix")
    vo, fo = len(before["vertices"]), len(before["triangles"])
    for body, slot in bodies:
        n, f = len(body.vertices), len(body.triangles)
        if (
            not np.array_equal(after["vertices"][vo : vo + n], body.vertices)
            or not np.array_equal(after["triangles"][fo : fo + f], body.triangles + vo)
            or not np.all(after["materials"][fo : fo + f] == slot)
        ):
            raise ValueError(
                "saved scene differs from its actual admitted indexed body"
            )
        xyz = body.vertices[body.triangles]
        normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        normal /= np.linalg.norm(normal, axis=1)[:, None]
        expected = np.repeat(normal, 3, axis=0).astype(after["normals"].dtype)
        if not np.array_equal(
            after["normals"][fo * 3 : (fo + f) * 3], expected
        ) or not np.array_equal(
            after["face_base_colour_linear_rgba"][fo : fo + f],
            np.tile(palette[slot], (f, 1)).astype(
                after["face_base_colour_linear_rgba"].dtype
            ),
        ):
            raise ValueError("saved scene new body normals/material colour differs")
        vo, fo = vo + n, fo + f
    if vo != len(after["vertices"]) or fo != len(after["triangles"]):
        raise ValueError("unbound extra saved scene geometry")


def verify(args):
    report = json.loads((args.output / "comparison.json").read_text())
    for filename, key in (
        ("bridge.npz", "bridge_sha256"),
        ("piers.npz", "piers_sha256"),
        ("source-usage.json", "source_usage_sha256"),
    ):
        if digest(args.output / filename) != report[key]:
            raise ValueError("saved bridge/pier/source export binding changed")
    if (
        report["dirty"]
        or digest(args.output / "after-body.npz")
        != report["presentation"]["after_body_sha256"]
    ):
        raise ValueError("dirty or unbound complete bridge scene")
    fixture = json.loads(args.fixture.read_text())
    p = problem(fixture)
    if digest(args.output / "profile-input.npz") != report["profile_input_sha256"]:
        raise ValueError("saved measured profile input binding changed")
    with np.load(args.output / "profile-input.npz") as data:
        if any(
            not np.array_equal(data[k], p[k])
            for k in ("stations", "lower", "upper", "rate")
        ):
            raise ValueError(
                "saved measured profile input differs from actual whole-source conditions"
            )
    profiles = [
        dict(r, height_owner="affine_ground_plus_station_table")
        for r in fixture["sources"]
        if r["role"] in ("bridge", "approach")
    ]
    if (
        report["source_profiles"] != profiles
        or report["public_travel"] != p["public_travel"].tolist()
    ):
        raise ValueError("saved complete source/profile/access bindings differ")
    ribbon, table = read_ribbon(args.output / "bridge.npz")
    proof = audit(fixture, ribbon, table, p["domain"])
    before, fine, binding = inputs(args, fixture, p, ribbon)
    bodies, contacts, _data = unpack(args.output / "piers.npz")
    proof["supports"] = pier_proof(fixture, p, ribbon, bodies, contacts, fine)
    palette = np.array(
        json.loads((args.baseline / "after-scene.json").read_text())["colours"]
    )
    scene_proof(
        args.output / "after-body.npz",
        before,
        [(ribbon.body, 3), *[(b, 10) for b in bodies]],
        palette,
    )
    old = {
        r["source_id"]: r
        for r in json.loads((args.network / "comparison.json").read_text())[
            "source_usage"
        ]
    }
    rows = json.loads((args.output / "source-usage.json").read_text())
    changed = {
        r["source_id"]
        for r in fixture["sources"]
        if r["role"] in ("bridge", "approach")
    }
    if len(rows) != len(old) or {r["source_id"] for r in rows} != set(old):
        raise ValueError("saved source ledger lacks unique original source coverage")
    for r in rows:
        if r["source_id"] not in changed and r != old[r["source_id"]]:
            raise ValueError("unrelated old source decision changed")
        if r["source_id"] in changed and (
            r["status"] != "used"
            or r["reason"]
            or r.get("height_owner") != "cycle_bridge_station_profile"
        ):
            raise ValueError("admitted source lacks its selected actual height owner")
    proof.update(
        valid=True,
        bindings=binding,
        source_count=len(rows),
        used_sources=sum(r["status"] == "used" for r in rows),
        generation_commit=report["commit"],
        reader_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
    )
    (args.output / "verification.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=Path(__file__).with_name("fixtures") / "cycle-bridge-vienna.json",
    )
    for name in ("network", "baseline", "supports", "reference-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3, 4 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    verify(parser.parse_args())
