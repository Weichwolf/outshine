"""Actual vertical, ribbon and low fallback sources; no new provider acquisition."""

from clad_facade_sources import sources as clad
from roof_crown_sources import sources as crown


def sources():
    controls = crown()
    return [*controls[:3], clad()[1], controls[3]]
