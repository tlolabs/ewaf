import unittest
from unittest.mock import patch

from script.report_ci_failures import (
    GitHub,
    INITIAL_FAILURES,
    merge_run,
    migrate_unscoped_failures,
    render_body,
    state_from_body,
)
from script.update_release_notes import END, START, update


class CIFailureReportTests(unittest.TestCase):
    def test_github_api_requests_use_bearer_token(self):
        class Response:
            status = 200

            def __enter__(self):
                return self

            def __exit__(self, *_):
                return False

            def read(self):
                return b"{}"

        with patch("urllib.request.urlopen", return_value=Response()) as urlopen:
            GitHub("tlolabs/ewaf", "test-token").request("GET", "issues")
        request = urlopen.call_args.args[0]
        self.assertEqual(request.get_header("Authorization"), "Bearer test-token")

    def test_new_failures_are_labeled_and_include_platform(self):
        state = {"failures": {}}
        merge_run(
            state,
            "Native platforms",
            [{"name": "windows (windows-2022, x64)", "conclusion": "failure"}],
            "https://github.com/tlolabs/ewaf/actions/runs/1",
            "2026-10-11T12:00:00Z",
            "main",
        )
        record = state["failures"]["Native platforms / main / windows (windows-2022, x64)"]
        self.assertEqual(record["status"], "new")
        self.assertEqual(record["platform"], "Windows")

    def test_repeated_failure_is_persistent_then_success_resolves_it(self):
        old_key = next(iter(INITIAL_FAILURES))
        key = old_key.replace("Native platforms / ", "Native platforms / main / ", 1)
        state = {"failures": {key: dict(INITIAL_FAILURES[old_key], ref="main")}}
        name = old_key.split(" / ", 1)[1]
        merge_run(state, "Native platforms", [{"name": name, "conclusion": "failure"}], "run-2", "time-2", "main")
        self.assertEqual(state["failures"][key]["status"], "persistent")
        merge_run(state, "Native platforms", [{"name": name, "conclusion": "success"}], "run-3", "time-3", "main")
        self.assertEqual(state["failures"][key]["status"], "resolved")
        self.assertEqual(state["failures"][key]["resolved_at"], "time-3")

    def test_branch_failures_are_not_resolved_by_main_success(self):
        name = "windows (windows-11-arm, ARM64)"
        branch = "dependabot/github_actions/actions/upload-artifact-6"
        state = {"failures": {}}
        merge_run(state, "Native platforms", [{"name": name, "conclusion": "failure"}], "pr-run", "time-1", branch)
        merge_run(state, "Native platforms", [{"name": name, "conclusion": "success"}], "main-run", "time-2", "main")
        key = f"Native platforms / {branch} / {name}"
        self.assertEqual(state["failures"][key]["status"], "new")
        merge_run(state, "Native platforms", [{"name": name, "conclusion": "success"}], "pr-retry", "time-3", branch)
        self.assertEqual(state["failures"][key]["status"], "resolved")

    def test_legacy_failure_recovers_its_branch_from_run(self):
        class FakeGitHub:
            def request(self, method, path):
                assert method == "GET" and path == "actions/runs/38182156983"
                return {"head_branch": "dependabot/github_actions/actions/upload-artifact-6"}

        old_key = "Native platforms / windows (windows-11-arm, ARM64)"
        state = {"failures": {old_key: {"status": "persistent", "run_url": "https://github.com/tlolabs/ewaf/actions/runs/38182156983"}}}
        migrate_unscoped_failures(state, FakeGitHub())
        self.assertNotIn(old_key, state["failures"])
        scoped = "Native platforms / dependabot/github_actions/actions/upload-artifact-6 / windows (windows-11-arm, ARM64)"
        self.assertEqual(state["failures"][scoped]["status"], "persistent")

    def test_issue_body_preserves_state_for_next_run(self):
        state = {"failures": {}}
        merge_run(
            state,
            "Native platforms",
            [{"name": "quality", "conclusion": "failure"}],
            "run-1",
            "time-1",
            "main",
        )
        body = render_body(
            state,
            {"name": "Native platforms", "conclusion": "failure", "html_url": "run-1"},
            "time-1",
        )
        parsed = state_from_body(body)
        self.assertEqual(parsed, state)
        self.assertIn("Shared CI/CD", body)
        self.assertIn("not authenticated automatic-update artifacts", body)

    def test_closed_issue_stays_closed_after_all_failures_are_resolved(self):
        from script.report_ci_failures import update_issue

        class FakeGitHub:
            def __init__(self, body):
                self.issue = {"title": "Consolidated CI/CD failures and release restrictions", "number": 18,
                              "state": "closed", "body": body}
                self.payload = None

            def issues(self):
                yield self.issue

            def request(self, method, path, payload=None):
                if method == "GET":
                    return {"head_branch": "main"}
                self.payload = payload

        state = {"failures": {next(iter(INITIAL_FAILURES)): {"status": "resolved"}}}
        body = render_body(state, {"name": "Native platforms", "conclusion": "success"}, "time")
        api = FakeGitHub(body)
        update_issue(api, "tlolabs/ewaf", {"name": "Native platforms", "conclusion": "success"}, [])
        self.assertNotIn("state", api.payload)
        self.assertEqual(api.issue["state"], "closed")

    def test_closed_issue_reopens_for_unresolved_failure(self):
        from script.report_ci_failures import update_issue

        class FakeGitHub:
            def __init__(self):
                self.issue = {"title": "Consolidated CI/CD failures and release restrictions", "number": 18,
                              "state": "closed", "body": ""}
                self.payload = None

            def issues(self):
                yield self.issue

            def request(self, method, path, payload=None):
                if method == "GET":
                    return {"head_branch": "main"}
                self.payload = payload

        api = FakeGitHub()
        update_issue(
            api,
            "tlolabs/ewaf",
            {"name": "Native platforms", "conclusion": "failure"},
            [{"name": "quality", "conclusion": "failure"}],
        )
        self.assertEqual(api.payload["state"], "open")

    def test_release_notes_update_platform_status_without_losing_existing_notes(self):
        body = update(
            "Highlights\n- Existing release notes.\n",
            [("macOS ARM64", "available"), ("Windows x64", "held")],
        )
        self.assertIn("Existing release notes.", body)
        revised = update(body, [("macOS ARM64", "available"), ("Windows x64", "available")])
        self.assertEqual(revised.count(START), 1)
        self.assertEqual(revised.count(END), 1)
        self.assertIn("Windows x64: available", revised)


if __name__ == "__main__":
    unittest.main()
