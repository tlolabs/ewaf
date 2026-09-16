# Legacy archive and recovery

The owner authorized legacy retirement on 2026-09-16 after the Rust/native migration passed all six architecture jobs.

- Archive branch: [codex/archive-legacy-1.0.2](https://github.com/tlolabs/ewaf/tree/codex/archive-legacy-1.0.2).
- Immutable archive commit: `16004aa531107d5bcfe926089db3b9bdaa7fa1b5`, version 1.0.2.
- Original Swift/Python baseline: `4115cc40716296e3d0de0e4a58c85de5c3e59450`, preserved in archive history.

The archive contains the root Python application/core, duplicate legacy-python tree, their tests and requirements, the Python CI workflow and historical review plan. The original Swift source is accessible through its baseline commit. To inspect without altering the active checkout:

```sh
git show codex/archive-legacy-1.0.2:ewaf_core.py
git show 4115cc4:Sources/EWAFCore/Models/CivilDate.swift
git archive --format=tar --output=../ewaf-original-source.tar 4115cc4
```

The active 1.0.3 tree retains all current Rust, SwiftUI, WinUI 3 and GTK/libadwaita code, native tests and build tools. Calendar fixtures derived from the old implementation remain active regression data, not a competing implementation. Python is still used for native build/version/package scripts; a distributed application does not require Python. Generated folders and installed preferences are not changed by repository cleanup.
