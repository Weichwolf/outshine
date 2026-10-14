"""Copy selected local evidence into a fixed gallery; missing evidence remains visible."""

import hashlib
import json
import subprocess
from datetime import UTC, datetime
from html import escape
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


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
        if (
            proof["image_sha256"] != digest
            or proof["report_sha256"]
            != hashlib.sha256(report_path.read_bytes()).hexdigest()
        ):
            raise ValueError(
                "Bild, Report und veröffentlichter Nachweis stimmen nicht überein"
            )
        if not origin.get("commit"):
            raise ValueError("Erzeugungscommit fehlt")
        with Image.open(path) as picture:
            dimensions = list(picture.size)
            if picture.format != "PNG":
                raise ValueError("PNG-Bild erforderlich")
            picture.verify()
        name = digest + ".png"
        atomic_write(root / "assets" / name, data)
        record.update(
            status="ready",
            image="/assets/" + name,
            dimensions=dimensions,
            image_sha256=digest,
            render_commit=origin["commit"],
            working_tree_dirty=origin.get("working_tree_dirty", True),
            scope=entry.get("scope", report.get("scope", "")),
        )
    except (OSError, ValueError, KeyError, TypeError) as error:
        record.update(error=str(error))
    return record


def page(images):
    cards = []
    for record in images:
        title = escape(record["title"])
        caption = escape(record["caption"])
        if record["status"] == "ready":
            url = escape(record["image"], quote=True)
            width, height = record["dimensions"]
            picture = (
                f'<a href="{url}"><img src="{url}" alt="{title}" '
                f'width="{width}" height="{height}"></a>'
            )
        else:
            picture = '<p class="missing">Bild noch nicht verfügbar.</p>'
        cards.append(
            f"<figure>{picture}<figcaption><strong>{title}</strong>"
            f"<span>{caption}</span></figcaption></figure>"
        )
    return (
        '<!doctype html><html lang="de"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width, initial-scale=1">'
        '<title>Transport · Outshine</title><link rel="stylesheet" href="/showcase.css">'
        '</head><body><header><h1>Transport</h1><a href="/">Aktualisieren</a></header>'
        "<main>" + "".join(cards) + "</main></body></html>"
    )


def publish(root, catalog):
    root = root.resolve()
    selected = json.loads(catalog.read_text(encoding="utf-8"))
    entries = selected["images"]
    if len(entries) != 4 or len({entry["id"] for entry in entries}) != 4:
        raise ValueError("Die Showcase benötigt genau vier unterschiedliche Bildplätze")
    root.mkdir(parents=True, exist_ok=True)
    (root / "assets").mkdir(exist_ok=True)
    atomic_write(root / "showcase.css", (HERE / "showcase.css").read_bytes())
    images = [image_record(entry, root) for entry in entries]
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    manifest = {
        "updated": datetime.now(UTC).isoformat(timespec="seconds"),
        "commit": commit,
        "images": images,
    }
    atomic_write(
        root / "manifest.json",
        (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode(),
    )
    atomic_write(root / "index.html", page(images).encode("utf-8"))
    (root / "showcase.js").unlink(missing_ok=True)
    return {
        "root": str(root),
        "ready": sum(row["status"] == "ready" for row in images),
        "incomplete": sum(row["status"] == "incomplete" for row in images),
    }
