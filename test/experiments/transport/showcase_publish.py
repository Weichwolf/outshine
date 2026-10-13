"""Copy selected local evidence into a fixed gallery; missing evidence remains visible."""

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

from PIL import Image


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
METRICS = {
    "triangles": ("Dreiecke", ""),
    "modules": ("Bauteile", ""),
    "shared_port_segments": ("Geteilte Randsegmente", ""),
    "area_m2": ("Fahrfläche", "m²"),
    "coverage_error_m2": ("Flächenabweichung", "m²"),
    "maximum_boundary_error_m": ("Randabweichung", "m"),
    "plan_ms": ("Flächenplanung", "ms"),
    "mesh_and_audit_ms": ("Mesh und Prüfung", "ms"),
    "volume_m3": ("Geschlossenes Volumen", "m³"),
    "closing_ms": ("3D-Körperaufbau", "ms"),
    "clearance_m": ("Freie Durchfahrt", "m"),
    "raw_mesh_bytes": ("Geometriebuffer", "Byte"),
    "invalid_grade_faces": ("Flächen über Neigungsgrenze", ""),
    "maximum_normalized_grade": ("Größtes Neigungsverhältnis · Grenze 1", ""),
    "maximum_height_change_m": ("Größte Höhenkorrektur", "m"),
    "p50_fit_ms": ("Netzfit p50 · offline", "ms"),
    "peak_rss_bytes": ("Netzfit Worker-RSS · Peak", "Byte"),
}


def atomic_write(path, data):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def local_path(name):
    path = (ROOT / name).resolve()
    if not path.is_relative_to(ROOT / "build"):
        raise ValueError("Bildnachweise müssen unter build/ liegen")
    return path


def image_record(entry, root):
    record = dict(entry, status="incomplete")
    try:
        path = local_path(entry["image_source"])
        data = path.read_bytes()
        report_path = local_path(entry["report_source"])
        report = json.loads(report_path.read_text(encoding="utf-8"))
        origin_path = local_path(entry["origin_source"])
        origin = json.loads(origin_path.read_text(encoding="utf-8"))
        proof = origin["records"][report_path.stem]
        digest = hashlib.sha256(data).hexdigest()
        if proof["image_sha256"] != digest or proof["report_sha256"] != hashlib.sha256(report_path.read_bytes()).hexdigest():
            raise ValueError("Bild, Report und veröffentlichter Nachweis stimmen nicht überein")
        if not origin.get("commit"):
            raise ValueError("Erzeugungscommit fehlt")
        with Image.open(path) as picture:
            dimensions = list(picture.size)
            if picture.format != "PNG":
                raise ValueError("PNG-Bild erforderlich")
            picture.verify()
        name = digest + ".png"
        atomic_write(root / "assets" / name, data)
        metrics = [dict(label=label, value=report[key], unit=unit)
                   for key, (label, unit) in METRICS.items() if key in report]
        record.update(status="ready", image="/assets/" + name, dimensions=dimensions,
                      image_sha256=digest, metrics=metrics, render_commit=origin["commit"],
                      working_tree_dirty=origin.get("working_tree_dirty", True),
                      scope=entry.get("scope", report.get("scope", "")))
    except (OSError, ValueError, KeyError, TypeError) as error:
        record.update(error=str(error))
    return record


def publish(root, catalog):
    root = root.resolve()
    selected = json.loads(catalog.read_text(encoding="utf-8"))
    root.mkdir(parents=True, exist_ok=True)
    (root / "assets").mkdir(exist_ok=True)
    for name in ("showcase.html", "showcase.css", "showcase.js"):
        destination = "index.html" if name == "showcase.html" else name
        atomic_write(root / destination, (HERE / name).read_bytes())
    images = [image_record(entry, root) for entry in selected["images"]]
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    manifest = dict(updated=datetime.now(timezone.utc).isoformat(timespec="seconds"),
                    commit=commit, sections=selected["sections"], images=images,
                    note=selected["note"], gap=selected["gap"])
    atomic_write(root / "manifest.json", (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode())
    return dict(root=str(root), ready=sum(row["status"] == "ready" for row in images),
                incomplete=sum(row["status"] == "incomplete" for row in images))
