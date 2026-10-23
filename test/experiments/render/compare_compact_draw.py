"""Same existing view/C/T/S reader before and after exact draw compaction."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from compact_draw import canonical_faces, verify
from compare_directed_groups import retained_subset, transformed
from compare_source_material_fields import ready_attributed_source
from compare_vertex_cluster_meshes import pixel_fields, render, setup_view
from full_view_reference import load
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from triangle_image_reference import prepare_source


def read(path):
    with np.load(path, allow_pickle=False) as archive:
        return {key: archive[key] for key in archive.files}


def compare_fields(reference, draw, mapping, canonical_count, retained_count):
    mapped = canonical_faces(
        draw["source_face"], mapping, canonical_count, retained_count
    )
    equal = {
        key: np.array_equal(reference[key], draw[key])
        for key in draw
        if key != "source_face"
    }
    equal["canonical_first_face"] = np.array_equal(reference["source_face"], mapped)
    if not all(equal.values()):
        raise ValueError(
            "compaction changed diagnostic fields: "
            + ", ".join(k for k, v in equal.items() if not v)
        )
    return equal


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    canonical = {
        name: read(args.reference / f"{name}.npz") for name in ("control", "centre")
    }
    compacted = {name: read(args.product / f"{name}.npz") for name in canonical}
    for name, value in canonical.items():
        verify(value, compacted[name])
    a = canonical["control"]
    native, document, context, _ = load(args.input)
    original = transformed(native, a["basis"], a["anchor"])
    retained = retained_subset(original, a["needs_sources"])
    if not np.array_equal(
        np.sort(np.r_[a["accepted_sources"], a["needs_sources"]]),
        np.unique(native["binding"][:, 1]),
    ):
        raise ValueError("every original source must remain accounted for")
    sources = {}
    for name, value in canonical.items():
        sources[name + "-reference"], _ = ready_attributed_source(value, retained)
        sources[name + "-draw"], _ = ready_attributed_source(compacted[name], retained)
    profile = {
        "kind": "sun",
        "direction": [0.45, -0.5, 0.7],
        "intensity": 3.0,
        "ambient": 0.1,
        "sky": [0.1] * 3,
    }
    support = {"positions": np.concatenate([s["positions"] for s in sources.values()])}
    view, ground, box, origins, directions = setup_view(
        support, a, document, context, profile
    )
    raw, costs = {}, {}
    for name, source in sources.items():
        started = time.perf_counter()
        prepared = prepare_source(source)
        raw[name], costs[name] = render(
            source, prepared, origins, directions, ground, profile
        )
        costs[name]["prepare_and_render_seconds"] = time.perf_counter() - started
    checks = {}
    for name, value in canonical.items():
        checks[name] = compare_fields(
            raw[name + "-reference"],
            raw[name + "-draw"],
            compacted[name]["draw_to_reference"],
            len(value["indices"]),
            len(retained["indices"]),
        )
    width, height = box[1] - box[0]
    pw, ph = int(width * 8 + 16), int(height * 8 + 36)
    contact = Image.new("RGB", (pw * 4, ph), (28, 28, 28))
    text = ImageDraw.Draw(contact)
    for column, (name, fields) in enumerate(raw.items()):
        rgb, _ = pixel_fields(fields, width, height)
        full = np.zeros((view[4], view[3], 3), np.uint8)
        full[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = rgb
        Image.fromarray(full).save(args.output / (name + ".png"))
        contact.paste(
            Image.fromarray(rgb).resize(
                (int(width * 8), int(height * 8)), Image.Resampling.NEAREST
            ),
            (column * pw + 8, 28),
        )
        text.text((column * pw + 8, 6), name, fill="white")
    contact.save(args.output / "comparison.png")
    np.savez_compressed(
        args.output / "raw.npz",
        **{
            name + "__" + key: value
            for name, fields in raw.items()
            for key, value in fields.items()
        },
    )
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "pose": "moving-023",
        "resolution": [view[3], view[4]],
        "roi": box.tolist(),
        "rays": len(origins),
        "all_reference_fields_exact": checks,
        "costs": costs,
        "retained_original_triangles": len(retained["indices"]),
        "process_peak_rss_bytes": peak_rss_bytes(),
        "scope": "Actual existing CPU first-hit/C/T/S diagnostic and canonical face correspondence, with complete unchanged original rest. Canonical reference is loaded here only to compare; consuming packed draw does not require it resident. No new pose, material/normal approximation or native fragment/depth/caster/device/PBR admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
