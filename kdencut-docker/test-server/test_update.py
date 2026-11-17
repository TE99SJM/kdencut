"""Host-side regression tests; no Docker daemon or GitHub network required."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("updater", Path(__file__).with_name("update.py"))
updater = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updater)


class UpdaterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.root_patch = patch.object(updater, "ROOT", self.root)
        self.root_patch.start()

    def tearDown(self):
        self.root_patch.stop()
        self.temp.cleanup()

    def test_branch_selection_ignores_newer_branch_without_dockerfile(self):
        work = self.root / "work"
        work.mkdir()

        def git(*args, date=None):
            env = os.environ.copy()
            if date:
                env.update(GIT_AUTHOR_DATE=date, GIT_COMMITTER_DATE=date)
            return subprocess.check_output(["git", "-C", str(work), *args],
                                           env=env, stderr=subprocess.DEVNULL,
                                           text=True).strip()

        git("init", "-b", "old")
        git("config", "user.email", "test@example.invalid")
        git("config", "user.name", "Test")
        (work / "kdencut-docker").mkdir()
        (work / updater.DOCKERFILE).write_text("FROM scratch\n")
        git("add", ".")
        git("commit", "-m", "old", date="2026-01-01T12:00:00Z")
        git("switch", "-c", "feature/newest")
        git("commit", "--allow-empty", "-m", "new", date="2026-01-02T12:00:00Z")
        expected = git("rev-parse", "HEAD")
        git("switch", "-c", "no-docker")
        git("rm", updater.DOCKERFILE)
        git("commit", "-m", "no docker", date="2026-01-03T12:00:00Z")
        subprocess.run(["git", "clone", "--bare", str(work), str(self.root / "source.git")],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.assertEqual(updater.select_revision()["commit"], expected)
        self.assertEqual(updater.select_revision("old")["branch"], "old")
        with self.assertRaises(RuntimeError):
            updater.select_revision("no-docker")

    def exercise(self, previous=None, failed=None, build_error=None, ready=(True,), retry=False):
        revision = {"branch": "new", "commit": "b" * 40, "image": "kdencut-test:" + "b" * 40}
        if previous:
            updater.write_json(self.root / "deployed.json", previous)
        if failed:
            updater.write_json(self.root / "failed.json", failed)
        with patch.object(updater, "fetch_source"), \
             patch.object(updater, "select_revision", return_value=revision), \
             patch.object(updater, "build", side_effect=build_error) as build, \
             patch.object(updater, "compose") as compose, \
             patch.object(updater, "ready", side_effect=ready), \
             patch.object(updater, "run") as run:
            error = None
            try:
                updater.update(retry)
            except RuntimeError as exc:
                error = exc
        return revision, build, compose, run, error

    def test_failed_build_does_not_recreate_container_or_change_deployed_state(self):
        old = {"branch": "old", "commit": "a" * 40, "image": "kdencut-test:old"}
        revision, _, compose, _, error = self.exercise(old, build_error=RuntimeError("compiler failed"))
        self.assertIsNotNone(error)
        compose.assert_not_called()
        self.assertEqual(updater.read_json(self.root / "deployed.json"), old)
        self.assertEqual(updater.read_json(self.root / "failed.json")["commit"], revision["commit"])

    def test_startup_failure_restarts_old_image_and_keeps_state(self):
        old = {"branch": "old", "commit": "a" * 40, "image": "kdencut-test:old"}
        revision, _, compose, run, error = self.exercise(old, ready=(False, True))
        self.assertIsNotNone(error)
        self.assertEqual([x.args[0] for x in compose.call_args_list], [revision["image"], old["image"]])
        self.assertEqual(updater.read_json(self.root / "deployed.json"), old)
        run.assert_not_called()

    def test_success_records_commit_then_removes_only_previous_test_image(self):
        old = {"branch": "old", "commit": "a" * 40, "image": "kdencut-test:old"}
        revision, _, _, run, error = self.exercise(old)
        self.assertIsNone(error)
        self.assertEqual(updater.read_json(self.root / "deployed.json"), revision)
        run.assert_called_once_with("docker", "image", "rm", old["image"], check=False)

    def test_failed_commit_waits_for_new_commit_unless_retry_requested(self):
        failed = {"commit": "b" * 40}
        _, build, compose, _, _ = self.exercise(failed=failed)
        build.assert_not_called()
        compose.assert_not_called()
        _, build, _, _, error = self.exercise(failed=failed, retry=True)
        build.assert_called_once()
        self.assertIsNone(error)

    def test_deployed_commit_is_not_rebuilt(self):
        old = {"branch": "old", "commit": "b" * 40, "image": "kdencut-test:existing"}
        _, build, compose, _, error = self.exercise(old)
        build.assert_not_called()
        compose.assert_not_called()
        self.assertIsNone(error)


if __name__ == "__main__":
    unittest.main()
