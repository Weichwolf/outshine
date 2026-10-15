"""Reduce the four manually inspected local masters to the requested GitHub 720p."""

import argparse
import json
from pathlib import Path

from PIL import Image

NAMES = ("Laubwald", "Mischwald", "Steppe", "Heide")


def publish(source, output):
    images = []
    for name in NAMES:
        receipt = json.loads((source / (name + ".json")).read_text())
        cost = json.loads((source / (name + "-cost.json")).read_text())
        with Image.open(source / (name + ".png")) as original:
            if (
                receipt["tile"] != name
                or original.size != (1920, 1080)
                or cost["exit_code"] != 0
            ):
                raise ValueError(
                    "four completed, inspected local tile masters required"
                )
            images.append(
                original.convert("RGB").resize((1280, 720), Image.Resampling.LANCZOS)
            )
    output.mkdir(parents=True, exist_ok=True)
    for name, image in zip(NAMES, images, strict=True):
        image.save(output / (name + ".png"), optimize=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[3] / "doc/images/vegetation",
    )
    args = parser.parse_args()
    publish(args.source, args.output)
