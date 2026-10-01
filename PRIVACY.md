# Privacy

EWAF creates folders in a destination you choose. Its core operation works offline. The application does not collect, transmit, sell or share your dates, folder names, destination paths, preferences or diagnostics. No analytics, crash-reporting service, remote assets or remote fonts are used. Optional authenticated update checks are described below.

**Network access:** Update checks and downloads use GitHub Releases as described below; installation requires consent. Release-page and Help actions open GitHub in your browser, where your browser and GitHub apply their own privacy policies. Building from source or installing platform dependencies can also require network access.

**Shared desktop runtime components:** Windows, Linux and the internal Mac reference bundle Avalonia and the self-contained .NET runtime. The retired Windows App SDK is absent. Build entry points and CI disable Avalonia/.NET build telemetry. EWAF does not invoke runtime telemetry services; this audit has not measured every indirect framework network behavior on clean Windows/Linux systems. See the [dependency licensing audit](docs/LICENSE_AUDIT.md).

**Local data:** EWAF writes the folders you request. Native preferences retain the default weekday and, where implemented, window/range state. macOS uses app preferences and scene restoration; Windows uses the current user's registry; Linux uses a local JSON preferences file, importing the previous dconf values on first launch. Destinations are held for the current session only. Uninstalling does not delete generated folders.

**Diagnostics:** macOS can expose local system process logs through the optional developer `--logs`/`--telemetry` command; that command streams existing OS logs and adds no telemetry. The shared Avalonia application emits local `Trace` diagnostics and writes optional startup diagnostics only when `EWAF_DIAGNOSTICS_PATH` is set. OS libraries can also emit local diagnostics. EWAF does not upload these logs or impose a separate retention policy; the operating system or the person who chooses a diagnostic log path controls retention. Do not share logs publicly without checking them for local paths or other personal details.

## Optional update checks

Updater-enabled stable builds check GitHub Releases periodically unless disabled in application settings. Requests contain no application telemetry, user folders, identifiers or system profile; GitHub receives ordinary network metadata such as IP address. Downloads follow GitHub release storage redirects. Development builds do not start production checks. Checks and authenticated downloads can fail without preventing use of the application.

The shared Avalonia UI retains Windows HKCU preferences. Linux imports the old dconf path once into `XDG_CONFIG_HOME/com.tlolabs.ewaf/preferences.json` (normally `~/.config`), using a narrow native dconf read adapter; it does not ship the retired GTK UI. The internal Mac reference uses separate `com.tlolabs.ewaf.avalonia-internal` preferences. No destination is persisted. Avalonia/.NET build telemetry is opted out by CI and build entrypoints; there is no application telemetry.
