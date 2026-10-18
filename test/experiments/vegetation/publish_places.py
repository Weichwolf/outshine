"""Publish the four optically reviewed Python Place images without a web server."""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image
from place_camera import PLACES, selected_cameras

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def validate(camera, directory):
    image = directory / f"{camera.name}.png"
    report = json.loads(image.with_suffix(".json").read_text())
    if (
        report["place"] != camera.name
        or report["scenario_sha256"] != camera.scenario_sha256
        or report["source_modified"]
        or report["scene_source_modified"]
    ):
        raise ValueError("clean Python rendering at the actual Place camera required")
    with Image.open(image) as opened:
        resolution = opened.size
    if (
        list(resolution) != report["resolution"]
        or resolution[0] < 1920
        or resolution[1] < 1080
        or abs(resolution[0] / resolution[1] - 16 / 9) > 1e-12
    ):
        raise ValueError("reference Places require at least 1920x1080 at native aspect")
    actual = report["actual_camera"]
    np.testing.assert_allclose(
        actual["eye_enu_m"],
        camera.eye(report["ground_at_camera_asl_m"]),
        atol=0.001,
        rtol=0,
    )
    np.testing.assert_allclose(
        actual["basis_right_up_back"], camera.basis, atol=2e-6, rtol=0
    )
    if abs(actual["vertical_fov_deg"] - camera.vertical_fov_deg) > 2e-5:
        raise ValueError("actual renderer lens must retain the native vertical FOV")
    if not report["input_sha256"] or not report["implementation_sha256"]:
        raise ValueError("rendering inputs and implementation snapshot are required")
    for name, expected in report["input_sha256"].items():
        if digest(Path(name)) != expected:
            raise ValueError(f"rendering input changed: {name}")
    commit = report["commit"]
    if not isinstance(commit, str) or re.fullmatch(r"[0-9a-f]{40}", commit) is None:
        raise ValueError("full rendering commit required")
    for name, expected in report["implementation_sha256"].items():
        code = subprocess.check_output(["git", "show", f"{commit}:{name}"], cwd=ROOT)
        if hashlib.sha256(code).hexdigest() != expected:
            raise ValueError(f"rendering implementation differs from snapshot: {name}")
    return image, {"place": camera.name, "sha256": digest(image), "commit": commit}


def publish(inputs, output):
    rows = [validate(camera, inputs) for camera in selected_cameras(ROOT)]
    output.mkdir(parents=True, exist_ok=True)
    for source, record in rows:
        target = output / source.name
        if not target.is_file() or digest(target) != record["sha256"]:
            shutil.copyfile(source, target)
        if digest(target) != record["sha256"]:
            raise ValueError("published image differs from the reviewed source")
    for previous in output.glob("*.png"):
        if previous.stem not in PLACES:
            previous.unlink()
    return [record for _, record in rows]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "doc/images/vegetation")
    args = parser.parse_args()
    print(json.dumps(publish(args.inputs.resolve(), args.output.resolve()), indent=2))
