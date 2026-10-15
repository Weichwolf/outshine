"""Show actual rail/sleeper meshes at readable scale beside the complete source tile."""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent


def render(products, name):
    rails = json.loads((products / "rails/generation.json").read_text())["cases"][name]
    accepted = [r for r in rails["pieces"] if r["valid"]]
    chosen = max(range(len(accepted)), key=lambda i: accepted[i]["centreline_length_m"])
    station = (
        sum(r["sleepers"] for r in accepted[:chosen])
        + accepted[chosen]["sleepers"] // 2
    )
    with np.load(products / "rails" / (name + ".npz")) as payload:
        if digest(products / "rails" / (name + ".npz")) != rails["mesh_sha256"]:
            raise ValueError("detail hardware payload changed")
        transform = payload["sleeper_transforms"][station]
    target = transform[:, 3] + [0, 0, 0.18]
    across, along = transform[:, 0].copy(), transform[:, 1].copy()
    across /= np.linalg.norm(across)
    along /= np.linalg.norm(along)
    whole = products / (name + ".png")
    original = products / (name + "-whole.png")
    if original.exists():
        raise ValueError("fresh detail composition required")
    row = json.loads((products / (name + "-scene.json")).read_text())[0]
    if digest(Path(row["mesh"])) != row["mesh_sha256"]:
        raise ValueError("detail belongs to another whole transport body")
    detail = dict(
        row,
        id=name + "-rail-detail",
        output=str(products / (name + "-rail-detail.png")),
        eye=(target + across * 12 + along * 8 + [0, 0, 8]).tolist(),
        target=target.tolist(),
        span_m=25.0,
        profile=[384, 216],
        detail_view=True,
    )
    scene = products / (name + "-detail-scene.json")
    scene.write_text(json.dumps([detail], indent=2) + "\n")
    subprocess.run(
        [
            "nice",
            "-n",
            "10",
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
        timeout=90,
    )
    whole.rename(original)
    with Image.open(original) as source:
        image = source.convert("RGB")
    if image.size != (1280, 720):
        raise ValueError("only 1280x720 gallery compositions allowed")
    with Image.open(detail["output"]) as source:
        if source.size != (384, 216):
            raise ValueError("fresh full rail detail required")
        image.paste(source.convert("RGB"), (8, 8))
    draw = ImageDraw.Draw(image)
    draw.rectangle((7, 7, 392, 224), outline=(100, 110, 110))
    draw.rectangle((8, 8, 391, 30), fill=(244, 245, 242))
    draw.text(
        (14, 11),
        "Schienenprofil und Schwellen",
        font=ImageFont.load_default(size=14),
        fill=(45, 55, 55),
    )
    image.save(whole)
    proof = {
        "same_actual_mesh_sha256": row["mesh_sha256"],
        "chosen_piece": accepted[chosen],
        "whole_sha256": digest(original),
        "detail_sha256": digest(Path(detail["output"])),
        "image_sha256": digest(whole),
        "camera": detail,
        "scope": "One complete tile plus an actual 25m orthographic 3D rail detail; original exact shared mesh, no added geometry, no height exaggeration, native pixels or textures.",
    }
    (products / (name + "-detail-proof.json")).write_text(
        json.dumps(proof, indent=2) + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--place", required=True)
    args = parser.parse_args()
    render(args.products, args.place)


if __name__ == "__main__":
    main()
