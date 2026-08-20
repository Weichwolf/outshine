"""The global content store, as src/data/ContentStore.{h,cpp} defines it."""

import hashlib
import json
import os
import tempfile

from .refusal import Refusal

STORE_LEAF = "outshine-content"

DERIVATION_VERSION = 10

DERIVATION_ID = "outshine.corpus.prep"

def sha256_hex(data):
    return hashlib.sha256(data).hexdigest()

def sha256_of_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()

def canonical_json(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True)

def default_directory():
    return os.path.join(tempfile.gettempdir(), STORE_LEAF)

def derived_key(kind, recipe):
    """The name a derived artefact is filed under.

    Newline-separated with the version inside, exactly as `Data::ContentKey` builds it, so the two
    producers cannot form one key by accident and cannot form two keys for one thing.
    """
    subject = "\n".join([DERIVATION_ID, str(DERIVATION_VERSION), kind, canonical_json(recipe)])
    return sha256_hex(subject.encode("utf-8"))

class ContentStore:
    """A directory of files and nothing else: hash = filename, no index, no sidecar."""

    def __init__(self, directory=None, enabled=True):
        self.directory = directory or default_directory()
        self.enabled = enabled
        self.hits = 0
        self.misses = 0
        self.writes = 0
        if self.enabled:
            os.makedirs(self.directory, exist_ok=True)

    def path(self, key):
        _check_key(key)
        return os.path.join(self.directory, key)

    def has(self, key):
        if not self.enabled:
            return False
        found = os.path.isfile(self.path(key))
        if found:
            self.hits += 1
        else:
            self.misses += 1
        return found

    def read(self, key):
        if not self.has(key):
            return None
        with open(self.path(key), "rb") as f:
            return f.read()

    def keep(self, key, data):
        self.keep_stream(key, lambda f: f.write(data))

    def keep_file(self, key, source_path):
        def copy(out):
            with open(source_path, "rb") as src:
                for block in iter(lambda: src.read(1 << 20), b""):
                    out.write(block)

        self.keep_stream(key, copy)

    def keep_stream(self, key, write):
        """A name never precedes its bytes: the write lands on a temporary and rename publishes it."""
        if not self.enabled:
            return
        final = self.path(key)
        handle, temp = tempfile.mkstemp(prefix="." + key + ".", dir=self.directory)
        try:
            with os.fdopen(handle, "wb") as out:
                write(out)
            os.replace(temp, final)
            self.writes += 1
        except BaseException:
            if os.path.exists(temp):
                os.remove(temp)
            raise

    def forget(self, key):
        """DROP AN ENTRY THAT HAS BEEN COPIED OUT AND WILL NOT BE ASKED FOR AGAIN (board:1369).

        The owner's ruling: a Cycles render is not worth caching, because this machine has more CPU
        than disk. The store held 54 GB over 18 655 entries and could not tell a live entry from a
        dead one -- an access time this filesystem does not maintain, a modification time a cache hit
        never touches -- so it grew by a case's whole output on every manifest edit.

        THE FETCH CACHE IS NOT THIS AND MUST NOT BECOME IT. Upstream bytes cost the NETWORK, and the
        network rate-limits; a render costs the CPU, and the CPU is what this machine has.
        """
        if not self.enabled:
            return False
        here = self.path(key)
        try:
            os.remove(here)
            return True
        except OSError:
            return False

    def copy_out(self, key, destination_path):
        if not os.path.isfile(self.path(key)):
            raise Refusal(
                "content store read " + key,
                expected="a file named for its key",
                observed="no such file under " + self.directory,
            )
        os.makedirs(os.path.dirname(destination_path) or ".", exist_ok=True)
        temp = destination_path + ".partial"
        with open(self.path(key), "rb") as src, open(temp, "wb") as out:
            for block in iter(lambda: src.read(1 << 20), b""):
                out.write(block)
        os.replace(temp, destination_path)

def _check_key(key):
    if len(key) != 64 or any(c not in "0123456789abcdef" for c in key):
        raise Refusal(
            "content store key " + repr(key),
            expected="64 lowercase hex digits",
            observed=repr(key),
            why="hash = filename, so anything else could name a path outside the store",
        )
