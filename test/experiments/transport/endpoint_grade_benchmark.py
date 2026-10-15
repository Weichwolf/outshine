"""Three fresh bounded workers per method, equal full-width contacts and profile quality."""

import json
import os
import resource
import statistics
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/endpoint-grades"


def limits():
    resource.setrlimit(resource.RLIMIT_CPU, (15, 15))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    methods = []
    common_ids = None
    for method in ("envelope", "hermite"):
        workers = []
        for index in range(3):
            output = OUTPUT / f"{method}-{index}"
            result = subprocess.run(
                [
                    sys.executable,
                    "-W",
                    "error",
                    str(HERE / "endpoint_grade_experiment.py"),
                    "--method",
                    method,
                    "--output",
                    str(output),
                ],
                cwd=ROOT,
                env=dict(os.environ, OPENBLAS_NUM_THREADS="1"),
                capture_output=True,
                text=True,
                timeout=20,
                check=True,
                preexec_fn=limits,
            )
            report = json.loads((output / "report.json").read_text())
            ids = [r["source_id"] for r in report["cases"] if r["valid"]]
            if common_ids is None:
                common_ids = ids
            if ids != common_ids:
                raise ValueError(
                    "methods/workers did not use identical accepted contacts"
                )
            workers.append(report)
            print(result.stdout.strip(), flush=True)
        summaries = {
            "method": method,
            "common_cases": len(common_ids),
            "p50_plan_us_per_case": statistics.median(
                t for w in workers for t in w["plan_batch_ns"]
            )
            / len(common_ids)
            / 1000,
            "current_exec_peak_rss_bytes": max(
                w["current_exec_peak_rss_bytes"] for w in workers
            ),
            "workers": [
                {
                    key: w[key]
                    for key in (
                        "inputs",
                        "omissions",
                        "surface_preparation_ns",
                        "contact_selection_ns",
                        "first_lazy_import_and_profile_ns",
                        "plan_batch_ns",
                        "mesh_sample_audit_write_ns",
                        "current_exec_peak_rss_bytes",
                    )
                }
                for w in workers
            ],
        }
        methods.append(summaries)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "working_tree_dirty": bool(
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
        "methods": methods,
        "common_source_ids": common_ids,
        "limits": {
            "worker_cpu_s": 15,
            "worker_wall_s": 20,
            "worker_address_space_bytes": 2 * 1024**3,
            "blas_threads": 1,
            "max_selected_faces": 65536,
            "max_port_candidates": 65536,
            "max_profile_queries": 65536,
            "max_sample_vertices": 4096,
        },
        "scope": "Linux host, three fresh workers/method, seven batches on identical compatible contacts. Import/preparation/complete contact selection and sampled mesh audits separate. Whole current-exec RSS from /proc, not inherited ru_maxrss. Source capture, native runtime/device/frame budgets and complete C1 network remain unproved.",
    }
    (OUTPUT / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "methods": [
                    {k: v for k, v in m.items() if k != "workers"} for m in methods
                ]
            }
        )
    )


if __name__ == "__main__":
    main()
