"""Read declared corpus inputs and reject unknown fields, duplicate names and missing entries."""

import json
import os

from . import schema
from .refusal import Refusal

SCHEMA_VERSION = schema.SCHEMA_VERSION
RESERVED_OUTPUT_NAMES = frozenset(["manifest.json", ".prepared-by", "provenance.json"])

def load(path):
    try:
        with open(path, "r") as f:
            document = json.load(f)
    except (OSError, ValueError) as error:
        raise Refusal("manifest " + path, why=str(error))
    return Manifest(document, os.path.dirname(os.path.abspath(path)))

class Manifest:
    def __init__(self, document, directory):
        self.directory = directory
        self.document = schema.check("manifest", "manifest", document)
        self.id = self.document["id"]
        self.title = self.document["title"]
        self.covers = self.document["covers"]
        self.criterion = self.document["criterion"]
        self.subjects = _subjects(self.document["subjects"])
        self.viewport = self.document.get("viewport")
        _check_names(self.subjects)

def _check_names(subjects):
    """One file per name in a case directory, whatever produced it."""
    seen = {}
    for subject in subjects:
        for file in subject.files:
            if file["as"] in RESERVED_OUTPUT_NAMES:
                raise Refusal("manifest.subjects", expected="a name outside the reserved set",
                              observed=file["as"])
            if file["as"] in seen:
                raise Refusal(
                    "manifest.subjects",
                    expected="one file per name in the test directory",
                    observed="%s is claimed by both %s and %s" % (file["as"], seen[file["as"]], subject.id),
                )
            seen[file["as"]] = subject.id

def _subjects(value):
    subjects = [_Subject(i, v) for i, v in enumerate(value)]
    identifiers = [s.id for s in subjects]
    if len(set(identifiers)) != len(identifiers):
        raise Refusal("manifest.subjects", expected="distinct ids", observed=", ".join(identifiers))
    return subjects

class _Subject:
    def __init__(self, index, field):
        where = "manifest.subjects[%d]" % index
        self.id = field["id"]
        self.kind = field["kind"]
        self.name = field["name"]
        self.source = field["source"]
        self.files = field["files"]
        self.entry = field.get("entry", "")
        self.attributes = field.get("attributes", [])
        self.notes = field.get("notes", "")
        for position, file in enumerate(self.files):
            spelling = "%s.files[%d]" % (where, position)
            if (file["role"] == "archive-member") != ("member" in file):
                raise Refusal(
                    spelling + ".member",
                    expected="a member path exactly when the role is archive-member",
                    observed=repr(file.get("member")),
                )
        if self.entry and self.entry not in [f["as"] for f in self.files]:
            raise Refusal(where + ".entry", expected="one of the declared files", observed=self.entry)
