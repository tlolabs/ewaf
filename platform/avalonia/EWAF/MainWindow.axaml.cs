// SPDX-License-Identifier: GPL-3.0-or-later
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Input.Platform;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Avalonia.Automation;
using System.Globalization;
namespace EWAF;

public partial class MainWindow : Window, IWorkspaceDialogs
{
    internal Workspace Model { get; }
    private readonly DispatcherTimer updateTimer = new() { Interval = TimeSpan.FromHours(1) };
    private Point? dragStart;
    private PointerPressedEventArgs? dragPress;
    public MainWindow()
    {
        InitializeComponent(); Title = Identity.Title;
        Model = new(this, Preferences.Store); DataContext = Model;
        Width = Dimension("width", 850, 640, 3000); Height = Dimension("height", 650, 480, 2000);
        Opened += async (_, _) =>
        {
            StartDate.Focus();
            Model.Refresh();
            if (Program.Smoke)
            {
                Model.Start = "09-03-2026"; Model.End = "09-17-2026"; Model.Weekday = 3;
                await Model.RefreshTask;
                if (Model.Total != 3) throw new InvalidOperationException("Packaged UI validation failed.");
                Console.WriteLine("Avalonia packaged window, bindings and core passed.");
                Close();
            }
            else if (Environment.GetEnvironmentVariable("EWAF_TEST_SESSION") == null) { updateTimer.Start(); await UpdateCoordinator.Check(Model, false); }
        };
        updateTimer.Tick += async (_, _) => await UpdateCoordinator.Check(Model, false);
        Closing += (_, _) =>
        {
            updateTimer.Stop(); Model.Close();
            try { Preferences.Write("width", ((int)Width).ToString()); Preferences.Write("height", ((int)Height).ToString()); }
            catch (Exception e) { App.LogStartup(e.Message); }
        };
        KeyDown += async (_, e) =>
        {
            if (e.Key == Key.Escape) Model.CancelCommand.Execute(null);
            else if ((e.KeyModifiers & (KeyModifiers.Control | KeyModifiers.Meta)) != 0)
            {
                switch (e.Key)
                {
                    case Key.N: App.NewWindow(); break;
                    case Key.O: Model.ChooseCommand.Execute(null); break;
                    case Key.Enter: Model.CreateCommand.Execute(null); break;
                    case Key.F: SearchBox.Focus(); break;
                    case Key.W: Close(); break;
                    case Key.OemComma: await ShowSettings(); break;
                    case Key.C when PreviewList.IsKeyboardFocusWithin: await CopyName(); break;
                    default: return;
                }
                e.Handled = true;
            }
        };
    }
    private static int Dimension(string key, int fallback, int minimum, int maximum) => int.TryParse(Preferences.Read(key, fallback.ToString()), out var value) ? Math.Clamp(value, minimum, maximum) : fallback;
    public async Task<string?> ChooseFolder()
    {
        var result = await StorageProvider.OpenFolderPickerAsync(new() { Title = "Choose Destination", AllowMultiple = false });
        var selected = result.FirstOrDefault();
        try { return selected?.Path.IsFile == true ? selected.Path.LocalPath : null; }
        finally { selected?.Dispose(); }
    }
    public Task<bool> Confirm(string title, string message, string accept = "Continue") => ShowDialog(title, new TextBlock { Text = message, TextWrapping = Avalonia.Media.TextWrapping.Wrap }, accept);
    public async Task Message(string title, string message) => await ShowDialog(title, new SelectableTextBlock { Text = message, TextWrapping = Avalonia.Media.TextWrapping.Wrap }, null);
    private async Task<bool> ShowDialog(string title, Control content, string? accept)
    {
        var dialog = new Window { Title = title, Width = 460, SizeToContent = SizeToContent.Height, CanResize = false, WindowStartupLocation = WindowStartupLocation.CenterOwner, ShowInTaskbar = false };
        var buttons = new StackPanel { Orientation = Avalonia.Layout.Orientation.Horizontal, Spacing = 12, HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Right };
        var cancel = new Button { Content = accept == null ? "OK" : "Cancel", IsCancel = true };
        cancel.Click += (_, _) => dialog.Close(false); buttons.Children.Add(cancel);
        if (accept != null) { var action = new Button { Content = accept }; action.Click += (_, _) => dialog.Close(true); buttons.Children.Add(action); }
        dialog.Content = new StackPanel { Margin = new Thickness(20), Spacing = 16, Children = { content, buttons } };
        dialog.Opened += (_, _) => cancel.Focus();
        return await dialog.ShowDialog<bool>(this);
    }
    private void NewWindow(object? sender, RoutedEventArgs e) => App.NewWindow();
    private void CloseWindow(object? sender, RoutedEventArgs e) => Close();
    private void Find(object? sender, RoutedEventArgs e) => SearchBox.Focus();
    private async void Copy(object? sender, RoutedEventArgs e) => await CopyName();
    internal async Task CopyName()
    {
        try { if (Model.SelectedName is { } name && Clipboard != null) await Clipboard.SetTextAsync(name); }
        catch (Exception e) { Model.Status = "The folder name could not be copied. " + e.Message; }
    }
    private void PreviewPressed(object? sender, PointerPressedEventArgs e) { if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed) { dragStart = e.GetPosition(this); dragPress = e; } }
    private async void PreviewMoved(object? sender, PointerEventArgs e)
    {
        if (dragStart is not { } start || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed || Math.Abs(e.GetPosition(this).X - start.X) + Math.Abs(e.GetPosition(this).Y - start.Y) < 6 || Model.SelectedName == null) return;
        dragStart = null;
        var data = new DataTransfer(); data.Add(DataTransferItem.CreateText(Model.SelectedName));
        await DragDrop.DoDragDropAsync(dragPress!, data, DragDropEffects.Copy);
    }
    private void StartCalendar(object? sender, SelectionChangedEventArgs e) { if (sender is CalendarDatePicker { SelectedDate: { } date }) Model.Start = date.ToString("MM-dd-yyyy", CultureInfo.InvariantCulture); }
    private void EndCalendar(object? sender, SelectionChangedEventArgs e) { if (sender is CalendarDatePicker { SelectedDate: { } date }) Model.End = date.ToString("MM-dd-yyyy", CultureInfo.InvariantCulture); }
    private async void Reveal(object? sender, RoutedEventArgs e)
    {
        try { await DesktopIntegration.Open(Model.Destination); } catch (Exception ex) { Model.Status = "The destination could not be opened. " + ex.Message; }
    }
    private async void Help(object? sender, RoutedEventArgs e)
    {
        try { await DesktopIntegration.Open("https://github.com/tlolabs/ewaf#use"); }
        catch (Exception ex) { Model.Status = "Help could not be opened. " + ex.Message; }
    }
    private async void About(object? sender, RoutedEventArgs e) { if (!Model.IsWorking) await Model.Message(Identity.Title, "Every Week a Folder\nVersion " + Core.Call(new { op = "info" }).GetProperty("version").GetString()); }
    private async void Updates(object? sender, RoutedEventArgs e) => await UpdateCoordinator.Check(Model, true);
    private async void Settings(object? sender, RoutedEventArgs e) => await ShowSettings();
    internal async Task ShowSettings()
    {
        if (!Model.Editable) return;
        uint.TryParse(Preferences.Read("defaultWeekday", "5"), out var saved);
        var index = Array.IndexOf(Workspace.Days, saved);
        var days = new ComboBox { ItemsSource = Model.DayNames, SelectedIndex = index < 0 ? 3 : index };
        AutomationProperties.SetName(days, "Default weekday");
        var automatic = new CheckBox { Content = "Automatically check for updates", IsChecked = !Identity.Internal && Preferences.Read("automaticUpdates", "1") == "1", IsEnabled = !Identity.Internal };
        var content = new StackPanel { Spacing = 12, Children = { new TextBlock { Text = "Default weekday for new windows" }, days, automatic } };
        if (Identity.Internal) content.Children.Add(new TextBlock { Text = "Internal reference builds do not use production updates." });
        try
        {
            if (await Model.Dialog(() => ShowDialog("Settings", content, "Save"))) { Preferences.Write("defaultWeekday", Workspace.Days[days.SelectedIndex].ToString()); Preferences.Write("automaticUpdates", automatic.IsChecked == true ? "1" : "0"); }
        }
        catch (Exception e) { Model.Status = "Settings could not be saved. " + e.Message; }
    }
}
