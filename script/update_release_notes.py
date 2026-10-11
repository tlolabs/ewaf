#!/usr/bin/env python3
"""Replace the generated platform-availability section without clobbering notes."""
import argparse
import sys

START = "<!-- ewaf-platform-status:start -->"
END = "<!-- ewaf-platform-status:end -->"


def update(body, statuses):
    section = "\n".join(
        [START, "## Platform artifacts"]
        + [f"- {name}: {status}" for name, status in statuses]
        + [END]
    )
    if START in body and END in body:
        before, rest = body.split(START, 1)
        _, after = rest.split(END, 1)
        return f"{before.rstrip()}\n\n{section}{after}\n"
    return f"{body.rstrip()}\n\n{section}\n" if body.strip() else f"{section}\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--status", action="append", nargs=2, required=True, metavar=("PLATFORM", "STATUS"))
    args = parser.parse_args()
    print(update(sys.stdin.read(), args.status), end="")


if __name__ == "__main__":
    main()
