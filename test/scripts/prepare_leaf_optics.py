"""Emit native constants from the owned reference table, preserving its raw gamut values."""
import json
import math
from pathlib import Path


def main():
    source = Path("lib/src/outshine_ref/vegetation/data/leaf-optics-table-1.json")
    recipe = json.loads(source.read_text())
    values = recipe["values"]
    if recipe["version"] != "leaf-optics-table-1" or recipe["limits"] != [80.0, 10.0]:
        raise ValueError("unsupported owned leaf-optics recipe")
    if len(values) != 33 or any(len(row) != 33 for row in values):
        raise ValueError("bounded 33-by-33 raw optical table required")
    rows = [value for row in values for value in row]
    if any(len(row) != 6 or not all(math.isfinite(value) for value in row) for row in rows):
        raise ValueError("six finite raw optical channels required")
    text = "\n".join("{" + ", ".join(format(value, ".17g") for value in row) + "}," for row in rows)
    destination = Path("build/generated/LeafOpticsData.inc")
    destination.parent.mkdir(parents=True, exist_ok=True)
    text += "\n"
    if not destination.exists() or destination.read_text() != text:
        destination.write_text(text)


if __name__ == "__main__":
    main()
