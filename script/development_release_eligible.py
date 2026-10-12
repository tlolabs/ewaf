#!/usr/bin/env python3
"""Decide whether a main-branch build warrants a development release."""

import json
import os
import subprocess
from pathlib import Path


DOCUMENTATION_FILES = {
    "AGENTS.md",
    "CODE_SIGNING_POLICY.md",
    "CONTRIBUTING.md",
    "README.md",
    "SECURITY.md",
}


def release_affecting(paths: list[str]) -> bool:
    return any(
        not (
            path in DOCUMENTATION_FILES
            or path.endswith(".md")
            or path.startswith("docs/")
            or path.startswith(".github/ISSUE_TEMPLATE/")
        )
        for path in paths
    )


def eligible() -> bool:
    event = os.environ["GITHUB_EVENT_NAME"]
    if event == "workflow_dispatch":
        return True
    if event != "push":
        raise ValueError(f"Unsupported development release event: {event}")

    payload = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    previous = payload["before"]
    current = os.environ["GITHUB_SHA"]
    if previous == "0" * 40:
        return True
    result = subprocess.run(
        ["git", "diff", "--name-only", "-z", previous, current],
        check=True,
        stdout=subprocess.PIPE,
    )
    return release_affecting(
        [path.decode() for path in result.stdout.split(b"\0") if path]
    )


if __name__ == "__main__":
    print("true" if eligible() else "false")
