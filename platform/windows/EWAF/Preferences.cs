using Microsoft.Win32;
namespace EWAF;
// Unpackaged WinUI applications use the user's registry, never ApplicationData.Current.
internal static class Preferences
{
    private static string Key => Guid.TryParse(Environment.GetEnvironmentVariable("EWAF_TEST_SESSION"), out var id)
                                     ? @"Software\tlolabs\EWAF\TestSessions\" + id.ToString()
                                     : @"Software\tlolabs\EWAF";
    internal static string Read(string name, string fallback)
    {
        try
        {
            using var key = Registry.CurrentUser.OpenSubKey(Key);
            return key?.GetValue(name)?.ToString() ?? fallback;
        }
        catch (Exception e) when (e is UnauthorizedAccessException or System.Security.SecurityException or IOException)
        {
            System.Diagnostics.Trace.TraceWarning(e.Message);
            return fallback;
        }
    }
    internal static void Write(string name, string value)
    {
        using var key = Registry.CurrentUser.CreateSubKey(Key);
        key.SetValue(name, value);
    }
}
