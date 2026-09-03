#!/usr/bin/env python3
"""The scenario's GRAMMAR, read from the one table that states it.

Two checkers stand on this table -- the reader must read nothing it refuses, the writer must write
everything it declares -- and a second copy of the parser would be the very drift both guard
against.

A ROW'S CHILD LIST MAY BE SEVERAL ADJACENT LITERALS, which C concatenates and a regex over one of
them silently truncates. Joining them is the difference between reading the grammar and reading its
first line.
"""
import pathlib
import re

TREE = pathlib.Path(__file__).resolve().parents[2]
READER = TREE / "src" / "scenario" / "ScenarioRead.cpp"
WRITER = TREE / "src" / "scenario" / "ScenarioWrite.cpp"


def declared():
    source = READER.read_text()
    rows = {}
    # THE ROWS ARE DESIGNATED INITIALISERS. They were positional once, and this walk still looked
    # for `{"scenario/..."` long after the table had become `{.Path = "scenario/...", .Children =`.
    # It then found NO rows and reported every child the reader reads as undeclared -- a measure
    # that cannot see, reporting 76 defects where there were none. Both spellings are read now, so
    # the walk survives the table changing its mind again.
    for row in re.finditer(r'\{(?:\.Path = )?"(scenario[^"]*)",\s*(?:\.Children =)?((?:\s*"[^"]*")*)',
                           source):
        joined = "".join(re.findall(r'"([^"]*)"', row.group(2)))
        rows[row.group(1)] = set(joined.split())
    return rows


def children():
    every = set()
    for one in declared().values():
        every |= one
    return every
