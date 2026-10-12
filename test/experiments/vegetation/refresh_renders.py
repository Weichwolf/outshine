"""Refresh every declared showcase view sequentially after a shared renderer change."""

import hashlib
import importlib.metadata
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/vegetation"


def current(image, report):
    if not image.exists() or report.get("resolution") != [2560, 1440]:
        return False
    if not report.get("blender", "").startswith(importlib.metadata.version("bpy")):
        return False
    if Image.open(image).size != (2560, 1440):
        return False
    for name, digest in report.get("input_sha256", {}).items():
        path = Path(name)
        path = path if path.is_absolute() else ROOT / path
        if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            return False
    return all(
        (HERE / name).exists()
        and hashlib.sha256((HERE / name).read_bytes()).hexdigest() == digest
        for name, digest in report.get("implementation_sha256", {}).items()
    ) and bool(report.get("implementation_sha256"))


def main():
    catalog = json.loads((HERE / "showcase_catalog.json").read_text())
    entries = catalog["renders"] + [
        {"image": f"render/{name}.png"}
        for name in ("views-tree", "cloud-reference", "cloud-tree")
    ]
    for index, entry in enumerate(entries, 1):
        image = OUTPUT / entry["image"]
        report = json.loads(image.with_suffix(".json").read_text())
        if current(image, report):
            print(f"{index}/{len(entries)} current {image.name}", flush=True)
            continue
        scenario = Path(report["scenario"])
        if not scenario.is_absolute():
            scenario = ROOT / scenario
        recipe = json.loads(scenario.read_text())
        recipe["resolution"] = [2560, 1440]
        recipe["source_modified"] = True
        scenario.write_text(json.dumps(recipe, indent=2) + "\n")
        renderer = next(
            (
                name
                for name in ("bark_preview.py", "landscape_preview.py")
                if name in report.get("implementation_sha256", {})
            ),
            "organ_preview.py",
        )
        log = Path("/tmp") / f"outshine-refresh-{image.stem}.log"
        print(
            f"{index}/{len(entries)} rendering {image.name} · {log}",
            flush=True,
        )
        with log.open("w") as stream:
            result = subprocess.run(
                [
                    sys.executable,
                    str(HERE / renderer),
                    "--",
                    "--scenario",
                    str(scenario),
                    "--output",
                    str(image),
                ],
                cwd=ROOT,
                check=False,
                stdout=stream,
                stderr=subprocess.STDOUT,
            )
        if result.returncode:
            raise RuntimeError(
                f"{image.name}: renderer exit {result.returncode}; {log}"
            )
        report = json.loads(image.with_suffix(".json").read_text())
        assert current(image, report), image
        assert Image.open(image).size == (2560, 1440), image
        print(f"{index}/{len(entries)} complete {image.name}", flush=True)


if __name__ == "__main__":
    main()
