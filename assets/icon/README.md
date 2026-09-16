# EWAF application icon

The golden folder holds a seven-day calendar with Thursday highlighted. The folder front has no checkmark badge.

Open `EWAF.icon` in Apple Icon Composer to edit the macOS material, lighting, background and appearance settings. The document contains three vector layers: folder front, calendar and folder back. Default, Dark and Mono appearances use native Liquid Glass effects. The document targets macOS only.

`ewaf.svg` owns the shared geometry and the flat Windows/Linux design. To change geometry, edit its `folder-back`, `calendar` and `folder-front` elements, install librsvg (`rsvg-convert`), then run `python3 script/generate_icons.py` and `python3 script/check_icons.py`. The generator updates the SVGs inside `EWAF.icon/Assets` without changing Composer settings, plus the PNG, ICO and flat ICNS exports. Reopen Composer after regenerating geometry. The checker rejects stale layer geometry and exports.

macOS packaging requires Xcode 26 or later. Both `script/package.sh` and the Xcode project compile `EWAF.icon` into `Assets.car` and an ICNS compatibility icon with a macOS 14 deployment target. The compiler supplies `CFBundleIconName` and `CFBundleIconFile`. The compiled catalog retains the native light, dark and tinted image stacks; older macOS versions use the generated compatibility artwork. Build output stays outside Git.

Windows embeds `ewaf.ico` in the executable and loads it through `AppWindow.SetIcon`. Linux uses `ewaf.svg` in its hicolor theme and embedded GResource. `ewaf.png` is the shared flat preview; `ewaf.icns` is a flat design export, not the macOS app's compiled icon. These committed exports are artwork resources. Normal platform builds do not require librsvg. Package checks verify the native macOS appearance stacks, Windows executable/window artwork and Linux SVG installation.
