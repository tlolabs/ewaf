#!/usr/bin/env python3
"""Single application version from the Rust workspace manifest."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parent.parent
VERSION = re.search(r'^version = "([0-9]+\.[0-9]+\.[0-9]+)"$', (ROOT / 'Cargo.toml').read_text(), re.M).group(1)
if __name__ == '__main__':
    print(VERSION)
