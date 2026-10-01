// SPDX-License-Identifier: GPL-3.0-or-later
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
namespace EWAF;

public partial class App : Application
{
    internal static readonly List<MainWindow> Windows = new();
    public override void Initialize() { Name = Identity.Internal ? "EWAF Avalonia Internal" : "EWAF"; AvaloniaXamlLoader.Load(this); }
    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.ShutdownMode = Avalonia.Controls.ShutdownMode.OnExplicitShutdown;
            desktop.MainWindow = MakeWindow();
        }
        base.OnFrameworkInitializationCompleted();
    }
    internal static bool HasActiveWork => Windows.Any(w => w.Model.IsWorking);
    private static MainWindow MakeWindow()
    {
        var window = new MainWindow(); Windows.Add(window);
        window.Closed += async (_, _) =>
        {
            // Keep the process alive until Rust releases the directory capability after cancellation.
            await window.Model.OperationTask;
            Windows.Remove(window);
            if (Windows.Count == 0 && Current?.ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop) desktop.Shutdown();
        };
        return window;
    }
    internal static void NewWindow() => MakeWindow().Show();
    internal static void CloseForUpdate() { if (!HasActiveWork) foreach (var window in Windows.ToArray()) window.Close(); }
    internal static void LogStartup(string message)
    {
        System.Diagnostics.Trace.WriteLine(message);
        if (Environment.GetEnvironmentVariable("EWAF_DIAGNOSTICS_PATH") is { } path)
            try { File.AppendAllText(path, message + Environment.NewLine); } catch (IOException) { }
    }
}
