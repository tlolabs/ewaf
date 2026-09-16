#!/usr/bin/env python3
from pathlib import Path
import re
from version import ROOT, VERSION
text = (ROOT / 'platform/windows/Directory.Build.props').read_text()
assert '<Version>' + VERSION + '</Version>' in text
assert 'version="' + VERSION + '.0"' in (ROOT / 'platform/windows/EWAF/app.manifest').read_text()
for path in (ROOT / 'crates').glob('*/Cargo.toml'):
    assert 'version.workspace = true' in path.read_text(), str(path)
print('Unified version: ' + VERSION)
