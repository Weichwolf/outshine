"""Bytes from the glTF-Asset-Generator, under a pin the manifest states (board:1458).

IT IS THIS VENDOR'S OWN AND NOT THE SAMPLE ASSETS' (board:1451). A fetch step decides what lands on
disk, so it is digested with the corpus it serves -- and a corpus whose bytes come from another
repository under another licence is another corpus. Sharing one step would put both repositories'
allowlists in both corpora's cache keys, which is the coupling the per-case digest exists to remove.

WHAT MAY BE REACHED IS ONE REPOSITORY AT ONE COMMIT. A URL that is not this repository at a
forty-character commit is refused by shape rather than by trust: a pin is what makes a case's bytes
reproducible, and a branch name would make them a function of the day.
"""

import re
import urllib.request

from prep.refusal import Refusal
from prep.store import sha256_hex

HOST = "raw.githubusercontent.com"
REPO = "KhronosGroup/glTF-Asset-Generator"

PINNED = re.compile(r"^https://" + re.escape(HOST) + "/" + re.escape(REPO) + r"/([0-9a-f]{40})/(.+)$")


def check_url(url, commit=None):
    """The url, or a refusal naming what was expected. Returns the path inside the repository."""
    match = PINNED.match(url or "")
    if not match:
        raise Refusal("the url " + repr(url),
                      expected="https://" + HOST + "/" + REPO + "/<40-hex commit>/<path>",
                      observed=repr(url),
                      why="a source that is not this repository at a pinned commit is bytes that can "
                          "change under a run")
    if commit is not None and match.group(1) != commit:
        raise Refusal("the url " + url,
                      expected="the commit the manifest pins, " + commit,
                      observed=match.group(1),
                      why="a file fetched from another commit than the case declares is a case about "
                          "something else")
    return match.group(2)


def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    """The bytes into the content store, checked against what the manifest declares."""
    if member is not None:
        raise Refusal("the file " + url,
                      expected="a plain file",
                      observed="a member of an archive",
                      why="this vendor publishes loose files and an archive member here would be a "
                          "shape nobody declared")
    check_url(url, commit)
    if store.enabled and store.has(expected_sha256):
        return "store", len(store.read(expected_sha256))
    with urllib.request.urlopen(url, timeout=60) as response:
        payload = response.read()
    observed = sha256_hex(payload)
    if observed != expected_sha256:
        raise Refusal("the file " + url,
                      expected="sha256 " + expected_sha256,
                      observed="sha256 " + observed,
                      why="the pin is what makes a case's bytes reproducible, and bytes that do not "
                          "match it are not the bytes the case was written against")
    if expected_bytes is not None and len(payload) != expected_bytes:
        raise Refusal("the file " + url,
                      expected=str(expected_bytes) + " bytes",
                      observed=str(len(payload)) + " bytes",
                      why="a length that disagrees with the manifest is a declaration nobody checked")
    store.keep(expected_sha256, payload)
    return "network", len(payload)
