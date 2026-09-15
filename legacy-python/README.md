# E.W.A.F. - Every Week a Folder

E.W.A.F. is a small desktop app that creates one folder for every selected
weekday in an inclusive date range. Folder names use `MM-DD-YYYY`.

## Requirements

- Python 3.10 or newer
- Tkinter (included with standard Python installers on macOS and Windows)

## Setup

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

On Linux, install your distribution's Tk package if `import tkinter` fails
(for example, `python3-tk` on Debian or Ubuntu).

## Run

```bash
python ewaf.py
```

Choose the start date, end date, and weekday, then select an existing destination
directory. E.W.A.F. reports how many folders it created and how many were already
present. If an operation fails, it stops and reports the partial progress so the
operation can be retried safely.

## Test

```bash
python -m unittest discover -s tests -v
```
