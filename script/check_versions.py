#!/usr/bin/env python3
from pathlib import Path
import re
from version import ROOT, VERSION
cmake_text = (ROOT / 'CMakeLists.txt').read_text()
assert f'project(EWAF VERSION {VERSION} ' in cmake_text
for path in (ROOT / 'crates').glob('*/Cargo.toml'):
    if path.parent.name == 'tlo-updater': continue  # Independently versioned reusable library.
    assert 'version.workspace = true' in path.read_text(), str(path)
print('Unified version: ' + VERSION)
