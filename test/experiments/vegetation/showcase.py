"""Publish reviewed vegetation renders and local cost plots to a tiny /tmp showcase."""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from datetime import UTC, datetime
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from cloud_plots import group_figure, raster_figure, render_figure
from fork_plots import fork_figure
from profile_plots import profile_figure
from selection_plots import coverage_figure, storage_figure, views_render_figure
from showcase_plot_sources import PLOT_SOURCES
from spine_plots import spine_figure

ROOT = Path(__file__).resolve().parents[3]
EXPERIMENTS = ROOT / "build/experiments/vegetation"
HERE = Path(__file__).resolve().parent
COLOURS = ("#236c52", "#b98625")
plt.rcParams["svg.hashsalt"] = "outshine-vegetation-showcase"


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def atomic_text(path, value):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(value, encoding="utf-8")
    temporary.replace(path)


def save_plot(figure, name):
    path = EXPERIMENTS / "showcase" / f"{name}.png"
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160, facecolor="#f1f2e7", bbox_inches="tight")
    figure.savefig(
        path.with_suffix(".svg"),
        facecolor="#f1f2e7",
        bbox_inches="tight",
        metadata={"Date": None},
    )
    plt.close(figure)
    return path


def style_axes(axes):
    for axis in np.asarray(axes, dtype=object).flat:
        axis.set_facecolor("#f1f2e7")
        axis.spines[["top", "right"]].set_visible(False)
        axis.grid(axis="y", alpha=0.18)
        axis.set_axisbelow(True)


def growth_plot(report):
    figure, axes = plt.subplots(1, 3, figsize=(11, 4.2), layout="constrained")
    style_axes(axes)
    for axis, species, title in zip(
        axes, ("oak", "beech", "birch"), ("Eiche", "Buche", "Birke")
    ):
        cases = [case for case in report["cases"] if case["species"] == species]
        counts = sorted({case["attractors"] for case in cases})
        for offset, method, label, colour in zip(
            (-0.18, 0.18), ("brute", "tree"), ("Brute force", "cKDTree"), COLOURS
        ):
            samples = np.array(
                [
                    [
                        case["elapsed_ms"][method]
                        for case in cases
                        if case["attractors"] == count
                    ]
                    for count in counts
                ]
            )
            median = np.median(samples, axis=1)
            error = np.vstack(
                (median - samples.min(axis=1), samples.max(axis=1) - median)
            )
            axis.bar(
                np.arange(len(counts)) + offset,
                median,
                0.34,
                color=colour,
                label=label,
                yerr=error,
                capsize=4,
            )
        axis.set_xticks(np.arange(len(counts)), [str(count) for count in counts])
        axis.set_xlabel("Attraktoren")
        axis.set_title(title)
    axes[0].set_ylabel("Gesamte Erzeugung [ms]")
    axes[0].legend(frameon=False)
    figure.suptitle("Identische Astpläne · Aufwand der Nachbarsuche", fontsize=14)
    return save_plot(figure, "growth-cost")


def crown_plot(report):
    case = next(
        case
        for case in report["cases"]
        if case["species"] == "oak" and case["seed"] == 2
    )
    choices = case["camera_choices"]
    figure, axis = plt.subplots(figsize=(8, 4.2), layout="constrained")
    style_axes([axis])
    distance = [choice["distance_m"] for choice in choices]
    for key, label, colour in zip(
        ("full_band_triangles", "selected_triangles"),
        ("Alle Formbänder", "Bedarfsgerechte Formbänder"),
        COLOURS,
    ):
        axis.plot(
            distance,
            [choice[key] for choice in choices],
            "o-",
            color=colour,
            label=label,
            linewidth=2,
        )
    axis.set_xscale("log")
    axis.set_yscale("log", base=2)
    axis.set_xticks(distance, ["50", "180", "800", "15.000"])
    axis.set_xlabel("Kameraentfernung [m]")
    axis.set_ylabel("Hüllendreiecke [Anzahl]")
    axis.set_title("Eichenhülle · Auswahl mit geometrischer 1-px-Schranke")
    axis.legend(frameon=False, loc="upper right")
    return save_plot(figure, "crown-demand")


def demand_plot(report):
    figure, axis = plt.subplots(figsize=(8, 4.2), layout="constrained")
    style_axes([axis])
    labels = (
        "Vollständig aufgezählt",
        "Bei Nahbedarf erzeugt",
        "Davon angefordert",
        "Neue Produkte beim Cachehit",
    )
    values = (
        report["full_generated_members"],
        report["lazy_generated"]["members"],
        report["required_members"],
        report["cache_hit_generated_products"],
    )
    bars = axis.barh(
        labels[::-1], values[::-1], color=("#a8bba4", "#879d42", COLOURS[1], COLOURS[0])
    )
    axis.set_xlim(0, max(values) * 1.18)
    for bar, value in zip(bars, values[::-1]):
        axis.text(
            bar.get_width() + max(values) * 0.025,
            bar.get_y() + bar.get_height() / 2,
            f"{value:,}".replace(",", "."),
            va="center",
        )
    axis.set_xlabel("Standorte / Produkte [Anzahl]")
    axis.set_title("Stabiler Bestandsplan · Detail erst bei Bedarf")
    return save_plot(figure, "stand-demand")


def organ_storage_plot(report):
    figure, axes = plt.subplots(1, 2, figsize=(10, 4.2), layout="constrained")
    style_axes(axes)
    cases = report["storage"]
    x = np.arange(len(cases))
    for offset, method, label, colour in zip(
        (-0.18, 0.18),
        ("shared", "expanded"),
        ("Geteiltes Mesh + Instanzen", "Expandierte Geometrie"),
        COLOURS,
    ):
        memory = [case["bytes"][method] / 1024**2 for case in cases]
        elapsed = [case["median_build_ms"][method] for case in cases]
        samples = np.array([case["build_samples_ms"][method] for case in cases])
        spread = np.vstack(
            (elapsed - samples.min(axis=1), samples.max(axis=1) - elapsed)
        )
        for axis, values, error in zip(axes, (memory, elapsed), (None, spread)):
            bars = axis.bar(
                x + offset,
                values,
                0.34,
                label=label,
                color=colour,
                yerr=error,
                capsize=4 if error is not None else 0,
            )
            axis.bar_label(bars, labels=[f"{value:.3g}" for value in values], padding=5)
    for axis in axes:
        axis.set_xticks(x, [f"{case['leaves']:,}".replace(",", ".") for case in cases])
        axis.set_xlabel("Blätter [Anzahl]")
        axis.margins(y=0.22)
    axes[0].set_ylabel("Geometrie + Platzierungen [MiB]")
    axes[1].set_ylabel("CPU-Zusammenstellung [ms, logarithmisch]")
    axes[1].set_yscale("log")
    axes[0].legend(frameon=False, fontsize=8)
    figure.suptitle(
        "Identische Blattgeometrie · Instanzen statt Expansion", fontsize=14
    )
    return save_plot(figure, "organ-storage")


def plant_request_plot(report):
    cases = report["requests"]["cases"]
    figure, axes = plt.subplots(1, 2, figsize=(10, 4.2), layout="constrained")
    style_axes(axes)
    labels = ["Angeforderter Zweig", "Ganze Pflanze"]
    timings = [case["first_median_ms"] for case in cases]
    spans = np.array(
        [
            [median - min(case["first_ms"]), max(case["first_ms"]) - median]
            for case, median in zip(cases, timings)
        ]
    ).T
    bars = axes[0].bar(labels, timings, color=COLOURS, yerr=spans, capsize=4)
    axes[0].bar_label(bars, labels=[f"{value:.2f}" for value in timings], padding=5)
    axes[0].set_ylabel("Erste Detailanfrage [CPU ms]")
    values = [case["retained_array_bytes"] / 1024 for case in cases]
    bars = axes[1].bar(labels, values, color=COLOURS)
    axes[1].bar_label(bars, labels=[f"{value:.2f}" for value in values], padding=5)
    axes[1].set_ylabel("Gespeicherte Platzierungsarrays [KiB]")
    for axis in axes:
        axis.margins(y=0.22)
    figure.suptitle(
        "Identische angeforderte Blattgruppe · Details nach Bedarf", fontsize=14
    )
    return save_plot(figure, "plant-requests")


def copy_image(source, output):
    with source.open("rb") as handle:
        digest = hashlib.file_digest(handle, "sha256").hexdigest()
    name = f"{source.stem}-{digest[:16]}{source.suffix}"
    destination = output / "images" / name
    destination.parent.mkdir(exist_ok=True)
    if not destination.exists():
        shutil.copyfile(source, destination)
    return {"url": f"/images/{name}", "sha256": digest}


def render_entry(entry, output):
    source = (EXPERIMENTS / entry["image"]).resolve()
    source.relative_to(EXPERIMENTS)
    result = dict(entry)
    if not source.is_file():
        return result | {"missing": True, "quality": "Bild fehlt"}
    metadata = read_json(source.with_suffix(".json"))
    provenance = metadata
    if "commit" not in provenance and "source_report" in entry:
        provenance = read_json(EXPERIMENTS / entry["source_report"])
    return (
        result
        | copy_image(source, output)
        | {
            "commit": provenance.get("commit"),
            "source_modified": provenance.get("source_modified"),
            "resolution": metadata.get("resolution"),
            "engine": metadata.get("engine"),
            "blender": metadata.get("blender"),
        }
    )


def reference_entry(output):
    reference = read_json(HERE / "quality_reference.json")
    source = ROOT / reference["local_path"]
    if (
        not source.is_file()
        or hashlib.sha256(source.read_bytes()).hexdigest() != reference["sha256"]
    ):
        return reference | {"missing": True}
    return reference | copy_image(source, output)


def leaf_lod_plot(report):
    views = [view for case in report["sheets"] for view in case["views"]]
    figure, axes = plt.subplots(1, 2, figsize=(11, 4.2), layout="constrained")
    style_axes(axes)
    axes[0].plot(
        [100 * view["coverage_l1_over_union"] for view in views],
        label="Ebene Blattkarte",
    )
    axes[0].plot(
        [100 * view["spatial16"]["coverage_l1_over_union"] for view in views],
        label="Räumlich · 16 Segmente",
    )
    axes[0].set(
        xlabel="Ansicht (2 Blätter × 12 Richtungen)",
        ylabel="Deckungsabweichung / Vereinigung (%)",
        title="Einzelblatt bei 8 Pixeln",
    )
    axes[0].legend()
    triangles = report["tree"]["leaf_triangles"]
    axes[1].bar(
        ["Fein", "Räumlich", "Karten"],
        [triangles[key] / 1e6 for key in ("fine", "coarse", "cards")],
    )
    axes[1].set(ylabel="Millionen Laubdreiecke", title="Gleicher Plan · 19.208 Blätter")
    return save_plot(figure, "leaf-lod")


def cloud_group_plot(report):
    figure = group_figure(report)
    style_axes(figure.axes)
    return save_plot(figure, "cloud-coverage")


def cloud_raster_plot(report):
    figure = raster_figure(report)
    style_axes(figure.axes)
    return save_plot(figure, "cloud-raster-cost")


def cloud_render_plot(_report):
    return save_plot(render_figure(EXPERIMENTS), "cloud-render-comparison")


def selection_coverage_plot(report):
    figure = coverage_figure(report)
    style_axes(figure.axes)
    return save_plot(figure, "foliage-selection-coverage")


def selection_storage_plot(report):
    figure = storage_figure(report)
    style_axes(figure.axes)
    return save_plot(figure, "foliage-selection-storage")


def views_render_plot(_report):
    return save_plot(views_render_figure(EXPERIMENTS), "foliage-views-shadows")


def profile_plot(report):
    return save_plot(profile_figure(report), "branch-profiles")


def spine_plot(report):
    return save_plot(spine_figure(report), "spatial-frames")


def fork_plot(report):
    return save_plot(fork_figure(report), "organic-forks")


def plot_entries(output, plot_lods):
    builders = {
        method.__name__: method
        for method in (
            fork_plot,
            spine_plot,
            profile_plot,
            views_render_plot,
            selection_coverage_plot,
            selection_storage_plot,
            cloud_group_plot,
            cloud_raster_plot,
            cloud_render_plot,
            leaf_lod_plot,
            growth_plot,
            crown_plot,
            demand_plot,
            organ_storage_plot,
            plant_request_plot,
        )
    }
    definitions = [
        (filename, builders[name], title, caption)
        for filename, name, title, caption in PLOT_SOURCES
    ]
    entries = []
    reports = {}
    for filename, plot, title, description in definitions:
        if plot.__name__ not in plot_lods:
            continue
        lod = plot_lods[plot.__name__]
        report_path = EXPERIMENTS / filename
        if not report_path.is_file():
            entries.append(
                {
                    "title": title,
                    "description": description,
                    "lod": lod,
                    "missing": True,
                }
            )
            continue
        report = read_json(report_path)
        reports[filename] = report
        image_path = plot(report)
        entries.append(
            copy_image(image_path, output)
            | {
                "svg_url": copy_image(image_path.with_suffix(".svg"), output)["url"],
                "lod": lod,
                "title": title,
                "description": description,
                "commit": report.get("commit"),
                "source_modified": report.get("source_modified"),
            }
        )
    return entries, reports


def publish(output):
    output.mkdir(parents=True, exist_ok=True)
    catalog = read_json(HERE / "showcase_catalog.json")
    plots, _reports = plot_entries(output, catalog["plot_lods"])
    payload = {
        "project": "outshine-vegetation",
        "updated_at": datetime.now(UTC).isoformat(timespec="seconds"),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "branch": subprocess.check_output(
            ["git", "branch", "--show-current"], cwd=ROOT, text=True
        ).strip(),
        "lods": catalog["lods"],
        "genera": catalog["genera"],
        "current_work": catalog["current_work"],
        "quality_gap": catalog["quality_gap"],
        "reference": reference_entry(output),
        "renders": [render_entry(entry, output) for entry in catalog["renders"]],
        "plots": plots,
        "metrics": [],
    }
    tracking = subprocess.run(
        ["git", "rev-parse", f"refs/remotes/origin/{payload['branch']}"],
        cwd=ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    payload["pushed"] = (
        tracking.returncode == 0 and tracking.stdout.strip() == payload["commit"]
    )
    payload["working_tree_modified"] = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=ROOT, text=True
        ).strip()
    )
    payload["revision"] = hashlib.sha256(
        json.dumps(payload, sort_keys=True).encode()
    ).hexdigest()
    html = (HERE / "showcase.html").read_text(encoding="utf-8")
    for name in ("showcase.css", "showcase.js"):
        content = (HERE / name).read_text(encoding="utf-8")
        digest = hashlib.sha256(content.encode()).hexdigest()[:16]
        html = html.replace(f'/{name}"', f'/{name}?v={digest}"')
        atomic_text(output / name, content)
    atomic_text(output / "index.html", html)
    for lod in catalog["lods"]:
        page = output / lod["id"]
        page.mkdir(exist_ok=True)
        atomic_text(page / "index.html", html)
    atomic_text(
        output / "status.json", json.dumps(payload, ensure_ascii=False, indent=2) + "\n"
    )
    prune_images(output, payload)
    return payload


def prune_images(output, payload):
    current = {
        entry[key]
        for entry in [*payload["renders"], *payload["plots"], payload["reference"]]
        for key in ("url", "svg_url")
        if key in entry
    }
    for image in (output / "images").glob("*"):
        if image.is_file() and f"/images/{image.name}" not in current:
            image.unlink()
    for image in output.glob("*.png"):
        image.unlink()


def start_server(output, port):
    url = f"http://127.0.0.1:{port}/status.json"
    try:
        with urllib.request.urlopen(url, timeout=2) as response:
            if json.load(response).get("project") == "outshine-vegetation":
                return "läuft bereits"
    except (OSError, ValueError):
        pass
    log_path = Path("/tmp") / f"{output.name}.log"
    with log_path.open("ab") as log:
        process = subprocess.Popen(
            [
                sys.executable,
                str(HERE / "showcase_server.py"),
                "--port",
                str(port),
                "--bind",
                "0.0.0.0",
                "--directory",
                str(output),
            ],
            stdin=subprocess.DEVNULL,
            stdout=log,
            stderr=log,
            start_new_session=True,
        )
    atomic_text(output / "server.pid", str(process.pid) + "\n")
    for _ in range(30):
        if process.poll() is not None:
            raise RuntimeError(f"Webserver beendet; siehe {log_path}")
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                if json.load(response).get("project") == "outshine-vegetation":
                    return f"PID {process.pid}"
        except (OSError, ValueError):
            time.sleep(0.1)
    raise RuntimeError(f"Webserver antwortet nicht; siehe {log_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("/tmp/outshine-vegetation-showcase")
    )
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--start", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.port <= 65535:
        parser.error("Port muss zwischen 1 und 65535 liegen")
    output = args.output.resolve()
    payload = publish(output)
    status = start_server(output, args.port) if args.start else "aktualisiert"
    print(
        f"Showcase {status}: http://localhost:{args.port}/ · Commit {payload['commit'][:12]} · {len(payload['renders'])} Bilder / {len(payload['plots'])} Plots"
    )


if __name__ == "__main__":
    main()
