// SPDX-License-Identifier: GPL-3.0-or-later
using System.Diagnostics;
namespace EWAF;

internal static class DesktopIntegration
{
    internal static Task Open(string path)
    {
        var start = OperatingSystem.IsWindows() ? new ProcessStartInfo(path) { UseShellExecute = true } : new ProcessStartInfo(OperatingSystem.IsMacOS() ? "open" : "xdg-open") { UseShellExecute = false };
        if (!OperatingSystem.IsWindows()) start.ArgumentList.Add(path);
        using var process = Process.Start(start) ?? throw new IOException("Could not open the requested location.");
        return Task.CompletedTask;
    }
}
internal static class UpdateCoordinator
{
    internal static async Task Check(Workspace model, bool manual)
    {
        if (UpdateClient.Running || App.HasActiveWork || model.IsWorking || model.Closed) return;
        if (Identity.Internal) { if (manual) await model.Message("Internal reference build", "Production updates are disabled for this internal Avalonia build."); return; }
        UpdateClient.Running = true;
        string? staged = null; bool handedOff = false;
        try
        {
            if (!manual && !(await UpdateClient.Due()).GetProperty("due").GetBoolean()) return;
            Preferences.Write("updateLastAttempt", DateTimeOffset.UtcNow.ToUnixTimeSeconds().ToString());
            var update = await UpdateClient.Check();
            Preferences.Write("updateLastSuccess", update.GetProperty("checked_at").GetInt64().ToString());
            if (model.Closed || App.HasActiveWork) return;
            if (!update.GetProperty("available").GetBoolean()) { if (manual) await model.Message("EWAF Updates", "No compatible newer stable release is available."); return; }
            var version = update.GetProperty("version").GetString()!;
            var notes = "\nRelease notes: " + update.GetProperty("notes").GetString();
            if (OperatingSystem.IsLinux())
            {
                if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("APPIMAGE")))
                {
                    if (await model.Confirm("EWAF " + version + " is available", "This installation is managed by your package manager. Install the authenticated AppImage from GitHub to enable automatic replacement." + notes, "Open Releases")) await DesktopIntegration.Open("https://github.com/tlolabs/ewaf/releases");
                }
                else if (await model.Confirm("EWAF " + version + " is available", "Download, authenticate and replace this AppImage? A previous copy will be retained. Reopen EWAF when ready; this does not close your windows." + notes, "Download and Update") && !App.HasActiveWork && !model.Closed)
                {
                    var installed = await UpdateClient.Run("install-appimage", UpdateClient.OSVersion, version);
                    await model.Message("EWAF updated", "Reopen EWAF when ready. Previous version: " + installed.GetProperty("backup").GetString());
                }
                return;
            }
            if (!await model.Confirm("EWAF " + version + " is available", "Download and verify this update? Installation requires closing all EWAF windows." + notes, "Download")) return;
            var download = await UpdateClient.Download(version);
            staged = Path.GetDirectoryName(download.GetProperty("path").GetString());
            if (model.Closed || App.HasActiveWork) return;
            if (await model.Confirm("Install EWAF update?", "EWAF will close normally. Windows Installer will verify the publisher and install the update. Your settings and created folders are preserved. Reopen EWAF when installation finishes.", "Close and Install") && !App.HasActiveWork)
            {
                using var installer = await UpdateClient.PrepareInstall(download);
                if (model.Closed || App.HasActiveWork) return;
                installer.Commit(); handedOff = true; App.CloseForUpdate();
            }
        }
        catch (Exception e) { if (manual && !model.Closed) await model.Message("Update unavailable", e.Message); }
        finally
        {
            UpdateClient.Running = false;
            if (!handedOff && staged != null) try { Directory.Delete(staged, true); } catch (Exception e) when (e is IOException or UnauthorizedAccessException) { App.LogStartup(e.Message); }
        }
    }
}
