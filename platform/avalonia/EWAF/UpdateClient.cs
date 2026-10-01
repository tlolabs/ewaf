// SPDX-License-Identifier: GPL-3.0-or-later
using System.Diagnostics;
using System.Text.Json;
namespace EWAF;

internal static class UpdateClient
{
    internal static bool Running;
    internal static async Task<JsonElement> Run(params string[] args)
    {
        if (Identity.Internal) throw new InvalidOperationException("Internal reference builds cannot use production updates.");
        var start = new ProcessStartInfo(Path.Combine(AppContext.BaseDirectory, OperatingSystem.IsWindows() ? "ewaf-update.exe" : "ewaf-update"))
        { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (var arg in args) start.ArgumentList.Add(arg);
        using var process = Process.Start(start) ?? throw new IOException("Could not start the update helper.");
        var output = process.StandardOutput.ReadToEndAsync();
        var errors = process.StandardError.ReadToEndAsync();
        await process.WaitForExitAsync();
        using var json = JsonDocument.Parse(await output);
        await errors;
        if (process.ExitCode != 0) throw new IOException(json.RootElement.GetProperty("error").GetString());
        return json.RootElement.Clone();
    }
    [System.Runtime.InteropServices.DllImport("libc.so.6")]
    private static extern IntPtr gnu_get_libc_version();
    internal static string OSVersion => OperatingSystem.IsLinux() ? System.Runtime.InteropServices.Marshal.PtrToStringAnsi(gnu_get_libc_version())! : Environment.OSVersion.Version.ToString();
    internal static Task<JsonElement> Due() => Run("due", Preferences.Read("updateLastSuccess", "0"),
        Preferences.Read("updateLastAttempt", "0"), Preferences.Read("automaticUpdates", "1"));
    internal static Task<JsonElement> Check() => Run("check", OSVersion);
    internal static Task<JsonElement> Download(string version) => Run("download", OSVersion, version);
    internal sealed class PreparedInstall(Process process) : IDisposable
    {
        internal void Commit()
        {
            process.StandardInput.WriteLine("COMMIT");
            process.StandardInput.Flush();
        }
        public void Dispose()
        {
            // EOF aborts an uncommitted helper. Never terminate a process.
            process.StandardInput.Dispose();
            process.Dispose();
        }
    }
    internal static async Task<PreparedInstall> PrepareInstall(JsonElement download)
    {
        string package = download.GetProperty("path").GetString()!;
        string directory = Path.Combine(Path.GetDirectoryName(package)!, "installer");
        Directory.CreateDirectory(directory);
        string helperPath = Path.Combine(directory, "ewaf-installer.exe");
        File.Copy(Path.Combine(AppContext.BaseDirectory, "updater-installer", "ewaf-installer.exe"), helperPath);
        var start = new ProcessStartInfo(helperPath)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = directory,
            RedirectStandardInput = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        foreach (var arg in new[] { "--install", package, download.GetProperty("sha256").GetString()!,
            download.GetProperty("signer").GetString()!, download.GetProperty("version").GetString()!,
            download.GetProperty("architecture").GetString()!, Environment.ProcessId.ToString(),
            Process.GetCurrentProcess().StartTime.ToUniversalTime().Ticks.ToString() }) start.ArgumentList.Add(arg);
        var process = Process.Start(start) ?? throw new IOException("Could not start Windows Installer verification.");
        var prepared = new PreparedInstall(process);
        try
        {
            var errors = process.StandardError.ReadToEndAsync();
            string? ready = await process.StandardOutput.ReadLineAsync().WaitAsync(TimeSpan.FromMinutes(2));
            if (ready != "READY")
                throw new IOException("Update verification failed: " + await errors.WaitAsync(TimeSpan.FromSeconds(10)));
            return prepared;
        }
        catch { prepared.Dispose(); throw; }
    }
}
