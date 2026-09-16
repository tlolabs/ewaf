# EWAF legacy archive — 1.0.2

This branch preserves the complete repository before the owner-authorized legacy cleanup on 2026-09-16. It is an archive, not the active application or an official release.

The Python application, core, tests and dependencies are retained both at the original root paths and in legacy-python/. The complete original Swift implementation is preserved in this branch’s Git history at commit 4115cc40716296e3d0de0e4a58c85de5c3e59450. The Rust/native migration and its verification history are also retained.

Use git show 4115cc4:Sources/EWAFCore/Models/CivilDate.swift to inspect original Swift code, or git archive 4115cc4 to export the full pre-migration repository without changing the active checkout.

Active development continues on main with Rust, SwiftUI, WinUI 3 and GTK/libadwaita. Legacy implementations must not be restored as competing authorities for application behavior.
