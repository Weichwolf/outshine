"""GeographicLib's published test data: one archive, one digest, CC0."""

import gzip
import os
import re
import tempfile

from prep import fetching
from prep.refusal import Refusal
from prep.store import sha256_hex

GEOGRAPHICLIB_URL = re.compile(
    r"^https://(downloads|[a-z0-9-]+\.dl)\.sourceforge\.net/project/geographiclib/testdata/"
    r"[A-Za-z0-9._-]+(\?[A-Za-z0-9._=&-]*)?$"
)

def check_url(url, commit=None):
    if commit is not None:
        raise Refusal("url " + url, expected="no commit pin",
                      observed="a commit " + str(commit),
                      why="published test data is versioned by its own digest, not by a repository")
    if not GEOGRAPHICLIB_URL.match(url):
        raise Refusal("url " + url, expected="GeographicLib's published testdata directory",
                      observed=url,
                      why=("an unlisted source has no pin, no stated licence and no reason to be "
                           "trusted -- SourceForge redirects a download onto one of its own "
                           "mirrors, and the file's digest is what pins the bytes either way"))
    return "geographiclib"

_OPENER = fetching.opener(check_url)

def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    check_url(url, commit)
    if member is not None:
        raise Refusal("fetch " + url, expected="a plain file", observed="an archive member",
                      why="this corpus ships one file")
    return fetching.to_store(_OPENER, url, expected_sha256, store, expected_bytes)

def unpack(kind, source, destination):
    if kind != "gzip":
        raise Refusal("unpack " + str(kind), expected="gzip",
                      observed=str(kind), why="this corpus ships one gzip archive")
    with gzip.open(source, "rb") as held, open(destination, "wb") as out:
        while True:
            block = held.read(1 << 20)
            if not block:
                break
            out.write(block)
