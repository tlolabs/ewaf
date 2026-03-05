import os
import tkinter as tk
from tkinter import filedialog, messagebox
from datetime import datetime, timedelta

try:
    from tkcalendar import Calendar
except ImportError:
    Calendar = None


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
            folder_name = current_date.strftime("%m-%d-%Y")
            folders_to_create.append(folder_name)
        current_date += timedelta(days=1)
    return folders_to_create

def run():
    if Calendar is None:
        messagebox.showerror(
            "Missing Dependency",
            "Please install tkcalendar:\n\npip install tkcalendar",
        )
        return

    try:
        start_date = datetime.strptime(start_date_var.get(), "%m-%d-%Y")
        end_date = datetime.strptime(end_date_var.get(), "%m-%d-%Y")
    except ValueError:
        messagebox.showerror("Invalid Date", "Dates must use MM-DD-YYYY.")
        return

    if start_date > end_date:
        messagebox.showerror("Invalid Date Range", "Start date must be before end date.")
        return

    weekday = weekdays.index(weekday_var.get())
    directory = filedialog.askdirectory()

    if directory:
        folders = generate_folder_dates(start_date, end_date, weekday)
        create_folders(directory, folders)
        messagebox.showinfo("Complete", f"Processed {len(folders)} folder(s).")


def show_calendar(target_var):
    global active_date_var
    active_date_var = target_var
    try:
        selected = datetime.strptime(target_var.get(), "%m-%d-%Y").date()
        calendar_widget.selection_set(selected)
    except ValueError:
        pass
    calendar_frame.grid()


def apply_calendar_date():
    if active_date_var is None:
        return
    selected = calendar_widget.selection_get()
    active_date_var.set(selected.strftime("%m-%d-%Y"))
    calendar_frame.grid_remove()


def hide_calendar():
    calendar_frame.grid_remove()

# GUI setup
root = tk.Tk()
root.title("E.W.A.F. - Every Week a Folder")
root.geometry("640x240")
root.resizable(False, False)

if Calendar is None:
    messagebox.showerror(
        "Missing Dependency",
        "tkcalendar is required for the date picker.\n\nInstall it with:\npip install tkcalendar",
    )
    root.destroy()
    raise SystemExit(1)

main_frame = tk.Frame(root)
main_frame.grid(row=0, column=0, padx=8, pady=8, sticky="nw")

today = datetime.today().strftime("%m-%d-%Y")
start_date_var = tk.StringVar(value=today)
end_date_var = tk.StringVar(value=today)
active_date_var = None

tk.Label(main_frame, text="Start Date (MM-DD-YYYY):").grid(row=0, column=0, sticky="w")
start_date_entry = tk.Entry(main_frame, textvariable=start_date_var, width=14)
start_date_entry.grid(row=0, column=1, padx=8, pady=4, sticky="w")
tk.Button(main_frame, text="Pick", command=lambda: show_calendar(start_date_var)).grid(
    row=0, column=2, padx=4, pady=4
)

tk.Label(main_frame, text="End Date (MM-DD-YYYY):").grid(row=1, column=0, sticky="w")
end_date_entry = tk.Entry(main_frame, textvariable=end_date_var, width=14)
end_date_entry.grid(row=1, column=1, padx=8, pady=4, sticky="w")
tk.Button(main_frame, text="Pick", command=lambda: show_calendar(end_date_var)).grid(
    row=1, column=2, padx=4, pady=4
)

weekdays = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
weekday_var = tk.StringVar(root)
weekday_var.set(weekdays[3])  # Default to Thursday
tk.Label(main_frame, text="Select Day of Week:").grid(row=2, column=0, sticky="w")
weekday_menu = tk.OptionMenu(main_frame, weekday_var, *weekdays)
weekday_menu.grid(row=2, column=1, padx=8, pady=4, sticky="ew")

run_button = tk.Button(main_frame, text="Create Folders", command=run)
run_button.grid(row=3, column=0, columnspan=3, pady=8)

calendar_frame = tk.Frame(root, borderwidth=1, relief="solid")
calendar_frame.grid(row=0, column=1, padx=(0, 8), pady=8, sticky="ne")
calendar_widget = Calendar(calendar_frame, selectmode="day")
calendar_widget.grid(row=0, column=0, columnspan=2, padx=6, pady=6)
tk.Button(calendar_frame, text="Use Date", command=apply_calendar_date).grid(
    row=1, column=0, padx=6, pady=(0, 6), sticky="ew"
)
tk.Button(calendar_frame, text="Close", command=hide_calendar).grid(
    row=1, column=1, padx=6, pady=(0, 6), sticky="ew"
)
calendar_frame.grid_remove()

root.mainloop()
