"""Extract the user-required plant-name scope without treating names as generated plants."""

import argparse
import hashlib
import json
import re
from html.parser import HTMLParser
from pathlib import Path

SOURCE = "https://en.wikipedia.org/wiki/List_of_plants_by_common_name"


class PlantNames(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.letter = None
        self.items = []
        self.stack = []
        self.citation = 0

    def handle_starttag(self, tag, attrs):
        attributes = dict(attrs)
        if tag == "h2":
            section = attributes.get("id", "")
            self.letter = section if len(section) == 1 and section.isupper() else None
        if tag == "sup":
            self.citation += 1
        if tag in ("li", "figcaption") and self.letter:
            item = {
                "section": self.letter,
                "parts": [],
                "links": [],
                "source_id": attributes.get("id"),
            }
            self.items.append(item)
            self.stack.append((tag, item))
        if self.stack and tag == "a" and not self.citation:
            href = attributes.get("href", "")
            if href.startswith("https://en.wikipedia.org/wiki/"):
                self.stack[-1][1]["links"].append(href)

    def handle_endtag(self, tag):
        if tag == "section":
            self.letter = None
        if tag == "sup":
            self.citation = max(self.citation - 1, 0)
        if self.stack and tag == self.stack[-1][0]:
            self.stack.pop()

    def handle_data(self, data):
        if self.stack and not self.citation:
            self.stack[-1][1]["parts"].append(data)


def extract(source):
    html = source.read_text()
    revision = re.search(r'wgRevisionId\\?":([0-9]+)', html)
    if revision is None:
        raise ValueError("source revision must be identified")
    parser = PlantNames()
    parser.feed(html)
    records = []
    for item in parser.items:
        text = " ".join("".join(item["parts"]).split())
        if not text:
            continue
        if item["source_id"] is None:
            if text == "Top" or (len(text) == 1 and text.isupper()):
                continue
            raise ValueError("unidentified list entry must be reviewed")
        common, separator, targets = text.partition("–")
        if not separator:
            common, separator, targets = text.partition("—")
        record = {
            "common_name": common.strip(),
            "listed_target": targets.strip() if separator else None,
            "source_links": sorted(set(item["links"])),
            "source_section": item["section"],
            "source_id": item["source_id"],
            "status": "unreviewed target; no generated plant or LOD coverage implied",
        }
        records.append(record)
    assert len(records) > 100 and len({r["source_section"] for r in records}) > 20
    return {
        "source_url": SOURCE,
        "source_revision": int(revision.group(1)),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "source_license": "CC BY-SA 4.0; factual plant-name scope extracted from Wikipedia contributors",
        "scope": "all nonempty list items and plant figure captions in alphabetical sections; aliases, ambiguous names and source errors retained for botanical review",
        "entries": records,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument(
        "--output", type=Path, default=Path(__file__).with_name("plant_targets.json")
    )
    args = parser.parse_args()
    result = extract(args.source)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(
        f"{len(result['entries'])} source name entries; revision {result['source_revision']}; no coverage claim"
    )


if __name__ == "__main__":
    main()
