import os
import tkinter as tk
from tkinter import filedialog, messagebox
from datetime import datetime, timedelta

try:
    from tkcalendar import DateEntry
except ImportError:
    DateEntry = None


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
    if DateEntry is None:
        messagebox.showerror(
            "Missing Dependency",
            "Please install tkcalendar:\n\npip install tkcalendar",
        )
        return

    start_date = datetime.strptime(start_date_entry.get(), "%m-%d-%Y")
    end_date = datetime.strptime(end_date_entry.get(), "%m-%d-%Y")
    if start_date > end_date:
        messagebox.showerror("Invalid Date Range", "Start date must be before end date.")
        return

    weekday = weekdays.index(weekday_var.get())
    directory = filedialog.askdirectory()

    if directory:
        folders = generate_folder_dates(start_date, end_date, weekday)
        create_folders(directory, folders)
        messagebox.showinfo("Complete", f"Processed {len(folders)} folder(s).")

# GUI setup
root = tk.Tk()
root.title("E.W.A.F. - Every Week a Folder")
root.geometry("360x170")

if DateEntry is None:
    messagebox.showerror(
        "Missing Dependency",
        "tkcalendar is required for the date picker.\n\nInstall it with:\npip install tkcalendar",
    )
    root.destroy()
    raise SystemExit(1)

tk.Label(root, text="Start Date (MM-DD-YYYY):").grid(row=0, column=0)
start_date_entry = DateEntry(root, date_pattern="mm-dd-y")
start_date_entry.grid(row=0, column=1, padx=8, pady=4)

tk.Label(root, text="End Date (MM-DD-YYYY):").grid(row=1, column=0)
end_date_entry = DateEntry(root, date_pattern="mm-dd-y")
end_date_entry.grid(row=1, column=1, padx=8, pady=4)

weekdays = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
weekday_var = tk.StringVar(root)
weekday_var.set(weekdays[3])  # Default to Thursday
tk.Label(root, text="Select Day of Week:").grid(row=2, column=0)
weekday_menu = tk.OptionMenu(root, weekday_var, *weekdays)
weekday_menu.grid(row=2, column=1, padx=8, pady=4, sticky="ew")

run_button = tk.Button(root, text="Create Folders", command=run)
run_button.grid(row=3, column=0, columnspan=2, pady=8)

root.mainloop()
