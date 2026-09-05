"""The A9 survey, mirrored by TUM at one commit: three plain files, each pinned by its digest."""
import re

from prep import fetching
from prep.refusal import Refusal

A9_URL = re.compile(
    r"^https://raw\.githubusercontent\.com/tum-gis/opendrive-testfeld-a9/[0-9a-f]{40}/"
    r"[A-Za-z0-9._%-]+$"
)


def check_url(url, commit=None):
    if commit is None:
        raise Refusal("url " + url, expected="a commit pin",
                      observed="none",
                      why="a mirror moves; the commit is what makes the bytes the survey's")
    if not A9_URL.match(url) or ("/" + commit + "/") not in url:
        raise Refusal("url " + url, expected="TUM's opendrive-testfeld-a9 mirror at the pinned commit",
                      observed=url,
                      why="an unlisted source has no pin, no stated licence and no reason to be trusted")
    return "tum-gis"


_OPENER = fetching.opener(check_url)


def download_to_store(url, expected_sha256, store, expected_bytes=None, commit=None, member=None):
    check_url(url, commit)
    if member is not None:
        raise Refusal("fetch " + url, expected="a plain file", observed="an archive member",
                      why="this corpus ships plain files")
    return fetching.to_store(_OPENER, url, expected_sha256, store, expected_bytes)


def unpack(kind, source, destination):
    raise Refusal("unpack " + str(kind), expected="nothing to unpack", observed=str(kind),
                  why="this corpus ships plain files")
