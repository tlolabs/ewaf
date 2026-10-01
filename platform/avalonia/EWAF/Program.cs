// SPDX-License-Identifier: GPL-3.0-or-later
using Avalonia;
namespace EWAF;

internal static class Program
{
    internal static bool Smoke;
    [STAThread]
    public static int Main(string[] args)
    {
        if (args.Contains("--core-smoke"))
        {
            var info = Core.Call(new { op = "info" });
            Console.WriteLine("EWAF " + info.GetProperty("version").GetString() + " ABI " + info.GetRawText());
            var result = Core.Call(new { op = "plan", plan = new PlanInput("09-03-2026", "09-17-2026", 5), search = "" });
            return result.GetProperty("count").GetInt32() == 3 ? 0 : 1;
        }
        Smoke = args.Contains("--ui-smoke");
        if (Smoke) Environment.SetEnvironmentVariable("EWAF_TEST_SESSION", Guid.NewGuid().ToString());
        return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }
    public static AppBuilder BuildAvaloniaApp() => AppBuilder.Configure<App>().UsePlatformDetect().LogToTrace();
}
