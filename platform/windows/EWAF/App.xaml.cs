using Microsoft.UI.Xaml;
namespace EWAF;
public partial class App : Application {
    private static readonly List<MainWindow> Windows = new();
    public App() {
        Directory.SetCurrentDirectory(AppContext.BaseDirectory);
        LogStartup("Application construction");
        AppDomain.CurrentDomain.UnhandledException += (_,e) => LogStartup(e.ExceptionObject.ToString() ?? "Unknown startup error");
        UnhandledException += (_,e) => LogStartup(e.Message + " " + e.Exception?.ToString());
        InitializeComponent();
        LogStartup("Application resources initialized");
    }
    internal static void LogStartup(string message) {
        System.Diagnostics.Trace.WriteLine(message);
        var path=Environment.GetEnvironmentVariable("EWAF_DIAGNOSTICS_PATH");
        if(!string.IsNullOrEmpty(path)) { try { File.AppendAllText(path,message+Environment.NewLine); } catch(IOException) {} }
    }
    protected override void OnLaunched(LaunchActivatedEventArgs args) { try { LogStartup("Creating window"); NewWindow(); LogStartup("Window activated"); } catch(Exception e) { LogStartup(e.ToString()); throw; } }
    internal static void NewWindow() {
        var window = new MainWindow(); Windows.Add(window);
        window.Closed += (_, _) => Windows.Remove(window);
        window.Activate();
    }
}
