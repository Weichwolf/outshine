"""Existing fixed courtyard and compact provider controls at two height programmes."""

from terraced_admission_probe import controls
from terraced_crown_plan import plan as roofed_plan
from terraced_mass_plan import plan
from terraced_mass_sources import sources
from terraced_terrace_plan import plan as terrace_plan


def cases():
    rows = [
        (row, "cascade" if i % 2 == 0 else "paired-wings")
        for i, row in enumerate(sources())
    ]
    rows += [(row, row["programme"]) for row in controls() if row["shape"] == "compact"]
    return [
        (row, label, terrace_plan(mass))
        for row, kind in rows
        for label, mass in (
            ("original", plan(row, kind)),
            ("roof-coupled", roofed_plan(row, kind).body),
        )
    ]
