from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import cleanup_temp as cleanup


class CleanupTests(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="outshine-cleanup-test-")
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name).resolve()
        self.repo = self.root / "repo"
        self.repo.mkdir()
        cleanup.git(self.repo, "init", "-q", "-b", "master")
        (self.repo / "source.cpp").write_text("source")
        cleanup.git(self.repo, "add", "source.cpp")
        cleanup.git(self.repo, "-c", "user.name=cleanup-test", "-c", "user.email=test@example.invalid",
                    "commit", "-qm", "fixture")
        self.worktree = self.root / "outshine-worktree-old"
        cleanup.git(self.repo, "worktree", "add", "--detach", str(self.worktree))
        self.fields = dict(cleanup.worktrees(self.repo))[self.worktree]
        self.roots = {self.root}
        self.keep = {self.repo}
        self.active = ([], "")

    def eligible(self):
        return cleanup.disposable_worktree(self.repo, self.worktree, self.fields,
                                            self.roots, self.keep, self.active)

    def test_clean_merged_worktree_is_disposable(self):
        self.assertTrue(self.eligible())

    def test_removal_deletes_only_the_registered_clean_worktree(self):
        with patch.object(cleanup, "running", return_value=self.active):
            cleanup.remove(self.repo, "worktree", self.worktree, self.fields,
                           self.roots, self.keep)
        self.assertFalse(self.worktree.exists())
        self.assertEqual(len(cleanup.worktrees(self.repo)), 1)
        self.assertTrue((self.repo / "source.cpp").exists())

    def test_changes_and_untracked_work_are_preserved(self):
        (self.worktree / "source.cpp").write_text("edited")
        self.assertFalse(self.eligible())
        (self.worktree / "source.cpp").write_text("source")
        (self.worktree / "new.cpp").write_text("new work")
        self.assertFalse(self.eligible())

    def test_unmerged_commit_is_preserved(self):
        (self.worktree / "source.cpp").write_text("unmerged")
        cleanup.git(self.worktree, "add", "source.cpp")
        cleanup.git(self.worktree, "-c", "user.name=cleanup-test", "-c", "user.email=test@example.invalid",
                    "commit", "-qm", "unmerged")
        self.fields = dict(cleanup.worktrees(self.repo))[self.worktree]
        self.assertFalse(self.eligible())

    def test_keep_lock_live_process_and_foreign_root_are_preserved(self):
        self.keep.add(self.worktree)
        self.assertFalse(self.eligible())
        self.keep.remove(self.worktree)
        self.fields["locked"] = ""
        self.assertFalse(self.eligible())
        self.fields.pop("locked")
        self.active = ([self.worktree / "build"], "")
        self.assertFalse(self.eligible())
        self.active = ([], f"compiler --root {self.worktree}")
        self.assertFalse(self.eligible())
        self.active = ([], "")
        self.roots = {self.root / "foreign"}
        self.assertFalse(self.eligible())

    def test_reference_archive_preserves_exact_bytes(self):
        reference = self.worktree / "build/shots/reference/oracle.png"
        reference.parent.mkdir(parents=True)
        reference.write_bytes(b"reference bytes")
        cleanup.preserve_references(self.repo, self.worktree)
        preserved = self.repo / "build/shots/reference/retired-worktrees" / self.worktree.name / "oracle.png"
        self.assertEqual(preserved.read_bytes(), reference.read_bytes())
        preserved.write_bytes(b"existing reference")
        with self.assertRaises(RuntimeError):
            cleanup.preserve_references(self.repo, self.worktree)
        self.assertEqual(preserved.read_bytes(), b"existing reference")

    def test_cache_and_reference_symlinks_prevent_worktree_deletion(self):
        cache = self.worktree / "build/cache"
        cache.mkdir(parents=True)
        self.assertFalse(self.eligible())
        cache.rmdir()
        reference = self.worktree / "build/shots/reference"
        reference.parent.mkdir()
        reference.symlink_to(self.root)
        self.assertFalse(self.eligible())

    def test_candidates_retain_inputs_and_current_checkout_cache(self):
        for name in cleanup.PROTECTED:
            (self.root / name).mkdir()
        old_cache = self.root / ("outshine-tests." + str(self.worktree).replace("/", "_"))
        old_cache.mkdir()
        current_cache = self.root / ("outshine-tests." + str(self.repo).replace("/", "_"))
        current_cache.mkdir()
        foreign = self.root / "unrelated-data"
        foreign.mkdir()
        selected = {path for _, path, _ in cleanup.candidates(
            self.repo, self.roots, self.keep, self.active, 0)}
        self.assertEqual(selected, {self.worktree, old_cache})
        self.assertTrue(current_cache.exists() and foreign.exists())

    def test_new_owner_activity_prevents_test_cache_deletion(self):
        cache = self.root / "outshine-tests.fixture"
        cache.mkdir()
        with patch.object(cleanup, "running", return_value=([self.worktree], "")):
            with self.assertRaises(RuntimeError):
                cleanup.remove(self.repo, "tests", cache, {"owner": str(self.worktree)},
                               self.roots, self.keep)
        self.assertTrue(cache.exists())


if __name__ == "__main__":
    unittest.main()
