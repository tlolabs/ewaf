using System.Globalization;
using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Automation;
using Windows.ApplicationModel.DataTransfer;
using Windows.Storage.Pickers;
using Windows.System;
namespace EWAF;

public sealed class MainWindow : Window
{
    private readonly Grid Root = new();
    private readonly TextBox start = new() { Header = "Start date (MM-DD-YYYY)" };
    private readonly TextBox end = new() { Header = "End date (MM-DD-YYYY)" };
    private readonly ComboBox weekday = new() { Header = "Weekday", HorizontalAlignment = HorizontalAlignment.Stretch };
    private readonly AutoSuggestBox search = new() { PlaceholderText = "Find a folder date" };
    private readonly ListView preview = new() { SelectionMode = ListViewSelectionMode.Single, CanDragItems = true };
    private readonly TextBlock count = new();
    private readonly TextBlock status = new() { Text = "Choose a date range and weekday, then review your folders.",
                                                TextWrapping = TextWrapping.Wrap, IsTextSelectionEnabled = true };
    private readonly TextBlock destinationLabel =
        new() { Text = "Choose an existing destination folder.", TextWrapping = TextWrapping.Wrap,
                IsTextSelectionEnabled = true };
    private readonly Button create = new() { Content = "Create Folders", IsEnabled = false };
    private readonly Button choose = new() { Content = "Choose Destination…" };
    private readonly Button cancel = new() { Content = "Cancel", IsEnabled = false };
    private readonly Button open = new() { Content = "Open Folder", IsEnabled = false };
    private readonly ProgressBar progress = new() { Minimum = 0, Maximum = 1 };
    private readonly StackPanel form = new() { Spacing = 12 };
    private PlanInput? plan;
    private bool confirmation, busy, closed, dialogOpen;
    private int generation, total;
    private string? destination;
    private CancellationTokenSource? operation;
    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);
    private static readonly uint[] Days = [2, 3, 4, 5, 6, 7, 1];

    public MainWindow()
    {
        App.LogStartup("Window fields constructed");
        Content = Root;
        Title = "EWAF — Every Week a Folder";
        var scale = GetDpiForWindow(WinRT.Interop.WindowNative.GetWindowHandle(this)) / 96.0;
        int.TryParse(Preferences.Read("width", "850"), out var width);
        int.TryParse(Preferences.Read("height", "650"), out var height);
        AppWindow.Resize(new Windows.Graphics.SizeInt32((int)(Math.Clamp(width, 640, 3000) * scale),
                                                        (int)(Math.Clamp(height, 480, 2000) * scale)));
        Root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        Root.RowDefinitions.Add(new RowDefinition());
        Root.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        var menu = new MenuBar();
        var file = new MenuBarItem { Title = "File" };
        AddMenu(file, "New Window", App.NewWindow);
        AddMenu(file, "Choose Destination…", async () => await Choose());
        AddMenu(file, "Create Folders", async () => await Create());
        AddMenu(file, "Cancel Folder Creation", () => operation?.Cancel());
        AddMenu(file, "Settings", async () => await Settings());
        AddMenu(file, "Close", Close);
        var help = new MenuBarItem { Title = "Help" };
        AddMenu(help, "EWAF Help",
                async () => await Launcher.LaunchUriAsync(new Uri("https://github.com/tlolabs/ewaf#use")));
        AddMenu(help, "Download Updates…",
                async () => await Launcher.LaunchUriAsync(new Uri("https://github.com/tlolabs/ewaf/releases")));
        AddMenu(help, "About EWAF",
                async () =>
                    await Message("EWAF", "Every Week a Folder\nVersion " +
                                              Core.Call(new { op = "info" }).GetProperty("version").GetString()));
        menu.Items.Add(file);
        menu.Items.Add(help);
        Root.Children.Add(menu);
        var body = new Grid { Padding = new Thickness(20), ColumnSpacing = 24 };
        body.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(310) });
        body.ColumnDefinitions.Add(new ColumnDefinition());
        Grid.SetRow(body, 1);
        Root.Children.Add(body);
        form.Children.Add(new TextBlock { Text = "Every Week a Folder",
                                          Style = (Style)Application.Current.Resources["TitleTextBlockStyle"] });
        form.Children.Add(start);
        form.Children.Add(CalendarButton(start, "Choose start date"));
        form.Children.Add(end);
        form.Children.Add(CalendarButton(end, "Choose end date"));
        foreach (uint d in Days)
            weekday.Items.Add(CultureInfo.CurrentCulture.DateTimeFormat.DayNames[d - 1]);
        form.Children.Add(weekday);
        form.Children.Add(new TextBlock { Text = "Both dates are included. Exact entry supports years 0001–9999.",
                                          TextWrapping = TextWrapping.Wrap });
        form.Children.Add(destinationLabel);
        form.Children.Add(choose);
        form.Children.Add(open);
        body.Children.Add(
            new ScrollViewer { Content = form, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled });
        var right = new Grid { RowSpacing = 10 };
        right.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        right.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        right.RowDefinitions.Add(new RowDefinition());
        right.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        right.Children.Add(count);
        Grid.SetRow(search, 1);
        right.Children.Add(search);
        Grid.SetRow(preview, 2);
        right.Children.Add(preview);
        var note =
            new TextBlock { Text = "Preview shows up to 200 matches in date order. Existing folders will be kept.",
                            TextWrapping = TextWrapping.Wrap };
        Grid.SetRow(note, 3);
        right.Children.Add(note);
        Grid.SetColumn(right, 1);
        body.Children.Add(right);
        var footer = new StackPanel { Spacing = 8, Padding = new Thickness(20) };
        var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12 };
        actions.Children.Add(create);
        actions.Children.Add(cancel);
        footer.Children.Add(actions);
        footer.Children.Add(progress);
        footer.Children.Add(status);
        Grid.SetRow(footer, 2);
        Root.Children.Add(footer);
        foreach (var pair in new(DependencyObject control, string name)[] {
                     (start, "Start date"), (end, "End date"), (weekday, "Weekday"), (search, "Find a folder date"),
                     (preview, "Folder preview"), (progress, "Folder creation progress"), (status, "Operation status")
                 })
            AutomationProperties.SetName(pair.control, pair.name);
        AutomationProperties.SetLiveSetting(status, Microsoft.UI.Xaml.Automation.Peers.AutomationLiveSetting.Polite);
        App.LogStartup("Window controls constructed");
        var today = DateTime.Today.ToString("MM-dd-yyyy", CultureInfo.InvariantCulture);
        start.Text = Preferences.Read("rangeStart", today);
        end.Text = Preferences.Read("rangeEnd", today);
        var saved = uint.TryParse(Preferences.Read("defaultWeekday", "5"), out var dflt) ? dflt : 5;
        weekday.SelectedIndex = Array.IndexOf(Days, saved);
        if (weekday.SelectedIndex < 0)
            weekday.SelectedIndex = 3;
        start.TextChanged += (_, _) => Refresh();
        end.TextChanged += (_, _) => Refresh();
        weekday.SelectionChanged += (_, _) => Refresh();
        search.TextChanged += (_, _) => Refresh();
        choose.Click += async (_, _) => await Choose();
        create.Click += async (_, _) => await Create();
        cancel.Click += (_, _) => operation?.Cancel();
        open.Click += async (_, _) =>
        {
            try
            {
                if (destination != null)
                    await Launcher.LaunchFolderAsync(
                        await Windows.Storage.StorageFolder.GetFolderFromPathAsync(destination));
            }
            catch (Exception e)
            {
                if (!closed) status.Text = "The destination could not be opened. " + e.Message;
            }
        };
        var context = new MenuFlyout();
        var copy = new MenuFlyoutItem { Text = "Copy Folder Name" };
        copy.Click += (_, _) => Copy();
        context.Items.Add(copy);
        preview.ContextFlyout = context;
        preview.DragItemsStarting += (_, e) =>
        {
            if (e.Items.FirstOrDefault() is string name)
            {
                e.Data.SetText(name);
                e.Data.RequestedOperation = DataPackageOperation.Copy;
            }
        };
        Shortcut(VirtualKey.O, async () => await Choose());
        Shortcut(VirtualKey.Enter, async () => await Create());
        Shortcut(VirtualKey.N, App.NewWindow);
        Shortcut(VirtualKey.F, () => search.Focus(FocusState.Programmatic));
        Shortcut((VirtualKey)188, async () => await Settings());
        Shortcut(VirtualKey.Escape, () => operation?.Cancel(), VirtualKeyModifiers.None);
        Closed += (_, _) =>
        {
            closed = true;
            operation?.Cancel();
            try
            {
                Preferences.Write("rangeStart", start.Text);
                Preferences.Write("rangeEnd", end.Text);
                Preferences.Write("width", ((int)(AppWindow.Size.Width / Root.XamlRoot.RasterizationScale)).ToString());
                Preferences.Write("height",
                                  ((int)(AppWindow.Size.Height / Root.XamlRoot.RasterizationScale)).ToString());
            }
            catch (Exception e)
            {
                System.Diagnostics.Trace.TraceWarning(e.Message);
            }
        };
        App.LogStartup("Window handlers connected");
        Root.Loaded += (_, _) =>
        {
            App.LogStartup("Window loaded");
            void UpdateMinimumSize()
            {
                if (AppWindow.Presenter is Microsoft.UI.Windowing.OverlappedPresenter presenter)
                {
                    presenter.PreferredMinimumWidth = (int)(640 * Root.XamlRoot.RasterizationScale);
                    presenter.PreferredMinimumHeight = (int)(480 * Root.XamlRoot.RasterizationScale);
                }
            }
            UpdateMinimumSize();
            Root.XamlRoot.Changed += (_, _) => UpdateMinimumSize();
            Refresh();
        };
    }
    private void SetFormEnabled(bool enabled)
    {
        foreach (var control in form.Children.OfType<Control>())
            control.IsEnabled = enabled;
        open.IsEnabled = enabled && destination != null;
    }
    private static void AddMenu(MenuBarItem parent, string text, Action action)
    {
        var item = new MenuFlyoutItem { Text = text };
        item.Click += (_, _) => action();
        parent.Items.Add(item);
    }
    private void Shortcut(VirtualKey key, Action action, VirtualKeyModifiers modifiers = VirtualKeyModifiers.Control)
    {
        var accelerator = new KeyboardAccelerator { Key = key, Modifiers = modifiers };
        accelerator.Invoked += (_, e) =>
        {
            action();
            e.Handled = true;
        };
        Root.KeyboardAccelerators.Add(accelerator);
    }
    private CalendarDatePicker CalendarButton(TextBox target, string label)
    {
        var picker = new CalendarDatePicker { PlaceholderText = label,
                                              MinDate = new DateTimeOffset(1600, 1, 1, 0, 0, 0, TimeSpan.Zero),
                                              MaxDate = new DateTimeOffset(9999, 12, 31, 0, 0, 0, TimeSpan.Zero) };
        AutomationProperties.SetName(picker, label);
        picker.DateChanged += (_, _) =>
        {
            if (picker.Date is {} date)
                target.Text = date.ToString("MM-dd-yyyy", CultureInfo.InvariantCulture);
        };
        return picker;
    }
    private void Copy()
    {
        if (preview.SelectedItem is string name)
        {
            var data = new DataPackage();
            data.SetText(name);
            Clipboard.SetContent(data);
        }
    }
    private async void Refresh()
    {
        if (busy || weekday.SelectedIndex < 0)
            return;
        var current = ++generation;
        plan = null;
        create.IsEnabled = false;
        preview.ItemsSource = null;
        var first = start.Text;
        var last = end.Text;
        var day = Days[weekday.SelectedIndex];
        var query = search.Text;
        try
        {
            var result =
                await Task.Run(() =>
                               {
                                   var exact = Core.Call(new { op = "exact", start = first, end = last });
                                   var input = new PlanInput(exact.GetProperty("start").GetString()!,
                                                             exact.GetProperty("end").GetString()!, day);
                                   return (input, Core.Call(new { op = "plan", plan = input, search = query }));
                               });
            if (closed || current != generation)
                return;
            plan = result.input;
            total = result.Item2.GetProperty("count").GetInt32();
            confirmation = result.Item2.GetProperty("requiresConfirmation").GetBoolean();
            var rows = result.Item2.GetProperty("dates").EnumerateArray().Select(v => v.GetString()!).ToList();
            preview.ItemsSource = rows;
            count.Text = $"{total:N0} folders";
            create.IsEnabled = total > 0;
            status.Text = total == 0        ? "No matching dates. Choose a wider range or another weekday."
                          : rows.Count == 0 ? "No search matches. Creation still uses the full date range."
                                            : "Review your folders, then choose Create Folders.";
        }
        catch (Exception e)
        {
            if (current == generation && !closed)
            {
                count.Text = "Check Your Dates";
                status.Text = e.Message;
            }
        }
    }
    private async Task<bool> Choose()
    {
        if (busy || dialogOpen)
            return false;
        dialogOpen = true;
        try
        {
            var picker = new FolderPicker();
            picker.FileTypeFilter.Add("*");
            WinRT.Interop.InitializeWithWindow.Initialize(picker, WinRT.Interop.WindowNative.GetWindowHandle(this));
            var folder = await picker.PickSingleFolderAsync();
            if (folder == null || closed)
                return false;
            destination = folder.Path;
            destinationLabel.Text = destination;
            open.IsEnabled = true;
            return true;
        }
        catch (Exception e)
        {
            status.Text = "The folder could not be opened. " + e.Message;
            return false;
        }
        finally
        {
            dialogOpen = false;
        }
    }
    private async Task Create()
    {
        if (busy || dialogOpen || plan == null || total == 0)
            return;
        if (confirmation && !await Confirm())
            return;
        if (destination == null && !await Choose())
            return;
        if (plan == null || closed)
            return;
        var snapshot = plan;
        busy = true;
        ++generation;
        SetFormEnabled(false);
        search.IsEnabled = false;
        create.IsEnabled = false;
        cancel.IsEnabled = true;
        operation = new CancellationTokenSource();
        progress.Value = 0;
        progress.Maximum = Math.Max(1, total);
        try
        {
            var updates = new Progress<JsonElement>(
                r =>
                {
                    if (!closed && busy)
                    {
                        var processed = r.GetProperty("created").GetInt32() + r.GetProperty("existing").GetInt32();
                        progress.Value = processed;
                        status.Text = $"Processed {processed:N0} of {total:N0} folders.";
                    }
                });
            var result = await Core.Create(snapshot, destination!, operation.Token, updates);
            if (closed)
                return;
            status.Text = Core.Summary(result);
            if (!closed)
                await Message(result.GetProperty("failureReason").ValueKind == JsonValueKind.String
                                  ? "Folder Creation Stopped"
                              : result.GetProperty("cancelled").GetBoolean() ? "Creation Canceled"
                                                                             : "Complete",
                              status.Text);
        }
        catch (Exception e)
        {
            if (!closed)
            {
                status.Text = e.Message;
                await Message("Folder Creation Stopped", e.Message);
            }
        }
        finally
        {
            busy = false;
            operation.Dispose();
            operation = null;
            if (!closed)
            {
                SetFormEnabled(true);
                search.IsEnabled = true;
                create.IsEnabled = true;
                cancel.IsEnabled = false;
            }
        }
    }
    private async Task<bool> Confirm()
    {
        dialogOpen = true;
        try
        {
            return await new ContentDialog { XamlRoot = Root.XamlRoot,
                                             Title = $"Create {total:N0} folders?",
                                             Content = "This is a large operation. Existing folders and their " +
                                                       "contents will be preserved. You can cancel while it runs.",
                                             PrimaryButtonText = "Continue",
                                             CloseButtonText = "Cancel",
                                             DefaultButton = ContentDialogButton.Close }
                       .ShowAsync() == ContentDialogResult.Primary;
        }
        finally
        {
            dialogOpen = false;
        }
    }
    private async Task Message(string title, string text)
    {
        if (dialogOpen || closed)
            return;
        dialogOpen = true;
        try
        {
            await new ContentDialog { XamlRoot = Root.XamlRoot, Title = title, Content = text, CloseButtonText = "OK" }
                .ShowAsync();
        }
        finally
        {
            dialogOpen = false;
        }
    }
    private async Task Settings()
    {
        if (dialogOpen || closed)
            return;
        var choice = new ComboBox { Header = "Default weekday", HorizontalAlignment = HorizontalAlignment.Stretch };
        foreach (uint day in Days)
            choice.Items.Add(CultureInfo.CurrentCulture.DateTimeFormat.DayNames[day - 1]);
        uint.TryParse(Preferences.Read("defaultWeekday", "5"), out var saved);
        choice.SelectedIndex = Array.IndexOf(Days, saved);
        if (choice.SelectedIndex < 0)
            choice.SelectedIndex = 3;
        AutomationProperties.SetName(choice, "Default weekday");
        dialogOpen = true;
        try
        {
            if (await new ContentDialog { XamlRoot = Root.XamlRoot, Title = "Settings", Content = choice,
                                          PrimaryButtonText = "Save", CloseButtonText = "Cancel" }
                    .ShowAsync() == ContentDialogResult.Primary)
                Preferences.Write("defaultWeekday", Days[choice.SelectedIndex].ToString());
        }
        catch (Exception e)
        {
            status.Text = "Settings could not be saved. " + e.Message;
        }
        finally
        {
            dialogOpen = false;
        }
    }
}
