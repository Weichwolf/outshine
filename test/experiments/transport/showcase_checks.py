"""Exercise publication failures and the HTTP boundary without native engine tests."""

import hashlib
import json
import tempfile
import unittest
from functools import partial
from html.parser import HTMLParser
from http.server import ThreadingHTTPServer
from pathlib import Path
from threading import Thread
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import Request, urlopen

import showcase_publish
from PIL import Image
from showcase_server import ShowcaseHandler


class ShowcaseChecks(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_transport-showcase."
        )
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "input"
        self.source.mkdir(parents=True)
        self.site = self.root / "site"
        (self.site / "assets").mkdir(parents=True)
        self.entry = {
            "id": "T",
            "image_source": "input/T.png",
            "report_source": "input/T.json",
            "origin_source": "input/origin.json",
        }
        Image.new("RGB", (80, 40), "#456c58").save(self.source / "T.png")
        (self.source / "T.json").write_text(
            json.dumps({"triangles": 22, "plan_ms": 2.0})
        )
        proof = {
            key: hashlib.sha256((self.source / name).read_bytes()).hexdigest()
            for key, name in (("image_sha256", "T.png"), ("report_sha256", "T.json"))
        }
        origin = {
            "commit": "a" * 40,
            "working_tree_dirty": True,
            "records": {"T": proof},
        }
        (self.source / "origin.json").write_text(json.dumps(origin))

    def record(self):
        return showcase_publish.image_record(self.entry, self.site, self.root)

    def test_valid_image_keeps_origin_dimensions_and_dirty_state(self):
        record = self.record()
        self.assertEqual(record["status"], "ready")
        self.assertEqual(record["dimensions"], [80, 40])
        self.assertTrue(record["working_tree_dirty"])
        self.assertEqual(
            (self.site / record["image"].lstrip("/")).read_bytes(),
            (self.source / "T.png").read_bytes(),
        )

    def test_missing_and_changed_images_remain_incomplete(self):
        (self.source / "T.png").unlink()
        self.assertEqual(self.record()["status"], "incomplete")
        Image.new("RGB", (80, 40), "red").save(self.source / "T.png")
        self.assertEqual(self.record()["status"], "incomplete")

    def test_changed_report_and_missing_origin_remain_incomplete(self):
        (self.source / "T.json").write_text("{}")
        self.assertEqual(self.record()["status"], "incomplete")
        (self.source / "origin.json").unlink()
        self.assertEqual(self.record()["status"], "incomplete")

    def test_sources_cannot_escape_explicit_data_root(self):
        self.entry["image_source"] = "../secret.png"
        self.assertEqual(self.record()["status"], "incomplete")

    def catalog(self):
        entries = []
        for i in range(4):
            entry = dict(
                self.entry, id=str(i), title=f"Stage {i}", caption="Whole tile"
            )
            entries.append(entry)
        path = self.root / "catalog.json"
        path.write_text(json.dumps({"images": entries}))
        return path

    def test_publication_has_four_images_and_escapes_captions(self):
        catalog = self.catalog()
        selected = json.loads(catalog.read_text())
        selected["images"][0]["title"] = '<script>alert("title")</script>'
        selected["images"][0]["caption"] = "A & B"
        catalog.write_text(json.dumps(selected))
        with patch.object(
            showcase_publish.subprocess, "check_output", return_value="a" * 40
        ):
            result = showcase_publish.publish(self.site, catalog, source_root=self.root)
        self.assertEqual((result["ready"], result["incomplete"]), (4, 0))
        tags = []
        parser = HTMLParser()
        parser.handle_starttag = lambda tag, _attributes: tags.append(tag)
        parser.feed((self.site / "index.html").read_text())
        self.assertEqual(tags.count("img"), 4)
        self.assertNotIn("script", tags)
        self.assertIn("A &amp; B", (self.site / "index.html").read_text())

    def test_missing_evidence_keeps_four_slots_and_rejects_other_counts(self):
        catalog = self.catalog()
        selected = json.loads(catalog.read_text())
        selected["images"][0]["image_source"] = "input/missing.png"
        catalog.write_text(json.dumps(selected))
        with patch.object(
            showcase_publish.subprocess, "check_output", return_value="a" * 40
        ):
            result = showcase_publish.publish(self.site, catalog, source_root=self.root)
        self.assertEqual((result["ready"], result["incomplete"]), (3, 1))
        self.assertEqual((self.site / "index.html").read_text().count("<figure>"), 4)
        self.assertIn(
            "Bild noch nicht verfügbar", (self.site / "index.html").read_text()
        )
        for entries in (
            selected["images"][:3],
            selected["images"] + selected["images"][:1],
        ):
            catalog.write_text(json.dumps({"images": entries}))
            with self.assertRaises(ValueError):
                showcase_publish.publish(self.site, catalog, source_root=self.root)

    def test_server_delivers_published_files_and_blocks_directory_escape(self):
        (self.site / "index.html").write_text("<!doctype html><title>Transport</title>")
        (self.site / "manifest.json").write_text("{}")
        record = self.record()
        (self.root / "secret.png").write_bytes(b"private")
        (self.site / "assets/escape.png").symlink_to(self.root / "secret.png")
        server = ThreadingHTTPServer(
            ("127.0.0.1", 0), partial(ShowcaseHandler, directory=str(self.site))
        )
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"http://127.0.0.1:{server.server_port}"
        try:
            for path in ("/", "/manifest.json", record["image"]):
                with urlopen(base + path, timeout=3) as response:
                    self.assertEqual(response.status, 200)
                    self.assertEqual(response.headers["Cache-Control"], "no-store")
            with urlopen(Request(base + "/", method="HEAD"), timeout=3) as response:
                self.assertEqual(response.status, 200)
            for path in (
                "/assets/",
                "/assets/%2e%2e/manifest.json",
                "/assets/escape.png",
                "/secret.png",
                "/showcase.js",
            ):
                with self.assertRaises(HTTPError) as failure:
                    urlopen(base + path, timeout=3)
                self.assertEqual(failure.exception.code, 404)
                failure.exception.close()
        finally:
            server.shutdown()
            thread.join(timeout=3)
            server.server_close()


if __name__ == "__main__":
    unittest.main()
