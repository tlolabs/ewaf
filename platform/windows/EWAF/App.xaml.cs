using Microsoft.UI.Xaml;
namespace EWAF;
public partial class App : Application {
    private static readonly List<MainWindow> Windows = new();
    public App() { InitializeComponent(); }
    protected override void OnLaunched(LaunchActivatedEventArgs args) { NewWindow(); }
    internal static void NewWindow() {
        var window = new MainWindow(); Windows.Add(window);
        window.Closed += (_, _) => Windows.Remove(window);
        window.Activate();
    }
}
