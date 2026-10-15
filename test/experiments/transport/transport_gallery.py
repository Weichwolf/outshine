"""Generate and independently replay the four transport gallery cases offline."""

import argparse
import hashlib
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(label, command, logs, *, timeout=150):
    began = time.perf_counter()
    (logs / (label + "-load.txt")).write_text(
        subprocess.check_output(
            ["ps", "-eo", "pid,ni,pcpu,rss,comm", "--sort=-pcpu"], text=True
        )
    )
    with (logs / (label + ".log")).open("w") as stream:
        process = subprocess.Popen(
            command,
            stdout=stream,
            stderr=subprocess.STDOUT,
            cwd=ROOT,
            start_new_session=True,
        )
        while True:
            waited, status, usage = os.wait4(process.pid, os.WNOHANG)
            if waited:
                process.returncode = os.waitstatus_to_exitcode(status)
                break
            if time.perf_counter() - began > timeout:
                os.killpg(process.pid, signal.SIGKILL)
                os.wait4(process.pid, 0)
                raise TimeoutError("gallery stage exceeded deadline: " + label)
            time.sleep(0.1)
    row = {
        "stage": label,
        "seconds": time.perf_counter() - began,
        "returncode": process.returncode,
        "user_s": usage.ru_utime,
        "system_s": usage.ru_stime,
        "peak_child_rss_bytes": usage.ru_maxrss * 1024,
        "command": command,
    }
    print(json.dumps(row), flush=True)
    if process.returncode:
        raise RuntimeError("stage failed; inspect " + str(logs / (label + ".log")))
    return row


def python(script, *arguments):
    return [sys.executable, "-W", "error", str(HERE / script), *map(str, arguments)]


def render_case(case, args, rails, stages, spec):
    name = case["id"]
    if case.get("recipe") == "bridge_tunnel_reference":
        stages.append(
            run(
                "research-" + name,
                python(
                    "transport_gallery_wien.py",
                    "--reference-root",
                    args.research_root,
                    "--capture",
                    args.data_root,
                    "--output",
                    args.output,
                ),
                args.logs,
                timeout=300,
            )
        )
        return
    stages.append(
        run(
            "ground-" + name,
            python(
                "transport_gallery_surface.py",
                "--flat",
                args.data_root,
                "--rails",
                rails,
                "--dem-sources",
                args.dem_sources,
                "--place",
                name,
                "--output",
                args.output,
            ),
            args.logs,
        )
    )
    stages.append(
        run(
            "ground-replay-" + name,
            python(
                "camera_height_verify.py",
                "--inputs",
                args.output,
                "--result",
                args.output,
                "--rails",
                rails,
                "--place",
                name,
                "--mode",
                "least_squares",
                "--output",
                args.output / (name + "-verification.json"),
            ),
            args.logs,
        )
    )
    stages.append(
        run(
            "render-" + name,
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
                str(args.output / (name + "-scene.json")),
            ],
            args.logs,
        )
    )
    stages.append(
        run(
            "detail-" + name,
            python(
                "transport_gallery_detail.py",
                "--products",
                args.output,
                "--place",
                name,
            ),
            args.logs,
        )
    )
    with Image.open(args.output / (name + ".png")) as image:
        if image.format != "PNG" or list(image.size) != spec["profile"]:
            raise ValueError("fresh full gallery image required")
        image.verify()
    (args.output / "stages.json").write_text(json.dumps(stages, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--dem-sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--logs", type=Path, required=True)
    parser.add_argument("--research-root", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        parser.error("fresh empty gallery output required")
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", str(HERE)], cwd=ROOT
    ):
        parser.error("freeze generator sources before the measured run")
    args.output.mkdir(parents=True, exist_ok=True)
    args.logs.mkdir(parents=True, exist_ok=True)
    capture = json.loads((args.data_root / "capture.json").read_text())
    spec = json.loads((HERE / "reference/transport-gallery.json").read_text())
    if capture["spec_sha256"] != digest(HERE / "reference/transport-gallery.json"):
        raise ValueError("capture belongs to another gallery specification")
    stages = []

    for case in spec["cases"]:
        if case.get("recipe") == "bridge_tunnel_reference":
            continue
        stages.append(
            run(
                "flat-" + case["id"],
                python(
                    "component_surface_experiment.py",
                    "--data-root",
                    args.data_root,
                    "--worker",
                    case["source"],
                    "components",
                ),
                args.logs,
            )
        )
    files = {
        str(p.relative_to(args.data_root)): digest(p)
        for name in ("inputs", "results")
        for p in (args.data_root / name).rglob("*")
        if p.is_file()
    }
    generation = {
        "files": files,
        "rail_cases": {
            r["id"]: capture["cases"][r["id"]]
            for r in spec["cases"]
            if r.get("recipe") != "bridge_tunnel_reference"
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "dirty": False,
        "spec_sha256": capture["spec_sha256"],
    }
    (args.data_root / "generation.json").write_text(
        json.dumps(generation, indent=2) + "\n"
    )
    rails = args.output / "rails"
    stages.append(
        run(
            "rails",
            python(
                "rail_place_experiment.py",
                "--camera-products",
                args.data_root,
                "--output",
                rails,
            ),
            args.logs,
        )
    )
    stages.append(
        run(
            "rails-replay",
            python(
                "rail_place_verify.py",
                "--camera-products",
                args.data_root,
                "--products",
                rails,
                "--output",
                rails / "verification.json",
            ),
            args.logs,
        )
    )
    for case in spec["cases"]:
        render_case(case, args, rails, stages, spec)
    receipt = {
        "generation": generation,
        "stages": stages,
        "manual_review_required": True,
        "images": {
            c["id"]: digest(args.output / (c["id"] + ".png")) for c in spec["cases"]
        },
        "environment": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
    }
    (args.output / "proof.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
