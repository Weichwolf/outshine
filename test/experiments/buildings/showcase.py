"""Export four verified Python renders for local inspection; no server."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image
from place_reference import PLACES, read


def publish(source, output):
    snapshot = json.loads((source / "place-reports.json").read_text())
    reports = snapshot["reports"]
    if set(reports) != set(PLACES):
        raise ValueError("Publication requires all four fixed Python Places")
    commits = snapshot.get("commits")
    if commits is None:
        commits = {name: snapshot["commit"] for name in PLACES}
    if not isinstance(commits, dict) or set(commits) != set(PLACES):
        raise ValueError(
            "Publication requires an explicit render commit for each Place"
        )
    prepared = []
    for name in PLACES:
        filename = reports[name]
        if Path(filename).name != filename:
            raise ValueError("Place report must be inside the supplied snapshot")
        report = json.loads((source / filename).read_text())
        if (
            report["place"] != name
            or report["camera"]["scenario_sha256"] != read(name).scenario_sha256
        ):
            raise ValueError(
                "Place render does not match the current native scenario camera"
            )
        if (
            report.get("representation") != "python-place-buildings"
            or report["experiment_tree_dirty"]
        ):
            raise ValueError("Clean own Python render proof required")
        if report["commit"] != commits[name]:
            raise ValueError("Place report differs from its declared render snapshot")
        if (
            report.get("geometry_mesh_sha256", report["renderer"]["mesh_sha256"])
            != report["renderer"]["mesh_sha256"]
        ):
            raise ValueError("Cached geometry differs from the actual renderer input")
        reference = read(name)
        view = reference.view(
            report.get("camera_ground_height_m"), tuple(report["profile"])
        )
        projection = report["renderer"]["projection"]
        if (
            not np.allclose(projection["eye"], view.eye, rtol=0.0, atol=1e-6)
            or not np.allclose(projection["target"], view.target, rtol=0.0, atol=1e-6)
            or projection["vertical_fov_deg"] != view.fov_deg
            or projection["profile"] != report["profile"]
        ):
            raise ValueError("Rendered camera differs from the fixed native Place")
        image = report["image"]
        if Path(image).name != image:
            raise ValueError("Place image must be inside the supplied snapshot")
        data = (source / image).read_bytes()
        if hashlib.sha256(data).hexdigest() != report["image_sha256"]:
            raise ValueError("Place image hash differs from its render proof")
        with Image.open(source / image) as picture:
            picture.verify()
        with Image.open(source / image) as picture:
            if (
                list(picture.size) != report["profile"]
                or picture.width < 1920
                or picture.height < 1080
            ):
                raise ValueError(
                    "Python Place images require actual 1920×1080 or higher rendering"
                )
        prepared.append((name, data, report))
    output.mkdir(parents=True, exist_ok=True)
    for name, data, report in prepared:
        temporary = output / (name + ".png.tmp")
        temporary.write_bytes(data)
        temporary.replace(output / (name + ".png"))
    for path in output.glob("*.png"):
        if path.stem not in PLACES:
            path.unlink()
    caption = (
        "Aktueller Python-Gebäudegenerator an den festen Outshine-Place-Kameras.\n\n"
    )
    caption += (
        "\n\n".join(name + "\n\n![" + name + "](" + name + ".png)" for name in PLACES)
        + "\n"
    )
    (output / "README.md").write_text(caption)
    receipt = {
        "render_commits": commits,
        "places": {
            name: {
                key: report[key]
                for key in (
                    "camera",
                    "camera_ground_height_m",
                    "profile",
                    "image_sha256",
                    "triangles",
                    "source_radius_m",
                    "limits",
                )
            }
            | {
                "geometry_commit": report.get("geometry_commit", report["commit"]),
                "mesh_sha256": report["renderer"]["mesh_sha256"],
            }
            for name, _, report in prepared
        },
    }
    (output / "render-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return {"places": list(PLACES), "render_commits": commits}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(publish(args.input, args.output), indent=2))
