using Microsoft.UI.Xaml;
namespace EWAF;
public partial class App : Application {
    private static readonly List<MainWindow> Windows = new();
    public App() {
        AppDomain.CurrentDomain.UnhandledException += (_,e) => LogStartup(e.ExceptionObject.ToString() ?? "Unknown startup error");
        UnhandledException += (_,e) => LogStartup(e.Exception.ToString());
        InitializeComponent();
    }
    private static void LogStartup(string message) {
        System.Diagnostics.Trace.TraceError(message);
        var path=Environment.GetEnvironmentVariable("EWAF_DIAGNOSTICS_PATH");
        if(!string.IsNullOrEmpty(path)) { try { File.AppendAllText(path,message+Environment.NewLine); } catch(IOException) {} }
    }
    protected override void OnLaunched(LaunchActivatedEventArgs args) { try { NewWindow(); } catch(Exception e) { LogStartup(e.ToString()); throw; } }
    internal static void NewWindow() {
        var window = new MainWindow(); Windows.Add(window);
        window.Closed += (_, _) => Windows.Remove(window);
        window.Activate();
    }
}
