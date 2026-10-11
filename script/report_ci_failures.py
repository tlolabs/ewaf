#!/usr/bin/env python3
"""Update the repository's single non-blocking CI/CD failure issue."""
import datetime
import json
import os
import re
import sys
import urllib.error
import urllib.request

TITLE = "Consolidated CI/CD failures and release restrictions"
BEGIN = "<!-- ewaf-ci-state:begin -->"
END = "<!-- ewaf-ci-state:end -->"
INITIAL_FAILURES = {
    "Native platforms / linux (ubuntu-24.04)": {
        "status": "new",
        "platform": "Linux x64",
        "summary": "Packaged Linux window tests failed.",
        "first_seen": "2026-10-07T22:41:29Z",
        "last_seen": "2026-10-07T22:41:29Z",
        "run_url": "https://github.com/tlolabs/ewaf/actions/runs/37697771179",
    },
    "Native platforms / linux (ubuntu-24.04-arm)": {
        "status": "new",
        "platform": "Linux ARM64",
        "summary": "Packaged Linux window tests failed.",
        "first_seen": "2026-10-07T22:41:29Z",
        "last_seen": "2026-10-07T22:41:29Z",
        "run_url": "https://github.com/tlolabs/ewaf/actions/runs/37697771179",
    },
    "Native platforms / windows (windows-2022, x64, x86_64-pc-windows-msvc)": {
        "status": "new",
        "platform": "Windows x64",
        "summary": "Qt installation/package validation failed.",
        "first_seen": "2026-10-07T22:41:29Z",
        "last_seen": "2026-10-07T22:41:29Z",
        "run_url": "https://github.com/tlolabs/ewaf/actions/runs/37697771179",
    },
    "Native platforms / windows (windows-11-arm, ARM64, aarch64-pc-windows-msvc)": {
        "status": "new",
        "platform": "Windows ARM64",
        "summary": "Qt installation failed.",
        "first_seen": "2026-10-07T22:41:29Z",
        "last_seen": "2026-10-07T22:41:29Z",
        "run_url": "https://github.com/tlolabs/ewaf/actions/runs/37697771179",
    },
}


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat()


def state_from_body(body):
    try:
        start = body.index(BEGIN) + len(BEGIN)
        finish = body.index(END, start)
        return json.loads(body[start:finish].strip())
    except (ValueError, json.JSONDecodeError):
        return {"failures": json.loads(json.dumps(INITIAL_FAILURES))}


def run_ref(run):
    return run.get("head_branch") or run.get("head_sha") or "unknown"


def migrate_unscoped_failures(state, api):
    """Recover the branch of records written before failures were ref-scoped."""
    records = state.setdefault("failures", {})
    for key, record in list(records.items()):
        if record.get("ref"):
            continue
        match = re.search(r"/actions/runs/([0-9]+)(?:/|$)", record.get("run_url", ""))
        if match:
            run_id = match.group(1)
            try:
                ref = run_ref(api.request("GET", f"actions/runs/{run_id}") or {})
            except (RuntimeError, urllib.error.URLError):
                ref = f"unknown-{run_id}"
        else:
            ref = "legacy"
        record["ref"] = ref
        workflow, job = key.split(" / ", 1)
        records.setdefault(f"{workflow} / {ref} / {job}", record)
        del records[key]


def merge_run(state, workflow, jobs, run_url, timestamp, ref):
    records = state.setdefault("failures", {})
    for job in jobs:
        key = f"{workflow} / {ref} / {job['name']}"
        conclusion = job.get("conclusion")
        if conclusion in ("failure", "timed_out", "startup_failure", "action_required"):
            previous = records.get(key, {})
            records[key] = {
                "status": "persistent" if previous and previous.get("status") != "resolved" else "new",
                "platform": platform_for(job["name"]),
                "ref": ref,
                "summary": f"Job concluded {conclusion}.",
                "first_seen": previous.get("first_seen", timestamp),
                "last_seen": timestamp,
                "run_url": run_url,
            }
        elif conclusion == "success" and key in records and records[key].get("status") != "resolved":
            records[key].update(status="resolved", resolved_at=timestamp, run_url=run_url)
    return state


def platform_for(job_name):
    lowered = job_name.lower()
    if "windows" in lowered:
        return "Windows"
    if "linux" in lowered or "ubuntu" in lowered:
        return "Linux"
    if "macos" in lowered:
        return "macOS"
    return "Shared CI/CD"


def render_body(state, run, timestamp):
    records = state.get("failures", {})
    active = [(key, value) for key, value in records.items() if value.get("status") != "resolved"]
    resolved = [(key, value) for key, value in records.items() if value.get("status") == "resolved"]
    lines = [
        "## Scope",
        "This consolidated report is informational and never gates development or releases. "
        "Automated runs update failure status; only platform-verified artifacts are publishable.",
        "Failures are tracked by branch or tag so a passing `main` run cannot clear a failing proposal.",
        "",
        "## Latest CI/CD run",
        f"- Workflow: **{run.get('name', 'unknown')}** ({run.get('conclusion', 'unknown')})",
        f"- Branch/ref: `{run.get('head_branch', 'unknown')}` at `{run.get('head_sha', 'unknown')}`",
        f"- [Workflow run]({run.get('html_url', '')})",
        f"- Updated: {timestamp}",
        "",
        "## Unresolved failures",
    ]
    if active:
        lines += ["| Status | Platform | Job | Failure | Last run |", "|---|---|---|---|---|"]
        for key, value in sorted(active):
            lines.append(
                f"| {value.get('status', 'new')} | {value.get('platform', 'unknown')} | "
                f"`{key}` | {value.get('summary', 'Failure')} | "
                f"[run]({value.get('run_url', '')}) |"
            )
    else:
        lines.append("No unresolved workflow failures are currently recorded.")
    lines += ["", "## Resolved by subsequent verification"]
    if resolved:
        for key, value in sorted(resolved, key=lambda item: item[1].get("resolved_at", ""), reverse=True)[:30]:
            lines.append(
                f"- **{key}** passed on [verification run]({value.get('run_url', '')}) "
                f"at {value.get('resolved_at', 'unknown')}."
            )
    else:
        lines.append("No recorded failures have been resolved yet.")
    lines += [
        "",
        "## Security and dependency status",
        "- GitHub Dependabot security updates and vulnerability alerts are enabled.",
        "- The baseline Dependabot alert query returned zero open alerts; this is a point-in-time "
        "observation, not continuous alert telemetry.",
        "- The native workflow has no dedicated code-scanning or dependency-audit job; "
        "the workflow-run report does not query security-alert inventories.",
        "- No security findings are inferred from missing scan results. Failed security "
        "or dependency checks, when added, must be reported here without becoming a release gate.",
        "",
        "## External and authenticated-update restrictions",
        "- `updates/trust.json` has no enrolled update key, Apple team identity or Windows publisher. "
        "The signed updater must continue to fail closed.",
        "- The historical WinUI distribution concern was resolved by the Qt migration; "
        "it is not a Windows release approval gate.",
        "- Organization-level rulesets could not be inspected with available API access. "
        "Repository-level protection and rulesets were not found.",
        "- Direct-download packages are not authenticated automatic-update artifacts. "
        "Do not add them to the signed update manifest or weaken client verification.",
        "",
        "Keep this issue open while tracked workflow failures are unresolved. External "
        "restrictions are recorded for owner visibility but do not gate unrelated work or "
        "independently verified direct releases. Subsequent runs mark matching failures resolved.",
        "",
        BEGIN,
        json.dumps(state, sort_keys=True),
        END,
    ]
    return "\n".join(lines) + "\n"


class GitHub:
    def __init__(self, repo, token):
        self.repo = repo
        self.token = token

    def request(self, method, path, payload=None):
        data = json.dumps(payload).encode() if payload is not None else None
        request = urllib.request.Request(
            f"https://api.github.com/repos/{self.repo}/{path.lstrip('/')}",
            data=data,
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": f"Bearer {self.token}",
                "X-GitHub-Api-Version": "2022-11-28",
                "Content-Type": "application/json",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=30) as response:
                return json.loads(response.read()) if response.status != 204 else None
        except urllib.error.HTTPError as error:
            detail = error.read().decode(errors="replace")
            raise RuntimeError(f"GitHub API {method} {path} failed ({error.code}): {detail}") from error

    def issues(self):
        page = 1
        while True:
            items = self.request("GET", f"issues?state=all&per_page=100&page={page}")
            yield from items
            if len(items) < 100:
                return
            page += 1


def update_issue(api, repo, run, jobs):
    matches = [item for item in api.issues() if item.get("title") == TITLE and "pull_request" not in item]
    issue = matches[0] if matches else None
    body = issue.get("body", "") if issue else ""
    state = state_from_body(body)
    migrate_unscoped_failures(state, api)
    timestamp = utc_now()
    state = merge_run(
        state, run.get("name", "unknown"), jobs, run.get("html_url", ""),
        timestamp, run_ref(run),
    )
    new_body = render_body(state, run, timestamp)
    if issue:
        payload = {"body": new_body}
        if issue["state"] == "closed" and any(
            value.get("status") != "resolved" for value in state.get("failures", {}).values()
        ):
            payload["state"] = "open"
        api.request("PATCH", f"issues/{issue['number']}", payload)
    else:
        api.request("POST", "issues", {"title": TITLE, "body": new_body, "labels": ["build", "release"]})


def main():
    event_path = os.environ.get("GITHUB_EVENT_PATH")
    token = os.environ.get("GH_TOKEN")
    repo = os.environ.get("GITHUB_REPOSITORY")
    if not (event_path and token and repo):
        raise RuntimeError("GITHUB_EVENT_PATH, GH_TOKEN and GITHUB_REPOSITORY are required")
    with open(event_path, encoding="utf-8") as event_file:
        event = json.load(event_file)
    run = event["workflow_run"]
    api = GitHub(repo, token)
    jobs = api.request(
        "GET",
        f"actions/runs/{run['id']}/jobs?per_page=100",
    ).get("jobs", [])
    update_issue(api, repo, run, jobs)
    print(f"Updated the consolidated CI/CD report from run {run['id']}.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, KeyError, RuntimeError, urllib.error.URLError, json.JSONDecodeError) as error:
        print(f"CI/CD failure report update failed: {error}", file=sys.stderr)
        raise SystemExit(1)
