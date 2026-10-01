// SPDX-License-Identifier: GPL-3.0-or-later
using Avalonia;
using Avalonia.Controls;
using Avalonia.Headless;
using Avalonia.Threading;
using Avalonia.Automation;
using Avalonia.Interactivity;
using Avalonia.Input;
using Avalonia.Input.Platform;
using Avalonia.VisualTree;
using EWAF;
internal static class TestProgram
{
    static int passed;
    static void Check(bool value, string message) { if (!value) throw new Exception(message); }
    [STAThread]
    static int Main()
    {
        if (Environment.GetCommandLineArgs().Contains("--preferences-migration"))
        {
            Check(OperatingSystem.IsLinux(), "Migration probe requires actual Linux");
            Environment.SetEnvironmentVariable("EWAF_TEST_SESSION", null);
            var preferences = new DesktopPreferences();
            Check(preferences.Read("defaultWeekday", "") == "2", "dconf weekday import");
            Check(preferences.Read("rangeStart", "") == "09-03-2026", "dconf date import");
            Check(preferences.Read("automaticUpdates", "") == "0", "dconf boolean import");
            Check(preferences.Read("updateLastSuccess", "") == "123", "dconf timestamp import");
            preferences.Write("defaultWeekday", "7");
            Check(new DesktopPreferences().Read("defaultWeekday", "") == "7", "JSON was overwritten by repeat migration");
            Console.WriteLine("PASS actual Linux dconf import and one-time JSON persistence");
            return 0;
        }
        Environment.SetEnvironmentVariable("EWAF_TEST_SESSION", Guid.NewGuid().ToString());
        Preferences.Store = new MemoryPreferences();
        AppBuilder.Configure<App>().UseHeadless(new AvaloniaHeadlessPlatformOptions()).SetupWithoutStarting();
        using var done = new CancellationTokenSource();
        int exit = 1;
        Dispatcher.UIThread.Post(async () =>
        {
            try { await Tests(); Console.WriteLine($"PASS: {passed} shared presentation/FFI/headless scenarios"); exit = 0; }
            catch (Exception e) { Console.Error.WriteLine(e); }
            finally { done.Cancel(); }
        });
        Dispatcher.UIThread.MainLoop(done.Token);
        return exit;
    }
    static async Task Case(string name, Func<Task> body) { await body(); passed++; Console.WriteLine("PASS " + name); }
    static async Task Tests()
    {
        var prefs = new MemoryPreferences(); var dialogs = new Dialogs();
        var model = new Workspace(dialogs, prefs);
        await Case("planning and Unicode search preserve complete plan", async () =>
        {
            model.Start = "09-03-2026"; model.End = "09-17-2026"; await model.RefreshTask;
            Check(model.Total == 3 && model.CreateCommand.CanExecute(null), "September plan");
            model.Search = "０９-１０"; await model.RefreshTask;
            Check(model.Total == 3 && model.Preview.SequenceEqual(new[] { "09-10-2026" }), "Unicode filter or full plan");
        });
        await Case("invalid dates and latest asynchronous request", async () =>
        {
            model.Start = "02-29-2025"; await model.RefreshTask;
            Check(!model.CreateCommand.CanExecute(null) && model.Status.Contains("valid date"), "Invalid date");
            model.Start = "01-01-0001"; model.End = "12-31-9999"; model.Start = "09-03-2026"; model.End = "09-17-2026";
            await model.RefreshTask; await Task.Delay(100);
            Check(model.Total == 3, "Stale response overwrote newest plan");
        });
        var path = Path.Combine(Path.GetTempPath(), "ewaf-avalonia-" + Guid.NewGuid()); Directory.CreateDirectory(path);
        try
        {
            await Case("creation ignores search; retry preserves contents", async () =>
            {
                dialogs.Folder = path; await model.Choose(); await model.Create();
                Check(Directory.GetDirectories(path).Length == 3, "Filtered creation");
                var keep = Path.Combine(path, "09-03-2026", "keep.txt"); File.WriteAllText(keep, "preserved");
                await model.Create(); Check(model.Status.Contains("Already existed: 3") && File.ReadAllText(keep) == "preserved", "Retry");
            });
            await Case("confirmation defaults to cancel without filesystem work", async () =>
            {
                model.Start = "01-01-2000"; model.End = "12-31-2010"; await model.RefreshTask;
                dialogs.Accept = false; await model.Create();
                Check(dialogs.Confirmations == 1 && Directory.GetDirectories(path).Length == 3 && !model.IsWorking, "Cancel confirmation");
            });
            await Case("cancelled chooser does not create", async () =>
            {
                var second = new Workspace(new Dialogs(), prefs) { Start = "09-03-2026", End = "09-17-2026" }; await second.RefreshTask; await second.Create();
                Check(!second.HasDestination && !second.IsWorking, "Chooser cancellation"); second.Close();
            });
            await Case("conflict summary preserves existing data", async () =>
            {
                var conflict = Path.Combine(path, "09-24-2026"); File.WriteAllText(conflict, "do not replace");
                model.Start = "09-24-2026"; model.End = "09-24-2026"; await model.RefreshTask; await model.Create();
                Check(File.ReadAllText(conflict) == "do not replace" && model.Status.Contains("09-24-2026"), "Conflict");
            });
            await Case("close requests cancellation and releases creation", async () =>
            {
                var cancel = new Workspace(new Dialogs { Folder = path, Accept = true }, prefs) { Start = "01-01-0001", End = "12-31-9999" }; await cancel.RefreshTask;
                await cancel.Choose(); var work = cancel.Create(); cancel.Close(); await work;
                Check(cancel.Closed && !cancel.Busy && !cancel.CreateCommand.CanExecute(null), "Close/cancel");
            });
        }
        finally { Directory.Delete(path, true); }
        await Case("independent windows and persisted defaults/ranges", async () =>
        {
            prefs.Write("defaultWeekday", "99"); var fallback = new Workspace(dialogs, prefs); Check(fallback.Weekday == 3, "Invalid default");
            prefs.Write("defaultWeekday", "2"); var next = new Workspace(dialogs, prefs); Check(next.Weekday == 0 && fallback.Weekday == 3, "Window independence");
            next.Start = "01-01-2026"; next.End = "02-01-2026"; await next.RefreshTask; next.Close();
            var restored = new Workspace(dialogs, prefs); Check(restored.Start == next.Start && !restored.HasDestination, "Restoration/destination lifetime");
        });
        await Case("legacy Linux preference decoding", () =>
        {
            Check(DesktopPreferences.DecodeLegacy("'09-03-2026'") == "09-03-2026" && DesktopPreferences.DecodeLegacy("int64 123") == "123" && DesktopPreferences.DecodeLegacy("false") == "0", "Migration types"); return Task.CompletedTask;
        });
        await Case("AXAML bindings, accessible controls and keyboard focus", async () =>
        {
            var window = new MainWindow(); window.Show();
            var first = window.FindControl<TextBox>("StartDate")!; var last = window.FindControl<TextBox>("EndDate")!;
            first.Text = "09-03-2026"; last.Text = "09-17-2026"; await window.Model.RefreshTask;
            Check(window.Model.Total == 3, "Text bindings");
            Check(AutomationProperties.GetName(first) == "Start date", "Accessible name");
            first.Focus(); Check(first.IsFocused, "Keyboard focus");
            window.KeyPress(Key.Tab, RawInputModifiers.None, PhysicalKey.Tab, null); window.KeyRelease(Key.Tab, RawInputModifiers.None, PhysicalKey.Tab, null);
            Check(!first.IsFocused, "Tab did not advance focus");
            window.KeyPress(Key.F, RawInputModifiers.Control, PhysicalKey.F, "f"); window.KeyRelease(Key.F, RawInputModifiers.Control, PhysicalKey.F, "f");
            Check(window.FindControl<TextBox>("SearchBox")!.IsFocused, "Find shortcut");
            window.FindControl<TextBox>("SearchBox")!.Text = "09-10"; await window.Model.RefreshTask;
            Check(window.FindControl<ListBox>("PreviewList")!.ItemCount == 1 && window.Model.Total == 3, "List binding");
            window.Model.SelectedName = "09-10-2026";
            await window.CopyName();
            Check(await window.Clipboard!.TryGetTextAsync() == "09-10-2026", "Clipboard text");
            window.Close();
        });
        await Case("two-window updater guard, confirmation and settings lifecycle", async () =>
        {
            var first = new MainWindow(); var second = new MainWindow();
            App.Windows.Add(first); App.Windows.Add(second); first.Show(); second.Show();
            first.Model.Start = "01-01-2000"; first.Model.End = "12-31-2010"; await first.Model.RefreshTask;
            Check(!App.HasActiveWork, "Idle large range blocked updates");
            first.Model.CreateCommand.Execute(null);
            Check(App.HasActiveWork && !first.Model.Editable, "Open confirmation did not block updates");
            await UpdateCoordinator.Check(second.Model, true);
            Check(!UpdateClient.Running, "Updater started during another window dialog");
            var dialog = first.OwnedWindows.Single();
            var cancel = dialog.GetVisualDescendants().OfType<Button>().Single(b => Equals(b.Content, "Cancel"));
            cancel.RaiseEvent(new RoutedEventArgs(Button.ClickEvent)); await first.Model.OperationTask;
            Check(!App.HasActiveWork && first.Model.CreateCommand.CanExecute(null), "Cancelled dialog left stale busy state");
            var settings = first.ShowSettings(); Check(App.HasActiveWork, "Settings did not block updates");
            var options = first.OwnedWindows.Single();
            options.GetVisualDescendants().OfType<ComboBox>().Single().SelectedIndex = 0;
            options.GetVisualDescendants().OfType<Button>().Single(b => Equals(b.Content, "Save")).RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            await settings; Check(Preferences.Read("defaultWeekday", "5") == "2" && !App.HasActiveWork, "Settings did not persist/close");
            first.Close(); second.Close(); App.Windows.Clear();
        });
        await Case("internal Mac cannot invoke production update helper", async () =>
        {
            if (OperatingSystem.IsMacOS())
            {
                Check(Identity.Internal && Identity.ApplicationId.EndsWith("avalonia-internal"), "Internal identity");
                try { await UpdateClient.Run("check", "99"); throw new Exception("Updater allowed"); } catch (InvalidOperationException) { }
            }
        });
    }
}
internal sealed class MemoryPreferences : IPreferences
{
    readonly Dictionary<string, string> values = new();
    public string Read(string name, string fallback) => values.GetValueOrDefault(name, fallback);
    public void Write(string name, string value) => values[name] = value;
}
internal sealed class Dialogs : IWorkspaceDialogs
{
    internal string? Folder; internal bool Accept; internal int Confirmations;
    public Task<string?> ChooseFolder() => Task.FromResult(Folder);
    public Task<bool> Confirm(string title, string message, string accept = "Continue") { Confirmations++; return Task.FromResult(Accept); }
    public Task Message(string title, string message) => Task.CompletedTask;
}
