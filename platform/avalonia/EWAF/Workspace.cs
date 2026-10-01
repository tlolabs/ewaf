// SPDX-License-Identifier: GPL-3.0-or-later
using System.ComponentModel;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Text.Json;
using System.Windows.Input;
namespace EWAF;

internal sealed class Command(Action execute, Func<bool>? allowed = null) : ICommand
{
    public event EventHandler? CanExecuteChanged;
    public bool CanExecute(object? parameter) => allowed?.Invoke() ?? true;
    public void Execute(object? parameter) { if (CanExecute(parameter)) execute(); }
    internal void Changed() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
internal interface IWorkspaceDialogs
{
    Task<string?> ChooseFolder();
    Task<bool> Confirm(string title, string message, string accept = "Continue");
    Task Message(string title, string message);
}
internal sealed class Workspace : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;
    private readonly IWorkspaceDialogs dialogs;
    private readonly IPreferences preferences;
    private string start, end, search = "", status = "Choose a date range and weekday, then review your folders.";
    private int weekday, generation, total, processed;
    private bool busy, dialogOpen, closed, confirmation;
    private string? destination;
    private PlanInput? plan;
    private CancellationTokenSource? operation;
    private bool acceptingProgress;
    public static uint[] Days { get; } = [2, 3, 4, 5, 6, 7, 1];
    public string[] DayNames { get; } = Days.Select(d => CultureInfo.CurrentCulture.DateTimeFormat.DayNames[d - 1]).ToArray();
    public string Start { get => start; set { if (Set(ref start, value)) Refresh(); } }
    public string End { get => end; set { if (Set(ref end, value)) Refresh(); } }
    public string Search { get => search; set { if (Set(ref search, value)) Refresh(); } }
    public int Weekday { get => weekday; set { if (Set(ref weekday, value)) Refresh(); } }
    public string Status { get => status; internal set => Set(ref status, value); }
    public string Destination => destination ?? "Choose an existing destination folder.";
    public bool HasDestination => destination != null;
    public bool Busy => busy;
    public bool Editable => !busy && !dialogOpen && !closed;
    public bool IsWorking => busy || dialogOpen;
    public bool Closed => closed;
    public int Total => total;
    public int ProgressMaximum => Math.Max(1, total);
    public int Processed { get => processed; private set => Set(ref processed, value); }
    public string Count => plan == null ? "Check Your Dates" : $"{total:N0} folders";
    public IReadOnlyList<string> Preview { get; private set; } = Array.Empty<string>();
    public string? SelectedName { get; set; }
    public Command CreateCommand { get; }
    public Command ChooseCommand { get; }
    public Command CancelCommand { get; }
    public Task RefreshTask { get; private set; } = Task.CompletedTask;
    public Task OperationTask { get; private set; } = Task.CompletedTask;
    public Workspace(IWorkspaceDialogs dialogs, IPreferences preferences)
    {
        this.dialogs = dialogs; this.preferences = preferences;
        var today = DateTime.Today.ToString("MM-dd-yyyy", CultureInfo.InvariantCulture);
        start = preferences.Read("rangeStart", today); end = preferences.Read("rangeEnd", today);
        if (start.Length == 0) start = today; if (end.Length == 0) end = today;
        uint.TryParse(preferences.Read("defaultWeekday", "5"), out var saved);
        weekday = Array.IndexOf(Days, saved); if (weekday < 0) weekday = 3;
        CreateCommand = new(() => OperationTask = Create(), () => Editable && plan != null && total > 0);
        ChooseCommand = new(() => OperationTask = Choose(), () => Editable);
        CancelCommand = new(() => operation?.Cancel(), () => busy);
    }
    private bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value; Changed(name); return true;
    }
    private void Changed([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new(name));
    private void State()
    {
        foreach (var name in new[] { nameof(Busy), nameof(Editable), nameof(IsWorking), nameof(Count), nameof(Total), nameof(ProgressMaximum), nameof(Preview), nameof(Destination), nameof(HasDestination) }) Changed(name);
        CreateCommand.Changed(); ChooseCommand.Changed(); CancelCommand.Changed();
    }
    internal void Refresh() => RefreshTask = RefreshAsync();
    private async Task RefreshAsync()
    {
        if (!Editable || weekday < 0 || weekday >= Days.Length) return;
        var current = ++generation; plan = null; Preview = Array.Empty<string>(); State();
        var first = start; var last = end; var day = Days[weekday]; var query = search;
        try
        {
            var result = await Task.Run(() =>
            {
                var exact = Core.Call(new { op = "exact", start = first, end = last });
                var input = new PlanInput(exact.GetProperty("start").GetString()!, exact.GetProperty("end").GetString()!, day);
                return (input, value: Core.Call(new { op = "plan", plan = input, search = query }));
            });
            if (closed || current != generation) return;
            plan = result.input; total = result.value.GetProperty("count").GetInt32();
            confirmation = result.value.GetProperty("requiresConfirmation").GetBoolean();
            Preview = result.value.GetProperty("dates").EnumerateArray().Select(v => v.GetString()!).ToArray();
            Status = total == 0 ? "No matching dates. Choose a wider range or another weekday." : Preview.Count == 0 ? "No search matches. Creation still uses the full date range." : "Review your folders, then choose Create Folders.";
        }
        catch (Exception e) { if (current == generation && !closed) Status = e.Message; }
        if (current == generation && !closed) State();
    }
    internal async Task<T> Dialog<T>(Func<Task<T>> show)
    {
        dialogOpen = true; State();
        try { return await show(); }
        finally { dialogOpen = false; State(); }
    }
    internal async Task Message(string title, string message)
    {
        if (closed) return;
        await Dialog(async () => { await dialogs.Message(title, message); return true; });
    }
    internal async Task<bool> Confirm(string title, string message, string accept = "Continue") =>
        !closed && await Dialog(() => dialogs.Confirm(title, message, accept));
    internal async Task<bool> Choose()
    {
        if (!Editable) return false;
        try
        {
            var folder = await Dialog(dialogs.ChooseFolder);
            if (folder == null || closed) return false;
            destination = folder; State(); return true;
        }
        catch (Exception e) { Status = "The folder could not be opened. " + e.Message; return false; }
    }
    internal async Task Create()
    {
        if (!CreateCommand.CanExecute(null)) return;
        // Capture validated intent before any asynchronous dialog.
        var snapshot = plan!; var count = total;
        if (confirmation && !await Confirm($"Create {count:N0} folders?", "Existing folders and their contents will be preserved. You can cancel while this runs.")) return;
        if (closed || (destination == null && !await Choose())) return;
        busy = true; ++generation; operation = new(); acceptingProgress = true; Processed = 0; State();
        try
        {
            var progress = new Progress<JsonElement>(r =>
            {
                if (!closed && busy && acceptingProgress)
                {
                    Processed = r.GetProperty("created").GetInt32() + r.GetProperty("existing").GetInt32();
                    Status = $"Processed {Processed:N0} of {count:N0} folders.";
                }
            });
            var result = await Core.Create(snapshot, destination!, operation.Token, progress);
            acceptingProgress = false;
            if (closed) return;
            Processed = result.GetProperty("created").GetInt32() + result.GetProperty("existing").GetInt32();
            Status = Core.Summary(result);
            await Message(result.GetProperty("failureReason").ValueKind == JsonValueKind.String ? "Folder Creation Stopped" : result.GetProperty("cancelled").GetBoolean() ? "Creation Canceled" : "Complete", Status);
        }
        catch (Exception e) { if (!closed) { Status = e.Message; await Message("Folder Creation Stopped", Status); } }
        finally { acceptingProgress = false; busy = false; operation.Dispose(); operation = null; State(); }
    }
    internal void Close()
    {
        closed = true; ++generation; operation?.Cancel();
        try { preferences.Write("rangeStart", start); preferences.Write("rangeEnd", end); }
        catch (Exception e) { System.Diagnostics.Trace.TraceWarning(e.Message); }
        State();
    }
}
