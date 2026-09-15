"""Tkinter user interface for E.W.A.F. — Every Week a Folder."""

from __future__ import annotations

import sys
from datetime import date
from time import strptime
from tkinter import filedialog, messagebox

from ewaf_core import (
    DATE_FORMAT,
    WEEKDAYS,
    FolderCreationError,
    create_folders,
    generate_folder_dates,
)

try:
    import ttkbootstrap as ttk
    from ttkbootstrap.widgets import DateEntry
except ImportError:
    ttk = None
    DateEntry = None


APP_TITLE = "E.W.A.F. - Every Week a Folder"
CONFIRM_FOLDER_COUNT = 250


def parse_date(value: str) -> date:
    """Parse one date entry using the format shown in the interface."""

    parsed = strptime(value.strip(), DATE_FORMAT)
    return date(parsed.tm_year, parsed.tm_mon, parsed.tm_mday)


def build_app():
    """Build and return the application window."""

    if ttk is None or DateEntry is None:
        raise RuntimeError("ttkbootstrap is required to build the application")

    app = ttk.Window(themename="flatly")
    app.title(APP_TITLE)
    app.geometry("460x250")
    app.minsize(420, 235)

    content = ttk.Frame(app, padding=16)
    content.pack(fill="both", expand=True)
    content.columnconfigure(1, weight=1)

    ttk.Label(content, text="Start date (MM-DD-YYYY):").grid(
        row=0, column=0, sticky="w", pady=6
    )
    start_date_entry = DateEntry(
        content,
        dateformat=DATE_FORMAT,
        firstweekday=0,
        bootstyle="primary",
        width=14,
    )
    start_date_entry.grid(row=0, column=1, sticky="ew", padx=(12, 0), pady=6)

    ttk.Label(content, text="End date (MM-DD-YYYY):").grid(
        row=1, column=0, sticky="w", pady=6
    )
    end_date_entry = DateEntry(
        content,
        dateformat=DATE_FORMAT,
        firstweekday=0,
        bootstyle="primary",
        width=14,
    )
    end_date_entry.grid(row=1, column=1, sticky="ew", padx=(12, 0), pady=6)

    ttk.Label(content, text="Day of week:").grid(row=2, column=0, sticky="w", pady=6)
    weekday_var = ttk.StringVar(value="Thursday")
    weekday_menu = ttk.Combobox(
        content,
        textvariable=weekday_var,
        values=WEEKDAYS,
        state="readonly",
        width=14,
    )
    weekday_menu.grid(row=2, column=1, sticky="ew", padx=(12, 0), pady=6)

    status_var = ttk.StringVar(value="Choose a date range and a weekday.")
    status_label = ttk.Label(
        content,
        textvariable=status_var,
        anchor="center",
    )
    status_label.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(12, 0))

    def run() -> None:
        try:
            start_date = parse_date(start_date_entry.entry.get())
            end_date = parse_date(end_date_entry.entry.get())
            weekday = WEEKDAYS.index(weekday_var.get())
            folders = generate_folder_dates(start_date, end_date, weekday)
        except ValueError as error:
            if (
                start_date_entry.entry.get().strip()
                and end_date_entry.entry.get().strip()
            ):
                detail = str(error)
            else:
                detail = "Both dates are required."
            messagebox.showerror(
                "Invalid date range",
                f"Enter valid dates using MM-DD-YYYY.\n\n{detail}",
                parent=app,
            )
            status_var.set("Correct the date range and try again.")
            return

        if not folders:
            messagebox.showinfo(
                "No matching dates",
                f"The range contains no {weekday_var.get()}s.",
                parent=app,
            )
            status_var.set("No folders were needed for that range.")
            return

        if len(folders) > CONFIRM_FOLDER_COUNT and not messagebox.askyesno(
            "Create many folders?",
            f"This will process {len(folders):,} folders. Continue?",
            parent=app,
        ):
            status_var.set("Folder creation canceled.")
            return

        directory = filedialog.askdirectory(
            parent=app,
            title="Choose where to create the weekly folders",
            mustexist=True,
        )
        if not directory:
            status_var.set("Folder creation canceled.")
            return

        run_button.configure(state="disabled")
        status_var.set(f"Creating {len(folders):,} folder(s)…")
        app.update_idletasks()
        try:
            result = create_folders(directory, folders)
        except (FolderCreationError, NotADirectoryError, ValueError) as error:
            processed = getattr(error, "result", None)
            processed_count = processed.processed_count if processed else 0
            messagebox.showerror(
                "Folder creation stopped",
                f"Could not complete the operation.\n\n{error}\n\n"
                f"Processed {processed_count:,} of {len(folders):,} folders before the error.",
                parent=app,
            )
            status_var.set("Folder creation stopped because of an error.")
        else:
            summary = (
                f"Created {len(result.created):,} folder(s).\n"
                f"Already existed: {len(result.existing):,}."
            )
            messagebox.showinfo("Complete", summary, parent=app)
            status_var.set(summary.replace("\n", " "))
        finally:
            run_button.configure(state="normal")

    run_button = ttk.Button(
        content,
        text="Create Folders",
        command=run,
        bootstyle="primary",
    )
    run_button.grid(row=3, column=0, columnspan=2, pady=(16, 0))

    start_date_entry.entry.focus_set()
    return app


def main() -> int:
    """Launch the desktop application."""

    if ttk is None or DateEntry is None:
        print(
            "E.W.A.F. requires ttkbootstrap. Install dependencies with "
            "'python -m pip install -r requirements.txt'.",
            file=sys.stderr,
        )
        return 1

    build_app().mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
