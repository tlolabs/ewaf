import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from script.development_release_eligible import release_affecting


ROOT = Path(__file__).resolve().parents[2]


class DevelopmentReleaseEligibilityTests(unittest.TestCase):
    def test_documentation_only_changes_do_not_publish(self):
        self.assertFalse(
            release_affecting(
                ["README.md", "docs/RELEASING.md", "AGENTS.md"]
            )
        )
        self.assertFalse(release_affecting([]))

    def test_code_packaging_and_workflow_changes_publish(self):
        for path in (
            "crates/ewaf-core/src/lib.rs",
            "script/package_appimage.sh",
            ".github/workflows/native.yml",
            "Cargo.lock",
            "LICENSE",
        ):
            with self.subTest(path=path):
                self.assertTrue(release_affecting(["README.md", path]))

    def test_push_diff_skips_docs_then_accepts_workflow_change(self):
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)

            def git(*args):
                return subprocess.check_output(
                    ["git", *args], cwd=repo, text=True
                ).strip()

            def commit(message):
                git("add", ".")
                git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                    "commit", "-qm", message)
                return git("rev-parse", "HEAD")

            git("init", "-q")
            (repo / "Cargo.toml").write_text("[package]\nname='fixture'\n")
            initial = commit("initial")
            (repo / "README.md").write_text("documentation update\n")
            docs = commit("docs")
            event = repo / "event.json"

            def run(before, current):
                event.write_text(json.dumps({"before": before}))
                env = dict(os.environ, GITHUB_EVENT_NAME="push",
                           GITHUB_EVENT_PATH=str(event), GITHUB_SHA=current)
                return subprocess.check_output(
                    [sys.executable,
                     str(ROOT / "script/development_release_eligible.py")],
                    cwd=repo, env=env, text=True,
                ).strip()

            self.assertEqual(run(initial, docs), "false")
            workflow = repo / ".github/workflows/native.yml"
            workflow.parent.mkdir(parents=True)
            workflow.write_text("name: fixture\n")
            changed = commit("workflow")
            self.assertEqual(run(docs, changed), "true")


if __name__ == "__main__":
    unittest.main()
