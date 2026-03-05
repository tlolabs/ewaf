import os
from datetime import datetime, timedelta
from tkinter import filedialog, messagebox

try:
    import ttkbootstrap as ttk
    from ttkbootstrap.widgets import DateEntry
except ImportError:
    ttk = None
    DateEntry = None


DATE_FORMAT = "%m-%d-%Y"


def create_folders(base_path, folders):
    for folder in folders:
        path = os.path.join(base_path, folder)
        if not os.path.exists(path):
            os.makedirs(path)
            print(f"Folder created: {path}")
        else:
            print(f"Folder already exists: {path}")


def generate_folder_dates(start_date, end_date, weekday):
    folders_to_create = []
    current_date = start_date
    while current_date <= end_date:
        if current_date.weekday() == weekday:
            folders_to_create.append(current_date.strftime(DATE_FORMAT))
        current_date += timedelta(days=1)
    return folders_to_create


def run():
    try:
        start_date = datetime.strptime(start_date_entry.entry.get(), DATE_FORMAT)
        end_date = datetime.strptime(end_date_entry.entry.get(), DATE_FORMAT)
    except ValueError:
        messagebox.showerror("Invalid Date", "Dates must use MM-DD-YYYY.")
        return

    if start_date > end_date:
        messagebox.showerror("Invalid Date Range", "Start date must be before end date.")
        return

    weekday = weekdays.index(weekday_var.get())
    directory = filedialog.askdirectory()
    if not directory:
        return

    folders = generate_folder_dates(start_date, end_date, weekday)
    create_folders(directory, folders)
    messagebox.showinfo("Complete", f"Processed {len(folders)} folder(s).")


if ttk is None or DateEntry is None:
    messagebox.showerror(
        "Missing Dependency",
        "ttkbootstrap is required.\n\nInstall it with:\npip install ttkbootstrap",
    )
    raise SystemExit(1)


app = ttk.Window(themename="flatly")
app.title("E.W.A.F. - Every Week a Folder")
app.geometry("460x220")
app.resizable(False, False)

content = ttk.Frame(app, padding=14)
content.pack(fill="both", expand=True)

ttk.Label(content, text="Start Date (MM-DD-YYYY):").grid(row=0, column=0, sticky="w", pady=6)
start_date_entry = DateEntry(
    content,
    dateformat=DATE_FORMAT,
    firstweekday=0,
    bootstyle="primary",
    width=14,
)
start_date_entry.grid(row=0, column=1, sticky="w", padx=8, pady=6)

ttk.Label(content, text="End Date (MM-DD-YYYY):").grid(row=1, column=0, sticky="w", pady=6)
end_date_entry = DateEntry(
    content,
    dateformat=DATE_FORMAT,
    firstweekday=0,
    bootstyle="primary",
    width=14,
)
end_date_entry.grid(row=1, column=1, sticky="w", padx=8, pady=6)

weekdays = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
weekday_var = ttk.StringVar(value="Thursday")
ttk.Label(content, text="Select Day of Week:").grid(row=2, column=0, sticky="w", pady=6)

weekday_menu = ttk.Combobox(
    content,
    textvariable=weekday_var,
    values=weekdays,
    state="readonly",
    width=12,
)
weekday_menu.grid(row=2, column=1, sticky="w", padx=8, pady=6)

run_button = ttk.Button(content, text="Create Folders", command=run, bootstyle="success")
run_button.grid(row=3, column=0, columnspan=2, pady=(14, 0))

app.mainloop()
