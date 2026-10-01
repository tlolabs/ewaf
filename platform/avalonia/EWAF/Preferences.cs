// SPDX-License-Identifier: GPL-3.0-or-later
using Microsoft.Win32;
using System.Diagnostics;
using System.Text.Json;
namespace EWAF;

internal interface IPreferences
{
    string Read(string name, string fallback);
    void Write(string name, string value);
}
internal sealed class DesktopPreferences : IPreferences
{
    private readonly Dictionary<string, string> values = new();
    private readonly string file;
    private static string? Session => Guid.TryParse(Environment.GetEnvironmentVariable("EWAF_TEST_SESSION"), out var id) ? id.ToString() : null;
    private static string RegistryKey => @"Software\tlolabs\EWAF" + (Session is { } s ? @"\TestSessions\" + s : "");
    public DesktopPreferences()
    {
        var root = Environment.GetEnvironmentVariable("XDG_CONFIG_HOME");
        if (string.IsNullOrEmpty(root)) root = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        file = Path.Combine(root, Identity.Internal ? "com.tlolabs.ewaf.avalonia-internal" : "com.tlolabs.ewaf", Session ?? "", "preferences.json");
        if (OperatingSystem.IsWindows()) return;
        try
        {
            if (File.Exists(file)) values = JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText(file)) ?? new();
            else if (OperatingSystem.IsLinux() && Session == null) ImportLinux();
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or JsonException) { Trace.TraceWarning(e.Message); }
    }
    private void ImportLinux()
    {
        // Read the existing dconf path directly: no retired GTK schema or UI runtime is needed.
        foreach (var (oldName, name) in new[] { ("default-weekday", "defaultWeekday"), ("range-start", "rangeStart"), ("range-end", "rangeEnd"), ("width", "width"), ("height", "height"), ("automatic-updates", "automaticUpdates"), ("update-last-success", "updateLastSuccess"), ("update-last-attempt", "updateLastAttempt") })
        {
            var output = LinuxPreferences.Read(oldName);
            if (!string.IsNullOrEmpty(output)) values[name] = DecodeLegacy(output);

        }
        Save();
    }
    internal static string DecodeLegacy(string value) => value switch
    {
        "true" => "1",
        "false" => "0",
        _ when value.StartsWith("int64 ") => value[6..],
        _ => value.Trim('\'', '"')
    };
    public string Read(string name, string fallback)
    {
        try
        {
            if (OperatingSystem.IsWindows())
            {
                using var key = Registry.CurrentUser.OpenSubKey(RegistryKey);
                return key?.GetValue(name)?.ToString() ?? fallback;
            }
            return values.GetValueOrDefault(name, fallback);
        }
        catch (Exception e) when (e is IOException or UnauthorizedAccessException or System.Security.SecurityException) { Trace.TraceWarning(e.Message); return fallback; }
    }
    public void Write(string name, string value)
    {
        if (OperatingSystem.IsWindows())
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegistryKey);
            key.SetValue(name, value);
        }
        else { values[name] = value; Save(); }
    }
    private void Save()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(file)!);
        var temporary = file + "." + Guid.NewGuid() + ".tmp";
        try { File.WriteAllText(temporary, JsonSerializer.Serialize(values)); File.Move(temporary, file, true); }
        finally { if (File.Exists(temporary)) File.Delete(temporary); }
    }
}
internal static class Preferences
{
    internal static IPreferences Store { get; set; } = new DesktopPreferences();
    internal static string Read(string name, string fallback) => Store.Read(name, fallback);
    internal static void Write(string name, string value) => Store.Write(name, value);
}
internal static class Identity
{
#if INTERNAL_REFERENCE
    internal static bool Internal => true;
#else
    // Defense in depth: even an unqualified `dotnet run` on macOS cannot join production updates.
    internal static bool Internal => OperatingSystem.IsMacOS();
#endif
    internal static string Title => Internal ? "EWAF — INTERNAL Avalonia Reference" : "EWAF — Every Week a Folder";
    internal static string ApplicationId => Internal ? "com.tlolabs.ewaf.avalonia-internal" : "com.tlolabs.ewaf";
}
