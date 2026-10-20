"""Continuous observer-domain source pruning against unchanged original fields."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
import shapely
from attributed_prisms import native_inputs
from compare_attributed_prisms import picture
from compare_beam_field import control
from compare_layered_tree import one_view
from compare_visible_normals import inputs
from packed_normals import unpack
from region_occlusion import compile_projection
from region_occlusion_product import load_product, select, store
from triangle_image_reference import prepare_source
from viewcell_product import compact, query
from viewcell_visibility import select_support


def original_pool(source, faces):
    small = compact(source, faces)
    if not len(small["colour"]):
        small["colour"] = np.broadcast_to(
            np.ones(4, np.float32), (len(small["positions"]), 4)
        )
    if not len(small["metres"]):
        small["metres"] = np.broadcast_to(
            np.zeros(2, np.float32), (len(small["positions"]), 2)
        )
    return small


def render_cut(source, faces, domain, profile):
    small = original_pool(source, faces)
    small["vertex_normal"] = unpack(small["normal"])
    maximum = float(
        np.sqrt(
            1
            + (profile["width"] / (2 * profile["focal_px"])) ** 2
            + (profile["height"] / (2 * profile["focal_px"])) ** 2
        )
    )
    return {
        "source": small,
        "prepared": prepare_source(small),
        "inputs": native_inputs(small),
        "header": {"observer_bounds": domain.tolist(), "max_direction_length": maximum},
    }


def cost_controls(source, region_keep, domain):
    back = select_support(
        source["positions"], source["indices"], domain, max_blockers=0
    )
    result = {}
    for name, keep in (
        ("full", np.ones(len(region_keep), bool)),
        ("backface_only", back.keep),
        ("regional_sources", region_keep),
        ("regional_plus_backfaces", region_keep & back.keep),
    ):
        small = original_pool(source, np.flatnonzero(keep))
        result[name] = {
            "sources": len(np.unique(small["binding"][:, 1])),
            "triangles": int(keep.sum()),
            "factorized_attributes": control(small),
        }
    return result


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--area-reference", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    (
        source,
        document,
        context,
        receipt,
        poses,
        selected,
        observers,
        domain,
        profile,
        provenance,
    ) = inputs(args.input)
    start = time.perf_counter()
    projection = compile_projection(
        source, domain, certified_opaque=True, tight_hulls=True
    )
    compile_s = time.perf_counter() - start
    header = store(projection, document["sources"], output / "product", provenance)
    product = load_product(output / "product", observers, provenance)
    keep = select(product, observers, source["binding"][:, 1])
    cut = render_cut(source, np.flatnonzero(keep), domain, profile)
    report = {
        "provenance": provenance,
        "profile": profile,
        "binding_check": receipt,
        "compile_s": compile_s,
        "projection_work": projection["work"],
        "product": header,
        "geometry_controls": cost_controls(source, keep, domain),
        "poses": {},
    }
    if args.area_reference:
        area = json.loads(args.area_reference.read_text())
        if (
            area["product"]["provenance"] != provenance
            or area["product"]["observer_bounds"] != domain.tolist()
            or area["product"]["profile"] != profile
            or set(area["poses"]) != set(poses)
        ):
            raise ValueError("area reference belongs to another input/domain/pose set")
        hidden = set(projection["sources"][projection["hidden"]].tolist())
        observed = {
            i
            for p in area["poses"].values()
            if not p.get("offscreen")
            for i in p["reference"]["observed_sources"]
        }
        if hidden.intersection(observed):
            raise ValueError(
                "regional selection removes analytically visible original source"
            )
        report["reused_area_reference"] = {
            "sha256": hashlib.sha256(args.area_reference.read_bytes()).hexdigest(),
            "visible_pose_count": sum(
                not p.get("offscreen") for p in area["poses"].values()
            ),
            "observed_sources": len(observed),
            "removed_visible_sources": 0,
        }
    reference, material_inputs = prepare_source(source), native_inputs(source)
    up, east, north = (
        np.array(context["scene_frame"][k]) for k in ("y_ecef", "x_ecef", "z_ecef")
    )
    suns = [up + 0.5 * east + 0.3 * north, up - 0.7 * east - 0.3 * north]
    for name, view in poses.items():
        report["poses"][name] = one_view(
            source,
            reference,
            material_inputs,
            cut,
            view,
            suns,
            output / (name + ".npz"),
            query_fn=query,
        )
        p = report["poses"][name]
        if not p.get("offscreen"):
            with np.load(output / (name + ".npz")) as fields:
                for key in (
                    "depth",
                    "source",
                    "code",
                    "coordinates",
                    "metres",
                    "normal",
                    "colour",
                ):
                    np.testing.assert_array_equal(
                        fields["original_" + key], fields["candidate_" + key]
                    )
                hit = np.isfinite(fields["original_depth"])
                np.testing.assert_array_equal(
                    fields["original_face"][hit],
                    fields["candidate_original_face_tag"][hit],
                )
            print(
                json.dumps(
                    {
                        "pose": name,
                        "samples": p["subsamples"],
                        "lost": p["lost_samples"],
                        "added": p["added_samples"],
                    }
                ),
                flush=True,
            )
    for name, normal in (("material-ids.png", False), ("normals.png", True)):
        picture(
            list(selected),
            output,
            output / name,
            normals=normal,
            candidate_label="retained original inputs",
        )
    (output / "projection.wkb").write_bytes(shapely.to_wkb(projection["fused"]))
    np.savez_compressed(
        output / "projection.npz",
        **{k: v for k, v in projection.items() if isinstance(v, np.ndarray)},
    )
    request = {
        "provenance": provenance,
        "observers": observers.tolist(),
        "requested": projection["sources"].tolist(),
    }
    (output / "selection-request.json").write_text(json.dumps(request) + "\n")
    np.save(
        output / "expected-keep.npy", select(product, observers, projection["sources"])
    )
    report["process_peak_rss_bytes"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "terminal": "complete",
                "poses": len(poses),
                "hidden_sources": header["hidden_sources"],
                "selection_bytes": header["stored_array_bytes"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
