# EWAF application icon

Original vector artwork: `ewaf.svg`. A golden folder holds a seven-day calendar, with Thursday highlighted. The dark teal tile, warm folder and simple check remain recognizable at small sizes. Transparent margins preserve the native icon silhouette.

`ewaf.png` is the 1024-pixel preview. `ewaf.icns` supplies standard and Retina macOS sizes. `ewaf.ico` supplies 16, 24, 32, 48, 64, 128 and 256-pixel Windows sizes. Linux consumes the SVG directly. These checked-in files are artwork resources, not compiled application binaries.

To edit, change the SVG, install librsvg (`rsvg-convert`), then run `python3 script/generate_icons.py` and `python3 script/check_icons.py`. Generation uses only Python’s standard library and librsvg. Normal builds need neither an image renderer nor ImageMagick; they use the committed exports. The manifest records source/export hashes to detect stale or corrupt artwork.

macOS uses CFBundleIconFile in both package and Xcode builds. Windows uses ApplicationIcon for Explorer/shortcuts and AppWindow.SetIcon for running windows. GTK embeds the SVG as a GResource for local builds; Debian installs it in the hicolor theme for launchers. Package checks reject absent or mismatched artwork.
